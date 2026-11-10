#include <Arduino.h>
#include <FS.h>
#include <unity.h>

#include <span>
#include <string>
#include <vector>

#include "conversion/epub/EpubContentParser.h"
#include "conversion/epub/EpubPackage.h"
#include "conversion/rsvp/RsvpWriter.h"

void setUp() {}
void tearDown() {}

namespace {

    std::string deterministicWriterFixture() {
        File output;
        RsvpWriter writer(output,
                          {.source = "fixture.txt",
                           .title = "Fixture",
                           .author = "Author",
                           .converter = "fixture-v1",
                           .language = "en"},
                          3);
        writer.setLanguage("en");
        writer.setDirection("ltr");
        writer.setDirection("ltr");
        writer.writeChapter("One");
        writer.writeChapter("One");
        writer.writeText("one two three four");
        writer.finish();
        return output.contents();
    }

    void test_rsvp_writer_is_deterministic_and_idempotent() {
        const std::string first = deterministicWriterFixture();
        TEST_ASSERT_EQUAL_STRING(first.c_str(), deterministicWriterFixture().c_str());
        TEST_ASSERT_TRUE(first.starts_with("@rsvp 1\n@title Fixture\n@source fixture.txt\n@author Author\n"
                                          "@converter fixture-v1\n@language en\n"));
        TEST_ASSERT_TRUE(first.contains("@direction ltr\n@chapter One\none two three\n"));
        TEST_ASSERT_EQUAL(std::string::npos, first.find("@direction ltr", first.find("@direction ltr") + 1));
        TEST_ASSERT_EQUAL(std::string::npos, first.find("@chapter One", first.find("@chapter One") + 1));
    }

    void test_package_parses_nav_metadata_and_encoded_manifest_paths() {
        constexpr std::string_view opf = R"(<package version="3.0"><metadata><dc:title>Fixture</dc:title><dc:language>ja</dc:language></metadata><manifest>
            <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
            <item id="chapter" href="Text/Chapter%20One.xhtml" media-type="application/xhtml+xml"/>
            </manifest><spine><itemref idref="chapter"/></spine></package>)";

