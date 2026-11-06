// Port of the reader's word timing (src/reader/ReadingLoop.cpp) and focus letter (src/ui/screens/ReaderText.cpp).
// Verified token by token against the firmware by tests/timing.test.mjs using tests/golden/golden.json.
//
// A word's time is   60000 / wpm
//                  + longWordDelayMs    × lengthPercent     / 100
//                  + complexWordDelayMs × complexityPercent / 100
//                  + punctuationDelayMs × pausePercent      / 100
// Percentages depend only on the text, so a book is scored once and re-timed instantly per setting.

// Constants exactly as in ReadingLoop.cpp.
export const CONSTANTS = Object.freeze({
  longWordAfterChars: 6, longWordPercentPerChar: 6,
  veryLongWordAfterChars: 10, veryLongWordPercentPerChar: 9,
  ultraLongWordAfterChars: 14, ultraLongWordPercentPerChar: 12,
  longWordMaxPercent: 170, compoundJoinerPercent: 14, longCompoundWordPercent: 18, technicalConnectorPercent: 8,
  syllableBonusAfterCount: 2, syllableBonusPercentPerGroup: 10, syllableBonusMaxPercent: 50,
  allCapsComplexityPercent: 14, mixedTokenComplexityPercent: 22, numericTokenComplexityPercent: 10,
  denseConnectorComplexityPercent: 12, complexWordMaxPercent: 85,
  commaPausePercent: 45, dashPausePercent: 60, clausePausePercent: 80, ellipsisPausePercent: 110,
  sentencePausePercent: 135, strongSentencePausePercent: 150,
});

// Device limits from SettingsModel.h (BoundedValue<min, max, step>).
export const LIMITS = Object.freeze({
  wpm: { min: 10, max: 1000, step: 10 },
  delay: { min: 0, max: 600, step: 50 },
});

const LETTER = /\p{L}/u, DIGIT = /\p{Nd}/u, UPPER = /\p{Lu}/u, LOWER = /\p{Ll}/u;
const isLetter = (ch) => LETTER.test(ch);
const isDigit = (ch) => DIGIT.test(ch);
const isUpper = (ch) => UPPER.test(ch);
const isLower = (ch) => LOWER.test(ch);
export const isWordCharacter = (ch) => isLetter(ch) || isDigit(ch);

// Han, Hiragana and Katakana ranges (kept in sync with rigels_studio/text.py).
const CJK_RANGES = [
  [0x2E80, 0x2FDF], [0x3005, 0x3005], [0x3007, 0x3007], [0x3021, 0x3029], [0x3038, 0x303B],
  [0x3041, 0x3096], [0x309D, 0x309F], [0x30A1, 0x30FA], [0x30FD, 0x30FF], [0x31F0, 0x31FF],
  [0x32D0, 0x32FE], [0x3300, 0x3357], [0x3400, 0x4DBF], [0x4E00, 0x9FFF], [0xF900, 0xFAFF],
  [0xFF66, 0xFF6F], [0xFF71, 0xFF9D], [0x1B000, 0x1B16F], [0x20000, 0x3134F],
];
const isCjkChar = (ch) => { const c = ch.codePointAt(0); return CJK_RANGES.some(([lo, hi]) => c >= lo && c <= hi); };
export function isCjkText(word) {
  let found = false;
  for (const ch of word) {
    if (isCjkChar(ch)) found = true;
    else if (isWordCharacter(ch)) return false;
  }
  return found;
}

