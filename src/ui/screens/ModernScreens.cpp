#include "ui/screens/ModernScreens.h"
#include "ui/screens/OnboardingCopy.h"

namespace screens::modern {
    using ui::modern::button;
    using ui::modern::Style;

    Action dictionarySetup(ui::Context& ui, DictionarySetup& state, Screen& screen) {
        if (button(ui, {8,0,112,43}, tr(ui,"‹ Back"), Style::Plain, true, {}, 13)) {
            state.confirming = false;
            screen = state.onboarding ? Screen::Onboarding : Screen::DictionaryHelp;
            return Action::None;
        }
        ui.label({136,0,490,43}, tr(ui,"English dictionary · offline"), 17);
        ui::modern::rule(ui,{0,43,640,1});
        ui.label({16,52,346,30}, state.confirming ? tr(ui,"Replace your current pack?")
            : state.installed ? tr(ui,"Dictionary installed") : tr(ui,"Install once. Read offline."), 19);
        ui.label({16,88,346,72}, !state.message.empty() && !state.confirming ? std::string_view{state.message}
            : state.confirming ? tr(ui,"This replaces the current English pack. Keep a copy of a custom dictionary first.")
            : tr(ui,"About 48 MiB on SD. Download over Wi-Fi or copy to SD. More languages are planned."),
            13,ui::themes::Muted,ui::TextAlign::Start,4);
        if (button(ui,{386,54,238,50},state.confirming ? tr(ui,"Confirm replacement")
            : state.installed ? tr(ui,"Replace dictionary") : tr(ui,"Install dictionary"),Style::Primary,true,{},13)) {
            if (state.installed && !state.confirming) state.confirming = true;
            else return Action::InstallDictionary;
        }
        if (button(ui,{386,116,238,48},state.confirming ? tr(ui,"Keep current dictionary") : tr(ui,"Wi-Fi settings"),
                   Style::Secondary,true,{},13)) {
            if (state.confirming) state.confirming = false;
            else screen = Screen::NetworkSettings;
        }
        return Action::None;
    }

    Action header(ui::Context& ui, std::string_view title, Screen& screen, Screen parent, std::string_view trailing) {
        auto parentText = parent == Screen::NetworkSettings ? std::string_view{"Wi-Fi"} : parent == Screen::Help ? copy(ui,Copy::Help) : parent == Screen::Read ? caption(ui, UiText::Read, tr(ui,"Menu"))
                               : parent == Screen::ReadingHub ? caption(ui, UiText::Reading, tr(ui,"Pace"))
                               : parent == Screen::DisplayHub ? ui.text(UiText::Display)
                               : parent == Screen::Connections ? caption(ui, UiText::Network, tr(ui,"Connect"))
                               : parent == Screen::FocusTimers ? ui.text(UiText::Focus)
                               : ui.text(parent == Screen::Read ? UiText::Read
                                       : parent == Screen::Library ? UiText::Library
                                       : parent == Screen::Device ? UiText::Device : UiText::Settings);
        if (parent == Screen::Dictionary) parentText = tr(ui,"Dictionary");
        if (parent == Screen::GetBooks) parentText = tr(ui,"Add books");
        if (parent == Screen::FreeBooks) parentText = tr(ui,"Free books");
        const bool dictionaryParent = parent == Screen::Dictionary || parent == Screen::GetBooks || parent == Screen::FreeBooks;
        if (button(ui, {8, 0, int16_t(dictionaryParent ? 112 : 94), 43},
                   std::string{"‹ "} + std::string{parentText}, Style::Plain, true, {}, 13))
            screen = parent;
        ui.label({int16_t(dictionaryParent ? 132 : 112), 0, int16_t(dictionaryParent ? 202 : 222), 43}, title, 15);
        ui.label({340, 0, 132, 43}, trailing, 11, ui::themes::Accent, ui::TextAlign::Right);
        if (button(ui, {480, 0, 152, 43}, std::string{copy(ui,Copy::BackBook)}+" ↗", Style::Accent, true, {}, 13))
            return Action::Resume;
        ui::modern::rule(ui, {0, 43, 640, 1});
        return Action::None;
    }