        const auto manifest = EpubPackage::parseManifestItems(opf, "OEBPS/");
        TEST_ASSERT_EQUAL_STRING("3.0", EpubPackage::parsePackageVersion(opf).c_str());
        TEST_ASSERT_EQUAL_STRING("ja", EpubPackage::parseDcMetadata(opf, "language").c_str());
        TEST_ASSERT_EQUAL_UINT32(2, manifest.size());
        TEST_ASSERT_EQUAL_STRING("nav", manifest[0].properties.c_str());
        TEST_ASSERT_EQUAL_STRING("OEBPS/Text/Chapter One.xhtml", manifest[1].path.c_str());
    }

    void test_nav_toc_flattens_nested_entries_and_decodes_fragments() {
        constexpr std::string_view nav = R"(<html><body><nav epub:type="landmarks"><a href="cover.xhtml">Start</a></nav>
            <nav epub:type="toc"><ol>
            <li><a href="one.xhtml#one">Part One</a><ol>
                <li><a href="Text/Chapter%20One.xhtml#section%201">Chapter One A</a></li>
            </ol></li><li><a href="two.xhtml#two">Part Two</a></li>
            </ol></nav></body></html>)";

        const auto entries = EpubPackage::parseNavTocEntries(nav, "OEBPS/nav.xhtml", "Fixture");
        TEST_ASSERT_EQUAL_UINT32(3, entries.size());
        TEST_ASSERT_EQUAL_STRING("Part One", entries[0].title.c_str());
        TEST_ASSERT_EQUAL_STRING("OEBPS/Text/Chapter One.xhtml", entries[1].path.c_str());
        TEST_ASSERT_EQUAL_STRING("section 1", entries[1].fragment.c_str());
        TEST_ASSERT_EQUAL_STRING("Part Two", entries[2].title.c_str());
    }

    void test_ncx_toc_ignores_non_content_labels() {
        constexpr std::string_view ncx = R"(<ncx><navMap>
            <navPoint><navLabel><text>Fixture</text></navLabel><content src="title.xhtml"/></navPoint>
            <navPoint><navLabel><text>Contents</text></navLabel><content src="toc.xhtml"/></navPoint>
            <navPoint><navLabel><text>I. The Arrival</text></navLabel><content src="content.xhtml#chapter-1"/></navPoint>
            </navMap></ncx>)";

        const auto entries = EpubPackage::parseNcxTocEntries(ncx, "OEBPS/toc.ncx", "Fixture");
        TEST_ASSERT_EQUAL_UINT32(1, entries.size());
        TEST_ASSERT_EQUAL_STRING("I. The Arrival", entries[0].title.c_str());
        TEST_ASSERT_EQUAL_STRING("OEBPS/content.xhtml", entries[0].path.c_str());
        TEST_ASSERT_EQUAL_STRING("chapter-1", entries[0].fragment.c_str());
    }

    void test_toc_keeps_chapters_that_only_begin_like_the_book_title() {
        constexpr std::string_view ncx = R"(<ncx><navMap>
            <navPoint><navLabel><text>Introduction</text></navLabel><content src="intro.xhtml"/></navPoint>
            <navPoint><navLabel><text>Sorting</text></navLabel><content src="one.xhtml"/></navPoint>
            </navMap></ncx>)";
        const auto kept = EpubPackage::parseNcxTocEntries(ncx, "toc.ncx", "Introduction to Algorithms");
        TEST_ASSERT_EQUAL_UINT32(2, kept.size());
        TEST_ASSERT_EQUAL_STRING("Introduction", kept[0].title.c_str());

        // The title without its subtitle is still the title page, not a chapter.
        constexpr std::string_view shortTitle = R"(<ncx><navMap>
            <navPoint><navLabel><text>Sapiens</text></navLabel><content src="title.xhtml"/></navPoint>
            <navPoint><navLabel><text>An Animal of No Significance</text></navLabel><content src="one.xhtml"/></navPoint>
            </navMap></ncx>)";
        const auto dropped =
            EpubPackage::parseNcxTocEntries(shortTitle, "toc.ncx", "Sapiens: A Brief History of Humankind");
        TEST_ASSERT_EQUAL_UINT32(1, dropped.size());
        TEST_ASSERT_EQUAL_STRING("An Animal of No Significance", dropped[0].title.c_str());
    }

    void test_parser_uses_ordered_toc_labels_and_paragraph_markers() {
        File output;
        const std::vector<EpubPackage::TocEntry> toc = {
            {"content.xhtml", "I. The Arrival", "chapter-1"},
            {"content.xhtml", "II. Father and Son", "chapter-2"},
        };
        constexpr std::string_view markup = R"(<body><h1>Book Title</h1><h2 id="chapter-1">I</h2>
            <p>The harbour was bright.</p><h2 id="chapter-2">II</h2><p>The door opened.</p></body>)";

        RsvpWriter writer(output, {.source = "fixture.epub", .title = "Book Title"});
        EpubContent::Parser parser(writer, toc, true, "content", "Book Title");
        TEST_ASSERT_TRUE(parser.write(reinterpret_cast<const uint8_t*>(markup.data()), markup.length()));
        TEST_ASSERT_TRUE(parser.finish());
        TEST_ASSERT_TRUE(writer.finish());

        TEST_ASSERT_EQUAL_UINT32(2, writer.chapterCount());
        TEST_ASSERT_TRUE(output.contents().ends_with(
            "@chapter I. The Arrival\n\n@chapter II. Father and Son\nThe harbour was bright.\n\n@para\nThe door opened.\n"));
    }

    void test_parser_skips_project_gutenberg_boilerplate() {
        File output;
        const std::vector<EpubPackage::TocEntry> toc = {
            {"content.xhtml", "Chapter I", "chapter-1"},
        };
        // Shape of a current gutenberg.org EPUB: a header block with its own heading and
        // nested divs (one self-closing), the book, then the licence footer.
        constexpr std::string_view markup = R"(<body><div class="pg-boilerplate pgheader" id="pg-header" lang="en">
            <h2 id="pg-header-heading">The Project Gutenberg eBook of Fixture</h2>
            <div>This eBook is for the use of anyone anywhere &amp; at no cost.</div>
            <div id="pg-machine-header"><p>Title: Fixture</p><div class="pgmonospaced"/></div>
            <div id="pg-start-separator"><span>*** START OF THE PROJECT GUTENBERG EBOOK ***</span></div></div>
            <h2 id="chapter-1">I</h2><div><p>It was a quiet morning.</p></div>
            <section class="pg-boilerplate" id="pg-footer"><h2 id="pg-footer-heading">THE FULL LICENSE</h2>
            <section><p>Redistribution is subject to the license.</p></section></section>
            <p>After the footer.</p></body>)";

        RsvpWriter writer(output, {.source = "fixture.epub", .title = "Fixture"});
        EpubContent::Parser parser(writer, toc, true, "content", "Fixture");
        TEST_ASSERT_TRUE(parser.write(reinterpret_cast<const uint8_t*>(markup.data()), markup.length()));
        TEST_ASSERT_TRUE(parser.finish());
        TEST_ASSERT_TRUE(writer.finish());

        TEST_ASSERT_EQUAL_UINT32(1, writer.chapterCount());
        TEST_ASSERT_EQUAL_UINT32(8, writer.wordCount());
        TEST_ASSERT_TRUE(output.contents().ends_with("@chapter Chapter I\nIt was a quiet morning.\n\n@para\nAfter the footer.\n"));
        TEST_ASSERT_FALSE(output.contents().contains("Gutenberg"));
        TEST_ASSERT_FALSE(output.contents().contains("license"));
    }

    void test_parser_preserves_punctuation_only_inline_fragments_without_counting_them_as_words() {
        File output;
        constexpr std::string_view markup = R"(<body><h1>Letter</h1><p>Dear Reader <span>,</span></p></body>)";

        RsvpWriter writer(output, {.source = "fixture.epub", .title = "Letter"});
        EpubContent::Parser parser(writer, {}, false, "Letter", "Letter");
        TEST_ASSERT_TRUE(parser.write(reinterpret_cast<const uint8_t*>(markup.data()), markup.length()));
        TEST_ASSERT_TRUE(parser.finish());
        TEST_ASSERT_TRUE(writer.finish());

        TEST_ASSERT_EQUAL_UINT32(2, writer.wordCount());
        TEST_ASSERT_TRUE(output.contents().ends_with("@chapter Letter\nDear Reader ,\n"));
    }

    void test_parser_preserves_nested_language_and_direction_changes() {
        File output;
        constexpr std::string_view markup =
            R"(<body lang="ja"><p>日本語 <span xml:lang="en" dir="ltr"><span>English</span></span> 続き</p><p lang="ar" dir="rtl">مرحبا 123</p></body>)";

        RsvpWriter writer(output, {.source = "fixture.epub", .title = "Fixture", .language = "ja"});
        EpubContent::Parser parser(writer, {}, false, "Fixture", "Fixture");
        TEST_ASSERT_TRUE(parser.write(reinterpret_cast<const uint8_t*>(markup.data()), markup.length()));
        TEST_ASSERT_TRUE(parser.finish());
        TEST_ASSERT_TRUE(writer.finish());

        TEST_ASSERT_TRUE(output.contents().contains("日本語\n@language en\n@direction ltr\nEnglish\n"));
        TEST_ASSERT_TRUE(output.contents().contains("@language ja\n@direction auto\n続き\n"));
        TEST_ASSERT_TRUE(output.contents().contains("@language ar\n@direction rtl\n"));
        TEST_ASSERT_TRUE(output.contents().contains("مرحبا 123\n@language ja\n@direction auto\n"));
    }

    void test_parser_emits_explicit_vertical_writing_mode_once() {
        File output;
        constexpr std::string_view markup =
            R"(<head><style>html { -epub-writing-mode: vertical-rl; }</style></head><body><p style="writing-mode: vertical-rl">日本語</p></body>)";

        RsvpWriter writer(output, {.source = "fixture.epub", .title = "Fixture", .language = "ja"});
        EpubContent::Parser firstParser(writer, {}, false, "Fixture", "Fixture");
        TEST_ASSERT_TRUE(firstParser.write(reinterpret_cast<const uint8_t*>(markup.data()), markup.length()));
        TEST_ASSERT_TRUE(firstParser.finish());
        EpubContent::Parser secondParser(writer, {}, false, "Second", "Fixture");
        TEST_ASSERT_TRUE(secondParser.write(reinterpret_cast<const uint8_t*>(markup.data()), markup.length()));
        TEST_ASSERT_TRUE(secondParser.finish());
        TEST_ASSERT_TRUE(writer.finish());
        const size_t first = output.contents().find("@writing-mode vertical-rl");
        TEST_ASSERT_NOT_EQUAL(std::string::npos, first);
        TEST_ASSERT_EQUAL(std::string::npos, output.contents().find("@writing-mode vertical-rl", first + 1));
    }

    // Buffered text is flushed once it grows past 220 bytes. When the byte that crosses the
    // threshold is the word boundary itself, that boundary has to survive the flush, otherwise the
    // next word is glued onto the buffered remainder.
    //
    // "Dear Reader " is 12 bytes, so 18 of them fill the buffer to 216; the following "Dear" ends
    // it at exactly 220 and the separator after it is the byte that triggers the flush.
    std::string convertFragment(std::string_view markup) {
        File output;
        const std::vector<EpubPackage::TocEntry> toc = {{"content.xhtml", "Chapter 1", "c1"}};
        RsvpWriter writer(output, {.source = "fixture.epub", .title = "Fixture"});
        EpubContent::Parser parser(writer, toc, true, "content", "Fixture");
        TEST_ASSERT_TRUE(parser.write(reinterpret_cast<const uint8_t*>(markup.data()), markup.length()));
        TEST_ASSERT_TRUE(parser.finish());
        TEST_ASSERT_TRUE(writer.finish());
        return output.contents();
    }

    void test_parser_drops_in_book_contents_lists() {
        // Shapes taken from a Gutenberg table and a publisher's toc page, with page numbers.
        const std::string table = convertFragment(R"(<body><p>A Novel</p><table>
            <tr><td>VOLUME ONE</td></tr>
            <tr><td><a href="b-1.xhtml#c1" class="pginternal">Chapter 1. The Arrival</a></td><td>3</td></tr>
            <tr><td><a href="b-2.xhtml#c2" class="pginternal">Chapter 2. Father and Son</a></td><td>19</td></tr>
            <tr><td><a href="b-3.xhtml#c3" class="pginternal">Chapter 3. The Catalans</a></td><td>31</td></tr>
            <tr><td><a href="b-4.xhtml#c4" class="pginternal">Chapter 4. Conspiracy</a></td><td>48</td></tr>
            <tr><td><a href="b-5.xhtml#c5" class="pginternal">Chapter 5. The Marriage Feast</a></td><td>60</td></tr>
            </table><p>The harbour was bright that morning, and the ship came in slowly under sail.</p></body>)");
        TEST_ASSERT_FALSE(table.contains("Catalans"));
        TEST_ASSERT_FALSE(table.contains("Feast"));
        TEST_ASSERT_FALSE(table.contains("60"));
        TEST_ASSERT_TRUE(table.contains("A Novel"));
        TEST_ASSERT_TRUE(table.contains("The harbour was bright"));

        const std::string list = convertFragment(R"(<body><div class="toc"><a href="p1.htm"><strong>Part One</strong></a></div>
            <div class="toc"><a href="c01.htm">1 An Animal of No Significance</a></div>
            <div class="toc"><a href="c02.htm">2 The Tree of Knowledge</a></div>
            <div class="toc"><a href="c03.htm">3 A Day in the Life</a></div></body>)");
        TEST_ASSERT_FALSE(list.contains("Knowledge"));
    }

    void test_parser_keeps_links_that_are_not_a_contents_list() {
        // Footnote references, a few linked lines, and web addresses stay in the text.
        const std::string text = convertFragment(R"(<body>
            <p>It is like the house of the Atreidae;<a href="notes.xhtml#fn19"><sup>[19]</sup></a> they must submit.</p>
            <p><a href="toc.xhtml#c1">CHAPTER ONE</a></p><p>The door opened and nobody came in at all.</p>
            <p><a href="a.xhtml">First</a></p><p><a href="b.xhtml">Second</a></p><p><a href="c.xhtml">Third</a></p>
            <p>That was the whole of the list, and the story went on afterwards.</p>
            <p><a href="https://example.org/a">example.org/a</a></p><p><a href="https://example.org/b">example.org/b</a></p>
            <p><a href="https://example.org/c">example.org/c</a></p><p><a href="https://example.org/d">example.org/d</a></p>
            </body>)");
        TEST_ASSERT_TRUE(text.contains("Atreidae;[19] they must submit."));
        TEST_ASSERT_TRUE(text.contains("CHAPTER ONE"));
        TEST_ASSERT_TRUE(text.contains("First\n\n@para\nSecond\n\n@para\nThird\n\n@para\nThat was the whole"));
        TEST_ASSERT_TRUE(text.contains("example.org/d"));
    }

    void test_parser_drops_pages_about_the_edition() {
        // Shape of a Standard Ebooks file: title page and imprint before the work, colophon and
        // copyright page after it.
        const std::string text = convertFragment(R"(<body epub:type="frontmatter">
            <section id="titlepage" epub:type="titlepage"><h1>Fixture</h1><p>By An Author.</p></section>
            <section id="imprint" epub:type="imprint"><h2>Imprint</h2><p>This ebook is the product of many hours
            of hard work by volunteers.</p></section>
            <section id="chapter-1" epub:type="chapter"><p>It was a quiet morning.</p></section>
            <section id="colophon" epub:type="colophon"><p>Fixture was published in 1900.</p></section>
            <section id="uncopyright" epub:type="copyright-page"><p>May you do good and not evil.</p></section>
            </body>)");
        TEST_ASSERT_TRUE(text.contains("It was a quiet morning."));
        TEST_ASSERT_FALSE(text.contains("An Author"));
        TEST_ASSERT_FALSE(text.contains("volunteers"));
        TEST_ASSERT_FALSE(text.contains("published"));
        TEST_ASSERT_FALSE(text.contains("evil"));
    }

    void test_parser_keeps_word_boundary_across_buffered_text_flush() {
        const std::string padding = [] {
            std::string filled;
            for (int i = 0; i < 18; ++i)
                filled += "Dear Reader ";
            return filled;
        }();

        // A literal space and a "&nbsp;" entity are the same boundary: the entity decodes to a
        // plain space before it ever reaches the buffer.
        for (const std::string_view separator : {" ", "&nbsp;"}) {
            File output;
            const std::string markup =
                "<body><p>" + padding + "Dear" + std::string{separator} + "Reader follows</p></body>";

            RsvpWriter writer(output, {.source = "fixture.epub", .title = "Fixture"});
            EpubContent::Parser parser(writer, {}, false, "Fixture", "Fixture");
            TEST_ASSERT_TRUE(parser.write(reinterpret_cast<const uint8_t*>(markup.data()), markup.length()));
            TEST_ASSERT_TRUE(parser.finish());
            TEST_ASSERT_TRUE(writer.finish());

            TEST_ASSERT_EQUAL(std::string::npos, output.contents().find("DearReader"));
            TEST_ASSERT_TRUE(output.contents().contains("Dear\nReader follows\n"));
        }
    }

} // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rsvp_writer_is_deterministic_and_idempotent);
    RUN_TEST(test_package_parses_nav_metadata_and_encoded_manifest_paths);
    RUN_TEST(test_nav_toc_flattens_nested_entries_and_decodes_fragments);
    RUN_TEST(test_ncx_toc_ignores_non_content_labels);
    RUN_TEST(test_toc_keeps_chapters_that_only_begin_like_the_book_title);
    RUN_TEST(test_parser_uses_ordered_toc_labels_and_paragraph_markers);
    RUN_TEST(test_parser_skips_project_gutenberg_boilerplate);
    RUN_TEST(test_parser_preserves_punctuation_only_inline_fragments_without_counting_them_as_words);
    RUN_TEST(test_parser_preserves_nested_language_and_direction_changes);
    RUN_TEST(test_parser_emits_explicit_vertical_writing_mode_once);
    RUN_TEST(test_parser_drops_in_book_contents_lists);
    RUN_TEST(test_parser_keeps_links_that_are_not_a_contents_list);
    RUN_TEST(test_parser_drops_pages_about_the_edition);
    RUN_TEST(test_parser_keeps_word_boundary_across_buffered_text_flush);
    return UNITY_END();
}
