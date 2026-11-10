#pragma once

#include <cstddef>
#include <functional>
#include <span>
#include <string_view>

#include "library/BookMetadata.h"

namespace ReadingStart {

    // Where a book should begin when there is no saved place: the first chapter that isn't
    // front matter (cover, copyright, dedication, epigraph, contents, lists of illustrations,
    // or a short title/author block). Commentary such as a preface or introduction is kept.
    // Only looks within the first fifth of the book and falls back to 0 when unsure.
    // With `wordAt`, a section that just lists the chapter titles (a contents page filed under
    // some other heading) is skipped too.
    using WordAt = std::function<std::string_view(size_t)>;
    size_t firstWord(std::span<const ChapterMarker> chapters, size_t wordCount, const WordAt& wordAt = {});

} // namespace ReadingStart