// UnicodeText::toLowercase — a fixed table, not full Unicode case mapping.
function toLowercase(c) {
  if (c >= 0x41 && c <= 0x5A) return c + 0x20;
  if ((c >= 0xC0 && c <= 0xD6) || (c >= 0xD8 && c <= 0xDE)) return c + 0x20;
  const even = (c & 1) === 0;
  if ((even && ((c >= 0x100 && c <= 0x12E) || (c >= 0x132 && c <= 0x136) || (c >= 0x14A && c <= 0x176)
      || (c >= 0x460 && c <= 0x480) || (c >= 0x48A && c <= 0x4BE) || (c >= 0x4D0 && c <= 0x4FE)))
      || (!even && ((c >= 0x139 && c <= 0x147) || (c >= 0x179 && c <= 0x17D) || (c >= 0x4C1 && c <= 0x4CD)))) return c + 1;
  if (c >= 0x400 && c <= 0x40F) return c + 0x50;
  if (c >= 0x410 && c <= 0x42F) return c + 0x20;
  if (c === 0x130) return 0x69;
  if (c === 0x178) return 0xFF;
  if (c === 0x4C0) return 0x4CF;
  return c;
}

const VOWELS = new Set([
  0x61, 0x65, 0x69, 0x6F, 0x75, 0x79,
  0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
  0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFF,
  0x101, 0x103, 0x105, 0x113, 0x115, 0x117, 0x119, 0x11B, 0x129, 0x12B, 0x12D, 0x12F, 0x131,
  0x14D, 0x14F, 0x151, 0x153, 0x169, 0x16B, 0x16D, 0x16F, 0x171, 0x173, 0x177,
  0x430, 0x435, 0x438, 0x43E, 0x443, 0x44B, 0x44D, 0x44E, 0x44F, 0x451, 0x454, 0x456, 0x457,
]);
const isVowel = (c) => VOWELS.has(toLowercase(c));

const count = (chars, predicate) => chars.reduce((n, ch) => n + (predicate(ch) ? 1 : 0), 0);
const isSegmentSeparator = (ch) => ch === '-' || ch === '/' || ch === '_';
const isTechnicalConnector = (ch) => '-/_.+\\'.includes(ch);
const IGNORED_TRAILING = new Set(['"', "'", ')', ']', '}', '’', '”', '〉', '》', '」', '』',
  '】', '〕', '〗', '〙', '〛', '）', '］', '｝']);
const ASCII_IGNORED_TRAILING = new Set(['"', "'", ')', ']', '}']);
const RHYTHM_ALIASES = { '、': ',', '，': ',', '。': '.', '！': '!', '：': ':', '；': ';', '？': '?' };

function connectorCount(chars, predicate) {
  let n = 0;
  for (let i = 1; i + 1 < chars.length; ++i)
    if (predicate(chars[i]) && isWordCharacter(chars[i - 1]) && isWordCharacter(chars[i + 1])) ++n;
  return n;
}

function syllableGroups(chars) {
  let groups = 0, letters = 0, previousWasVowel = false, previousLetter = 0, lastLetter = 0;
  for (const ch of chars) {
    if (!isLetter(ch)) { previousWasVowel = false; continue; }
    ++letters;
    const lowered = toLowercase(ch.codePointAt(0));
    previousLetter = lastLetter; lastLetter = lowered;
    const vowel = isVowel(lowered);
    if (vowel && !previousWasVowel) ++groups;
    previousWasVowel = vowel;
  }
  if (groups > 1 && letters > 3 && lastLetter === 0x65 && previousLetter !== 0x6C && previousLetter !== 0x79) --groups;
  if (groups === 0 && letters > 0) groups = 1;
  return groups;
}

function trailingRhythm(chars) {
  for (let i = chars.length - 1; i >= 0; --i) {
    if (IGNORED_TRAILING.has(chars[i])) continue;
    return RHYTHM_ALIASES[chars[i]] ?? chars[i];
  }
  return '';
}

// Byte-oriented in the firmware; only ASCII trailing characters are skipped there.
function lastMeaningfulIndex(word) {
  for (let i = word.length - 1; i >= 0; --i) if (!ASCII_IGNORED_TRAILING.has(word[i])) return i;
  return -1;
}

function trailingDots(word) {
  let n = 0;
  for (let i = lastMeaningfulIndex(word); i >= 0 && word[i] === '.'; --i) ++n;
  return n;
}

