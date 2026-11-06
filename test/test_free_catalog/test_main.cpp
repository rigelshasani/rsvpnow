#include <unity.h>

#include <set>
#include <string>

#include "catalog/FreeCatalog.h"

void setUp() {}
void tearDown() {}

namespace {

    const freebooks::Title* find(uint32_t id) {
        for (const auto& shelf: freebooks::shelves())
            for (const auto& title: shelf.titles)
                if (title.gutenbergId == id)
                    return &title;
        return nullptr;
    }

    void test_catalog_is_unique_and_within_download_cap() {
        std::set<uint32_t> ids;
        std::set<std::string> files;
        size_t count = 0;
        for (const auto& shelf: freebooks::shelves()) {
            TEST_ASSERT_FALSE(shelf.titles.empty());
            for (const auto& title: shelf.titles) {
                ++count;
                TEST_ASSERT_TRUE(ids.insert(title.gutenbergId).second);
                TEST_ASSERT_TRUE(files.insert(freebooks::fileName(title)).second);
                TEST_ASSERT_TRUE(title.bytes > 0 && title.bytes < freebooks::kMaxDownloadBytes / 4);
                TEST_ASSERT_FALSE(title.title.empty());
                TEST_ASSERT_FALSE(title.author.empty());
            }
        }
        TEST_ASSERT_EQUAL_UINT32(count, freebooks::titleCount());
        TEST_ASSERT_EQUAL_UINT32(6, freebooks::shelves().size());
    }

    void test_file_names_are_plain_ascii() {
        for (const auto& shelf: freebooks::shelves())
            for (const auto& title: shelf.titles) {
                const std::string name = freebooks::fileName(title);
                TEST_ASSERT_TRUE(name.ends_with(".epub"));
                TEST_ASSERT_FALSE(name.contains("  "));
                for (const char c: name)
                    TEST_ASSERT_TRUE((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                                     || c == ' ' || c == '-' || c == '.');
            }
        TEST_ASSERT_EQUAL_STRING("Les Miserables - Victor Hugo.epub", freebooks::fileName(*find(135)).c_str());
        TEST_ASSERT_EQUAL_STRING("Alices Adventures in Wonderland - Lewis Carroll.epub",
                                 freebooks::fileName(*find(11)).c_str());
        TEST_ASSERT_EQUAL_STRING("Essays First Series - Ralph Waldo Emerson.epub",
                                 freebooks::fileName(*find(2944)).c_str());
        TEST_ASSERT_EQUAL_STRING("The Prince - Niccolo Machiavelli.epub", freebooks::fileName(*find(1232)).c_str());
    }

    void test_urls_and_sizes() {
        TEST_ASSERT_EQUAL_STRING("https://www.gutenberg.org/cache/epub/1342/pg1342.epub",
                                 freebooks::downloadUrl(*find(1342)).c_str());
        TEST_ASSERT_EQUAL_STRING("610 KB", freebooks::sizeLabel(609138).c_str());
        TEST_ASSERT_EQUAL_STRING("88 KB", freebooks::sizeLabel(87914).c_str());
        TEST_ASSERT_EQUAL_STRING("1.8 MB", freebooks::sizeLabel(1802784).c_str());
        TEST_ASSERT_EQUAL_STRING("0 KB", freebooks::sizeLabel(0).c_str());
    }

    void test_library_matching() {
        const auto& frankenstein = *find(84);
        const auto& emma = freebooks::Title{1, "Emma", "Jane Austen", 1};
        TEST_ASSERT_TRUE(freebooks::sameBook(frankenstein, "/library/books/Frankenstein - Mary Shelley.epub", ""));
        TEST_ASSERT_TRUE(freebooks::sameBook(frankenstein, "/library/books/pg84.epub",
                                             "Frankenstein; or, The Modern Prometheus"));
        TEST_ASSERT_TRUE(freebooks::sameBook(frankenstein, "/x.epub", "FRANKENSTEIN"));
        TEST_ASSERT_FALSE(freebooks::sameBook(frankenstein, "/x.epub", "Frankenstein Unbound"));
        TEST_ASSERT_FALSE(freebooks::sameBook(emma, "/x.epub", "Emmanuel"));
        TEST_ASSERT_FALSE(freebooks::sameBook(emma, "/x.epub", ""));
        TEST_ASSERT_TRUE(freebooks::sameBook(*find(135), "/x.epub", "LES MISERABLES"));
        TEST_ASSERT_TRUE(freebooks::sameBook(*find(1184), "/library/books/dumas-count-of-monte-cristo.epub",
                                             "The Count of Monte Cristo"));
    }

} // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_catalog_is_unique_and_within_download_cap);
    RUN_TEST(test_file_names_are_plain_ascii);
    RUN_TEST(test_urls_and_sizes);
    RUN_TEST(test_library_matching);
    return UNITY_END();
}