    Action menu(ui::Context& ui, std::string_view title, std::string_view author, uint8_t progress,
                bool hasBook, settings::InterfaceSettings& interface, Screen& screen, size_t chapter,
                std::string_view bookStatus) {
        if (ui.focusBadge().empty() && hasBook && !bookStatus.empty()) {
            // Time left and read; tap for the full reading stats.
            if (button(ui, {8, 2, 326, 30}, bookStatus, Style::Plain, true, {}, 11, ui::TextAlign::Start))
                screen = Screen::ReadingStats;
        } else {
            ui.label({16, 8, 318, 20}, !ui.focusBadge().empty() ? std::string_view{ui.focusBadge()} : hasBook ? copy(ui,Copy::Saved) : ui.text(UiText::NoLibraryItems), 1,
                     ui::themes::Muted);
        }
        ui.label({16, 32, 318, 35}, title, 3);
        ui.label({16, 67, 318, 23}, author.empty() ? ui.text(UiText::Unknown) : author, 1, ui::themes::Muted);
        ui.progress({16, 103, 178, 2}, progress);
        const auto location = std::to_string(progress) + "%" + (chapter ? " · " + std::string{ui.text(UiText::Chapter)} + " " + std::to_string(chapter) : "");
        ui.label({204, 94, 130, 20}, location, 11, ui::themes::Muted, ui::TextAlign::Right);
        if (button(ui, {16, 120, 154, 44}, std::string{copy(ui,Copy::BackBook)}+" ↗", Style::Primary, hasBook))
            return Action::Resume;
        if (button(ui, {180, 120, 154, 44}, ui.focusBadge().empty()?copy(ui,Copy::StartFocus):std::string_view{ui.focusBadge()}, Style::Accent))
            screen = Screen::FocusTimers;
        ui::modern::rule(ui, {352, 8, 1, 156});
        constexpr UiText titles[]{UiText::Chapters, UiText::Library, UiText::Settings};
        constexpr Screen destinations[]{Screen::Chapters, Screen::Library, Screen::Settings};
        constexpr ui::modern::Symbol symbols[]{ui::modern::Symbol::Chapters, ui::modern::Symbol::Book, ui::modern::Symbol::Settings};
        for (int row = 0; row < 3; ++row) {
            const ui::Rect rect{364, static_cast<int16_t>(8 + 52 * row), 260, 52};
            if (ui::modern::row(ui, rect, ui.text(titles[row]), {}, symbols[row])) screen = destinations[row];
        }
        return Action::None;
    }

    namespace {
        std::string duration(const ui::Context& ui, uint32_t seconds) {
            return readingStats::duration(seconds, tr(ui, "under a minute"));
        }
        std::string count(const ui::Context& ui, uint32_t value) {
            const auto locale = ui.locale();
            const bool dots = locale == "de" || locale.starts_with("de-") || locale == "ro" || locale.starts_with("ro-");
            return readingStats::count(value, dots ? "." : ",");
        }
    } // namespace

    std::string bookStatus(const ui::Context& ui, const ReadingStats& book, uint32_t wordsLeft, uint32_t fallbackWpm) {
        std::string status = trf(ui, "{} left", {duration(ui, readingStats::secondsFor(wordsLeft, readingStats::pace(book, fallbackWpm)))});
        if (book.secondsRead >= 60)
            status += " · " + trf(ui, "{} read", {duration(ui, book.secondsRead)});
        return status;
    }

    Action readingStats(ui::Context& ui, const ReadingStats& book, uint32_t wordsLeft, uint32_t fallbackWpm,
                        bool hasBook, const ReadingStats& all, Screen& screen) {
        if (const auto action = header(ui, tr(ui, "Reading stats"), screen); action != Action::None)
            return action;
        const auto row = [&](int16_t x, int16_t y, std::string_view name, const std::string& value) {
            ui.label({x, y, 140, 24}, tr(ui, name), 13, ui::themes::Muted);
            ui.label({static_cast<int16_t>(x + 140), y, 150, 24}, value, 15, ui::themes::Foreground,
                     ui::TextAlign::Right);
        };
        const auto paceText = [](const ReadingStats& stats) {
            return stats.wordsRead >= readingStats::kMinWordsForPace && stats.secondsRead >= readingStats::kMinSecondsForPace
                       ? std::to_string(readingStats::pace(stats, 0)) + " wpm"
                       : std::string{"—"};
        };
        ui.label({16, 50, 290, 22}, tr(ui, "This book"), 15, ui::themes::Accent);
        if (hasBook) {
            row(16, 76, tr(ui,"Time read"), duration(ui, book.secondsRead));
            row(16, 100, tr(ui,"Words read"), count(ui, book.wordsRead));
            row(16, 124, tr(ui,"Your pace"), paceText(book));
            row(16, 148, tr(ui,"Time left"),
                duration(ui, readingStats::secondsFor(wordsLeft, readingStats::pace(book, fallbackWpm))));
        } else {
            ui.label({16, 76, 290, 24}, tr(ui, "No book open"), 13, ui::themes::Muted);
        }
        ui::modern::rule(ui, {320, 52, 1, 112});
        ui.label({334, 50, 290, 22}, tr(ui, "All books"), 15, ui::themes::Accent);
        row(334, 76, tr(ui,"Time read"), duration(ui, all.secondsRead));
        row(334, 100, tr(ui,"Words read"), count(ui, all.wordsRead));
        row(334, 124, tr(ui,"Your pace"), paceText(all));
        ui.label({334, 148, 290, 20}, tr(ui, "Time left uses your own pace."), 11, ui::themes::Muted);
        return Action::None;
    }

