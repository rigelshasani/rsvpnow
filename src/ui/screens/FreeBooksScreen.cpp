#include "ui/screens/FreeBooksScreen.h"

#include <algorithm>

#include "ui/Modern.h"
#include "ui/screens/ModernScreens.h"

namespace screens {
    namespace {
        using ui::modern::button;
        using ui::modern::Direction;
        using ui::modern::Style;

        constexpr std::string_view kNeedsWifi = "Set up Wi-Fi first";

        std::string_view libraryPathFor(const freebooks::Title& title, std::span<const BookLibrary::Entry> library) {
            for (const BookLibrary::Entry& book: library)
                if (freebooks::sameBook(title, book.path, book.title))
                    return book.path;
            return {};
        }
    } // namespace

    const freebooks::Title& FreeBooksScreen::selected() const {
        const auto shelves = freebooks::shelves();
        const auto& titles = shelves[std::min(shelf_, shelves.size() - 1)].titles;
        return titles[std::min(index_, titles.size() - 1)];
    }

    void FreeBooksScreen::select(size_t index) {
        if (index != index_)
            message_.clear();
        index_ = index;
    }

    Action FreeBooksScreen::draw(ui::Context& ui, std::span<const BookLibrary::Entry> library, Screen& screen) {
        return screen == Screen::FreeShelf ? drawShelf(ui, library, screen) : drawShelves(ui, screen);
    }

    Action FreeBooksScreen::drawShelves(ui::Context& ui, Screen& screen) {
        if (const auto action = modern::header(ui, "Free books", screen, Screen::GetBooks, "Public domain");
            action != Action::None)
            return action;
        const auto shelves = freebooks::shelves();
        // Two rows of four.
        for (size_t i = 0; i < shelves.size() && i < 8; ++i) {
            const ui::Rect rect{static_cast<int16_t>(12 + (i % 4) * 155), static_cast<int16_t>(i < 4 ? 51 : 111), 151,
                                static_cast<int16_t>(i < 4 ? 52 : 53)};
            const std::string count = std::to_string(shelves[i].titles.size()) + " books";
            if (button(ui, rect, shelves[i].name, Style::Secondary, true, count, 13)) {
                if (shelf_ != i) {
                    index_ = 0;
                    message_.clear();
                }
                shelf_ = i;
                screen = Screen::FreeShelf;
            }
        }
        return Action::None;
    }

