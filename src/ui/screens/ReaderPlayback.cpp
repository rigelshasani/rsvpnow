#include "ui/screens/ReaderScreen.h"

#include "reader/ResumePolicy.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace screens {
    namespace {
        constexpr uint32_t kChapterCardMs = 1500;
    } // namespace

    void ReaderScreen::toggle(Preferences& preferences, uint32_t nowMs) {
        if (pageBrowsing_) return;
        if (session.playing)
            requestPause(preferences, nowMs);
        else
            start(nowMs, true);
    }

    void ReaderScreen::update(Preferences& preferences, uint32_t nowMs) {
        // Browsing owns the page font. Preparing a chunk here would replace its
        // cached strike with the RSVP size on every paused browser frame.
        if (pageBrowsing_) return;
        if (wpmFeedbackUntilMs_ > 0 && nowMs >= wpmFeedbackUntilMs_) {
            wpmFeedbackUntilMs_ = 0;
        }
        prepareChunk();
        if (shouldFinishPause(nowMs)) {
            finishPause(preferences, nowMs);
            return;
        }
        prefetchNextWord(nowMs);
        nowMs = millis();
        if (!session.playing)
            return;
        if (chapterCardUntilMs_ != 0) {
            if (nowMs < chapterCardUntilMs_)
                return;
            // The chapter's first word gets its full time once the title has been shown.
            session.lastAdvanceMs = chapterCardUntilMs_;
            chapterCardUntilMs_ = 0;
        }
#if defined(RSVP_BENCHMARK_MODE)
        // Reading benchmarks load indexed fixtures but intentionally exclude persistence I/O.
        ReadingLoop::update(session, settings_, nowMs, true, pauseAtSentenceEndRequested_);
#else
        const size_t previousIndex = session.state.wordIndex;
        if (ReadingLoop::update(session, settings_, nowMs, true, pauseAtSentenceEndRequested_)) {
            ReadingProgress::saveChapterTransition(session, preferences, store, previousIndex, session.state.wordIndex,
                                                   nowMs);
            const ChapterMarker* reached = session.metadata.chapterAt(session.state.wordIndex);
            if (reached != nullptr && reached->wordIndex > previousIndex && settings_.pacing.chapterPause
                && settings_.mode != settings::ReadingMode::page
                && session.metadata.writingMode != WritingMode::verticalRl && session.playing)
                chapterCardUntilMs_ = std::max<uint32_t>(nowMs, 1) + kChapterCardMs;
        }
#endif
        if (!session.playing && pauseAtSentenceEndRequested_)
            finishPause(preferences, nowMs);
    }

    void ReaderScreen::rewindAfterBreak(uint32_t nowMs) {
        const size_t pausedAt = pausedWordIndex_;
        pausedWordIndex_ = SIZE_MAX;
        // A place chosen while paused (a chapter, a search result, a scrub) is where to start.
        if (!settings_.pacing.rewindOnResume || pausedAt != session.state.wordIndex)
            return;
        const uint32_t breakMs = !pauseTimeKnown_ ? ResumePolicy::kUnknownBreak
                               : nowMs >= pausedAtMs_ ? nowMs - pausedAtMs_ : 0;
        const uint8_t sentences = ResumePolicy::rewindSentences(breakMs);
        if (sentences == 0)
            return;
        const ChapterMarker* chapter = session.metadata.chapterAt(pausedAt);
        const size_t floor = chapter == nullptr ? 0 : chapter->wordIndex;
        for (uint8_t i = 0; i < sentences && session.state.wordIndex > floor; ++i)
            ReadingLoop::rewindSentence(session);
        if (session.state.wordIndex < floor)
            ReadingLoop::seekTo(session, floor);
    }

    void ReaderScreen::start(uint32_t nowMs, bool locked) {
        rewindAfterBreak(nowMs);
        chapterCardUntilMs_ = 0;
        pagePreview_ = false;
        pageBrowsing_ = false;
        playLocked_ = locked;
        pauseAtSentenceEndRequested_ = false;
        wpmFeedbackUntilMs_ = 0;
        prepareChunk();
        ReadingLoop::start(session, nowMs);
    }

    void ReaderScreen::requestPause(Preferences& preferences, uint32_t nowMs) {
        if (!session.playing)
            return;
        playLocked_ = false;
        if (settings_.pauseMode == settings::PauseMode::instant) {
            finishPause(preferences, nowMs);
            return;
        }
        pauseAtSentenceEndRequested_ = true;
        prepareChunk();
        if (shouldFinishPause(nowMs)) {
            finishPause(preferences, nowMs);
        }
    }

    bool ReaderScreen::shouldFinishPause(uint32_t nowMs) const {
        if (!session.playing || !pauseAtSentenceEndRequested_)
            return false;
        // A chapter boundary is the end of a sentence.
        if (chapterCardActive(nowMs))
            return true;
        const uint32_t durationMs = ReadingLoop::currentWordDurationMs(session, settings_);
        return durationMs > 0 && ReadingLoop::elapsedInCurrentWordMs(session, nowMs) >= durationMs
            && (ReadingLoop::currentWordEndsSentence(session) || ReadingLoop::atEnd(session));
    }

    void ReaderScreen::finishPause(Preferences& preferences, uint32_t nowMs) {
        chapterCardUntilMs_ = 0;
        ReadingLoop::pause(session);
        pausedAtMs_ = nowMs;
        pauseTimeKnown_ = true;
        pausedWordIndex_ = session.state.wordIndex;
        pauseAtSentenceEndRequested_ = false;
        playLocked_ = false;
        ReadingProgress::save(session, preferences, true, nowMs);
#if !defined(RSVP_BENCHMARK_MODE)
        // Write the place to the card on every pause, not only at chapter changes and a clean
        // power-off, so a flat battery or a reset costs nothing.
        if (session.stored() && session.state.wordIndex != mirroredWord_) {
            ReadingProgress::mirror(session, store);
            mirroredWord_ = session.state.wordIndex;
        }
#endif
    }

} // namespace screens
