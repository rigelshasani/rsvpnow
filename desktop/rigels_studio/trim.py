"""Find and remove front/back matter so a book starts and ends where the actual work does.

A book is split into sections at its chapters (plus any untitled opening and Project Gutenberg's
START/END markers). Each section gets a kind and a suggested keep/remove; the user reviews them and
the kept sections are written out as a standalone .rsvp the reader opens directly.
"""
from __future__ import annotations

import re
from dataclasses import asdict, dataclass

from .books import WRAP, Book, parse_rsvp

BODY, COMMENTARY, FRONT, CONTENTS, BACK, GUTENBERG = "body", "commentary", "front matter", "contents", "back matter", "gutenberg"

_FRONT = re.compile(
    r"^(cover|title ?page|half[- ]title|copyright( page)?|also by\b|books by\b|praise( for)?\b|dedication|"
    r"acknowledge?ments?|about the (author|authors|publisher|book)|other books\b|frontispiece|map|maps|"
    r"list of (illustrations|maps|figures)|illustrations|timeline( of history)?)\b", re.I)
_BACK = re.compile(
    r"^((selected |select )?bibliography|index|notes|endnotes|footnotes|sources|references|further reading|glossary|"
    r"about the (author|authors|publisher|book)|also by\b|books by\b|acknowledge?ments?|copyright|colophon|credits|"
    r"permissions|reading group|discussion questions|a conversation with|an excerpt|excerpt from|sneak peek|"
    r"for the best in paperbacks|newsletter|connect with|more from|praise( for)?\b|loved this book|about epubbooks)", re.I)
_CONTENTS = re.compile(r"^(table of )?contents$", re.I)
_COMMENTARY = re.compile(
    r"^(foreword|preface|introduction|translator'?s? (note|preface|introduction)|editor'?s? (note|preface|introduction)|"
    r"a note on (the )?(text|translation)|note on (the )?(text|translation)|author'?s note|afterword|"
    r"introductory note|critical introduction)", re.I)
_BOILERPLATE_TEXT = re.compile(
    r"(all rights reserved|isbn|project gutenberg|epubbooks|published by|printed in|library of congress|"
    r"this ebook is for the use of anyone|without the permission of the publisher)", re.I)

GUTENBERG_START = ("***", "START", "OF")
GUTENBERG_END = ("***", "END", "OF")


@dataclass
class Section:
    title: str
    start: int
    end: int
    kind: str
    reason: str
    keep: bool
    preview: str = ""
    has_chapter: bool = True  # whether the section began with an @chapter marker

    @property
    def words(self) -> int:
        return self.end - self.start


def _find_marker(words: list[str], marker: tuple[str, ...], from_end: bool = False) -> int | None:
    """Word index just after the line containing a Gutenberg *** START/END OF ... *** marker."""
    upper = [w.upper() for w in words]
    indices = range(len(words) - len(marker), -1, -1) if from_end else range(len(words) - len(marker) + 1)
    for i in indices:
        if tuple(upper[i : i + len(marker)]) == marker and any("GUTENBERG" in w for w in upper[i : i + 12]):
            if from_end:
                return i
            # Skip to the closing *** of the marker line.
            for j in range(i + 1, min(i + 40, len(words))):
                if words[j].endswith("***"):
                    return j + 1
            return i + len(marker)
    return None


def _boundaries(book: Book) -> list[tuple[int, str, bool]]:
    """Sorted section starts: (word index, title, starts at a chapter marker)."""
    starts = [(index, title, True) for title, index in book.chapters]
    if not starts or starts[0][0] > 0:
        starts.insert(0, (0, "Opening (before the first chapter)", False))
    start_marker = _find_marker(book.words, GUTENBERG_START)
    end_marker = _find_marker(book.words, GUTENBERG_END, from_end=True)
    if start_marker:
        starts.append((start_marker, " ", False))
    if end_marker:
        starts.append((end_marker, "Project Gutenberg licence", False))
    merged: dict[int, tuple[int, str, bool]] = {}
    for index, title, chapter in sorted(starts, key=lambda s: (s[0], not s[2])):
        merged.setdefault(index, (index, title, chapter))
    return sorted(merged.values())


def _title_words(book: Book) -> set[str]:
    return {w.strip(".,:;—-").lower() for title, _ in book.chapters for w in title.split() if len(w) > 2}


