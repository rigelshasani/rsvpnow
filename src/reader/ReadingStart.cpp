#include "reader/ReadingStart.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
#include <string>
#include <string_view>

namespace ReadingStart {
    namespace {

        // Lower-case title prefixes, matched at a word boundary. Mirrors desktop/rigels_studio/trim.py.
        constexpr std::array<std::string_view, 30> kFrontMatter{
            "cover", "title page", "titlepage", "half title", "half-title", "copyright", "also by", "books by",
            "praise", "dedication", "epigraph", "acknowledgement", "acknowledgements", "acknowledgment",
            "acknowledgments", "about the author",
            "about the publisher", "about the book", "other books", "frontispiece", "map", "maps",
            "list of illustrations", "list of maps", "list of figures", "list of tables", "illustrations",
            "contents", "table of contents", "imprint",
        };

        // A heading shorter than this before the first chapter is a title or author block.
        constexpr size_t kTitleBlockWords = 100;

        constexpr char lower(char c) {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        }

        constexpr bool letter(char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        }

        bool isFrontMatter(std::string_view title) {
            std::string text;
            for (const char c: title)
                text += lower(c);
            const size_t start = text.find_first_not_of(" \t");
            if (start == std::string::npos)
                return false;
            const std::string_view trimmed = std::string_view{text}.substr(start);
            for (const std::string_view prefix: kFrontMatter)
                if (trimmed.starts_with(prefix) && (trimmed.size() == prefix.size() || !letter(trimmed[prefix.size()])))
                    return true;
            return false;
        }

        // A contents list is made of chapter-title words; prose is not.
        constexpr size_t kContentsSampleWords = 240;
        constexpr size_t kContentsMinKeyWords = 12;
        constexpr size_t kMaxTitlesCompared = 400;
        constexpr size_t kKeyWordBytes = 4;

        // Calls `visit` with a hash of each word of four or more bytes, ignoring case and punctuation.
        template<typename Visitor>
        void forEachKeyWord(std::string_view text, Visitor&& visit) {
            uint32_t hash = 2166136261u;
            size_t length = 0;
            for (size_t i = 0; i <= text.size(); ++i) {
                const unsigned char c = i < text.size() ? static_cast<unsigned char>(text[i]) : ' ';
                if (c >= 0x80 || letter(static_cast<char>(c)) || (c >= '0' && c <= '9')) {
                    hash = (hash ^ static_cast<unsigned char>(lower(static_cast<char>(c)))) * 16777619u;
                    ++length;
                    continue;
                }
                if (length >= kKeyWordBytes)
                    visit(hash);
                hash = 2166136261u;
                length = 0;
            }
        }

        bool listsChapterTitles(std::span<const ChapterMarker> chapters, size_t begin, size_t end,
                                const WordAt& wordAt) {
            std::vector<uint32_t> titleWords;
            for (size_t i = 0; i < chapters.size() && i < kMaxTitlesCompared; ++i)
                forEachKeyWord(chapters[i].title, [&](uint32_t hash) { titleWords.push_back(hash); });
            std::ranges::sort(titleWords);
            size_t keyWords = 0;
            size_t listed = 0;
            for (size_t word = begin; word < end && word < begin + kContentsSampleWords; ++word)
                forEachKeyWord(wordAt(word), [&](uint32_t hash) {
                    ++keyWords;
                    listed += std::ranges::binary_search(titleWords, hash);
                });
            return keyWords >= kContentsMinKeyWords && listed * 10 >= keyWords * 6;
        }

    } // namespace

    size_t firstWord(std::span<const ChapterMarker> chapters, size_t wordCount, const WordAt& wordAt) {
        // A long opening without chapters is probably the book itself.
        if (chapters.empty() || wordCount == 0 || chapters.front().wordIndex > wordCount / 10)
            return 0;
        const size_t limit = wordCount / 5;
        for (size_t i = 0; i < chapters.size() && chapters[i].wordIndex < limit; ++i) {
            const size_t end = i + 1 < chapters.size() ? chapters[i + 1].wordIndex : wordCount;
            const bool titleBlock = end - chapters[i].wordIndex < kTitleBlockWords && i + 1 < chapters.size();
            if (isFrontMatter(chapters[i].title) || titleBlock)
                continue;
            // A contents page is short next to the book it lists.
            if (wordAt && i + 1 < chapters.size() && end - chapters[i].wordIndex < wordCount / 20
                && listsChapterTitles(chapters, chapters[i].wordIndex, end, wordAt))
                continue;
            return chapters[i].wordIndex;
        }
        return 0;
    }

} // namespace ReadingStart
