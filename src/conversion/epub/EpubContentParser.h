#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "conversion/epub/EpubPackage.h"
#include "conversion/rsvp/RsvpWriter.h"

namespace EpubContent {

    std::string plainTextFromXmlFragment(std::string_view fragment);
    class Parser {
    public:
        Parser(RsvpWriter& writer, std::span<const EpubPackage::TocEntry> tocEntries, bool hasToc,
               std::string_view fallbackChapterTitle, std::string_view bookTitle);

        bool write(const uint8_t* data, size_t length);
        bool finish();
        bool reachedWordLimit() const;

    private:
        enum class Mode {
            Text,
            Tag,
            Entity,
            Comment,
        };

        bool flushLine(bool endParagraph = true);
        bool releaseHeldBlocks();
        void resetLine();
        bool flushWordAlignedPrefix();
        bool writeChapter(std::string_view title);
        bool emitTocEntriesThrough(size_t index);
        int matchingTocEntry(std::string_view anchor) const;
        bool suppressHeading(std::string_view heading) const;
        void appendToActiveText(char c);
        bool processDecodedText(char c);
        bool processTextChar(char c);
        bool processTag(std::string_view tag);
        bool processEntityChar(char c);
        bool processCommentChar(char c);
        bool processChar(char c);
        bool changeLanguageState(std::string_view locale, std::string_view direction);
        bool emitVerticalWriting();

        struct LanguageScope {
            std::string tag;
            std::string locale;
            std::string direction;
            bool changed = false;
        };

        RsvpWriter& writer_;
        const std::span<const EpubPackage::TocEntry> tocEntries_;
        const bool hasToc_;
        const std::string fallbackChapterTitle_;
        const std::string bookTitle_;
        std::string line_;
        std::string heading_;
        std::string tag_;
        std::string entity_;
        std::string commentTail_;
        std::vector<LanguageScope> languageScopes_;
        Mode mode_ = Mode::Text;
        bool inHeading_ = false;
        bool documentChapterWritten_ = false;
        bool inStyle_ = false;
        bool styleVertical_ = false;
        uint8_t verticalCssMatch_ = 0;
        size_t nextTocEntry_ = 0;
        int skipDepth_ = 0;
        // Contents-list filter: a run of blocks that are mostly internal links is navigation,
        // not prose. Blocks are held until the run is long enough to be sure, then dropped.
        struct HeldBlock {
            std::string text;
            bool link = false;
        };
        std::vector<HeldBlock> heldBlocks_;
        size_t heldLinkBlocks_ = 0;
        size_t lineChars_ = 0;
        size_t lineLinkChars_ = 0;
        uint8_t shortBlocksSinceLink_ = 0;
        bool inInternalLink_ = false;
        bool blockPartlyWritten_ = false;
        bool droppingLinkList_ = false;
        std::string boilerplateTag_;
        int boilerplateDepth_ = 0;
    };

} // namespace EpubContent
