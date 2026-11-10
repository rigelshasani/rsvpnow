#include "catalog/FreeCatalog.h"

#include <array>
#include <cstdio>

namespace freebooks {
    namespace {

        // Checked against gutenberg.org and standardebooks.org in November 2026. Only works that
        // are public domain in the US and whose authors and translators died before 1956 (so the
        // life + 70 years rule has also expired). Sizes are Gutenberg's image-free EPUBs and
        // Standard Ebooks' compatible EPUBs.
        constexpr Title kClassics[]{
            {1342, "Pride and Prejudice", "Jane Austen", 609138},
            {158, "Emma", "Jane Austen", 476459},
            {161, "Sense and Sensibility", "Jane Austen", 409699},
            {105, "Persuasion", "Jane Austen", 330193},
            {1260, "Jane Eyre", "Charlotte Brontë", 607998},
            {768, "Wuthering Heights", "Emily Brontë", 435353},
            {969, "The Tenant of Wildfell Hall", "Anne Brontë", 529856},
            {98, "A Tale of Two Cities", "Charles Dickens", 487766},
            {1400, "Great Expectations", "Charles Dickens", 554794},
            {766, "David Copperfield", "Charles Dickens", 971995},
            {730, "Oliver Twist", "Charles Dickens", 506530},
            {1023, "Bleak House", "Charles Dickens", 1001978},
            {0, "Hard Times", "Charles Dickens", 984250, "charles-dickens/hard-times"},
            {46, "A Christmas Carol", "Charles Dickens", 147940},
            {145, "Middlemarch", "George Eliot", 904978},
            {0, "Silas Marner", "George Eliot", 721482, "george-eliot/silas-marner"},
            {599, "Vanity Fair", "William Makepeace Thackeray", 832392},
            {4276, "North and South", "Elizabeth Gaskell", 522485},
            {110, "Tess of the d'Urbervilles", "Thomas Hardy", 489555},
            {0, "Far from the Madding Crowd", "Thomas Hardy", 1025546, "thomas-hardy/far-from-the-madding-crowd"},
            {0, "Jude the Obscure", "Thomas Hardy", 763530, "thomas-hardy/jude-the-obscure"},
            {0, "New Grub Street", "George Gissing", 1237118, "george-gissing/new-grub-street"},
            {2600, "War and Peace", "Leo Tolstoy", 1802784},
            {1399, "Anna Karenina", "Leo Tolstoy", 874055},
            {0, "Resurrection", "Leo Tolstoy", 960011, "leo-tolstoy/resurrection/louise-maude"},
            {2554, "Crime and Punishment", "Fyodor Dostoyevsky", 627089},
            {28054, "The Brothers Karamazov", "Fyodor Dostoyevsky", 1006641},
            {0, "Demons", "Fyodor Dostoyevsky", 937635, "fyodor-dostoevsky/demons/constance-garnett"},
            {600, "Notes from the Underground", "Fyodor Dostoyevsky", 233211},
            {0, "Fathers and Children", "Ivan Turgenev", 630435, "ivan-turgenev/fathers-and-children/constance-garnett"},
            {135, "Les Misérables", "Victor Hugo", 1828389},
            {2610, "Notre-Dame de Paris", "Victor Hugo", 581983},
            {2413, "Madame Bovary", "Gustave Flaubert", 420471},
            {0, "Father Goriot", "Honoré de Balzac", 672822, "honore-de-balzac/father-goriot/ellen-marriage"},
            {0, "Germinal", "Émile Zola", 888398, "emile-zola/germinal/havelock-ellis"},
            {2701, "Moby-Dick", "Herman Melville", 728739},
            {996, "Don Quixote", "Miguel de Cervantes", 1195700},
            {174, "The Picture of Dorian Gray", "Oscar Wilde", 460326},
            {25344, "The Scarlet Letter", "Nathaniel Hawthorne", 474408},
            {0, "The House of the Seven Gables", "Nathaniel Hawthorne", 810351, "nathaniel-hawthorne/the-house-of-the-seven-gables"},
            {514, "Little Women", "Louisa May Alcott", 550144},
            {0, "The Portrait of a Lady", "Henry James", 1041686, "henry-james/the-portrait-of-a-lady"},
        };
        constexpr Title kModern[]{
            {64317, "The Great Gatsby", "F. Scott Fitzgerald", 181457},
            {0, "This Side of Paradise", "F. Scott Fitzgerald", 494692, "f-scott-fitzgerald/this-side-of-paradise"},
            {0, "The Beautiful and Damned", "F. Scott Fitzgerald", 634596, "f-scott-fitzgerald/the-beautiful-and-damned"},
            {4300, "Ulysses", "James Joyce", 768096},
            {4217, "A Portrait of the Artist as a Young Man", "James Joyce", 313527},
            {0, "Mrs Dalloway", "Virginia Woolf", 756214, "virginia-woolf/mrs-dalloway"},
            {0, "To the Lighthouse", "Virginia Woolf", 1116110, "virginia-woolf/to-the-lighthouse"},
            {0, "Sons and Lovers", "D. H. Lawrence", 1030881, "d-h-lawrence/sons-and-lovers"},
            {541, "The Age of Innocence", "Edith Wharton", 319904},
            {0, "The House of Mirth", "Edith Wharton", 744471, "edith-wharton/the-house-of-mirth"},
            {4517, "Ethan Frome", "Edith Wharton", 260599},
            {160, "The Awakening", "Kate Chopin", 234120},
            {0, "My Ántonia", "Willa Cather", 1062035, "willa-cather/my-antonia"},
            {0, "O Pioneers!", "Willa Cather", 615189, "willa-cather/o-pioneers"},
            {0, "Death Comes for the Archbishop", "Willa Cather", 728503, "willa-cather/death-comes-for-the-archbishop"},
            {543, "Main Street", "Sinclair Lewis", 515021},
            {0, "Babbitt", "Sinclair Lewis", 861341, "sinclair-lewis/babbitt"},
            {0, "Lord Jim", "Joseph Conrad", 1196114, "joseph-conrad/lord-jim"},
            {0, "Nostromo", "Joseph Conrad", 1078516, "joseph-conrad/nostromo"},
            {0, "Martin Eden", "Jack London", 894549, "jack-london/martin-eden"},
            {0, "The Good Soldier", "Ford Madox Ford", 727681, "ford-madox-ford/the-good-soldier"},
            {0, "The Old Wives' Tale", "Arnold Bennett", 1041727, "arnold-bennett/the-old-wives-tale"},
            {0, "The Forsyte Saga", "John Galsworthy", 1324100, "john-galsworthy/the-forsyte-saga"},
            {0, "Hunger", "Knut Hamsun", 702716, "knut-hamsun/hunger/george-egerton"},
        };
        constexpr Title kAdventure[]{
            {1184, "The Count of Monte Cristo", "Alexandre Dumas", 1295079},
            {1257, "The Three Musketeers", "Alexandre Dumas", 678068},
            {1259, "Twenty Years After", "Alexandre Dumas", 811340},
            {120, "Treasure Island", "Robert Louis Stevenson", 274139},
            {521, "Robinson Crusoe", "Daniel Defoe", 326171},
            {829, "Gulliver's Travels", "Jonathan Swift", 357451},
            {103, "Around the World in Eighty Days", "Jules Verne", 269267},
            {164, "Twenty Thousand Leagues Under the Sea", "Jules Verne", 370585},
            {1268, "The Mysterious Island", "Jules Verne", 533118},
            {3748, "A Journey into the Interior of the Earth", "Jules Verne", 337807},
            {215, "The Call of the Wild", "Jack London", 156181},
            {910, "White Fang", "Jack London", 241356},
            {0, "The Sea-Wolf", "Jack London", 746333, "jack-london/the-sea-wolf"},
            {76, "Adventures of Huckleberry Finn", "Mark Twain", 347233},
            {74, "The Adventures of Tom Sawyer", "Mark Twain", 288680},
            {86, "A Connecticut Yankee in King Arthur's Court", "Mark Twain", 412032},
            {1837, "The Prince and the Pauper", "Mark Twain", 268409},
            {35, "The Time Machine", "H. G. Wells", 162730},
            {36, "The War of the Worlds", "H. G. Wells", 242183},
            {5230, "The Invisible Man", "H. G. Wells", 215109},
            {159, "The Island of Doctor Moreau", "H. G. Wells", 200188},
            {62, "A Princess of Mars", "Edgar Rice Burroughs", 249454},
            {78, "Tarzan of the Apes", "Edgar Rice Burroughs", 292977},
            {139, "The Lost World", "Arthur Conan Doyle", 249305},
            {236, "The Jungle Book", "Rudyard Kipling", 182017},
            {2226, "Kim", "Rudyard Kipling", 337992},
            {0, "King Solomon's Mines", "H. Rider Haggard", 852511, "h-rider-haggard/king-solomons-mines"},
            {0, "She", "H. Rider Haggard", 1192516, "h-rider-haggard/she"},
            {0, "Captain Blood", "Rafael Sabatini", 940771, "rafael-sabatini/captain-blood"},
            {0, "Scaramouche", "Rafael Sabatini", 778649, "rafael-sabatini/scaramouche"},
            {0, "The Prisoner of Zenda", "Anthony Hope", 606333, "anthony-hope/the-prisoner-of-zenda"},
            {0, "The Scarlet Pimpernel", "Baroness Orczy", 661044, "baroness-orczy/the-scarlet-pimpernel"},
            {558, "The Thirty-Nine Steps", "John Buchan", 170023},
            {0, "Three Men in a Boat", "Jerome K. Jerome", 685719, "jerome-k-jerome/three-men-in-a-boat"},
            {55, "The Wonderful Wizard of Oz", "L. Frank Baum", 181802},
            {11, "Alice's Adventures in Wonderland", "Lewis Carroll", 136756},
            {16, "Peter Pan", "J. M. Barrie", 390213},
            {289, "The Wind in the Willows", "Kenneth Grahame", 242674},
            {113, "The Secret Garden", "Frances Hodgson Burnett", 271039},
            {45, "Anne of Green Gables", "L. M. Montgomery", 330396},
        };
        constexpr Title kMystery[]{
            {1661, "The Adventures of Sherlock Holmes", "Arthur Conan Doyle", 311635},
            {834, "The Memoirs of Sherlock Holmes", "Arthur Conan Doyle", 290916},
            {108, "The Return of Sherlock Holmes", "Arthur Conan Doyle", 340009},
            {2350, "His Last Bow", "Arthur Conan Doyle", 205730},
            {2852, "The Hound of the Baskervilles", "Arthur Conan Doyle", 228273},
            {244, "A Study in Scarlet", "Arthur Conan Doyle", 192281},
            {2097, "The Sign of the Four", "Arthur Conan Doyle", 187643},
            {0, "The Valley of Fear", "Arthur Conan Doyle", 684335, "arthur-conan-doyle/the-valley-of-fear"},
            {155, "The Moonstone", "Wilkie Collins", 553361},
            {583, "The Woman in White", "Wilkie Collins", 715675},
            {204, "The Innocence of Father Brown", "G. K. Chesterton", 257182},
            {1695, "The Man Who Was Thursday", "G. K. Chesterton", 196030},
            {345, "Dracula", "Bram Stoker", 433654},
            {84, "Frankenstein", "Mary Shelley", 294175},
            {43, "Dr Jekyll and Mr Hyde", "Robert Louis Stevenson", 142426},
            {10007, "Carmilla", "J. Sheridan Le Fanu", 150215},
            {175, "The Phantom of the Opera", "Gaston Leroux", 276174},
            {209, "The Turn of the Screw", "Henry James", 183533},
            {696, "The Castle of Otranto", "Horace Walpole", 211774},
            {0, "The King in Yellow", "Robert W. Chambers", 740441, "robert-w-chambers/the-king-in-yellow"},
            {219, "Heart of Darkness", "Joseph Conrad", 187216},
        };
        constexpr Title kStories[]{
            {2814, "Dubliners", "James Joyce", 234652},
            {0, "Chekhov's Short Fiction", "Anton Chekhov", 2411005, "anton-chekhov/short-fiction/constance-garnett"},
            {0, "Poe's Short Fiction", "Edgar Allan Poe", 1279652, "edgar-allan-poe/short-fiction"},
            {0, "O. Henry's Short Fiction", "O. Henry", 3122876, "o-henry/short-fiction"},
            {0, "Saki's Short Fiction", "Saki", 1286948, "saki/short-fiction"},
            {0, "Lovecraft's Short Fiction", "H. P. Lovecraft", 947586, "h-p-lovecraft/short-fiction"},
            {1952, "The Yellow Wallpaper", "Charlotte Perkins Gilman", 87914},
            {41, "The Legend of Sleepy Hollow", "Washington Irving", 88385},
        };
        constexpr Title kPhilosophy[]{
            {2680, "Meditations", "Marcus Aurelius", 306337},
            {0, "Enchiridion and Short Works", "Epictetus", 502353, "epictetus/short-works/george-long"},
            {1497, "The Republic", "Plato", 532537},
            {1656, "Apology", "Plato", 91249},
            {1600, "Symposium", "Plato", 141369},
            {1974, "Poetics", "Aristotle", 103427},
            {216, "Tao Te Ching", "Laozi", 100816},
            {3296, "Confessions", "Saint Augustine", 383257},
            {1232, "The Prince", "Niccolò Machiavelli", 217516},
            {2130, "Utopia", "Thomas More", 170076},
            {3600, "Essays", "Michel de Montaigne", 1373457},
            {59, "Discourse on the Method", "René Descartes", 116826},
            {3207, "Leviathan", "Thomas Hobbes", 847487},
            {3800, "Ethics", "Benedict de Spinoza", 232996},
            {7370, "Second Treatise of Government", "John Locke", 204024},
            {4705, "A Treatise of Human Nature", "David Hume", 654259},
            {4280, "The Critique of Pure Reason", "Immanuel Kant", 509621},
            {34901, "On Liberty", "John Stuart Mill", 212053},
            {4363, "Beyond Good and Evil", "Friedrich Nietzsche", 241611},
            {1998, "Thus Spake Zarathustra", "Friedrich Nietzsche", 502712},
            {2944, "Essays: First Series", "Ralph Waldo Emerson", 268522},
            {205, "Walden", "Henry David Thoreau", 390266},
            {71, "Civil Disobedience", "Henry David Thoreau", 97932},
            {0, "Pragmatism", "William James", 732809, "william-james/pragmatism"},
            {0, "Orthodoxy", "G. K. Chesterton", 787634, "g-k-chesterton/orthodoxy"},
        };
        constexpr Title kIdeas[]{
            {4507, "As a Man Thinketh", "James Allen", 91302},
            {2274, "How to Live on 24 Hours a Day", "Arnold Bennett", 103665},
            {20203, "Autobiography of Benjamin Franklin", "Benjamin Franklin", 217524},
            {23, "Narrative of the Life of Frederick Douglass", "Frederick Douglass", 175859},
            {2376, "Up from Slavery", "Booker T. Washington", 257910},
            {58585, "The Prophet", "Kahlil Gibran", 116878},
            {147, "Common Sense", "Thomas Paine", 176329},
            {1404, "The Federalist Papers", "Hamilton, Madison and Jay", 521100},
            {815, "Democracy in America, Volume 1", "Alexis de Tocqueville", 541819},
            {61, "The Communist Manifesto", "Karl Marx and Friedrich Engels", 105969},
            {3300, "The Wealth of Nations", "Adam Smith", 971954},
            {3420, "A Vindication of the Rights of Woman", "Mary Wollstonecraft", 277064},
            {1228, "On the Origin of Species", "Charles Darwin", 466441},
            {944, "The Voyage of the Beagle", "Charles Darwin", 584847},
            {621, "The Varieties of Religious Experience", "William James", 638170},
            {852, "Democracy and Education", "John Dewey", 400569},
            {3176, "The Innocents Abroad", "Mark Twain", 570020},
            {972, "The Devil's Dictionary", "Ambrose Bierce", 339250},
            {1080, "A Modest Proposal", "Jonathan Swift", 107350},
        };
        constexpr Title kPoetry[]{
            {1727, "The Odyssey", "Homer", 380333},
            {6130, "The Iliad", "Homer", 784047},
            {228, "The Aeneid", "Virgil", 342734},
            {8800, "The Divine Comedy", "Dante Alighieri", 492526},
            {16328, "Beowulf", "Anonymous", 291980},
            {26, "Paradise Lost", "John Milton", 334706},
            {31, "Oedipus the King and Other Plays", "Sophocles", 170435},
            {1524, "Hamlet", "William Shakespeare", 214077},
            {1533, "Macbeth", "William Shakespeare", 183069},
            {1532, "King Lear", "William Shakespeare", 204925},
            {1531, "Othello", "William Shakespeare", 202242},
            {1513, "Romeo and Juliet", "William Shakespeare", 199257},
            {1514, "A Midsummer Night's Dream", "William Shakespeare", 187913},
            {1515, "The Merchant of Venice", "William Shakespeare", 193172},
            {1120, "Julius Caesar", "William Shakespeare", 177337},
            {1041, "Sonnets", "William Shakespeare", 223482},
            {779, "Doctor Faustus", "Christopher Marlowe", 130243},
            {1934, "Songs of Innocence and of Experience", "William Blake", 121670},
            {1322, "Leaves of Grass", "Walt Whitman", 692147},
            {2542, "A Doll's House", "Henrik Ibsen", 163930},
            {844, "The Importance of Being Earnest", "Oscar Wilde", 104216},
            {3825, "Pygmalion", "Bernard Shaw", 156345},
        };

        constexpr Shelf kShelves[]{
            {"Classics", kClassics},
            {"Modern classics", kModern},
            {"Adventure", kAdventure},
            {"Mystery & gothic", kMystery},
            {"Short stories", kStories},
            {"Philosophy", kPhilosophy},
            {"Life & ideas", kIdeas},
            {"Poetry & drama", kPoetry},
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
        if (!title.standardEbooks.empty()) {
            // The file is named after its path; "?source=download" asks for the file, not its page.
            std::string file{title.standardEbooks};
            for (char& c: file)
                if (c == '/')
                    c = '_';
            return "https://standardebooks.org/ebooks/" + std::string{title.standardEbooks} + "/downloads/" + file
                 + ".epub?source=download";
        }
        const std::string id = std::to_string(title.gutenbergId);
        return "https://www.gutenberg.org/cache/epub/" + id + "/pg" + id + ".epub";
    }

    std::string_view sourceName(const Title& title) {
        return title.standardEbooks.empty() ? "Project Gutenberg" : "Standard Ebooks";
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
