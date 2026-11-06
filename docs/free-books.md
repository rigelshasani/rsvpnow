# Free books

**Library → + Add → Free books** downloads public-domain classics straight to the SD card over the
saved Wi-Fi. There are 70 titles on six shelves (Classics, Adventure, Mystery & gothic, Philosophy,
Life & ideas, Poetry & drama), defined in `src/catalog/FreeCatalog.cpp`.

## Where the files come from

Each title is Project Gutenberg's image-free EPUB,
`https://www.gutenberg.org/cache/epub/<id>/pg<id>.epub` (0.1–1.8 MB). The reader fetches it the way
a browser would, one file when the reader asks, over verified HTTPS with the firmware's certificate
bundle. The download goes to a hidden `.<name>.part` file in `/library/books` and is renamed to
`<Title> - <Author>.epub` only once it is complete and starts like an EPUB (a ZIP), so an
interrupted download never shows up in the library. The middle button cancels.

The library converts the EPUB on first open, as with any other book. The converter drops Project
Gutenberg's header and licence (`class="pg-boilerplate"` blocks and their `pg-header`/`pg-footer`
table-of-contents entries), so a downloaded book starts at the work itself. Books already on the
card show **On your reader** and **Read now**; a title matches by file name or by EPUB title, so a
copy from elsewhere (for example "The Count of Monte Cristo") is recognised too.

## Choosing titles

Only works that are public domain in the US **and** whose authors and translators died before 1956,
so the life + 70 years term has also expired in the EU and UK. Copyright still depends on the
reader's country, which is why the screen says "public domain (US)". Gutenberg entries it marks as
copyrighted, and editions with a recent translator (for example Lionel Giles' *Art of War*, d. 1958)
are left out.

To change the shelves, edit `FreeCatalog.cpp`, check each ID and size with
`curl -sI https://www.gutenberg.org/cache/epub/<id>/pg<id>.epub`, and run
`pio test -e native_test -f test_free_catalog`. Sizes only label the list; downloads are capped at
16 MB.

## Other ways to add books

**+ Add** also offers USB transfer (the card appears as a drive), the companion, and **Where to buy**,
a short list of shops and publishers that sell EPUBs without DRM. Kindle and Apple Books purchases
are locked to their own apps and can't be read on the device. The Rigels Studio desktop app
(`desktop/`) adds a drop zone that converts EPUB and TXT files on the computer and installs them over
USB.