export function startsWithLowercaseLetter(word) {
  for (const ch of word) {
    if (isLower(ch)) return true;
    if (isLetter(ch)) return false;
  }
  return false;
}

function isDottedInitialism(word) {
  const end = lastMeaningfulIndex(word);
  if (end <= 0) return false;
  let letters = 0, expectLetter = true;
  for (const ch of word.slice(0, end + 1)) {
    if (expectLetter) {
      if (!isLetter(ch)) return false;
      ++letters; expectLetter = false;
    } else if (ch === '.') expectLetter = true;
    else return false;
  }
  return expectLetter && letters >= 2;
}

const KNOWN_ABBREVIATIONS = new Set(['mr.', 'mrs.', 'ms.', 'dr.', 'prof.', 'sr.', 'jr.', 'st.', 'vs.', 'etc.', 'e.g.',
  'i.e.', 'cf.', 'no.', 'fig.', 'eq.', 'inc.', 'ltd.', 'co.', 'dept.', 'mt.', 'ft.']);
const asciiLower = (s) => s.replace(/[A-Z]/g, (c) => c.toLowerCase());

function looksLikeAbbreviation(word, chars, nextLower) {
  if (KNOWN_ABBREVIATIONS.has(asciiLower(word))) return true;
  if (!word.endsWith('.')) return false;
  if (isDottedInitialism(word)) return true;
  const readable = count(chars, isWordCharacter);
  if (readable === 1 && word !== 'I.') return true;
  return nextLower && readable <= 4;
}

function lengthPercent(chars, k) {
  const readable = count(chars, isWordCharacter);
  if (readable === 0) return 0;
  let p = 0;
  if (readable > k.longWordAfterChars) p += (readable - k.longWordAfterChars) * k.longWordPercentPerChar;
  if (readable > k.veryLongWordAfterChars) p += (readable - k.veryLongWordAfterChars) * k.veryLongWordPercentPerChar;
  if (readable > k.ultraLongWordAfterChars) p += (readable - k.ultraLongWordAfterChars) * k.ultraLongWordPercentPerChar;
  const joiners = connectorCount(chars, isSegmentSeparator);
  if (joiners > 0) {
    p += joiners * k.compoundJoinerPercent;
    if (readable >= k.veryLongWordAfterChars) p += k.longCompoundWordPercent;
  }
  const tech = connectorCount(chars, isTechnicalConnector);
  if (tech > joiners) p += (tech - joiners) * k.technicalConnectorPercent;
  return Math.min(k.longWordMaxPercent, p);
}

function complexityPercent(chars, k) {
  let p = 0;
  const groups = syllableGroups(chars);
  if (groups > k.syllableBonusAfterCount)
    p += Math.min(k.syllableBonusMaxPercent, (groups - k.syllableBonusAfterCount) * k.syllableBonusPercentPerGroup);
  const letters = count(chars, isLetter), digits = count(chars, isDigit), upper = count(chars, isUpper);
  if (letters > 0 && digits > 0) p += k.mixedTokenComplexityPercent;
  else if (digits >= 3) p += k.numericTokenComplexityPercent;
  if (upper >= 2 && upper === letters) p += k.allCapsComplexityPercent;
  const tech = connectorCount(chars, isTechnicalConnector);
  if (tech >= 2) p += (tech - 1) * k.denseConnectorComplexityPercent;
  return Math.min(k.complexWordMaxPercent, p);
}

function pausePercent(word, chars, nextLower, k) {
  const rhythm = trailingRhythm(chars);
  if (rhythm === '…' || trailingDots(word) >= 3) return k.ellipsisPausePercent;
  switch (rhythm) {
    case ',': return k.commaPausePercent;
    case '-': case '—': return k.dashPausePercent;
    case ';': case ':': return k.clausePausePercent;
    case '.': return looksLikeAbbreviation(word, chars, nextLower) ? 0 : k.sentencePausePercent;
    case '!': case '?': return k.strongSentencePausePercent;
    default: return 0;
  }
}

