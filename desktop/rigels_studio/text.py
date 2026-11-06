"""Port of the firmware's display-text normalizer and RSVP tokenizer.

Mirrors src/text/TextNormalizer.cpp (normalizeDisplayText) and src/text/RsvpTokenizer.h
(appendNormalizedLineWords), so the desktop sees exactly the word stream the reader shows.
Verified against the firmware by tests/test_text.py using tests/golden/golden.json.
"""
from __future__ import annotations

import unicodedata
from collections.abc import Iterator

_ASCII_WHITESPACE = frozenset(" \t\n\r\f\v")
_DROPPED = frozenset({0x00AD, 0x200B, 0xFEFF})
_LETTER_CATEGORIES = frozenset({"Lu", "Ll", "Lt", "Lm", "Lo"})
_CJK_PHRASE_CHARACTERS = 2
_CJK_PHRASE_ENDS = frozenset(",;:!?、。！，：；？")
# Unicode Han, Hiragana and Katakana ranges (HarfBuzz scripts the firmware treats as CJK).
_CJK_RANGES = (
    (0x2E80, 0x2FDF), (0x3005, 0x3005), (0x3007, 0x3007), (0x3021, 0x3029), (0x3038, 0x303B),
    (0x3041, 0x3096), (0x309D, 0x309F), (0x30A1, 0x30FA), (0x30FD, 0x30FF), (0x31F0, 0x31FF),
    (0x32D0, 0x32FE), (0x3300, 0x3357), (0x3400, 0x4DBF), (0x4E00, 0x9FFF), (0xF900, 0xFAFF),
    (0xFF66, 0xFF6F), (0xFF71, 0xFF9D), (0x1B000, 0x1B16F), (0x20000, 0x3134F),
)


def is_whitespace(ch: str) -> bool:
    return ch in _ASCII_WHITESPACE or ch == "\x85" or unicodedata.category(ch) in ("Zs", "Zl", "Zp")


def is_letter(ch: str) -> bool:
    return unicodedata.category(ch) in _LETTER_CATEGORIES


def is_word_character(ch: str) -> bool:
    return unicodedata.category(ch) in _LETTER_CATEGORIES or unicodedata.category(ch) == "Nd"


def is_cjk_char(ch: str) -> bool:
    code = ord(ch)
    return any(low <= code <= high for low, high in _CJK_RANGES)


def is_cjk_text(text: str) -> bool:
    found = False
    for ch in text:
        if is_cjk_char(ch):
            found = True
        elif is_word_character(ch):
            return False
    return found


def normalize_display_text(text: str) -> str:
    """Collapse whitespace to single spaces and drop invisible/control characters."""
    out: list[str] = []
    for ch in text:
        if is_whitespace(ch):
            if out and out[-1] != " ":
                out.append(" ")
            continue
        code = ord(ch)
        if code in _DROPPED or code < 0x20 or 0x7F <= code <= 0x9F:
            continue
        out.append(ch)
    if out and out[-1] == " ":
        out.pop()
    return "".join(out)


def _is_inline_word_hyphen(text: str, index: int) -> bool:
    if index == 0 or index + 1 >= len(text) or text[index] != "-":
        return False
    if text[index - 1] == "-" or text[index + 1] == "-":
        return False
    return is_word_character(text[index - 1]) and is_word_character(text[index + 1])


def tokenize_normalized_line(line: str) -> Iterator[str]:
    """Yield the reader's display tokens for one already-normalized line."""
    pending = ""
    current: list[str] = []

    def queue(token: str) -> Iterator[str]:
        nonlocal pending
        if not token:
            return
        if token == "...":
            if pending:
                pending += "..."
            return
        if token in ("-", "—"):
            if pending:
                yield pending
                pending = ""
            yield token
            return
        if pending:
            yield pending
        pending = token

    def finish(token: str) -> Iterator[str]:
        if not is_cjk_text(token):
            yield from queue(token)
            return
        phrase_start = 0
        characters = 0
        break_before = False
        for offset, ch in enumerate(token):
            if is_word_character(ch):
                if characters >= _CJK_PHRASE_CHARACTERS or break_before:
                    yield from queue(token[phrase_start:offset])
                    phrase_start, characters, break_before = offset, 0, False
                characters += 1
            if ch in _CJK_PHRASE_ENDS:
                break_before = True
        yield from queue(token[phrase_start:])

    def flush_current() -> Iterator[str]:
        if current:
            word = "".join(current)
            current.clear()
            yield from finish(word)

    i = 0
    length = len(line)
    while i < length:
        ch = line[i]
        if ch <= " ":
            yield from flush_current()
        elif ch == "—":
            yield from flush_current()
            yield from finish("—")
        elif ch == "-":
            if _is_inline_word_hyphen(line, i):
                current.append("-")
            else:
                yield from flush_current()
                yield from finish("-")
                while i + 1 < length and line[i + 1] == "-":
                    i += 1
        elif ch == "." and i + 2 < length and line[i + 1] == "." and line[i + 2] == ".":
            current.append("...")
            i += 2
            while i + 1 < length and line[i + 1] == ".":
                i += 1
            yield from flush_current()
        else:
            current.append(ch)
        i += 1

    yield from flush_current()
    if pending:
        yield pending


def tokenize_line(line: str) -> list[str]:
    return list(tokenize_normalized_line(normalize_display_text(line)))
