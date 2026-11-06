"""Load books into the reader's word stream: .rsvp files exactly, EPUB and text per docs/conversion/spec.md.

An .rsvp file (including the reader's own converted cache on the SD card) gives the exact words the
device shows. EPUB conversion here follows the spec but is independent of the firmware converter, so
rare markup edge cases can differ by a few words; the library view reports the device's own word
count next to ours whenever the reader has converted the book.
"""
from __future__ import annotations

import html
import posixpath
import re
import zipfile
from dataclasses import dataclass, field
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote
from xml.etree import ElementTree

from .text import tokenize_line

WRAP = 96


@dataclass
class Book:
    title: str
    author: str = ""
    words: list[str] = field(default_factory=list)
    paragraphs: list[int] = field(default_factory=list)  # word index where each paragraph starts
    chapters: list[tuple[str, int]] = field(default_factory=list)  # (title, first word index)
    source: str = ""
    language: str = ""

    def to_json(self) -> dict:
        return {
            "title": self.title, "author": self.author, "source": self.source, "words": self.words,
            "paragraphs": self.paragraphs, "chapters": [{"title": t, "wordIndex": i} for t, i in self.chapters],
        }


# --- .rsvp ---------------------------------------------------------------------------------------

def parse_rsvp(text: str, source: str = "") -> Book:
    book = Book(title=Path(source).stem, source=source)
    new_paragraph = True
    for line in text.split("\n"):
        if line.startswith("@") and not line.startswith("@@"):
            name, _, value = line[1:].partition(" ")
            if name == "title":
                book.title = value
            elif name == "author":
                book.author = value
            elif name == "language" and not book.language:
                book.language = value
            elif name == "chapter":
                book.chapters.append((value, len(book.words)))
                new_paragraph = True
            elif name == "para":
                new_paragraph = True
            continue
        tokens = tokenize_line(line[1:] if line.startswith("@@") else line)
        if tokens and new_paragraph:
            book.paragraphs.append(len(book.words))
            new_paragraph = False
        book.words.extend(tokens)
    return book


def load_rsvp(path: Path) -> Book:
    return parse_rsvp(path.read_bytes().decode("utf-8", errors="replace"), path.name)


# --- HTML / XHTML ---------------------------------------------------------------------------------

IGNORED = {"head", "math", "nav", "script", "style", "svg", "title"}
BLOCKS = {
    "address", "article", "aside", "blockquote", "body", "br", "caption", "dd", "div", "dl", "dt", "figcaption",
    "figure", "footer", "h1", "h2", "h3", "h4", "h5", "h6", "header", "hr", "li", "main", "ol", "p", "pre",
    "section", "table", "td", "th", "tr", "ul",
}
HEADINGS = {"h1", "h2", "h3", "h4", "h5", "h6"}
VOID = {"br", "hr", "img", "meta", "link", "input", "col", "area", "base", "wbr", "source", "embed"}


class _Content(HTMLParser):
    """Turns one content document into paragraphs, noting headings and anchor positions."""

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.paragraphs: list[tuple[str, str | None]] = []  # (text, heading tag or None)
        self.anchors: dict[str, int] = {}  # id -> paragraph index where it appears
        self.parts: list[str] = []
        self.ignore_depth = 0
        self.heading: str | None = None

    def _flush(self) -> None:
        text = " ".join("".join(self.parts).split())
        self.parts.clear()
        if text:
            self.paragraphs.append((text, self.heading))

    def handle_starttag(self, tag, attrs):
        tag = tag.lower().rpartition(":")[2]
        # Like the firmware, drop Project Gutenberg's header and licence blocks whole.
        boilerplate = "pg-boilerplate" in (dict(attrs).get("class") or "").split()
        if self.ignore_depth or tag in IGNORED or boilerplate:
            if tag not in VOID:
                self.ignore_depth += 1
            return
        if tag in BLOCKS:
            self._flush()
            if tag in HEADINGS:
                self.heading = tag
        for name, value in attrs:
            if name in ("id", "name") and value and value not in self.anchors:
                self.anchors[value] = len(self.paragraphs) + (1 if self.parts else 0)

    def handle_startendtag(self, tag, attrs):
        tag = tag.lower().rpartition(":")[2]
        if self.ignore_depth:
            return
        if tag in BLOCKS:
            self._flush()
        for name, value in attrs:
            if name in ("id", "name") and value and value not in self.anchors:
                self.anchors[value] = len(self.paragraphs)

    def handle_endtag(self, tag):
        tag = tag.lower().rpartition(":")[2]
        if self.ignore_depth:
            if tag in IGNORED or tag not in VOID:
                self.ignore_depth -= 1
            return
        if tag in BLOCKS:
            self._flush()
            if tag in HEADINGS:
                self.heading = None

    def handle_data(self, data):
        if not self.ignore_depth:
            self.parts.append(data)

    def close(self):
        super().close()
        self._flush()


