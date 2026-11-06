#pragma once

#include <cstddef>
#include <span>

#include "library/BookMetadata.h"

namespace ReadingStart {

    // Where a book should begin when there is no saved place: the first chapter that isn't
    // front matter (cover, copyright, dedication, epigraph, contents, lists of illustrations,
    // or a short title/author block). Commentary such as a preface or introduction is kept.
    // Only looks within the first fifth of the book and falls back to 0 when unsure.
    size_t firstWord(std::span<const ChapterMarker> chapters, size_t wordCount);

} // namespace ReadingStart