    Action FreeBooksScreen::drawShelf(ui::Context& ui, std::span<const BookLibrary::Entry> library, Screen& screen) {
        const auto& shelf = freebooks::shelves()[std::min(shelf_, freebooks::shelves().size() - 1)];
        const auto& titles = shelf.titles;
        index_ = std::min(index_, titles.size() - 1);
        const std::string position = std::to_string(index_ + 1) + " / " + std::to_string(titles.size());
        if (const auto action = modern::header(ui, shelf.name, screen, Screen::FreeBooks, position);
            action != Action::None)
            return action;

        // Swipe up for later titles, like the chapter list.
        if (const int swipe = swipe_.update(ui, {16, 52, 358, 112}) + modern::takeListStep(); swipe != 0)
            select(std::clamp<int>(static_cast<int>(index_) + swipe, 0, static_cast<int>(titles.size()) - 1));
        if (ui::modern::arrow(ui, {386, 52, 56, 52}, Direction::Up, index_ > 0))
            select(index_ - 1);
        if (ui::modern::arrow(ui, {386, 112, 56, 52}, Direction::Down, index_ + 1 < titles.size()))
            select(index_ + 1);

        const size_t first = index_ == 0 ? 0 : std::min(index_ - 1, titles.size() >= 3 ? titles.size() - 3 : 0);
        for (size_t row = 0; row < 3; ++row) {
            const size_t index = first + row;
            const ui::Rect rect{16, static_cast<int16_t>(52 + row * 38), 358, 36};
            if (index >= titles.size()) {
                button(ui, rect, {}, Style::Plain, false);
                continue;
            }
            const bool owned = !libraryPathFor(titles[index], library).empty();
            const std::string detail = std::string{titles[index].author} + (owned ? "  ·  on your reader" : "");
            if (button(ui, rect, titles[index].title, index == index_ ? Style::Secondary : Style::Plain, true, detail,
                       13, ui::TextAlign::Start))
                select(index);
        }

        const freebooks::Title& title = titles[index_];
        selectedPath_ = std::string{libraryPathFor(title, library)};
        const bool owned = !selectedPath_.empty();
        ui::modern::rule(ui, {448, 52, 1, 112});
        ui.label({460, 50, 164, 38}, title.title, 15, ui::themes::Foreground, ui::TextAlign::Start, 2);
        if (!message_.empty()) {
            ui.label({460, 90, 164, 30}, message_, 11, ui::themes::Accent, ui::TextAlign::Start, 2);
        } else {
            ui.label({460, 90, 164, 15}, title.author, 11, ui::themes::Muted);
            ui.label({460, 105, 164, 15},
                     owned ? std::string{"On your reader"}
                           : freebooks::sizeLabel(title.bytes) + " · " + std::string{freebooks::sourceName(title)},
                     11, ui::themes::Muted);
        }
        if (message_ == kNeedsWifi) {
            if (button(ui, {460, 124, 164, 40}, "Wi-Fi settings", Style::Primary, true, {}, 13)) {
                message_.clear();
                screen = Screen::NetworkSettings;
            }
            return Action::None;
        }
        if (button(ui, {460, 124, 164, 40}, owned ? "Read now ↗" : "Download ↓", Style::Primary, true, {}, 13))
            return owned ? Action::OpenFreeBook : Action::DownloadFreeBook;
        return Action::None;
    }

    namespace modern {

        Action addBooks(ui::Context& ui, Screen& screen) {
            if (const auto action = header(ui, "Add books", screen, Screen::Library); action != Action::None)
                return action;
            const std::string free = std::to_string(freebooks::titleCount()) + " classics over Wi-Fi";
            if (button(ui, {12, 51, 300, 52}, "Free books", Style::Secondary, true, free, 13))
                screen = Screen::FreeBooks;
            if (button(ui, {328, 51, 300, 52}, "From a computer", Style::Secondary, true, "Drop EPUBs in over USB", 13))
                return Action::UsbTransfer;
            if (button(ui, {12, 111, 300, 53}, "Companion", Style::Secondary, true, "Upload in the browser", 13))
                return Action::CompanionSync;
            if (button(ui, {328, 111, 300, 53}, "Where to buy", Style::Secondary, true, "Shops without DRM", 13))
                screen = Screen::BookShops;
            return Action::None;
        }

        Action bookShops(ui::Context& ui, Screen& screen) {
            if (const auto action = header(ui, "Where to buy", screen, Screen::GetBooks); action != Action::None)
                return action;
            ui.label({16, 50, 296, 20}, "Reads EPUB, TXT and PDF without DRM", 13);
            ui.label({16, 74, 296, 48},
                     "Free: Standard Ebooks, Project Gutenberg\n"
                     "Paid: Kobo and eBooks.com (titles marked DRM-free)\n"
                     "Direct: Baen, Verso, Leanpub, Smashwords, Humble Bundle",
                     11, ui::themes::Muted, ui::TextAlign::Start, 6);
            ui::modern::rule(ui, {328, 52, 1, 112});
            ui.label({340, 50, 284, 20}, "Then add them", 13);
            ui.label({340, 74, 284, 64},
                     "Copy the files over USB or with the companion.\n"
                     "Kindle and Apple Books purchases are locked to their own apps and can't be read here.",
                     11, ui::themes::Muted, ui::TextAlign::Start, 6);
            return Action::None;
        }

    } // namespace modern

} // namespace screens