def parse_html(markup: str) -> _Content:
    content = _Content()
    content.feed(markup)
    content.close()
    return content


def _wrap(text: str) -> list[str]:
    """Wrap at 96 characters without splitting words, as the converters do."""
    lines, current = [], ""
    for word in text.split(" "):
        if current and len(current) + 1 + len(word) > WRAP:
            lines.append(current)
            current = word
        else:
            current = f"{current} {word}" if current else word
    if current:
        lines.append(current)
    return lines


class _Builder:
    def __init__(self, book: Book) -> None:
        self.book = book

    def chapter(self, title: str) -> None:
        if self.book.chapters and self.book.chapters[-1][1] == len(self.book.words):
            self.book.chapters[-1] = (title, len(self.book.words))
        else:
            self.book.chapters.append((title, len(self.book.words)))

    def paragraph(self, text: str) -> None:
        tokens = [token for line in _wrap(text) for token in tokenize_line(line)]
        if tokens:
            self.book.paragraphs.append(len(self.book.words))
            self.book.words.extend(tokens)


def load_text(path: Path) -> Book:
    book = Book(title=path.stem, source=path.name)
    builder = _Builder(book)
    text = path.read_bytes().decode("utf-8", errors="replace")
    builder.chapter(book.title)
    for block in re.split(r"\n\s*\n", text):
        cleaned = " ".join(block.split())
        if not cleaned:
            continue
        if len(cleaned) <= 80 and re.match(r"^(#+\s|chapter\b|part\b|book\b)", cleaned, re.I):
            builder.chapter(cleaned.lstrip("# "))
        else:
            builder.paragraph(cleaned)
    return book


# --- EPUB -----------------------------------------------------------------------------------------

class UnsupportedBook(ValueError):
    pass


def _local(tag: str) -> str:
    return tag.rpartition("}")[2]


def _resolve(base: str, href: str) -> tuple[str, str]:
    path, _, fragment = unquote(href).partition("#")
    return posixpath.normpath(posixpath.join(posixpath.dirname(base), path)) if path else base, fragment


def _flatten_nav(element, base: str, out: list) -> None:
    for li in [c for c in element if _local(c.tag) == "li"]:
        link = next((c for c in li.iter() if _local(c.tag) in ("a", "span")), None)
        if link is not None:
            label = " ".join("".join(link.itertext()).split())
            href = link.get("href")
            if label and href:
                out.append((label, *_resolve(base, href)))
        for sub in [c for c in li if _local(c.tag) == "ol"]:
            _flatten_nav(sub, base, out)


