#include "ui/screens/ScreenCommon.h"
#include "ui/screens/ModernScreens.h"
#include "ui/screens/OnboardingCopy.h"

using screens::modern::tr;
using screens::modern::trf;

namespace screens {
    bool pacingSettings(ui::Context& ui, settings::PacingSettings& config, Screen& screen) {
        bool changed = false;
        if (ui.modernPresentation()) {
            if (modern::header(ui, ui.text(UiText::WordPacing), screen, Screen::ReadingHub) == Action::Resume) screen = Screen::Reader;
            changed |= ui.slider({12, 49, 198, 50}, ui.text(UiText::LongWords), config.longWordDelayMs, " ms");
            changed |= ui.slider({222, 49, 198, 50}, ui.text(UiText::Complexity), config.complexWordDelayMs, " ms");
            changed |= ui.slider({432, 49, 196, 50}, ui.text(UiText::Punctuation), config.punctuationDelayMs, " ms");
            using ui::modern::button;
            using ui::modern::Style;
            const auto onOff = [&](bool on) { return ui.text(on ? UiText::On : UiText::Off); };
            if (button(ui, {12, 111, 148, 53}, tr(ui,"Chapter pause"), Style::Secondary, true, onOff(config.chapterPause), 13)) {
                config.chapterPause = !config.chapterPause;
                changed = true;
            }
            if (button(ui, {166, 111, 148, 53}, tr(ui,"Rewind on resume"), Style::Secondary, true, onOff(config.rewindOnResume),
                       13)) {
                config.rewindOnResume = !config.rewindOnResume;
                changed = true;
            }
            if (button(ui, {320, 111, 148, 53}, tr(ui,"Speed ramp"), Style::Secondary, true, onOff(config.speedRamp), 13)) {
                config.speedRamp = !config.speedRamp;
                changed = true;
            }
            if (ui::modern::button(ui,{480,111,148,53},ui.text(UiText::Reset),ui::modern::Style::Secondary)) {
                changed |= config != settings::PacingSettings{}; config = {};
            }
            return changed;
        }
        const ui::Rect content = detail::content(ui);
        constexpr int16_t gap = 6;
        constexpr int16_t backWidth = 56;
        constexpr int16_t resetWidth = 90;
        const int16_t rowHeight = static_cast<int16_t>((content.h - gap * 2) / 3);

        if (ui.button({content.x, content.y, backWidth, rowHeight}, "<<"))
            screen = ui.modernPresentation() ? Screen::ReadingHub : Screen::Settings;
        changed |= ui.slider({static_cast<int16_t>(content.x + backWidth + gap), content.y,
                              static_cast<int16_t>(content.w - backWidth - gap), rowHeight},
                             ui.text(UiText::LongWords), config.longWordDelayMs, " ms");

        const int16_t secondRowY = static_cast<int16_t>(content.y + rowHeight + gap);
        changed |= ui.slider({content.x, secondRowY, content.w, rowHeight}, ui.text(UiText::Complexity),
                             config.complexWordDelayMs, " ms");

        const int16_t thirdRowY = static_cast<int16_t>(secondRowY + rowHeight + gap);
        if (ui.button({content.x, thirdRowY, resetWidth, rowHeight}, ui.text(UiText::Reset))) {
            changed |= config != settings::PacingSettings{};
            config = {};
        }
        changed |= ui.slider({static_cast<int16_t>(content.x + resetWidth + gap), thirdRowY,
                              static_cast<int16_t>(content.w - resetWidth - gap), rowHeight},
                             ui.text(UiText::Punctuation), config.punctuationDelayMs, " ms");
        return changed;
    }

} // namespace screens
