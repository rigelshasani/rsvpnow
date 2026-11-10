#pragma once

#include <cstdint>
#include <string>
#include <string_view>

// Time and words actually read, kept per book (with the reading place) and for all books.
// The reader has no clock that survives power-off, so there are no per-day figures.
struct ReadingStats {
    uint32_t wordsRead = 0;
    uint32_t secondsRead = 0;

    bool operator==(const ReadingStats&) const = default;
};

namespace readingStats {

    // Enough reading to trust the measured pace over the set speed.
    inline constexpr uint32_t kMinWordsForPace = 500;
    inline constexpr uint32_t kMinSecondsForPace = 120;

    // Words per minute actually read, including pauses at punctuation and chapter breaks;
    // `fallbackWpm` until there is enough reading to tell.
    constexpr uint32_t pace(const ReadingStats& stats, uint32_t fallbackWpm) {
        if (stats.wordsRead < kMinWordsForPace || stats.secondsRead < kMinSecondsForPace)
            return fallbackWpm;
        return static_cast<uint32_t>(static_cast<uint64_t>(stats.wordsRead) * 60 / stats.secondsRead);
    }

    constexpr uint32_t secondsFor(uint32_t words, uint32_t wordsPerMinute) {
        return wordsPerMinute == 0 ? 0 : static_cast<uint32_t>(static_cast<uint64_t>(words) * 60 / wordsPerMinute);
    }

    // "under a minute", "45 min", "3 h 20 min", "12 h"
    inline std::string duration(uint32_t seconds, std::string_view underAMinute = "under a minute") {
        const uint32_t minutes = seconds / 60;
        if (minutes == 0)
            return std::string{underAMinute};
        if (minutes < 60)
            return std::to_string(minutes) + " min";
        const uint32_t hours = minutes / 60;
        if (hours >= 10 || minutes % 60 == 0)
            return std::to_string(hours) + " h";
        return std::to_string(hours) + " h " + std::to_string(minutes % 60) + " min";
    }

    // "987", "12,345", "1,234,567"; German and Romanian group with "."
    inline std::string count(uint32_t value, std::string_view separator = ",") {
        std::string digits = std::to_string(value);
        for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3)
            digits.insert(static_cast<size_t>(i), separator);
        return digits;
    }

} // namespace readingStats
