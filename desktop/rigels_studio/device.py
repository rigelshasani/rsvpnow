"""USB companion client for the reader (docs/companion-serial.md).

Speaks the firmware's RSV2 framing over the reader's USB serial port and exposes the same
/api/v2 endpoints the web companion uses. One request at a time; every data frame is acknowledged.
"""
from __future__ import annotations

import json
import struct
import threading
import time
import zlib
from dataclasses import dataclass

import serial
from serial.tools import list_ports

ESPRESSIF_VID = 0x303A
HANDSHAKE = b"RSVPNANO/COMPANION/1\n"
MAGIC = b"RSV2"
VERSION = 1
CHUNK = 4096
HEADER = struct.Struct("<4sBBIII")  # magic, version, type, request id, sequence, payload length

REQUEST, DATA, END, RESPONSE, ACK, ERROR, PING, PONG, CLOSE = range(1, 10)


class DeviceError(RuntimeError):
    pass


@dataclass
class Frame:
    type: int
    request_id: int
    sequence: int
    payload: bytes


def encode(frame_type: int, request_id: int = 0, sequence: int = 0, payload: bytes = b"") -> bytes:
    head = HEADER.pack(MAGIC, VERSION, frame_type, request_id, sequence, len(payload))
    return head + payload + struct.pack("<I", zlib.crc32(head[4:] + payload))


class Decoder:
    """Resynchronizing frame decoder, matching companion::serial::Decoder."""

    def __init__(self) -> None:
        self.buffer = bytearray()

    def feed(self, data: bytes) -> list[Frame]:
        self.buffer += data
        frames = []
        while True:
            start = self.buffer.find(MAGIC)
            if start < 0:
                del self.buffer[: max(0, len(self.buffer) - 3)]
                return frames
            del self.buffer[:start]
            if len(self.buffer) < HEADER.size + 4:
                return frames
            _, version, frame_type, request_id, sequence, length = HEADER.unpack_from(self.buffer)
            if version != VERSION or not REQUEST <= frame_type <= CLOSE or length > 64 * 1024:
                del self.buffer[0]
                continue
            total = HEADER.size + length + 4
            if len(self.buffer) < total:
                return frames
            (expected,) = struct.unpack_from("<I", self.buffer, HEADER.size + length)
            if expected != zlib.crc32(bytes(self.buffer[4 : HEADER.size + length])):
                del self.buffer[0]
                continue
            frames.append(Frame(frame_type, request_id, sequence, bytes(self.buffer[HEADER.size : HEADER.size + length])))
            del self.buffer[:total]


def find_port() -> str | None:
    """The reader's USB serial port (Espressif vendor ID), if one is plugged in."""
    for port in list_ports.comports():
        if port.vid == ESPRESSIF_VID:
            return port.device
    return None


class Reader:
    """A connected reader. Use as a context manager or call close()."""

    def __init__(self, port: str | None = None, timeout: float = 15.0) -> None:
        self.port = port or find_port()
        if not self.port:
            raise DeviceError("No reader found on USB. Plug it in with a data cable.")
        self.timeout = timeout
        self.lock = threading.Lock()
        self.decoder = Decoder()
        self.next_id = 1
        # Never touch line signals or 1200 baud: on this board that resets into the bootloader.
        self.serial = serial.Serial(self.port, 115200, timeout=0.1, rtscts=False, dsrdtr=False)
        try:
            self._handshake()
        except Exception:
            self.serial.close()
            raise

    def _handshake(self) -> None:
        self.serial.reset_input_buffer()
        self.serial.write(HANDSHAKE)
        deadline = time.monotonic() + 4
        line = b""
        while time.monotonic() < deadline:
            line += self.serial.read(256)
            for text in line.split(b"\n"):
                if text.startswith(b"RSVPNANO/COMPANION/1 "):
                    state = text.split(b" ", 2)[1].decode(errors="replace")
                    if state == "READY":
                        return
                    if state == "BUSY":
                        raise DeviceError("The reader is in USB transfer mode. Turn that off on the reader first.")
                    raise DeviceError(f"The reader refused the connection ({state}). Update its firmware.")
        raise DeviceError("The reader did not answer. Make sure it is on and not busy converting a book.")

    def _send(self, *args) -> None:
        self.serial.write(encode(*args))

    def _frames(self, request_id: int, deadline: float):
        while True:
            if time.monotonic() > deadline:
                raise DeviceError("The reader stopped responding.")
            for frame in self.decoder.feed(self.serial.read(8192)):
                if frame.type == PING:
                    self._send(PONG)
                elif frame.type == ERROR and frame.request_id in (request_id, 0):
                    raise DeviceError(frame.payload.decode(errors="replace"))
                elif frame.request_id == request_id:
                    yield frame

    def request(self, method: str, path: str, body: bytes = b"", query: dict[str, str] | None = None):
        """Send one API request; returns (status, decoded JSON or None)."""
        with self.lock:
            request_id, self.next_id = self.next_id, self.next_id + 1
            deadline = time.monotonic() + self.timeout
            meta = {"method": method, "path": path, "query": query or {}, "totalBytes": len(body)}
            self._send(REQUEST, request_id, 0, json.dumps(meta).encode())
            frames = self._frames(request_id, deadline)
            for sequence, offset in enumerate(range(0, len(body), CHUNK)):
                self._send(DATA, request_id, sequence, body[offset : offset + CHUNK])
                ack = next(frames)
                if ack.type != ACK or ack.sequence != sequence:
                    raise DeviceError("The reader rejected an upload chunk.")
            self._send(END, request_id, 0, json.dumps({"totalBytes": len(body), "crc32": zlib.crc32(body)}).encode())

            response = None
            chunks: list[bytes] = []
            for frame in frames:
                if frame.type == RESPONSE:
                    response = json.loads(frame.payload)
                elif frame.type == DATA:
                    chunks.append(frame.payload)
                    self._send(ACK, request_id, frame.sequence)
                elif frame.type == END and response is not None:
                    break
            data = b"".join(chunks)
            # Older firmware can cut a chapter title mid-character; don't let one title break the listing.
            return response["status"], (json.loads(data.decode("utf-8", errors="replace")) if data else None)

    def get(self, path: str):
        status, data = self.request("GET", path)
        if status >= 400:
            raise DeviceError((data or {}).get("message", f"HTTP {status}"))
        return data

    def patch(self, path: str, payload: dict) -> None:
        status, data = self.request("PATCH", path, json.dumps(payload).encode())
        if status >= 400:
            raise DeviceError((data or {}).get("message", f"HTTP {status}"))

    def close(self) -> None:
        try:
            with self.lock:
                self._send(CLOSE)
                self.serial.flush()
        finally:
            self.serial.close()

    def __enter__(self) -> "Reader":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
