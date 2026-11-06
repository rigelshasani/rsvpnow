#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

// Built-in shelves of public-domain books that the reader can download over
// Wi-Fi from Project Gutenberg. Pure data and string helpers: no hardware.
namespace freebooks {

    struct Title {
        uint32_t gutenbergId = 0;
        std::string_view title;
        std::string_view author;
        uint32_t bytes = 0;  // Gutenberg's image-free EPUB when the catalog was checked
    };

    struct Shelf {
        std::string_view name;
        std::span<const Title> titles;
    };

    // Image-free EPUBs stay small (about 0.1-2 MB). Files can be regenerated,
    // so this is a safety cap rather than an exact size.
    constexpr size_t kMaxDownloadBytes = 16UL * 1024UL * 1024UL;

    std::span<const Shelf> shelves();
    size_t titleCount();

    std::string downloadUrl(const Title& title);
    // "Pride and Prejudice - Jane Austen.epub": ASCII, safe on FAT32.
    std::string fileName(const Title& title);
    // "0.6 MB" / "87 KB"
    std::string sizeLabel(uint32_t bytes);
    // Letters and digits only, lower case, accents folded: "Les Misérables" -> "lesmiserables".
    std::string matchKey(std::string_view text);
    // True when a library book (by its file name or EPUB title) is this catalog title.
    bool sameBook(const Title& title, std::string_view libraryPath, std::string_view libraryTitle);

} // namespace freebooks
