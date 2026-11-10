# Free books

**Library → + Add → Free books** downloads public-domain books straight to the SD card over the
saved Wi-Fi. There are 201 titles on eight shelves (Classics, Modern classics, Adventure, Mystery &
gothic, Short stories, Philosophy, Life & ideas, Poetry & drama), defined in
`src/catalog/FreeCatalog.cpp`.

## Where the files come from

Most titles are Project Gutenberg's image-free EPUB,
`https://www.gutenberg.org/cache/epub/<id>/pg<id>.epub` (0.1–1.8 MB). The rest are Standard Ebooks'
compatible EPUB (0.5–3.1 MB), which is carefully proofread and has clean chapter structure:
`https://standardebooks.org/ebooks/<path>/downloads/<path with _ for />.epub?source=download`, where
the path is `author/title` or `author/title/translator`. Without `?source=download` that address
returns a web page instead of the file. The shelf shows which source each title comes from.

The reader fetches a book the way a browser would, one file when the reader asks, over verified
HTTPS with the firmware's certificate bundle. The download goes to a hidden `.<name>.part` file in
`/library/books` and is renamed to `<Title> - <Author>.epub` only once it is complete and starts
like an EPUB (a ZIP), so an interrupted download never shows up in the library. The middle button
cancels.

The library converts the EPUB on first open, as with any other book. The converter drops the pages
that are about the edition rather than the work: Project Gutenberg's header and licence
(`class="pg-boilerplate"` blocks and their `pg-header`/`pg-footer` table-of-contents entries), and
sections marked `epub:type` `titlepage`, `halftitlepage`, `imprint`, `colophon` or `copyright-page`,
as Standard Ebooks marks its title page, imprint, colophon and Uncopyright. A downloaded book starts
and ends at the work itself. Books already on the card show **On your reader** and **Read now**; a
title matches by file name or by EPUB title, so a copy from elsewhere (for example "The Count of
Monte Cristo") is recognised too.

## Choosing titles

Only works that are public domain in the US **and** whose authors and translators died before 1956,
so the life + 70 years term has also expired in the EU and UK. Copyright still depends on the
reader's country, which is why the screen says "public domain (US)". Gutenberg entries it marks as
copyrighted, and editions with a recent translator (for example Lionel Giles' *Art of War*, d. 1958)
are left out.

Standard Ebooks only requires US public domain, so its titles need the same check: Maugham
(d. 1965) and the Muirs' Kafka translations are left out, for example. Gutenberg lists authors' and
translators' dates in `https://www.gutenberg.org/cache/epub/<id>/pg<id>.rdf`; Standard Ebooks does
not, so those dates were checked by hand.

To change the shelves, edit `FreeCatalog.cpp`, check each address and size with `curl -sIL` (for
Standard Ebooks, with `?source=download`), and run `pio test -e native_test -f test_free_catalog`.
Titles must be unique across shelves, which is why Standard Ebooks' several "Short Fiction"
collections carry the author's name. Sizes only label the list; downloads are capped at 16 MB. The
shelf screen has room for eight shelves.

## Other ways to add books

**+ Add** also offers USB transfer (the card appears as a drive), the companion, and **Where to buy**,
a short list of shops and publishers that sell EPUBs without DRM. Kindle and Apple Books purchases
are locked to their own apps and can't be read on the device. The Rigels Studio desktop app
(`desktop/`) adds a drop zone that converts EPUB and TXT files on the computer and installs them over
USB.
