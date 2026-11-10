#include "ui/screens/ModernScreens.h"
#include "ui/screens/OnboardingCopy.h"
#include <algorithm>
#include <cstdio>

using screens::modern::tr;
using screens::modern::trf;

namespace screens {
namespace {
std::string clockText(uint32_t ms) {
    const auto seconds = (ms + 999) / 1000;
    char text[24];
    std::snprintf(text, sizeof(text), "%lu:%02lu", static_cast<unsigned long>(seconds/60), static_cast<unsigned long>(seconds%60));
    return text;
}
}
std::string FocusScreen::badge(const ui::Context& ui, uint32_t nowMs) const {
    if (!manualActive_) return {};
    if (session_.phase() == focus::Phase::Complete) return std::string{tr(ui,"Focus done")};
    if (session_.phase() == focus::Phase::WaitingFocus) return std::string{tr(ui,"Next round")};
    const std::string clock = clockText(session_.remainingMs(nowMs));
    if (session_.phase() == focus::Phase::PausedFocus) return trf(ui, "Paused {}", {clock});
    return trf(ui, session_.phase() == focus::Phase::Break ? "Break {}" : "Focus {}", {clock});
}
Action FocusScreen::drawModern(ui::Context& ui, uint32_t nowMs, Screen& screen) {
    using ui::modern::button;
    using ui::modern::Style;
    using focus::Phase;
    if (screen == Screen::FocusEditor) {
        drawEditor(ui, screen);
        if (screen == Screen::FocusTimers) screen = Screen::FocusPresets;
        return Action::None;
    }
    if (screen == Screen::FocusNameEdit) { drawNameEditor(ui, screen); return Action::None; }
    if (screen == Screen::FocusPresets) {
        if (button(ui, {8, 0, 100, 44}, tr(ui,"‹ Focus"), Style::Plain)) screen = Screen::FocusTimers;
        ui.label({120, 0, 210, 44}, tr(ui,"Focus presets"), 16);
        if (button(ui, {464, 0, 168, 44}, tr(ui,"+ Add preset"), Style::Accent, writable_ && timers_.timers.size() < focus::kMaxTimers)) edit(timers_.timers.size(), true, screen);
        const size_t pages = std::max<size_t>(1, (timers_.timers.size()+1)/2);
        const int swipe=presetSwipe_.update(ui,{12,48,550,120});
        if(swipe>0 && presetPage_+1<pages) ++presetPage_;
        if(swipe<0 && presetPage_>0) --presetPage_;
        presetPage_ = std::min(presetPage_, pages-1);
        for (size_t row = 0; row < 2 && presetPage_*2+row < timers_.timers.size(); ++row) {
            const auto index = presetPage_*2+row;
            const auto& timer = timers_.timers[index];
            const int16_t y = 49+row*60;
            if (button(ui, {12, y, 430, 54}, timer.name, index == selectedIndex_ ? Style::Accent : Style::Secondary, true,
                       trf(ui, "{}m focus · {}m break · ×{}", {std::to_string(timer.focusMinutes), std::to_string(timer.breakMinutes), std::to_string(timer.rounds)}), 14, ui::TextAlign::Start)) {
                selectedIndex_ = index; screen = Screen::FocusTimers;
            }
            if (button(ui, {452, y, 112, 54}, tr(ui,"Edit"), Style::Secondary, writable_)) edit(index, false, screen);
        }
        if (button(ui, {576, 49, 52, 54}, "↑", Style::Plain, presetPage_>0)) --presetPage_;
        if (button(ui, {576, 109, 52, 54}, "↓", Style::Plain, presetPage_+1<pages)) ++presetPage_;
        return Action::None;
    }
    if (screen == Screen::FocusSession && manualActive_) {
        const auto phase = session_.phase();
        const bool resting = phase == Phase::Break;
        const bool complete = phase == Phase::Complete;
        const bool ready = phase == Phase::WaitingFocus;
        const bool paused = phase == Phase::PausedFocus;
        const bool canPause = !resting && !complete && !ready && !paused;
        if (!breakVisible() && button(ui, {8, 0, 100, 44}, tr(ui,"‹ Menu"), Style::Plain)) screen = Screen::Read;
        ui.label({120, 0, 320, 44}, resting ? tr(ui,"Time for a break") : complete ? tr(ui,"Focus complete") : ready ? tr(ui,"Ready when you are") : paused ? tr(ui,"Focus paused") : tr(ui,"Focus is running"), 16);
        ui.label({452, 0, 172, 44}, trf(ui, "Round {}", {std::to_string(session_.round())+" / "+std::to_string(session_.rounds())}), 13, ui::themes::Muted, ui::TextAlign::Right);
        ui::modern::rule(ui, {0,43,640,1});
        ui.label({16, 46, 330, 76}, complete || ready ? tr(ui,"Take your time.") : clockText(session_.remainingMs(nowMs)), complete || ready ? 28 : 48, resting ? ui::themes::BreakAccent : ui::themes::Foreground, ui::TextAlign::Center);
        ui.label({16, 130, 330, 30}, resting ? tr(ui,"Reading is paused. Rest your eyes.") : complete ? tr(ui,"Your place is saved.") : readWithFocus_ ? tr(ui,"Reading resumes only when you choose.") : tr(ui,"Timer only · your book stays paused."), 12, ui::themes::Muted, ui::TextAlign::Center);
        if (resting) {
            ui.label({372, 52, 252, 54}, tr(ui,"No rush. We'll wait for you."), 16, ui::themes::Muted, ui::TextAlign::Center, 2);
        } else if (button(ui, {372, 52, 252, 50}, complete ? (readWithFocus_ ? tr(ui,"Resume reading") : tr(ui,"Back to book")) : ready ? tr(ui,"Start next round") : paused ? tr(ui,"Resume Focus") : tr(ui,"Back to book"), Style::Primary, !ui.backgroundBusy())) {
            if (complete) { const bool read = readWithFocus_; close(); screen = Screen::Reader; return read ? Action::FocusRead : Action::Resume; }
            if (ready || paused) {
                session_.resume(nowMs);
                if (!readWithFocus_) { screen=Screen::FocusSession; return Action::None; }
            }
            screen = Screen::Reader;
            return readWithFocus_ ? Action::FocusRead : Action::Resume;
        }
        if (canPause && button(ui, {372, 108, 120, 54}, tr(ui,"Pause"), Style::Secondary)) session_.pause(nowMs);
        if (button(ui, {int16_t(canPause ? 500 : 372), 108, int16_t(canPause ? 124 : 252), 54}, complete ? tr(ui,"Done") : tr(ui,"End Focus"), Style::Secondary)) { close(); screen = Screen::Read; }
        return Action::None;
    }
    if (button(ui, {8, 0, 100, 44}, tr(ui,"‹ Menu"), Style::Plain)) screen = Screen::Read;
    ui.label({120, 0, 240, 44}, manualActive_ ? tr(ui,"Your Focus session") : tr(ui,"Start Focus"), 16);
    if (button(ui, {464, 0, 168, 44}, tr(ui,"Presets →"), Style::Accent)) screen = Screen::FocusPresets;
    ui::modern::rule(ui, {0,43,640,1});
    if (manualActive_) {
        ui.label({16, 64, 330, 62}, badge(ui, nowMs), 32, ui::themes::Foreground, ui::TextAlign::Center);
        ui.label({16, 132, 330, 28}, activeTimer_.name, 14, ui::themes::Muted, ui::TextAlign::Center);
        if (button(ui, {372, 56, 252, 50}, tr(ui,"View session"), Style::Primary)) screen = Screen::FocusSession;
        if (button(ui, {372, 114, 252, 50}, tr(ui,"End Focus"), Style::Secondary)) close();
        return Action::None;
    }
    if (timers_.timers.empty()) {
        ui.label({16, 60, 340, 80}, tr(ui,"Make a little time for a book."), 24, ui::themes::Foreground, ui::TextAlign::Start, 2);
        if (button(ui, {372, 85, 252, 54}, tr(ui,"+ Add preset"), Style::Primary, writable_)) edit(0,true,screen);
        return Action::None;
    }
    selectedIndex_ = std::min(selectedIndex_, timers_.timers.size()-1);
    if (button(ui, {16, 77, 48, 48}, "‹", Style::Secondary, timers_.timers.size()>1, {}, 26)) selectedIndex_=(selectedIndex_+timers_.timers.size()-1)%timers_.timers.size();
    if (button(ui, {298, 77, 48, 48}, "›", Style::Secondary, timers_.timers.size()>1, {}, 26)) selectedIndex_=(selectedIndex_+1)%timers_.timers.size();
    const auto& timer=timers_.timers[selectedIndex_];
    ui.label({74, 62, 214, 62}, std::to_string(timer.focusMinutes)+" min", 36, ui::themes::Foreground, ui::TextAlign::Center);
    ui.label({16, 132, 330, 28}, timer.name+" · "+trf(ui, "{}m break · ×{}", {std::to_string(timer.breakMinutes), std::to_string(timer.rounds)}), 12, ui::themes::Muted, ui::TextAlign::Center);
    const bool read = button(ui, {372, 56, 252, 50}, tr(ui,"Read with Focus"), Style::Primary);
    const bool timerOnly = button(ui, {372, 114, 252, 50}, tr(ui,"Timer only"), Style::Secondary);
    if (read || timerOnly) {
        activeTimer_=timer; manualActive_=true; readWithFocus_=read;
        session_.beginManual(activeTimer_, nowMs); screen=read ? Screen::Reader : Screen::FocusSession;
        return read ? Action::Resume : Action::None;
    }
    return Action::None;
}
} // namespace screens
