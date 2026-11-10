#pragma once

#include <array>
#include <string_view>

#include "text/AsciiText.h"

namespace RsvpText {

    // Whether the text after "Chapter", "Part" or "Book" on a plain-text line numbers a heading:
    // nothing, digits, a Roman numeral or a number word, as in "Part II" or "Book the First".
    // Prose that merely begins with the word ("Part of the crew went ashore") does not.
    inline bool numbersHeading(std::string_view rest) {
        constexpr std::array<std::string_view, 34> kNumberWords{
            "one",     "two",      "three",    "four",    "five",    "six",       "seven",    "eight",  "nine",
            "ten",     "eleven",   "twelve",   "thirteen", "fourteen", "fifteen", "sixteen",  "seventeen",
            "eighteen", "nineteen", "twenty",  "thirty",  "forty",   "fifty",     "first",    "second", "third",
            "fourth",  "fifth",    "sixth",    "seventh", "eighth",  "ninth",     "tenth",    "last",
        };
        const auto skip = [&](std::string_view characters) {
            while (!rest.empty() && characters.find(rest.front()) != std::string_view::npos)
                rest.remove_prefix(1);
        };
        skip(" \t:.-");
        if (rest.empty())
            return true;
        size_t length = 0;
        while (length < rest.size() && AsciiText::isAlphaNumeric(rest[length]))
            ++length;
        std::string_view token = rest.substr(0, length);
        if (token.size() == 3 && AsciiText::toLower(token[0]) == 't' && AsciiText::toLower(token[1]) == 'h'
            && AsciiText::toLower(token[2]) == 'e') {
            // "Book the First"
            rest.remove_prefix(length);
            skip(" \t");
            length = 0;
            while (length < rest.size() && AsciiText::isAlphaNumeric(rest[length]))
                ++length;
            token = rest.substr(0, length);
        }
        if (token.empty())
            return false;
        const auto all = [&](auto&& predicate) {
            for (const char c: token)
                if (!predicate(c))
                    return false;
            return true;
        };
        if (all([](char c) { return c >= '0' && c <= '9'; }))
            return true;
        // Upper-case only, so "Part I" counts and "Part mild, part civil" does not.
        if (all([](char c) { return std::string_view{"IVXLCDM"}.find(c) != std::string_view::npos; }))
            return true;
        for (const std::string_view word: kNumberWords) {
            if (token.size() != word.size())
                continue;
            bool same = true;
            for (size_t i = 0; i < word.size() && same; ++i)
                same = AsciiText::toLower(token[i]) == word[i];
            if (same)
                return true;
        }
        return false;
    }

} // namespace RsvpText