    Action hub(ui::Context& ui, Screen& screen) {
        const Screen current = screen;
        const auto title = current == Screen::Connections ? caption(ui, UiText::Network, tr(ui,"Connections"))
                           : current == Screen::DictionaryHelp ? std::string_view{tr(ui,"Dictionary setup")}
                           : current == Screen::Help ? std::string_view{tr(ui,"How to read")}
                           : ui.text(current == Screen::ReadingHub ? UiText::Reading : UiText::Display);
        if (auto action = header(ui, title, screen, current == Screen::DictionaryHelp ? Screen::Dictionary : current == Screen::Help ? Screen::Device : Screen::Settings);
            action != Action::None) return action;
        if (current == Screen::ReadingHub) {
            if (button(ui, {16, 52, 298, 112}, ui.text(UiText::Reading), Style::Secondary, true,
                       tr(ui,"Speed, mode & reading controls"))) screen = Screen::ReadingSettings;
            if (button(ui, {326, 52, 298, 112}, ui.text(UiText::WordPacing), Style::Secondary, true,
                       tr(ui,"Punctuation, length & complexity"))) screen = Screen::PacingSettings;
        } else if (current == Screen::DisplayHub) {
            if (button(ui, {16, 52, 298, 112}, ui.text(UiText::ReaderLayout), Style::Secondary, true,
                       tr(ui,"Preview your font, guides & controls"))) screen = Screen::ReaderAppearance;
            if (button(ui, {326, 52, 298, 112}, ui.text(UiText::Display), Style::Secondary, true,
                       tr(ui,"Theme & sleep"))) screen = Screen::InterfaceSettings;
        } else if (current == Screen::Connections) {
            if (button(ui, {12, 51, 300, 52}, tr(ui,"Wi-Fi & identity"), Style::Secondary, true, tr(ui,"Networks & device name"),13)) screen = Screen::NetworkSettings;
            if (button(ui, {328, 51, 300, 52}, ui.text(UiText::CompanionSync), Style::Secondary, true, tr(ui,"Local companion"),13)) return Action::CompanionSync;
            if (button(ui, {12, 111, 300, 53}, ui.text(UiText::UsbTransfer), Style::Secondary, true, tr(ui,"Transfer with a cable"),13)) return Action::UsbTransfer;
            if (button(ui, {328, 111, 300, 53}, ui.text(UiText::RefreshRss), Style::Secondary, true, tr(ui,"Refresh saved feeds"),13)) return Action::RssRefresh;
        } else if (current == Screen::DictionaryHelp) {
            ui.label({16,50,298,27},tr(ui,"Offline English dictionary"),17);
            ui.label({16,80,298,40},tr(ui,"Install over Wi-Fi or copy to SD.\nMore languages are planned."),13,ui::themes::Muted,ui::TextAlign::Start,2);
            if(button(ui,{16,124,298,40},tr(ui,"Install / manage dictionary"),Style::Primary,true,{},13))return Action::OpenDictionarySetup;
            ui.label({334,50,290,27},tr(ui,"Look online when needed"),17);
            ui.label({334,82,290,43},tr(ui,"Wiktionary receives the word when you tap Look online. Requires Wi-Fi."),13,ui::themes::Muted,ui::TextAlign::Start,2);
            if(button(ui,{334,128,290,36},tr(ui,"Wi-Fi settings"),Style::Secondary,true,{},13))screen=Screen::NetworkSettings;
        } else if (current == Screen::Help) {
            ui.label({16, 52, 608, 112},
                     tr(ui,"BOOT starts or pauses reading.\nPWR switches between the menu and your book.\nIn lists, PWR moves up and BOOT moves down.\nHold PWR to turn off. Hold BOOT to sleep."),
                     13, ui::themes::Foreground, ui::TextAlign::Start, 4);
        }
        return Action::None;
    }
}

namespace screens::modern {
Action quickControls(ui::Context& ui,settings::ReadingSettings& reading,settings::InterfaceSettings& interface,Screen& screen){
    using ui::modern::button;using ui::modern::Style;
    if(button(ui,{16,8,104,72},copy(ui,Copy::Menu),Style::Secondary,true,{},13))screen=Screen::Read;
    if(button(ui,{128,8,104,72},copy(ui,Copy::Dictionary),Style::Secondary,true,{},13))return Action::OpenDictionary;
    if(button(ui,{16,90,216,74},ui.text(UiText::Chapters),Style::Secondary,true,{},13))screen=Screen::Chapters;
    ui::modern::rule(ui,{246,8,1,156});
    ui.label({260,8,308,20},copy(ui,Copy::TrayTitle),11,ui::themes::Muted);
    if(button(ui,{260,42,58,58},"−",Style::Secondary,true,{},26))reading.wpm=std::max<int>(reading.wpm.min(),int(reading.wpm)-10);
    ui.label({326,36,176,56},std::to_string(reading.wpm),39,ui::themes::Foreground,ui::TextAlign::Center);
    ui.label({326,94,176,18},"WPM",11,ui::themes::Muted,ui::TextAlign::Center);
    if(button(ui,{510,42,58,58},"+",Style::Secondary,true,{},26))reading.wpm=std::min<int>(reading.wpm.max(),int(reading.wpm)+10);
    if(button(ui,{260,120,308,44},copy(ui,Copy::BackBook),Style::Primary))return Action::Resume;
    int brightness=interface.brightnessPercent;
    if(ui.verticalSlider({580,8,52,156},brightness,5,100))interface.brightnessPercent=brightness;
    return Action::None;
}
}
