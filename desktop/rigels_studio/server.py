"""Local web server for Rigels Studio: serves the UI, converts books, and relays settings to the reader."""
from __future__ import annotations

import hashlib
import json
import mimetypes
import tempfile
import threading
import webbrowser
from functools import lru_cache
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

from . import device, publish, trim
from .books import Book, UnsupportedBook, load_book
from .text import tokenize_line
from .web_limits import clamp_settings

WEB = Path(__file__).parent / "web"
FORMATS = {".epub", ".rsvp", ".txt", ".md", ".markdown"}
TEXT_FORMATS = {".txt", ".md", ".markdown"}
MIN_TEXT_BOOK_BYTES = 20_000  # skip readmes, notes and torrent leftovers
HOME = Path.home()
SCAN = [  # (folder, label, depth)
    (HOME / "Downloads", "Downloads", 2),
    (HOME / "Documents", "Documents", 2),
    (HOME / "Desktop", "Desktop", 1),
]


def _card_folders() -> list[tuple[Path, str, int]]:
    """Mounted SD cards that hold a reader library."""
    volumes = Path("/Volumes")
    if not volumes.is_dir():
        return []
    return [(v / "library" / "books", f"SD card ({v.name})", 1) for v in volumes.iterdir() if (v / "library" / "books").is_dir()]


def _walk(folder: Path, depth: int):
    try:
        for entry in folder.iterdir():
            if entry.name.startswith("."):
                continue
            suffix = entry.suffix.lower()
            if entry.is_file() and suffix in FORMATS:
                if suffix not in TEXT_FORMATS or entry.stat().st_size >= MIN_TEXT_BOOK_BYTES:
                    yield entry
            elif entry.is_dir() and depth > 1 and not entry.name.endswith((".app", ".photoslibrary")):
                yield from _walk(entry, depth - 1)
    except (PermissionError, FileNotFoundError):
        return


_paths: dict[str, Path] = {}


def library() -> list[dict]:
    books = []
    for folder, label, depth in _card_folders() + SCAN:
        files = list(_walk(folder, depth)) if folder.is_dir() else []
        # On the card the reader's .rsvp cache sits next to the source; it is the exact word stream.
        cached = {f.with_suffix("") for f in files if f.suffix == ".rsvp"}
        for path in sorted(files, key=lambda p: p.name.lower()):
            if path.suffix != ".rsvp" and path.with_suffix("") in cached:
                continue
            book_id = hashlib.sha1(str(path).encode()).hexdigest()[:12]
            _paths[book_id] = path
            books.append({
                "id": book_id, "name": path.stem, "format": path.suffix.lstrip(".").lower(), "location": label,
                "exact": path.suffix == ".rsvp", "bytes": path.stat().st_size, "path": str(path).replace(str(HOME), "~"),
            })
    return books


@lru_cache(maxsize=6)
def _load(path: Path, mtime: float) -> Book:
    return load_book(path)


@lru_cache(maxsize=6)
def _book_json(path: Path, mtime: float) -> bytes:
    return json.dumps(_load(path, mtime).to_json(), ensure_ascii=False, separators=(",", ":")).encode()


def book_for(book_id: str) -> tuple[Path, Book]:
    path = _paths.get(book_id)
    if not path or not path.exists():
        raise LookupError("Book not found. Refresh the library.")
    return path, _load(path, path.stat().st_mtime)


def sections_for(book_id: str) -> dict:
    path, book = book_for(book_id)
    return {
        "sections": trim.summary(trim.analyze(book)),
        "onCard": publish.card_books_folder(path) is not None and path.parent == publish.card_books_folder(path),
        "cardMounted": publish.card_books_folder() is not None,
        "fileName": publish.safe_name(book.title) + ".rsvp",
    }


# Reader connections are opened per action so the reader stays usable between them.
_device_lock = threading.Lock()


def read_device() -> dict:
    with _device_lock, device.Reader() as reader:
        info = reader.get("/api/v2/device")
        reading = reader.get("/api/v2/settings")["reading"]
        books = reader.get("/api/v2/library")
        restored = _restore_positions(reader, books)
    return {
        "restored": restored,
        "info": info,
        "settings": {"wpm": reading["wpm"], **reading["pacing"]},
        "mode": reading["mode"],
        "library": [
            {"title": b["metadata"]["title"], "wordCount": b["metadata"]["wordCount"], "name": b["name"]} for b in books
        ],
    }


def _restore_positions(reader: device.Reader, books: list[dict]) -> list[dict]:
    """Carry reading positions over to trimmed books once the reader has indexed them."""
    restored = []
    pending = publish.load_pending()
    for book in books:
        file_name = book["name"].rsplit("/", 1)[-1]
        if file_name in pending and book["metadata"]["wordCount"]:
            body = json.dumps({"wordIndex": pending[file_name]}).encode()
            status, _ = reader.request("PUT", f"/api/v2/library/{book['id']}/position", body)
            if status < 400:
                publish.forget_position(file_name)
                restored.append({"title": book["metadata"]["title"], "wordIndex": pending[file_name]})
    return restored


