"""EPUB loading details that must match the firmware converter."""
import io
import pathlib
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from rigels_studio.books import load_epub, parse_html  # noqa: E402
from rigels_studio.publish import send_to_reader  # noqa: E402

GUTENBERG_BODY = """<html><body><div class="pg-boilerplate pgheader" id="pg-header">
<h2 id="pg-header-heading">The Project Gutenberg eBook of Fixture</h2>
<div>This eBook is for the use of anyone anywhere.</div><div class="pgmonospaced"/>
<div id="pg-start-separator"><span>*** START OF THE PROJECT GUTENBERG EBOOK ***</span></div></div>
<h2 id="chapter-1">I</h2><p>It was a quiet morning.</p>
<section class="pg-boilerplate" id="pg-footer"><h2 id="pg-footer-heading">THE FULL LICENSE</h2>
<section><p>Redistribution is subject to the license.</p></section></section></body></html>"""


def gutenberg_epub(path: pathlib.Path) -> None:
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr("mimetype", "application/epub+zip")
        archive.writestr("META-INF/container.xml", '<container><rootfiles><rootfile full-path="OEBPS/content.opf"/>'
                         "</rootfiles></container>")
        archive.writestr("OEBPS/content.opf", """<package xmlns="http://www.idpf.org/2007/opf" version="2.0">
<metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Fixture</dc:title></metadata>
<manifest><item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>
<item id="text" href="text.html" media-type="application/xhtml+xml"/></manifest>
<spine toc="ncx"><itemref idref="text"/></spine></package>""")
        archive.writestr("OEBPS/toc.ncx", """<ncx><navMap>
<navPoint><navLabel><text>Chapter I</text></navLabel><content src="text.html#chapter-1"/></navPoint>
<navPoint><navLabel><text>THE FULL LICENSE</text></navLabel><content src="text.html#pg-footer-heading"/></navPoint>
</navMap></ncx>""")
        archive.writestr("OEBPS/text.html", GUTENBERG_BODY)


class GutenbergBoilerplate(unittest.TestCase):
    def test_parser_drops_boilerplate_blocks(self):
        content = parse_html(GUTENBERG_BODY)
        self.assertEqual([text for text, _ in content.paragraphs], ["I", "It was a quiet morning."])

    def test_epub_has_no_licence_chapter_or_words(self):
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / "pg1.epub"
            gutenberg_epub(path)
            book = load_epub(path)
        self.assertEqual([title for title, _ in book.chapters], ["Chapter I"])
        text = " ".join(book.words)
        self.assertIn("quiet morning", text)
        self.assertNotIn("Gutenberg", text)
        self.assertNotIn("license", text)


class FakeReader:
    def __init__(self, status: int = 201) -> None:
        self.status, self.sent = status, []

    def request(self, method, path, body=b"", query=None):
        self.sent.append((method, path, body, query))
        return self.status, ({"message": "A library item with that name already exists"} if self.status >= 400 else {})


class SendToReader(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        path = pathlib.Path(folder.name) / "pg1.epub"
        gutenberg_epub(path)
        self.book = load_epub(path)

    def test_uploads_converted_rsvp_as_a_book(self):
        reader = FakeReader()
        result = send_to_reader(self.book, reader)
        method, path, body, query = reader.sent[0]
        self.assertEqual((method, path), ("POST", "/api/v2/library"))
        self.assertEqual(query, {"name": "Fixture.rsvp", "category": "book"})
        document = body.decode("utf-8")
        self.assertTrue(document.startswith("@rsvp 1"))
        self.assertIn("quiet morning", document)
        self.assertNotIn("license", document)
        self.assertEqual(result["name"], "Fixture.rsvp")

    def test_rsvp_files_are_sent_unchanged(self):
        reader = FakeReader()
        send_to_reader(self.book, reader, b"@rsvp 1\nhello\n")
        self.assertEqual(reader.sent[0][2], b"@rsvp 1\nhello\n")

    def test_duplicate_is_reported_plainly(self):
        with self.assertRaisesRegex(ValueError, "already on the reader"):
            send_to_reader(self.book, FakeReader(409))


if __name__ == "__main__":
    unittest.main()
