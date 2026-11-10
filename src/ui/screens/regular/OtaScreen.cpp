#include "ui/screens/ScreenCommon.h"
#include "ui/screens/ModernScreens.h"
#include "ui/screens/OnboardingCopy.h"

using screens::modern::tr;
using screens::modern::trf;

namespace screens {

    Action ota(ui::Context& ui, std::string_view firmwareVersion, Screen& screen, const OtaUpdater::Offer* offer) {
        if (const Action action = detail::navigation(ui, Screen::Ota, screen); action != Action::None)
            return action;
        const ui::Rect content = detail::tabContent(ui);
        if (ui.modernPresentation()) {
            const bool ready = offer && offer->ready;
            ui.label({16, 50, 340, 22}, trf(ui, "Installed: {}", {firmwareVersion}), 11, ui::themes::Muted);
            ui.label({16, 78, 340, 40}, offer && !offer->version.empty()
                ? trf(ui, "Selected: {}", {offer->version}) : std::string{tr(ui,"Choose software to check.")}, 15, ui::themes::Foreground, ui::TextAlign::Start, 2);
            ui.label({16, 124, 340, 40}, offer && !offer->message.empty()
                ? tr(ui, offer->message) : tr(ui,"Wi-Fi update · keep the reader powered."), 12, ui::themes::Muted, ui::TextAlign::Start, 2);
            if (ui::modern::button(ui, {374, 50, 250, 34}, tr(ui,"Check current software"), ui::modern::Style::Secondary, true, {}, 13)) return Action::OtaCheck;
            if (ui::modern::button(ui, {374, 90, 250, 34}, tr(ui,"Return to Classic"), ui::modern::Style::Secondary, true, {}, 13)) return Action::OtaSwitchCheck;
            if (ui::modern::button(ui, {374, 130, 250, 34}, ready && offer->switching ? tr(ui,"Confirm switch & install") : tr(ui,"Confirm installation"), ui::modern::Style::Primary, ready, {}, 13)) return Action::OtaInstall;
            return Action::None;
        }
        constexpr int16_t gap = 6;
        ui::Grid actions{content, 4, 56, gap};
        if (ui.button(actions.next(), "<<"))
            screen = Screen::Device;
        if (ui.button(actions.next(), ui.text(UiText::CheckOnly)))
            return Action::OtaCheck;
        if (ui.button(actions.next(), tr(ui,"Try New UI")))
            return Action::OtaSwitchCheck;
        if (ui.button(actions.next(), tr(ui,"Confirm install"), offer && offer->ready))
            return Action::OtaInstall;
        if (offer) ui.label({content.x, static_cast<int16_t>(content.y + 65), content.w, 48},
            offer->version.empty() ? std::string{tr(ui, offer->message)} : trf(ui, "Selected: {}", {offer->version}), 1, ui::themes::Muted, ui::TextAlign::Start, 2);
        const uint8_t versionSize = firmwareVersion.size() * 12U <= static_cast<size_t>(content.w) ? 2 : 1;
        ui.label({content.x, static_cast<int16_t>(content.y + content.h - 24), content.w, 24}, firmwareVersion,
                 versionSize, ui::themes::ColorRole::Muted, ui::TextAlign::Center);
        return Action::None;
    }

} // namespace screens
