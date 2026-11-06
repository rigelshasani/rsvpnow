"""Save trimmed books: onto the reader's SD card (moving the originals aside) or into Downloads.

The reader scans only the files directly in /library/books, so originals moved into
/library/books/originals stay on the card as a backup without showing up twice in the library.
Reading positions are carried over through the companion API once the reader has indexed the new file.
"""
from __future__ import annotations

import json
import re
import shutil
import tomllib
from pathlib import Path

from . import device, trim
from .books import Book

STATE_DIR = Path.home() / ".rigels_studio"
PENDING = STATE_DIR / "pending-positions.json"
TRIM_MARKER = "@converter rigels-studio-trim-"


def card_books_folder(path: Path | None = None) -> Path | None:
    """The card's library/books folder containing `path`, else the first mounted card's."""
    if path is not None:
        for parent in path.parents:
            if parent.name == "books" and parent.parent.name == "library" and str(parent).startswith("/Volumes/"):
                return parent
    volumes = Path("/Volumes")
    for volume in sorted(volumes.iterdir()) if volumes.is_dir() else []:
        folder = volume / "library" / "books"
        if folder.is_dir():
            return folder
    return None


def safe_name(title: str) -> str:
    name = re.sub(r'[\\/:*?"<>|\x00-\x1f]', "", title).strip(" .") or "Book"
    return name[:120]


def _saved_position(folder: Path, stem: str) -> int | None:
    state = folder / f"{stem}.rstate.toml"
    try:
        return int(tomllib.loads(state.read_text(encoding="utf-8"))["reading"]["wordIndex"])
    except (OSError, KeyError, ValueError, tomllib.TOMLDecodeError):
        return None


def _unique(path: Path) -> Path:
    candidate, n = path, 2
    while candidate.exists():
        candidate = path.with_name(f"{path.stem} ({n}){path.suffix}")
        n += 1
    return candidate


def _is_ours(path: Path) -> bool:
    try:
        with path.open(encoding="utf-8", errors="replace") as file:
            return any(line.startswith(TRIM_MARKER) for _, line in zip(range(8), file))
    except OSError:
        return False


def remember_position(file_name: str, word_index: int) -> None:
    STATE_DIR.mkdir(exist_ok=True)
    pending = load_pending()
    pending[file_name] = word_index
    PENDING.write_text(json.dumps(pending, indent=1))


def load_pending() -> dict[str, int]:
    try:
        return json.loads(PENDING.read_text())
    except (OSError, ValueError):
        return {}


def forget_position(file_name: str) -> None:
    pending = load_pending()
    if pending.pop(file_name, None) is not None:
        PENDING.write_text(json.dumps(pending, indent=1))


def save(source: Path, book: Book, keep: list[bool], target: str) -> dict:
    sections = trim.analyze(book)
    if len(keep) != len(sections):
        raise ValueError("The section list changed; reload the book and try again.")
    if not any(keep):
        raise ValueError("Keep at least one section.")
    document = trim.write_rsvp(book, sections, keep, book.language)
    kept = sum(s.words for s, k in zip(sections, keep) if k)
    name = safe_name(book.title) + ".rsvp"
    result = {"keptWords": kept, "removedWords": len(book.words) - kept, "moved": [], "position": None}

    if target == "downloads":
        destination = _unique(Path.home() / "Downloads" / name)
        destination.write_text(document, encoding="utf-8")
        result["path"] = str(destination)
        return result

    folder = card_books_folder(source)
    if folder is None:
        raise ValueError("No reader SD card is mounted on this Mac. Insert the card, or save to Downloads.")

    destination = folder / name
    if source.parent == folder:
        # Book already on the card: back up its source, converted cache, index and progress.
        stem = source.name[: -len(source.suffix)]
        old_position = _saved_position(folder, stem)
        originals = folder / "originals"
        originals.mkdir(exist_ok=True)
        for file in sorted(folder.iterdir()):
            if file.is_file() and file.name.startswith(stem + "."):
                target_path = _unique(originals / file.name)
                shutil.move(str(file), target_path)
                result["moved"].append(file.name)
        if old_position:
            new_position = trim.map_position(old_position, trim.kept_ranges(sections, keep))
            remember_position(name, new_position)
            result["position"] = {"from": old_position, "to": new_position}
    if destination.exists() and not _is_ours(destination):
        destination = _unique(destination)

    partial = folder / f".{destination.name}.partial"
    partial.write_text(document, encoding="utf-8")
    partial.replace(destination)
    result["path"] = str(destination)
    return result


def send_to_reader(book: Book, reader: "device.Reader", original: bytes | None = None) -> dict:
    """Install a book on the reader over USB as .rsvp, trimmed with the suggested sections.

    The reader's companion API only accepts converted text, so EPUB and TXT are converted here
    with the same rules as the firmware. An .rsvp file (`original`) is sent unchanged.
    """
    if original is not None:
        document, kept = original, len(book.words)
    else:
        sections = trim.analyze(book)
        keep = [s.keep for s in sections] if any(s.keep for s in sections) else [True] * len(sections)
        document = trim.write_rsvp(book, sections, keep, book.language).encode("utf-8")
        kept = sum(s.words for s, k in zip(sections, keep) if k)
    name = safe_name(book.title) + ".rsvp"
    status, data = reader.request("POST", "/api/v2/library", document, {"name": name, "category": "book"})
    if status == 409:
        raise ValueError(f"{book.title} is already on the reader.")
    if status >= 400:
        raise ValueError((data or {}).get("message", f"The reader refused the book (HTTP {status})."))
    return {"title": book.title, "name": name, "words": kept, "removedWords": len(book.words) - kept}
