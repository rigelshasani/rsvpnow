#include "catalog/FreeCatalog.h"

#include <array>
#include <cstdio>

namespace freebooks {
    namespace {

        // Checked against gutenberg.org in November 2026. Only works that are public
        // domain in the US and whose authors and translators died before 1956 (so
        // the life + 70 years rule has also expired). Sizes are the image-free EPUBs.
        constexpr Title kClassics[]{
            {1342, "Pride and Prejudice", "Jane Austen", 609138},
            {1260, "Jane Eyre", "Charlotte Brontë", 607998},
            {768, "Wuthering Heights", "Emily Brontë", 435353},
            {98, "A Tale of Two Cities", "Charles Dickens", 487766},
            {1400, "Great Expectations", "Charles Dickens", 554794},
            {2600, "War and Peace", "Leo Tolstoy", 1802784},
            {1399, "Anna Karenina", "Leo Tolstoy", 874055},
            {2554, "Crime and Punishment", "Fyodor Dostoyevsky", 627089},
            {28054, "The Brothers Karamazov", "Fyodor Dostoyevsky", 1006641},
            {135, "Les Misérables", "Victor Hugo", 1828389},
            {64317, "The Great Gatsby", "F. Scott Fitzgerald", 181457},
            {2701, "Moby-Dick", "Herman Melville", 728739},
            {996, "Don Quixote", "Miguel de Cervantes", 1195700},
            {174, "The Picture of Dorian Gray", "Oscar Wilde", 460326},
            {25344, "The Scarlet Letter", "Nathaniel Hawthorne", 474408},
            {4300, "Ulysses", "James Joyce", 768096},
        };
        constexpr Title kAdventure[]{
            {1184, "The Count of Monte Cristo", "Alexandre Dumas", 1295079},
            {1259, "Twenty Years After", "Alexandre Dumas", 811340},
            {120, "Treasure Island", "Robert Louis Stevenson", 274139},
            {103, "Around the World in Eighty Days", "Jules Verne", 269267},
            {164, "Twenty Thousand Leagues Under the Sea", "Jules Verne", 370585},
            {215, "The Call of the Wild", "Jack London", 156181},
            {76, "Adventures of Huckleberry Finn", "Mark Twain", 347233},
            {74, "The Adventures of Tom Sawyer", "Mark Twain", 288680},
            {55, "The Wonderful Wizard of Oz", "L. Frank Baum", 181802},
            {11, "Alice's Adventures in Wonderland", "Lewis Carroll", 136756},
            {35, "The Time Machine", "H. G. Wells", 162730},
            {36, "The War of the Worlds", "H. G. Wells", 242183},
        };
        constexpr Title kMystery[]{
            {1661, "The Adventures of Sherlock Holmes", "Arthur Conan Doyle", 311635},
            {2852, "The Hound of the Baskervilles", "Arthur Conan Doyle", 228273},
            {244, "A Study in Scarlet", "Arthur Conan Doyle", 192281},
            {2097, "The Sign of the Four", "Arthur Conan Doyle", 187643},
            {345, "Dracula", "Bram Stoker", 433654},
            {84, "Frankenstein", "Mary Shelley", 294175},
            {43, "Dr Jekyll and Mr Hyde", "Robert Louis Stevenson", 142426},
            {10007, "Carmilla", "J. Sheridan Le Fanu", 150215},
            {1952, "The Yellow Wallpaper", "Charlotte Perkins Gilman", 87914},
            {41, "The Legend of Sleepy Hollow", "Washington Irving", 88385},
            {219, "Heart of Darkness", "Joseph Conrad", 187216},
        };
        constexpr Title kPhilosophy[]{
            {2680, "Meditations", "Marcus Aurelius", 306337},
            {1232, "The Prince", "Niccolò Machiavelli", 217516},
            {1497, "The Republic", "Plato", 532537},
            {4363, "Beyond Good and Evil", "Friedrich Nietzsche", 241611},
            {1998, "Thus Spake Zarathustra", "Friedrich Nietzsche", 502712},
            {3296, "Confessions", "Saint Augustine", 383257},
            {34901, "On Liberty", "John Stuart Mill", 212053},
            {7370, "Second Treatise of Government", "John Locke", 204024},
            {3600, "Essays", "Michel de Montaigne", 1373457},
            {2944, "Essays: First Series", "Ralph Waldo Emerson", 268522},
            {205, "Walden", "Henry David Thoreau", 390266},
            {2130, "Utopia", "Thomas More", 170076},
        };
        constexpr Title kIdeas[]{
            {4507, "As a Man Thinketh", "James Allen", 91302},
            {2274, "How to Live on 24 Hours a Day", "Arnold Bennett", 103665},
            {20203, "Autobiography of Benjamin Franklin", "Benjamin Franklin", 217524},
            {23, "Narrative of the Life of Frederick Douglass", "Frederick Douglass", 175859},
            {58585, "The Prophet", "Kahlil Gibran", 116878},
            {147, "Common Sense", "Thomas Paine", 176329},
            {1404, "The Federalist Papers", "Hamilton, Madison and Jay", 521100},
            {3300, "The Wealth of Nations", "Adam Smith", 971954},
            {1228, "On the Origin of Species", "Charles Darwin", 466441},
            {1080, "A Modest Proposal", "Jonathan Swift", 107350},
        };
        constexpr Title kPoetry[]{
            {1727, "The Odyssey", "Homer", 380333},
            {6130, "The Iliad", "Homer", 784047},
            {8800, "The Divine Comedy", "Dante Alighieri", 492526},
            {16328, "Beowulf", "Anonymous", 291980},
            {1524, "Hamlet", "William Shakespeare", 214077},
            {1513, "Romeo and Juliet", "William Shakespeare", 199257},
            {1322, "Leaves of Grass", "Walt Whitman", 692147},
            {2542, "A Doll's House", "Henrik Ibsen", 163930},
            {3825, "Pygmalion", "Bernard Shaw", 156345},
        };

