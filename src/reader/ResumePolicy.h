#pragma once

#include <cstdint>

// How reading picks up again after a break.
namespace ResumePolicy {

    inline constexpr uint32_t kShortBreakMs = 2UL * 60UL * 1000UL;
    inline constexpr uint32_t kLongBreakMs = 30UL * 60UL * 1000UL;
    // A break whose length is unknown, such as reopening a book after power-off.
    inline constexpr uint32_t kUnknownBreak = UINT32_MAX;

    // Sentences to step back so the reader regains the thread: none after a short pause, back to
    // the start of the current sentence after a couple of minutes, one more after a long break.
    constexpr uint8_t rewindSentences(uint32_t breakMs) {
        return breakMs >= kLongBreakMs ? 2 : breakMs >= kShortBreakMs ? 1 : 0;
    }

} // namespace ResumePolicy
