#include "reader/ReadingStart.h"

#include <array>
#include <string>
#include <string_view>

namespace ReadingStart {
    namespace {

        // Lower-case title prefixes, matched at a word boundary. Mirrors desktop/rigels_studio/trim.py.
        constexpr std::array<std::string_view, 29> kFrontMatter{
            "cover", "title page", "titlepage", "half title", "half-title", "copyright", "also by", "books by",
            "praise", "dedication", "epigraph", "acknowledgement", "acknowledgements", "acknowledgment",
            "acknowledgments", "about the author",
            "about the publisher", "about the book", "other books", "frontispiece", "map", "maps",
            "list of illustrations", "list of maps", "list of figures", "list of tables", "illustrations",
            "contents", "table of contents",
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

    } // namespace

    size_t firstWord(std::span<const ChapterMarker> chapters, size_t wordCount) {
        // A long opening without chapters is probably the book itself.
        if (chapters.empty() || wordCount == 0 || chapters.front().wordIndex > wordCount / 10)
            return 0;
        const size_t limit = wordCount / 5;
        for (size_t i = 0; i < chapters.size() && chapters[i].wordIndex < limit; ++i) {
            const size_t end = i + 1 < chapters.size() ? chapters[i + 1].wordIndex : wordCount;
            const bool titleBlock = end - chapters[i].wordIndex < kTitleBlockWords && i + 1 < chapters.size();
            if (!isFrontMatter(chapters[i].title) && !titleBlock)
                return chapters[i].wordIndex;
        }
        return 0;
    }

} // namespace ReadingStart