        constexpr Shelf kShelves[]{
            {"Classics", kClassics},          {"Adventure", kAdventure},   {"Mystery & gothic", kMystery},
            {"Philosophy", kPhilosophy},      {"Life & ideas", kIdeas},    {"Poetry & drama", kPoetry},
        };

        // Latin-1 letters (U+00C0-U+00FF, UTF-8 lead byte 0xC3) folded to ASCII; ' ' drops × and ÷.
        constexpr std::string_view kFolded = "AAAAAAACEEEEIIIIDNOOOOO OUUUUYTsaaaaaaaceeeeiiiidnooooo ouuuuyty";
        static_assert(kFolded.size() == 64);

        // Appends each character as ASCII: accents folded, other non-ASCII dropped.
        template <typename Sink>
        void forEachAscii(std::string_view text, Sink sink) {
            for (size_t i = 0; i < text.size(); ++i) {
                const auto byte = static_cast<unsigned char>(text[i]);
                if (byte < 0x80) {
                    sink(static_cast<char>(byte));
                } else if (byte == 0xC3 && i + 1 < text.size()) {
                    const auto next = static_cast<unsigned char>(text[i + 1]);
                    if (next >= 0x80 && next <= 0xBF && kFolded[next - 0x80] != ' ')
                        sink(kFolded[next - 0x80]);
                    ++i;
                }
            }
        }

        constexpr bool alphaNumeric(char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        }

        std::string_view baseName(std::string_view path) {
            const size_t slash = path.find_last_of('/');
            return slash == std::string_view::npos ? path : path.substr(slash + 1);
        }

        void appendFilePart(std::string& out, std::string_view text) {
            forEachAscii(text, [&](char c) {
                if (alphaNumeric(c)) {
                    out += c;
                } else if ((c == ' ' || c == '-' || c == ',' || c == ':' || c == '&') && !out.empty()
                           && out.back() != ' ') {
                    out += ' ';  // FAT32-safe separators only; apostrophes and dots vanish
                }
            });
            while (!out.empty() && out.back() == ' ')
                out.pop_back();
        }

    } // namespace

    std::span<const Shelf> shelves() {
        return kShelves;
    }

    size_t titleCount() {
        size_t count = 0;
        for (const Shelf& shelf: kShelves)
            count += shelf.titles.size();
        return count;
    }

    std::string downloadUrl(const Title& title) {
        const std::string id = std::to_string(title.gutenbergId);
        return "https://www.gutenberg.org/cache/epub/" + id + "/pg" + id + ".epub";
    }

    std::string fileName(const Title& title) {
        std::string name;
        appendFilePart(name, title.title);
        name += " - ";
        appendFilePart(name, title.author);
        return name + ".epub";
    }

    std::string sizeLabel(uint32_t bytes) {
        char label[16];
        if (bytes < 1000UL * 1000UL)
            std::snprintf(label, sizeof(label), "%u KB", static_cast<unsigned>((bytes + 999) / 1000));
        else
            std::snprintf(label, sizeof(label), "%u.%u MB", static_cast<unsigned>(bytes / 1000000),
                          static_cast<unsigned>(bytes % 1000000 / 100000));
        return label;
    }

    std::string matchKey(std::string_view text) {
        std::string key;
        key.reserve(text.size());
        forEachAscii(text, [&](char c) {
            if (c >= 'A' && c <= 'Z')
                key += static_cast<char>(c - 'A' + 'a');
            else if (alphaNumeric(c))
                key += c;
        });
        return key;
    }

    bool sameBook(const Title& title, std::string_view libraryPath, std::string_view libraryTitle) {
        if (baseName(libraryPath) == fileName(title))
            return true;
        if (libraryTitle.empty())
            return false;
        if (matchKey(libraryTitle) == matchKey(title.title))
            return true;
        // Gutenberg titles often carry a subtitle: "Frankenstein; or, The Modern Prometheus".
        const auto lower = [](std::string_view text) {
            std::string out;
            forEachAscii(text, [&](char c) { out += c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; });
            return out;
        };
        const std::string actual = lower(libraryTitle);
        const std::string wanted = lower(title.title);
        return actual.size() > wanted.size() && actual.starts_with(wanted)
            && std::string_view{";:,(["}.contains(actual[wanted.size()]);
    }

} // namespace freebooks