/**
 * Score one word. `nextLower` is whether the following word starts with a lowercase letter.
 * Returns the three bonus percentages plus whether the reader times it as a CJK phrase.
 */
export function scoreWord(word, nextLower, k = CONSTANTS) {
  const chars = Array.from(word);
  if (isCjkText(word)) {
    // CJK phrases are timed per character; next-word context is not used for their pause.
    return { length: 0, complexity: 0, pause: pausePercent(word, chars, false, k), cjkChars: Math.max(1, count(chars, isWordCharacter)) };
  }
  if (!word) return { length: 0, complexity: 0, pause: 0, cjkChars: 0 };
  return {
    length: lengthPercent(chars, k),
    complexity: complexityPercent(chars, k),
    pause: pausePercent(word, chars, nextLower, k),
    cjkChars: 0,
  };
}

/** Score a whole word list into compact typed arrays (one entry per word). */
export function scoreWords(words, k = CONSTANTS) {
  const n = words.length;
  const length = new Uint8Array(n), complexity = new Uint8Array(n), pause = new Uint8Array(n), cjkChars = new Uint16Array(n);
  for (let i = 0; i < n; ++i) {
    const nextLower = i + 1 < n && startsWithLowercaseLetter(words[i + 1]);
    const s = scoreWord(words[i], nextLower, k);
    length[i] = s.length; complexity[i] = s.complexity; pause[i] = s.pause; cjkChars[i] = s.cjkChars;
  }
  return { length, complexity, pause, cjkChars };
}

const scaled = (percent, delayMs) => Math.floor((percent * delayMs) / 100);

/** Milliseconds the reader shows word i, split into its parts. */
export function wordTiming(scores, i, settings) {
  const { wpm, longWordDelayMs: L, complexWordDelayMs: C, punctuationDelayMs: P } = settings;
  if (scores.cjkChars[i]) {
    const base = Math.floor((60000 * scores.cjkChars[i] + wpm - 1) / wpm);
    const pause = scaled(scores.pause[i], P);
    return { base, length: 0, complexity: 0, pause, total: base + pause };
  }
  const base = Math.floor(60000 / wpm);
  const length = scaled(scores.length[i], L), complexity = scaled(scores.complexity[i], C), pause = scaled(scores.pause[i], P);
  return { base, length, complexity, pause, total: base + length + complexity + pause };
}

/** Per-word total durations as a Uint32Array, for analysis. */
export function durations(scores, settings) {
  const n = scores.length.length, out = new Uint32Array(n);
  const base = Math.floor(60000 / settings.wpm);
  const { longWordDelayMs: L, complexWordDelayMs: C, punctuationDelayMs: P, wpm } = settings;
  for (let i = 0; i < n; ++i) {
    out[i] = scores.cjkChars[i]
      ? Math.floor((60000 * scores.cjkChars[i] + wpm - 1) / wpm) + scaled(scores.pause[i], P)
      : base + scaled(scores.length[i], L) + scaled(scores.complexity[i], C) + scaled(scores.pause[i], P);
  }
  return out;
}

const FOCUS_ORDINAL = (n) => (n <= 1 ? 0 : n <= 5 ? 1 : n <= 9 ? 2 : n <= 13 ? 3 : 4);

/** Index (in code points) of the highlighted focus letter, or -1 for an empty word. */
export function focusIndex(word) {
  const chars = Array.from(word), positions = [];
  chars.forEach((ch, i) => { if (isWordCharacter(ch)) positions.push(i); });
  if (!positions.length) return chars.length ? 0 : -1;
  return positions[Math.min(FOCUS_ORDINAL(positions.length), positions.length - 1)];
}

export const clampSetting = (value, { min, max, step }) => Math.min(max, Math.max(min, Math.round(value / step) * step));
