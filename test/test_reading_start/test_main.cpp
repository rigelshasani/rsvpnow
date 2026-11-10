// Chapter lists below are the real openings of books on a test reader.
#include <unity.h>

#include <vector>

#include "reader/ReadingStart.h"
#include "text/ChapterHeading.h"

void setUp() {}
void tearDown() {}

namespace {
    size_t start(std::vector<ChapterMarker> chapters, size_t words) {
        return ReadingStart::firstWord(chapters, words);
    }

    void test_gutenberg_contents_and_illustration_list_are_skipped() {
        TEST_ASSERT_EQUAL_UINT32(327, start({{"List of Illustrations", 0}, {"CHAPTER I A SHIFTING REEF", 327},
                                             {"CHAPTER II PRO AND CON", 2296}}, 104807));
    }

    void test_copyright_and_epigraph_skipped_but_introduction_kept() {
        TEST_ASSERT_EQUAL_UINT32(450, start({{"Copyright", 0}, {"Epigraph", 145}, {"Introduction: My Story", 450},
                                             {"1: The Surprising Power of Atomic Habits", 3531}}, 76643));
        TEST_ASSERT_EQUAL_UINT32(1924, start({{"Copyright Page", 588}, {"Dedication", 1548}, {"Acknowledgements", 1557},
                                              {"PREFACE", 1924}, {"LAW 1 - NEVER OUTSHINE THE MASTER", 5734}}, 229484));
        TEST_ASSERT_EQUAL_UINT32(259, start({{"Preface: Singletonville", 259}, {"Introduction", 3258}}, 59549));
    }

    void test_short_author_block_is_skipped() {
        TEST_ASSERT_EQUAL_UINT32(93, start({{"Alexandre Dumas", 3}, {"Chapter 1.", 93}, {"Chapter 2.", 3248}}, 463922));
    }

    // Plain-text books: the text after "Chapter", "Part" or "Book" on a line.
    void test_text_headings_need_a_number_not_just_the_word() {
        for (const char* heading: {"", " 1", " 12.", " II", " XIV. The Storm", " One", " the First", ": 3", " Twenty-One",
                                   " 2 - The Tree of Knowledge"})
            TEST_ASSERT_TRUE_MESSAGE(RsvpText::numbersHeading(heading), heading);
        // Hard-wrapped prose lines that happen to start with the word.
        for (const char* prose: {" of the crew went ashore.", " of me wanted to leave", " in hand, he sat down.",
                                 " and verse he quoted", " mild, part civil", " after book he read", " the cat"})
            TEST_ASSERT_FALSE_MESSAGE(RsvpText::numbersHeading(prose), prose);
    }

    void test_unsure_cases_start_at_the_beginning() {
        TEST_ASSERT_EQUAL_UINT32(0, start({}, 5000));
        TEST_ASSERT_EQUAL_UINT32(0, start({{"Chapter 1", 0}, {"Chapter 2", 900}}, 9000));
        // A long untitled opening is probably the text itself.
        TEST_ASSERT_EQUAL_UINT32(0, start({{"Contents", 2000}, {"Chapter 1", 2100}}, 10000));
        // Nothing but front matter in the first fifth.
        TEST_ASSERT_EQUAL_UINT32(0, start({{"Copyright", 0}, {"Contents", 500}, {"Chapter 1", 5000}}, 10000));
        // "Mapping the Mind" is not "Map".
        TEST_ASSERT_EQUAL_UINT32(0, start({{"Mapping the Mind", 0}, {"Two", 4000}}, 40000));
    }
} // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_gutenberg_contents_and_illustration_list_are_skipped);
    RUN_TEST(test_copyright_and_epigraph_skipped_but_introduction_kept);
    RUN_TEST(test_short_author_block_is_skipped);
    RUN_TEST(test_text_headings_need_a_number_not_just_the_word);
    RUN_TEST(test_unsure_cases_start_at_the_beginning);
    return UNITY_END();
}
