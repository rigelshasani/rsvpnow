# Rigels Studio

A desktop companion for analyzing and tuning how the reader paces words. It times every word of a
book exactly like the firmware does, shows where reading slows down and why, lets you feel a
setting in a live preview, and sends the settings you choose to the reader over USB.

## Run it

```bash
cd desktop
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
.venv/bin/python -m rigels_studio          # opens http://127.0.0.1:8765/
```

Books are found in `~/Downloads`, `~/Documents`, `~/Desktop`, and on a reader SD card mounted on
this computer (`/Volumes/*/library/books`). EPUB, `.rsvp`, and plain-text/Markdown books are
supported; PDFs are not yet.

**Adding books:** drop EPUB, TXT, Markdown or `.rsvp` files on **Add books** in the sidebar (or click
it to choose files). Each book is converted on this computer, trimmed with the suggested
**Start and end** sections (Project Gutenberg licences, contents pages), and installed on the reader
over USB, one at a time. PDFs and DRM-protected books can't be sent this way: copy PDFs over the
reader's USB transfer mode, where the reader converts them itself.

**Reader connection:** plug the reader in over USB and press **Load from reader**. The reader shows
its companion screen while a request is in flight and returns to normal afterwards. If it says
the reader is busy, turn off USB transfer mode on the reader.

## What it shows

- **Pace through the book:** effective words per minute across the whole book (your WPM setting
  is the dashed line; everything below it is time added by pacing). Click to jump there.
- **Where the time goes:** total time split into base pace, long-word delay, complex-word delay
  and punctuation pauses.
- **Slowest words** and a per-chapter table, both clickable.
- **Try a sentence:** paste any text to see exactly how long each word would be shown.
- **Preview:** plays the book with the reader's exact timing and focus letter
  (space = play/pause, ←/→ = word, `[`/`]` = sentence).

## How the timing works

Each word is shown for

```
60000 / wpm
+ long-word delay     × length score      (letters past 6/10/14, hyphenated compounds)
+ complex-word delay  × complexity score  (syllables, ALL CAPS, digits, mixed tokens)
+ punctuation delay   × pause score       (comma 45%, dash 60%, ;: 80%, … 110%, . 135%, !? 150%)
```

The scores are fixed in the firmware (`src/reader/ReadingLoop.cpp`); only WPM and the three
delays are settings.

## Accuracy

- `rigels_studio/web/timing.js` ports the firmware's timing and focus letter;
  `rigels_studio/text.py` ports its text normalizer and tokenizer.
- Both are checked against the firmware itself: `tests/golden/build_harness.sh` compiles the real
  firmware sources for this computer and records tokens and durations for `tests/golden/corpus.txt`
  (edge cases plus public-domain Monte Cristo text) at seven setting combinations.
- `.rsvp` files, including the reader's own converted cache on the SD card, give the exact word
  stream. EPUBs are converted here following `docs/conversion/spec.md`; on The 48 Laws of Power
  this lands within 0.05% of the reader's word count (229,597 vs 229,484), and all 55 chapters line up,
  drifting by at most 113 words by the end of the book.

## Tests

```bash
cd desktop
python3 -m unittest discover -s tests        # tokenizer vs firmware, EPUB loading, sending books
node --test 'tests/*.test.mjs'                # timing vs firmware
tests/golden/build_harness.sh                 # regenerate golden.json after firmware timing changes
```