def analyze(book: Book) -> list[Section]:
    n = len(book.words)
    if n == 0:
        return []
    bounds = _boundaries(book)
    start_marker = _find_marker(book.words, GUTENBERG_START)
    end_marker = _find_marker(book.words, GUTENBERG_END, from_end=True)
    title_words = _title_words(book)
    sections: list[Section] = []
    for k, (start, title, chapter) in enumerate(bounds):
        end = bounds[k + 1][0] if k + 1 < len(bounds) else n
        if end <= start:
            continue
        words = book.words[start:end]
        text = " ".join(words[:400])
        clean_title = title.strip()
        # A continuation after a Gutenberg START marker inherits the chapter it splits.
        if clean_title == "" and sections:
            clean_title = sections[-1].title
        kind, reason = BODY, ""
        position = start / n
        if (start_marker and end <= start_marker) or (end_marker and start >= end_marker):
            kind, reason = GUTENBERG, "Project Gutenberg header/licence"
        elif "project gutenberg" in clean_title.lower():
            kind, reason = GUTENBERG, "Project Gutenberg title block"
        elif _CONTENTS.match(clean_title):
            kind, reason = CONTENTS, "Table of contents"
        elif _COMMENTARY.match(clean_title) and position < 0.25:
            kind, reason = COMMENTARY, "Commentary before the text"
        elif _COMMENTARY.match(clean_title) and position > 0.75 and clean_title.lower().startswith("afterword"):
            kind, reason = COMMENTARY, "Commentary after the text"
        elif position < 0.2 and _FRONT.match(clean_title):
            kind, reason = FRONT, "Front matter"
        elif position > 0.6 and _BACK.match(clean_title):
            kind, reason = BACK, "Back matter"
        elif not chapter and start == 0 and len(words) < 0.1 * n:
            listed = sum(1 for w in words if w.strip(".,:;—-").lower() in title_words)
            if listed > 0.35 * len(words):
                kind, reason = CONTENTS, "Lists the chapter titles"
            else:
                kind, reason = FRONT, "Text before the first chapter"
        elif len(words) < 1500 and len(_BOILERPLATE_TEXT.findall(text)) >= 2 and (position < 0.15 or position > 0.85):
            kind, reason = (FRONT if position < 0.5 else BACK), "Publisher boilerplate (copyright, ISBN, ads)"
        sections.append(Section(
            title=clean_title, start=start, end=end, kind=kind, reason=reason, keep=kind in (BODY, COMMENTARY),
            preview=" ".join(words[:28]), has_chapter=chapter,
        ))
    return sections


def kept_ranges(sections: list[Section], keep: list[bool]) -> list[tuple[int, int]]:
    ranges: list[tuple[int, int]] = []
    for section, flag in zip(sections, keep):
        if not flag:
            continue
        if ranges and ranges[-1][1] == section.start:
            ranges[-1] = (ranges[-1][0], section.end)
        else:
            ranges.append((section.start, section.end))
    return ranges


def map_position(index: int, ranges: list[tuple[int, int]]) -> int:
    """Where an old word index lands in the trimmed book (start of the next kept text if it was cut)."""
    offset = 0
    for start, end in ranges:
        if index < start:
            return offset
        if index < end:
            return offset + index - start
        offset += end - start
    return max(0, offset - 1)


def _escape(line: str) -> str:
    return "@" + line if line.startswith("@") else line


def _spell(word: str) -> str:
    """Text that tokenizes back to `word`. The tokenizer glues each standalone "..." onto the previous
    word (so "evidence...." came from "evidence. ..."); writing every trailing "..." as its own spaced
    group reproduces the same token."""
    groups = 0
    while word.endswith("...") and word[:-3].rstrip("."):
        word = word[:-3]
        groups += 1
    return word + " ..." * groups


def _wrap(words: list[str]) -> list[str]:
    lines, current = [], ""
    for word in map(_spell, words):
        if current and len(current) + 1 + len(word) > WRAP:
            lines.append(current)
            current = word
        else:
            current = f"{current} {word}" if current else word
    if current:
        lines.append(current)
    return lines


def render_rsvp(book: Book, sections: list[Section], keep: list[bool], language: str = "") -> tuple[str, list[str]]:
    """Render the kept sections as an .rsvp document; also returns the words it should read back as."""
    paragraph_starts = set(book.paragraphs)
    chapter_titles = {index: title for title, index in book.chapters}
    out = ["@rsvp 1", f"@title {book.title}", f"@source {book.source}"]
    if book.author:
        out.append(f"@author {book.author}")
    out.append("@converter rigels-studio-trim-1")
    if language:
        out.append(f"@language {language}")
    expected: list[str] = []
    started = False
    for section, flag in zip(sections, keep):
        if not flag:
            continue
        paragraph: list[str] = []

        def flush() -> None:
            nonlocal started
            if paragraph:
                if started:
                    out.extend(["", "@para"])
                out.extend(_escape(line) for line in _wrap(paragraph))
                started = True
                paragraph.clear()

        for i in range(section.start, section.end):
            if i in chapter_titles and (i != section.start or section.has_chapter):
                flush()
                out.extend(["", f"@chapter {chapter_titles[i]}"])
                started = False
            elif i in paragraph_starts:
                flush()
            paragraph.append(book.words[i])
            expected.append(book.words[i])
        flush()
    return "\n".join(out) + "\n", expected


def write_rsvp(book: Book, sections: list[Section], keep: list[bool], language: str = "") -> str:
    """render_rsvp, verified to re-read as exactly the kept words."""
    document, expected = render_rsvp(book, sections, keep, language)
    if parse_rsvp(document).words != expected:
        raise ValueError("The trimmed text did not round-trip exactly; refusing to write it.")
    return document


def summary(sections: list[Section]) -> list[dict]:
    return [{**asdict(s), "words": s.words} for s in sections]
