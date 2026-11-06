// Golden-data generator for Rigels Studio.
//
// Runs text through the firmware's own normalizer, tokenizer and ReadingLoop timing so the desktop
// ports (rigels_studio/text.py and web/timing.js) can be checked against the reader byte for byte.
// Built against the native_test objects by build_harness.sh; reads readable lines on stdin and
// writes one JSON object per token on stdout.
#include <iostream>
#include <string>
#include <vector>

#include "reader/ReadingLoop.h"
#include "text/RsvpTokenizer.h"

namespace {

    struct Variant {
        uint16_t wpm;
        uint16_t longWordDelayMs;
        uint16_t complexWordDelayMs;
        uint16_t punctuationDelayMs;
    };

    // The first three isolate each bonus percentage (delay 100 ms => bonus ms == percent at 60 ms base).
    // The rest are end-to-end checks across the setting ranges.
    constexpr Variant kVariants[] = {
        {1000, 100, 0, 0}, {1000, 0, 100, 0}, {1000, 0, 0, 100},
        {300, 200, 200, 200}, {450, 50, 350, 600}, {120, 600, 0, 250}, {700, 0, 600, 50},
    };

    void writeJsonString(std::ostream& out, const std::string& text) {
        out << '"';
        for (const unsigned char c: text) {
            switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    static constexpr char kHex[] = "0123456789abcdef";
                    out << "\\u00" << kHex[c >> 4] << kHex[c & 15];
                } else {
                    out << c;
                }
            }
        }
        out << '"';
    }

} // namespace

int main() {
    std::vector<std::string> words;
    std::vector<size_t> lineOf;
    std::string line;
    size_t lineNumber = 0;
    while (std::getline(std::cin, line)) {
        RsvpText::appendLineWords(
            line,
            [&](const std::string& token) {
                words.push_back(token);
                lineOf.push_back(lineNumber);
                return true;
            },
            words.size(), nullptr);
        ++lineNumber;
    }

    std::vector<std::vector<uint32_t>> durations(words.size());
    for (const Variant& variant: kVariants) {
        settings::ReadingSettings reading;
        reading.wpm = variant.wpm;
        reading.pacing.longWordDelayMs = variant.longWordDelayMs;
        reading.pacing.complexWordDelayMs = variant.complexWordDelayMs;
        reading.pacing.punctuationDelayMs = variant.punctuationDelayMs;
        ReadingSession session;
        ReadingLoop::setWords(session, words, 0);
        for (size_t i = 0; i < words.size(); ++i) {
            // Step through like playback does: pacing mode (words vs CJK phrases) follows the current word.
            session.state.wordIndex = static_cast<uint32_t>(i);
            session.currentWord = words[i];
            durations[i].push_back(ReadingLoop::currentWordDurationMs(session, reading));
        }
    }

    std::cout << "{\"variants\":[";
    for (size_t v = 0; v < std::size(kVariants); ++v) {
        const Variant& variant = kVariants[v];
        std::cout << (v ? "," : "") << "{\"wpm\":" << variant.wpm << ",\"longWordDelayMs\":" << variant.longWordDelayMs
                  << ",\"complexWordDelayMs\":" << variant.complexWordDelayMs
                  << ",\"punctuationDelayMs\":" << variant.punctuationDelayMs << "}";
    }
    std::cout << "],\"tokens\":[\n";
    for (size_t i = 0; i < words.size(); ++i) {
        std::cout << (i ? ",\n" : "") << "{\"line\":" << lineOf[i] << ",\"t\":";
        writeJsonString(std::cout, words[i]);
        std::cout << ",\"ms\":[";
        for (size_t v = 0; v < durations[i].size(); ++v)
            std::cout << (v ? "," : "") << durations[i][v];
        std::cout << "]}";
    }
    std::cout << "\n]}\n";
}
