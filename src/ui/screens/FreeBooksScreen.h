#pragma once

#include <span>
#include <string>

#include "catalog/FreeCatalog.h"
#include "library/BookLibrary.h"
#include "ui/Modern.h"
#include "ui/Ui.h"
#include "ui/screens/Screens.h"

namespace screens {

    // Free public-domain books: a shelf picker (Screen::FreeBooks) and one shelf's
    // titles (Screen::FreeShelf). Downloads run as an App background job.
    class FreeBooksScreen {
    public:
        Action draw(ui::Context& ui, std::span<const BookLibrary::Entry> library, Screen& screen);

        // The title the last DownloadFreeBook / OpenFreeBook action refers to.
        const freebooks::Title& selected() const;
        // Shown in the detail panel until the selection changes.
        void setMessage(std::string message) { message_ = std::move(message); }
        // Library path of the selected title, when it is already on the card.
        std::string_view selectedPath() const { return selectedPath_; }

    private:
        Action drawShelves(ui::Context& ui, Screen& screen);
        Action drawShelf(ui::Context& ui, std::span<const BookLibrary::Entry> library, Screen& screen);
        void select(size_t index);

        size_t shelf_ = 0;
        size_t index_ = 0;
        std::string message_;
        std::string selectedPath_;
        ui::modern::ListSwipe swipe_;
    };

    namespace modern {
        // "Add books": free books, USB, companion and where to buy.
        Action addBooks(ui::Context& ui, Screen& screen);
        Action bookShops(ui::Context& ui, Screen& screen);
    } // namespace modern

} // namespace screens
