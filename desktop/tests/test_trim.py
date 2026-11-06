"""Front/back matter detection, exact .rsvp round-trips, position mapping, and saving onto a card."""
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from rigels_studio import publish, trim  # noqa: E402
from rigels_studio.books import Book, parse_rsvp  # noqa: E402
from rigels_studio.text import tokenize_line  # noqa: E402


def make_book(parts: list[tuple[str | None, str]]) -> Book:
    """Build a Book from (chapter title or None, text) parts; blank lines separate paragraphs."""
    book = Book(title="Sample Book", author="A. Writer", source="sample.epub", language="en")
    for title, text in parts:
        if title is not None:
            book.chapters.append((title, len(book.words)))
        for paragraph in text.strip().split("\n\n"):
            tokens = tokenize_line(paragraph)
            if tokens:
                book.paragraphs.append(len(book.words))
                book.words.extend(tokens)
    return book


BODY_TEXT = " ".join(["The sailor walked along the harbour, thinking of home and the sea."] * 40)

BOOK = make_book([
    (None, "Chapter One Chapter Two The Ending"),
    ("Copyright", "Copyright 2001. All rights reserved. ISBN 978-0-00-000000-0. Published by Example Press."),
    ("Dedication", "For my mother."),
    ("Introduction", "This translation follows the 1846 text... and keeps its rhythm."),
    ("Chapter One", BODY_TEXT + "\n\nHe paused... ... then wondered. ... Was it Mr. Smith?"),
    ("Chapter Two", BODY_TEXT),
    ("The Ending", BODY_TEXT),
    ("Index", "harbour, 3, 7 sailor, 1-9 sea, 2"),
    ("About the Author", "A. Writer lives by the sea."),
])


class Detection(unittest.TestCase):
    def test_kinds_and_defaults(self):
        sections = trim.analyze(BOOK)
        found = {s.title: (s.kind, s.keep) for s in sections}
        self.assertEqual(found["Opening (before the first chapter)"], (trim.CONTENTS, False))
        self.assertEqual(found["Copyright"], (trim.FRONT, False))
        self.assertEqual(found["Dedication"], (trim.FRONT, False))
        self.assertEqual(found["Introduction"], (trim.COMMENTARY, True))
        self.assertEqual(found["Chapter One"], (trim.BODY, True))
        self.assertEqual(found["Index"], (trim.BACK, False))
        self.assertEqual(found["About the Author"], (trim.BACK, False))

    def test_gutenberg_markers_split_sections(self):
        book = make_book([
            ("The Project Gutenberg eBook of Sample", "This eBook is for the use of anyone anywhere. "
             "*** START OF THE PROJECT GUTENBERG EBOOK SAMPLE ***"),
            ("Chapter 1", BODY_TEXT + " *** END OF THE PROJECT GUTENBERG EBOOK SAMPLE *** Updated editions will replace the previous one."),
        ])
        sections = trim.analyze(book)
        kept = [w for s in sections if s.keep for w in book.words[s.start:s.end]]
        self.assertEqual(kept[0], "The")
        self.assertEqual(kept[-1], "sea.")
        self.assertTrue(all(s.kind == trim.GUTENBERG for s in sections if not s.keep))


class RoundTrip(unittest.TestCase):
    def test_trimmed_rsvp_reads_back_exactly(self):
        sections = trim.analyze(BOOK)
        keep = [s.keep for s in sections]
        document = trim.write_rsvp(BOOK, sections, keep, "en")
        parsed = parse_rsvp(document)
        expected = [w for s, k in zip(sections, keep) if k for w in BOOK.words[s.start:s.end]]
        self.assertEqual(parsed.words, expected)
        self.assertEqual([t for t, _ in parsed.chapters], ["Introduction", "Chapter One", "Chapter Two", "The Ending"])
        self.assertIn("paused......", parsed.words)  # glued ellipses survive
        self.assertIn("wondered....", parsed.words)

    def test_position_mapping(self):
        ranges = [(10, 20), (30, 40)]
        self.assertEqual(trim.map_position(5, ranges), 0)    # cut text before the book -> its start
        self.assertEqual(trim.map_position(15, ranges), 5)
        self.assertEqual(trim.map_position(25, ranges), 10)  # inside a cut -> next kept word
        self.assertEqual(trim.map_position(35, ranges), 15)
        self.assertEqual(trim.map_position(99, ranges), 19)


class SaveToCard(unittest.TestCase):
    def test_moves_originals_and_carries_position(self):
        with tempfile.TemporaryDirectory() as tmp:
            books = pathlib.Path(tmp) / "library" / "books"
            books.mkdir(parents=True)
            source = books / "sample.rsvp"
            for name in ("sample.epub", "sample.rsvp.ridx", "sample.rsvp.rdat", "other.epub"):
                (books / name).write_bytes(b"x")
            source.write_text(trim.write_rsvp(BOOK, trim.analyze(BOOK), [True] * len(trim.analyze(BOOK))))
            (books / "sample.rstate.toml").write_text("[reading]\nwordIndex = 120\n")
            book = parse_rsvp(source.read_text(), source.name)
            sections = trim.analyze(book)
            keep = [s.keep for s in sections]
            with mock.patch.object(publish, "card_books_folder", lambda path=None: books), \
                 mock.patch.object(publish, "STATE_DIR", pathlib.Path(tmp) / "state"), \
                 mock.patch.object(publish, "PENDING", pathlib.Path(tmp) / "state" / "pending.json"):
                result = publish.save(source, book, keep, "card")
                pending = publish.load_pending()
            self.assertEqual(sorted(p.name for p in (books / "originals").iterdir()),
                             sorted(["sample.epub", "sample.rsvp", "sample.rsvp.rdat", "sample.rsvp.ridx", "sample.rstate.toml"]))
            self.assertTrue((books / "other.epub").exists())
            written = books / "Sample Book.rsvp"
            self.assertEqual(result["path"], str(written))
            self.assertEqual(parse_rsvp(written.read_text()).words[0], "This")  # starts at the Introduction
            expected = trim.map_position(120, trim.kept_ranges(sections, keep))
            self.assertEqual(result["position"], {"from": 120, "to": expected})
            self.assertEqual(pending, {"Sample Book.rsvp": expected})


if __name__ == "__main__":
    unittest.main()