def write_device(settings: dict) -> dict:
    values = clamp_settings(settings)
    patch = {"wpm": values["wpm"], "pacing": {k: values[k] for k in ("longWordDelayMs", "complexWordDelayMs", "punctuationDelayMs")}}
    with _device_lock, device.Reader() as reader:
        reader.patch("/api/v2/settings/reading", patch)
        reading = reader.get("/api/v2/settings")["reading"]
    return {"wpm": reading["wpm"], **reading["pacing"]}


MAX_UPLOAD_BYTES = 64 * 1024 * 1024


def send_book(name: str, data: bytes) -> dict:
    """Convert a dropped book on this Mac and install it on the reader over USB."""
    suffix = Path(name).suffix.lower()
    if suffix == ".pdf":
        raise ValueError("PDFs convert on the reader: copy them over USB transfer instead.")
    if suffix not in FORMATS:
        raise ValueError("Drop EPUB, TXT, Markdown or RSVP files.")
    with tempfile.TemporaryDirectory() as folder:
        path = Path(folder) / Path(name).name
        path.write_bytes(data)
        book = load_book(path)
    if not book.words:
        raise ValueError("No readable text found in this file.")
    # A slow USB link still moves a 2 MB book in well under a minute.
    with _device_lock, device.Reader(timeout=180) as reader:
        return publish.send_to_reader(book, reader, data if suffix == ".rsvp" else None)


class Handler(BaseHTTPRequestHandler):
    server_version = "RigelsStudio/1"

    def log_message(self, *args) -> None:  # keep the terminal quiet
        pass

    def _send(self, status: int, body: bytes, content_type: str = "application/json") -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _json(self, value, status: int = 200) -> None:
        self._send(status, json.dumps(value).encode())

    def _error(self, status: int, message: str) -> None:
        self._json({"error": message}, status)

    def do_GET(self) -> None:
        path = self.path.split("?", 1)[0]
        if path == "/api/library":
            return self._json(library())
        if path.startswith("/api/books/") and path.endswith("/sections"):
            try:
                return self._json(sections_for(path.split("/")[3]))
            except LookupError as error:
                return self._error(404, str(error))
            except Exception as error:
                return self._error(422, f"Could not read this book: {error}")
        if path.startswith("/api/books/"):
            book = _paths.get(path.rsplit("/", 1)[1])
            if not book or not book.exists():
                return self._error(404, "Book not found. Refresh the library.")
            try:
                return self._send(200, _book_json(book, book.stat().st_mtime))
            except UnsupportedBook as error:
                return self._error(422, str(error))
            except Exception as error:  # malformed EPUBs come in many shapes
                return self._error(422, f"Could not read this book: {error}")
        if path == "/api/device":
            try:
                return self._json(read_device())
            except device.DeviceError as error:
                return self._error(503, str(error))
            except Exception as error:
                return self._error(503, f"Could not talk to the reader: {error}")
        file = (WEB / ("index.html" if path == "/" else path.lstrip("/"))).resolve()
        if WEB not in file.parents or not file.is_file():
            return self._error(404, "Not found")
        content_type = mimetypes.guess_type(file.name)[0] or "application/octet-stream"
        if file.suffix == ".js":
            content_type = "text/javascript"
        self._send(200, file.read_bytes(), content_type)

    def do_POST(self) -> None:
        if self.path.startswith("/api/device/books"):
            length = int(self.headers.get("Content-Length", 0))
            if length > MAX_UPLOAD_BYTES:
                return self._error(413, "That file is too large for a book.")
            name = parse_qs(urlsplit(self.path).query).get("name", ["book.epub"])[0]
            data = self.rfile.read(length)
            try:
                return self._json(send_book(name, data))
            except (UnsupportedBook, ValueError) as error:
                return self._error(422, str(error))
            except device.DeviceError as error:
                return self._error(503, str(error))
            except Exception as error:
                return self._error(422, f"Could not send this book: {error}")
        if self.path.startswith("/api/books/") and self.path.endswith("/trim"):
            try:
                request = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))))
                source, book = book_for(self.path.split("/")[3])
                result = publish.save(source, book, [bool(k) for k in request["keep"]], request.get("target", "card"))
                return self._json(result)
            except (LookupError, ValueError, KeyError) as error:
                return self._error(400, str(error))
            except OSError as error:
                return self._error(500, f"Could not write the file: {error}")
        if self.path != "/api/tokenize":
            return self._error(404, "Not found")
        text = self.rfile.read(min(int(self.headers.get("Content-Length", 0)), 100_000)).decode("utf-8", "replace")
        self._json([token for line in text.split("\n") for token in tokenize_line(line)])

    def do_PUT(self) -> None:
        if self.path != "/api/device/settings":
            return self._error(404, "Not found")
        try:
            settings = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))))
            self._json(write_device(settings))
        except (ValueError, KeyError) as error:
            self._error(400, f"Bad settings: {error}")
        except device.DeviceError as error:
            self._error(503, str(error))
        except Exception as error:
            self._error(503, f"Could not talk to the reader: {error}")


def serve(port: int = 8765, open_browser: bool = True) -> None:
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    url = f"http://127.0.0.1:{port}/"
    print(f"Rigels Studio running at {url}  (Ctrl+C to stop)")
    if open_browser:
        threading.Timer(0.5, webbrowser.open, [url]).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