def _toc(archive: zipfile.ZipFile, opf_path: str, package, manifest: dict) -> list[tuple[str, str, str]]:
    """Flattened (label, document path, fragment) entries: NCX for EPUB2, nav document for EPUB3."""
    version = package.get("version", "2.0")
    entries: list[tuple[str, str, str]] = []

    def from_nav():
        nav = next((item for item in manifest.values() if "nav" in item["properties"].split()), None)
        if not nav:
            return []
        root = ElementTree.fromstring(archive.read(nav["path"]))
        found = []
        for element in root.iter():
            if _local(element.tag) == "nav" and any(v == "toc" for k, v in element.attrib.items() if k.endswith("type")):
                for ol in [c for c in element if _local(c.tag) == "ol"]:
                    _flatten_nav(ol, nav["path"], found)
        return found

    def from_ncx():
        spine = package.find("{*}spine")
        ncx_id = spine.get("toc") if spine is not None else None
        ncx = manifest.get(ncx_id) or next((i for i in manifest.values() if i["type"] == "application/x-dtbncx+xml"), None)
        if not ncx:
            return []
        root = ElementTree.fromstring(archive.read(ncx["path"]))
        found = []
        for point in root.iter():
            if _local(point.tag) != "navPoint":
                continue
            label = point.find("{*}navLabel/{*}text")
            content = point.find("{*}content")
            if label is not None and content is not None and content.get("src"):
                found.append((" ".join("".join(label.itertext()).split()), *_resolve(ncx["path"], content.get("src"))))
        return found

    try:
        entries = from_nav() if version.startswith("3") else from_ncx()
        if not entries:
            entries = from_ncx() if version.startswith("3") else from_nav()
    except ElementTree.ParseError:
        entries = []
    return entries


SKIP_LABEL = re.compile(r"^(cover|title ?page|table of contents|contents)$", re.I)


def load_epub(path: Path) -> Book:
    with zipfile.ZipFile(path) as archive:
        names = set(archive.namelist())
        if "META-INF/encryption.xml" in names:
            raise UnsupportedBook("This EPUB is encrypted (DRM or obfuscated fonts); the reader cannot convert it.")
        container = ElementTree.fromstring(archive.read("META-INF/container.xml"))
        opf_path = next(e.get("full-path") for e in container.iter() if _local(e.tag) == "rootfile")
        package = ElementTree.fromstring(archive.read(opf_path))
        metadata = package.find("{*}metadata")

        def meta(name: str) -> str:
            element = metadata.find("{http://purl.org/dc/elements/1.1/}" + name) if metadata is not None else None
            return " ".join("".join(element.itertext()).split()) if element is not None else ""

        manifest = {}
        for item in package.iterfind("{*}manifest/{*}item"):
            manifest[item.get("id")] = {
                "path": _resolve(opf_path, item.get("href", ""))[0],
                "type": item.get("media-type", ""),
                "properties": item.get("properties", ""),
            }
        spine = [manifest[ref.get("idref")] for ref in package.iterfind("{*}spine/{*}itemref") if ref.get("idref") in manifest]
        is_content = lambda i: i["type"] in ("application/xhtml+xml", "text/html") or i["path"].endswith((".xhtml", ".html", ".htm"))
        documents = [i["path"] for i in (spine or manifest.values()) if is_content(i) and i["path"] in names]

        title = meta("title") or path.stem
        book = Book(title=title, author=meta("creator"), source=path.name, language=meta("language"))
        builder = _Builder(book)
        toc = [(label, doc, frag) for label, doc, frag in _toc(archive, opf_path, package, manifest)
               if not SKIP_LABEL.match(label) and label.casefold() != title.casefold()
               and not frag.startswith(("pg-header", "pg-footer"))]
        toc_documents = {doc for _, doc, _ in toc}

        for document in documents:
            content = parse_html(archive.read(document).decode("utf-8", errors="replace"))
            # Chapter starts for this document, keyed by the paragraph they precede.
            starts: dict[int, str] = {}
            for label, doc, fragment in toc:
                if doc == document:
                    starts.setdefault(content.anchors.get(fragment, 0) if fragment else 0, label)
            mapped = document in toc_documents
            for index, (text, heading) in enumerate(content.paragraphs):
                if index in starts:
                    builder.chapter(starts[index])
                if heading:
                    # TOC-mapped documents already carry the chapter label; skip duplicate headings.
                    if not toc or (not mapped and heading in ("h1", "h2", "h3")):
                        builder.chapter(text)
                        continue
                    if mapped and index in starts and len(text) <= 80:
                        continue
                builder.paragraph(text)
        if not book.chapters:
            book.chapters.append((title, 0))
        return book


def load_book(path: Path) -> Book:
    suffix = path.suffix.lower()
    if suffix == ".rsvp":
        return load_rsvp(path)
    if suffix == ".epub":
        return load_epub(path)
    if suffix in (".txt", ".md", ".markdown"):
        return load_text(path)
    raise UnsupportedBook(f"{path.suffix} files are not supported yet.")
