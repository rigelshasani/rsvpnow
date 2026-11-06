import { scoreWords, durations, wordTiming, focusIndex, clampSetting, LIMITS } from './timing.js';

const $ = (selector) => document.querySelector(selector);
const DEFAULTS = { wpm: 300, longWordDelayMs: 200, complexWordDelayMs: 200, punctuationDelayMs: 200 };
const SETTINGS = [
  { key: 'wpm', label: 'Words per minute', unit: 'wpm', limits: LIMITS.wpm, color: 'var(--base)' },
  { key: 'longWordDelayMs', label: 'Long words', unit: 'ms', limits: LIMITS.delay, color: 'var(--long)' },
  { key: 'complexWordDelayMs', label: 'Complex words', unit: 'ms', limits: LIMITS.delay, color: 'var(--complex)' },
  { key: 'punctuationDelayMs', label: 'Punctuation pauses', unit: 'ms', limits: LIMITS.delay, color: 'var(--pause)' },
];
const PARTS = [
  { key: 'base', label: 'Base pace', color: 'var(--base)' },
  { key: 'length', label: 'Long words', color: 'var(--long)' },
  { key: 'complexity', label: 'Complex words', color: 'var(--complex)' },
  { key: 'pause', label: 'Punctuation pauses', color: 'var(--pause)' },
];
const SENTENCE_PAUSE = 135; // pause percent at sentence ends (see timing.js CONSTANTS)

const store = {
  get(key, fallback) { try { return JSON.parse(localStorage.getItem(key)) ?? fallback; } catch { return fallback; } },
  set(key, value) { try { localStorage.setItem(key, JSON.stringify(value)); } catch { /* private mode */ } },
};

const state = {
  library: [],
  bookId: null,
  book: null,
  scores: null,
  unique: null,
  settings: { ...DEFAULTS, ...store.get('settings', {}) },
  reader: null,
  analysis: null,
  trim: null,
  pos: 0,
  playing: false,
  timer: 0,
};

// ---------------------------------------------------------------------------------------------- utils
const escapeHtml = (s) => s.replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const fmt = (n) => Math.round(n).toLocaleString();
function fmtDuration(ms) {
  const minutes = Math.round(ms / 60000);
  if (minutes < 1) return `${Math.round(ms / 1000)} s`;
  const h = Math.floor(minutes / 60), m = minutes % 60;
  return h ? `${h} h ${m} m` : `${m} m`;
}
function chapterAt(index) {
  const chapters = state.book.chapters;
  let lo = 0, hi = chapters.length - 1, found = -1;
  while (lo <= hi) {
    const mid = (lo + hi) >> 1;
    if (chapters[mid].wordIndex <= index) { found = mid; lo = mid + 1; } else hi = mid - 1;
  }
  return found;
}
function wordHtml(word) {
  const chars = Array.from(word), focus = focusIndex(word);
  if (focus < 0) return '';
  return `${escapeHtml(chars.slice(0, focus).join(''))}<span class="focus">${escapeHtml(chars[focus])}</span>${escapeHtml(chars.slice(focus + 1).join(''))}`;
}
async function api(path, options) {
  const response = await fetch(path, options);
  const body = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(body.error || `Request failed (${response.status})`);
  return body;
}

// -------------------------------------------------------------------------------------------- library
async function loadLibrary() {
  try {
    state.library = await api('/api/library');
  } catch (error) {
    $('#library').innerHTML = `<p class="muted pad">${escapeHtml(error.message)}</p>`;
    return;
  }
  renderLibrary();
  const last = store.get('book', null);
  if (last && state.library.some((b) => b.id === last)) openBook(last);
}

function renderLibrary() {
  const query = $('#search').value.trim().toLowerCase();
  const groups = new Map();
  for (const book of state.library) {
    if (query && !book.name.toLowerCase().includes(query)) continue;
    if (!groups.has(book.location)) groups.set(book.location, []);
    groups.get(book.location).push(book);
  }
  if (!groups.size) {
    $('#library').innerHTML = `<p class="muted pad">${query ? 'No matching books.' : 'No books found in Downloads, Documents, Desktop or an SD card.'}</p>`;
    return;
  }
  $('#library').innerHTML = [...groups].map(([location, books]) => `
    <h3>${escapeHtml(location)}</h3>
    ${books.map((b) => `
      <button class="book-item" data-id="${b.id}" title="${escapeHtml(b.path)}" aria-current="${b.id === state.bookId}">
        <span class="name">${escapeHtml(b.name)}</span>
        <span class="badge ${b.exact ? 'exact' : ''}" title="${b.exact ? 'The reader\'s own converted text: exact match' : 'Converted here per the RSVP spec'}">${b.exact ? 'exact' : b.format}</span>
      </button>`).join('')}`).join('');
}

async function openBook(id) {
  stop();
  state.bookId = id;
  renderLibrary();
  const entry = state.library.find((b) => b.id === id);
  $('#empty').hidden = true;
  $('#workspace').hidden = false;
  $('#book-title').textContent = entry?.name ?? 'Loading…';
  $('#book-meta').textContent = 'Converting…';
  try {
    state.book = await api(`/api/books/${id}`);
  } catch (error) {
    $('#book-meta').textContent = error.message;
    state.book = null;
    return;
  }
  store.set('book', id);
  state.scores = scoreWords(state.book.words);
  state.unique = uniqueTokens(state.book.words, state.scores);
  state.pos = Math.min(store.get(`pos:${id}`, 0), state.book.words.length - 1);
  $('#scrub').max = String(Math.max(0, state.book.words.length - 1));
  $('#book-title').textContent = state.book.title;
  renderBookMeta();
  analyze();
  showWord();
  loadSections(id);
}

// ----------------------------------------------------------------------------------------------- trim
async function loadSections(id) {
  state.trim = null;
  $('#trim-list').innerHTML = '<li class="more">Looking for front and back matter…</li>';
  $('#trim-status').textContent = '';
  try {
    const info = await api(`/api/books/${id}/sections`);
    if (state.bookId !== id) return;
    state.trim = { ...info, keep: info.sections.map((s) => s.keep) };
  } catch (error) {
    $('#trim-list').innerHTML = `<li class="more">${escapeHtml(error.message)}</li>`;
    return;
  }
  renderTrim();
  // A book opened for the first time starts where the work starts, not at the publisher's pages.
  const firstKept = state.trim.sections.find((s, i) => state.trim.keep[i]);
  if (state.pos === 0 && firstKept?.start) seek(firstKept.start);
  renderTimeline();
}

const KIND_CLASS = { gutenberg: 'gutenberg', contents: 'contents', 'front matter': 'front', 'back matter': 'back', commentary: 'commentary', body: 'body' };

function renderTrim() {
  const { sections, keep, onCard, cardMounted, fileName } = state.trim;
  const rows = [];
  let run = [];
  const flushRun = () => {
    if (run.length > 3) {
      rows.push(trimRow(run[0]));
      rows.push(`<li class="more">… ${run.length - 2} more chapters of the book (kept)</li>`);
      rows.push(trimRow(run[run.length - 1]));
    } else run.forEach((i) => rows.push(trimRow(i)));
    run = [];
  };
  sections.forEach((section, i) => {
    if (section.kind === 'body' && keep[i]) run.push(i);
    else { flushRun(); rows.push(trimRow(i)); }
  });
  flushRun();
  $('#trim-list').innerHTML = rows.join('');
  const removed = sections.reduce((n, s, i) => n + (keep[i] ? 0 : s.words), 0);
  const ms = state.analysis ? sections.reduce((t, s, i) => t + (keep[i] ? 0 : state.analysis.prefix[s.end] - state.analysis.prefix[s.start]), 0) : 0;
  const first = sections.find((s, i) => keep[i]);
  $('#trim-summary').textContent = removed
    ? `Removes ${fmt(removed)} words (${fmtDuration(ms)}) · starts at “${first?.title ?? ''}”`
    : 'Nothing removed';
  $('#trim-card').disabled = !removed || !cardMounted;
  $('#trim-card').title = cardMounted ? `Writes ${fileName}${onCard ? ' and moves the original into library/books/originals' : ''}` : 'Insert the reader\'s SD card into this Mac';
  $('#trim-downloads').disabled = !removed;
}

function trimRow(i) {
  const section = state.trim.sections[i], kept = state.trim.keep[i];
  return `<li class="${kept ? '' : 'cut'}" title="${escapeHtml(section.preview)}…">
    <input type="checkbox" data-section="${i}" ${kept ? 'checked' : ''} aria-label="Keep ${escapeHtml(section.title)}">
    <span class="title">${escapeHtml(section.title)}${section.reason ? `<small>${escapeHtml(section.reason)}</small>` : ''}</span>
    <span class="kind ${KIND_CLASS[section.kind]}">${escapeHtml(section.kind)}</span>
    <span class="count">${fmt(section.words)} words</span>
  </li>`;
}

async function saveTrim(target) {
  const status = $('#trim-status');
  status.textContent = 'Saving…';
  for (const b of ['#trim-card', '#trim-downloads']) $(b).disabled = true;
  try {
    const result = await api(`/api/books/${state.bookId}/trim`, { method: 'POST', body: JSON.stringify({ keep: state.trim.keep, target }) });
    const notes = [`Saved ${fmt(result.keptWords)} words to ${result.path.replace(/^\/Users\/[^/]+/, '~')}.`];
    if (result.moved.length) notes.push(`Moved ${result.moved.length} original files to library/books/originals.`);
    if (result.position) notes.push(`Your place (word ${fmt(result.position.from)}) becomes word ${fmt(result.position.to)}: open the book once on the reader, then press Load from reader to restore it.`);
    else if (target === 'card') notes.push('Put the card back in the reader; the trimmed book appears in the library.');
    status.textContent = notes.join(' ');
    loadLibraryQuietly();
  } catch (error) {
    status.textContent = error.message;
  } finally {
    renderTrim();
  }
}

async function loadLibraryQuietly() {
  try { state.library = await api('/api/library'); renderLibrary(); } catch { /* keep the old list */ }
}

function renderBookMeta() {
  const book = state.book, entry = state.library.find((b) => b.id === state.bookId);
  const parts = [book.author, `${fmt(book.words.length)} words`, `${book.chapters.length} chapters`];
  const onReader = state.reader?.library.find((b) => b.title === book.title && b.wordCount);
  if (onReader) {
    const diff = book.words.length - onReader.wordCount;
    parts.push(diff ? `reader counts ${fmt(onReader.wordCount)} (${diff > 0 ? '+' : ''}${diff} here)` : 'matches the reader exactly');
  } else if (entry?.exact) parts.push("the reader's own text");
  $('#book-meta').textContent = parts.filter(Boolean).join(' · ');
}

// For "slowest words": one representative occurrence per distinct token (the one with the longest pause).
function uniqueTokens(words, scores) {
  const best = new Map();
  for (let i = 0; i < words.length; ++i) {
    const seen = best.get(words[i]);
    if (seen === undefined) best.set(words[i], [i, 1]);
    else { seen[1]++; if (scores.pause[i] > scores.pause[seen[0]]) seen[0] = i; }
  }
  return [...best.values()];
}

// ------------------------------------------------------------------------------------------- analysis
let analyzeQueued = false;
function scheduleAnalyze() {
  if (analyzeQueued) return;
  analyzeQueued = true;
  requestAnimationFrame(() => { analyzeQueued = false; analyze(); });
}

function analyze() {
  if (!state.book) return;
  const { scores, settings } = state;
  const ms = durations(scores, settings);
  const n = ms.length;
  const sums = { base: 0, length: 0, complexity: 0, pause: 0 };
  const L = settings.longWordDelayMs, C = settings.complexWordDelayMs, P = settings.punctuationDelayMs;
  const base = Math.floor(60000 / settings.wpm);
  let total = 0;
  for (let i = 0; i < n; ++i) {
    total += ms[i];
    if (scores.cjkChars[i]) { const t = wordTiming(scores, i, settings); sums.base += t.base; sums.pause += t.pause; continue; }
    sums.base += base;
    sums.length += Math.floor((scores.length[i] * L) / 100);
    sums.complexity += Math.floor((scores.complexity[i] * C) / 100);
    sums.pause += Math.floor((scores.pause[i] * P) / 100);
  }
  const prefix = new Float64Array(n + 1);
  for (let i = 0; i < n; ++i) prefix[i + 1] = prefix[i] + ms[i];
  state.analysis = { ms, prefix, total, sums, base };
  renderStats();
  renderTimeline();
  renderShare();
  renderSlowest();
  renderChapters();
  renderInspector();
  showWordBreakdown();
}

function renderStats() {
  const { total, sums } = state.analysis, n = state.book.words.length;
  const effective = n / (total / 60000);
  const extra = (total / sums.base - 1) * 100;
  const left = total - state.analysis.prefix[state.pos];
  $('#stats').innerHTML = [
    [fmtDuration(total), 'reading time'],
    [fmt(effective), 'effective wpm'],
    [`+${extra.toFixed(0)}%`, 'added by pacing', extra > 60 ? 'warn' : ''],
    [fmtDuration(left), 'left from here'],
  ].map(([value, label, cls = '']) => `<div class="stat ${cls}"><b>${value}</b><span>${label}</span></div>`).join('');
}

// ------------------------------------------------------------------------------------------- timeline
const timeline = { buckets: [], width: 0 };

function renderTimeline() {
  const canvas = $('#timeline'), dpr = window.devicePixelRatio || 1;
  const width = canvas.clientWidth, height = canvas.clientHeight;
  if (!width) return;
  canvas.width = width * dpr; canvas.height = height * dpr;
  const g = canvas.getContext('2d');
  g.scale(dpr, dpr);
  const css = getComputedStyle(document.documentElement);
  const color = (name) => css.getPropertyValue(name).trim();

  const { ms, prefix } = state.analysis, n = ms.length, wpm = state.settings.wpm;
  const pad = { l: 40, r: 8, t: 10, b: 20 }, plotW = width - pad.l - pad.r, plotH = height - pad.t - pad.b;
  const count = Math.max(1, Math.min(n, Math.floor(plotW / 2)));
  const buckets = [];
  for (let b = 0; b < count; ++b) {
    const start = Math.floor((b * n) / count), end = Math.max(start + 1, Math.floor(((b + 1) * n) / count));
    const time = prefix[end] - prefix[start];
    buckets.push({ start, end, wpm: (end - start) / (time / 60000) });
  }
  timeline.buckets = buckets; timeline.pad = pad; timeline.plotW = plotW;
  const top = Math.max(wpm, ...buckets.map((b) => b.wpm)) * 1.08;
  const x = (i) => pad.l + (i / n) * plotW, y = (v) => pad.t + plotH - (v / top) * plotH;

  // Chapter bands.
  state.book.chapters.forEach((chapter, i) => {
    const next = state.book.chapters[i + 1]?.wordIndex ?? n;
    if (i % 2) { g.fillStyle = 'rgba(255,255,255,0.025)'; g.fillRect(x(chapter.wordIndex), pad.t, x(next) - x(chapter.wordIndex), plotH); }
  });
  // Sections that trimming removes.
  if (state.trim) {
    g.fillStyle = 'rgba(0,0,0,0.55)';
    state.trim.sections.forEach((section, i) => {
      if (!state.trim.keep[i]) g.fillRect(x(section.start), pad.t, Math.max(1.5, x(section.end) - x(section.start)), plotH);
    });
  }
  // Grid.
  g.font = '11px -apple-system, system-ui, sans-serif'; g.fillStyle = color('--subtle'); g.strokeStyle = color('--line'); g.lineWidth = 1;
  const step = top > 600 ? 200 : top > 300 ? 100 : 50;
  for (let v = 0; v <= top; v += step) {
    g.beginPath(); g.moveTo(pad.l, Math.round(y(v)) + 0.5); g.lineTo(width - pad.r, Math.round(y(v)) + 0.5); g.stroke();
    g.fillText(String(v), 4, y(v) + 4);
  }
  // Effective WPM (area + line).
  g.beginPath(); g.moveTo(x(0), y(0));
  buckets.forEach((b) => g.lineTo(x((b.start + b.end) / 2), y(b.wpm)));
  g.lineTo(x(n), y(0)); g.closePath();
  g.fillStyle = 'rgba(253,111,66,0.14)'; g.fill();
  g.beginPath();
  buckets.forEach((b, i) => (i ? g.lineTo : g.moveTo).call(g, x((b.start + b.end) / 2), y(b.wpm)));
  g.strokeStyle = color('--accent'); g.lineWidth = 1.5; g.stroke();
  // Setting line.
  g.setLineDash([4, 3]); g.strokeStyle = color('--muted'); g.lineWidth = 1;
  g.beginPath(); g.moveTo(pad.l, y(wpm)); g.lineTo(width - pad.r, y(wpm)); g.stroke(); g.setLineDash([]);
  // Current position.
  g.strokeStyle = color('--fg'); g.beginPath(); g.moveTo(x(state.pos) + 0.5, pad.t); g.lineTo(x(state.pos) + 0.5, pad.t + plotH); g.stroke();
  g.fillStyle = color('--subtle'); g.fillText('start', pad.l, height - 5);
  g.textAlign = 'right'; g.fillText('end', width - pad.r, height - 5); g.textAlign = 'left';
}

function bucketAt(event) {
  const rect = $('#timeline').getBoundingClientRect(), px = event.clientX - rect.left - timeline.pad.l;
  const i = Math.floor((px / timeline.plotW) * timeline.buckets.length);
  return timeline.buckets[Math.max(0, Math.min(timeline.buckets.length - 1, i))];
}

// ------------------------------------------------------------------------------------- share / words
function renderShare() {
  const { sums, total } = state.analysis;
  $('#share').innerHTML = PARTS.map((p) => `<i style="width:${(sums[p.key] / total) * 100}%;background:${p.color}" title="${p.label}"></i>`).join('');
  $('#share-legend').innerHTML = PARTS.map((p) => `
    <i style="background:${p.color}"></i><span>${p.label}</span>
    <b>${((sums[p.key] / total) * 100).toFixed(1)}% · ${fmtDuration(sums[p.key])}</b>`).join('');
}

function renderSlowest() {
  const { scores, settings } = state, words = state.book.words;
  const ranked = state.unique.map(([i, count]) => [i, count, wordTiming(scores, i, settings)])
    .sort((a, b) => b[2].total - a[2].total).slice(0, 14);
  $('#slowest').innerHTML = ranked.map(([i, count, t]) => `
    <li data-pos="${i}" title="Jump to its first use">
      <span class="w">${wordHtml(words[i])}</span><span class="ms">${t.total} ms${count > 1 ? ` · ×${count}` : ''}</span>
      <span class="parts">${PARTS.filter((p) => t[p.key]).map((p) => `<span style="--c:${p.color}">${p.key === 'base' ? 'base ' : '+'}${t[p.key]}</span>`).join('')}</span>
    </li>`).join('');
}

function renderChapters() {
  const { prefix, base } = state.analysis, chapters = state.book.chapters, n = state.book.words.length;
  const current = chapterAt(state.pos);
  const rows = chapters.map((chapter, i) => {
    const end = chapters[i + 1]?.wordIndex ?? n, words = end - chapter.wordIndex;
    const time = prefix[end] - prefix[chapter.wordIndex];
    return { i, chapter, words, time, slow: words ? time / (words * base) - 1 : 0 };
  }).filter((r) => r.words > 0);
  const maxSlow = Math.max(0.01, ...rows.map((r) => r.slow));
  $('#chapters').innerHTML = `
    <thead><tr><th>Chapter</th><th>Words</th><th>Time</th><th>Eff. wpm</th><th>Slower by</th></tr></thead>
    <tbody>${rows.map((r) => `
      <tr data-pos="${r.chapter.wordIndex}" class="${r.i === current ? 'current' : ''}">
        <td title="${escapeHtml(r.chapter.title)}">${escapeHtml(r.chapter.title)}</td>
        <td>${fmt(r.words)}</td><td>${fmtDuration(r.time)}</td><td>${fmt(r.words / (r.time / 60000))}</td>
        <td><span class="slow-cell">${(r.slow * 100).toFixed(0)}%<span class="meter"><i style="width:${(r.slow / maxSlow) * 100}%"></i></span></span></td>
      </tr>`).join('')}</tbody>`;
}

// ------------------------------------------------------------------------------------------ inspector
let inspectTimer = 0;
async function renderInspector() {
  clearTimeout(inspectTimer);
  inspectTimer = setTimeout(async () => {
    const text = $('#inspect-input').value;
    let tokens;
    try { tokens = await api('/api/tokenize', { method: 'POST', body: text }); } catch { return; }
    const scores = scoreWords(tokens);
    $('#inspect-output').innerHTML = tokens.map((token, i) => {
      const t = wordTiming(scores, i, state.settings);
      return `<div class="token" title="${PARTS.map((p) => `${p.label}: ${t[p.key]} ms`).join('\n')}">
        <div class="t">${wordHtml(token)}</div><div class="ms">${t.total} ms</div>
        <div class="bar">${PARTS.map((p) => `<i style="width:${(t[p.key] / t.total) * 100}%;background:${p.color}"></i>`).join('')}</div>
      </div>`;
    }).join('');
  }, 150);
}

// -------------------------------------------------------------------------------------------- preview
function showWord() {
  if (!state.book) return;
  const words = state.book.words, word = words[state.pos] ?? '';
  const el = $('#word'), screen = $('#screen');
  el.innerHTML = wordHtml(word);
  // Pin the focus letter to the reader's anchor (34% across), like the device does.
  const focus = el.querySelector('.focus'), anchor = screen.clientWidth * 0.34;
  el.style.left = '0px';
  if (focus) el.style.left = `${anchor - (focus.offsetLeft + focus.offsetWidth / 2)}px`;
  const chapter = chapterAt(state.pos);
  $('#screen-chapter').textContent = chapter >= 0 ? state.book.chapters[chapter].title : '';
  $('#screen-progress span').style.width = `${(state.pos / Math.max(1, words.length - 1)) * 100}%`;
  $('#scrub').value = String(state.pos);
  $('#position').textContent = `${fmt(state.pos + 1)} / ${fmt(words.length)}`;
  showWordBreakdown();
}

function showWordBreakdown() {
  if (!state.analysis) return;
  const t = wordTiming(state.scores, state.pos, state.settings);
  $('#breakdown-word').innerHTML = `
    <span class="mono">${t.total} ms</span>
    <span class="bar">${PARTS.map((p) => `<i style="width:${(t[p.key] / t.total) * 100}%;background:${p.color}" title="${p.label}: ${t[p.key]} ms"></i>`).join('')}</span>
    <span class="muted">${PARTS.filter((p) => p.key !== 'base' && t[p.key]).map((p) => `${p.label.toLowerCase()} +${t[p.key]}`).join(' · ') || 'base pace only'}</span>`;
}

function seek(pos, { persist = true } = {}) {
  if (!state.book) return;
  state.pos = Math.max(0, Math.min(state.book.words.length - 1, pos));
  showWord();
  if (persist) store.set(`pos:${state.bookId}`, state.pos);
  if (!state.playing) { renderTimeline(); renderStats(); renderChapters(); }
}

function tick() {
  if (!state.playing) return;
  if (state.pos >= state.book.words.length - 1) return stop();
  state.timer = setTimeout(() => {
    state.pos += 1;
    showWord();
    tick();
  }, state.analysis.ms[state.pos]);
}

function play() {
  if (!state.book || state.playing) return;
  state.playing = true;
  $('#play').textContent = '❚❚';
  tick();
}

function stop() {
  clearTimeout(state.timer);
  if (!state.playing) return;
  state.playing = false;
  $('#play').textContent = '▶';
  seek(state.pos);
}

const isSentenceEnd = (i) => state.scores.pause[i] >= SENTENCE_PAUSE;
function sentenceStart(from) {
  let i = from;
  while (i > 0 && !isSentenceEnd(i - 1)) --i;
  return i;
}
function previousSentence() {
  const start = sentenceStart(state.pos);
  seek(start === state.pos ? sentenceStart(Math.max(0, start - 1)) : start);
}
function nextSentence() {
  let i = state.pos;
  while (i < state.book.words.length - 1 && !isSentenceEnd(i)) ++i;
  seek(i + 1);
}

// ------------------------------------------------------------------------------------------- settings
function renderSliders() {
  $('#sliders').innerHTML = SETTINGS.map((s) => `
    <div class="slider">
      <label for="s-${s.key}"><i style="background:${s.color}"></i>${s.label}</label>
      <output id="o-${s.key}"></output>
      <input id="s-${s.key}" data-key="${s.key}" type="range" min="${s.limits.min}" max="${s.limits.max}" step="${s.limits.step}">
      <span class="reader-mark" id="r-${s.key}"></span>
    </div>`).join('');
  syncSliders();
}

function syncSliders() {
  for (const s of SETTINGS) {
    const value = state.settings[s.key];
    $(`#s-${s.key}`).value = String(value);
    $(`#o-${s.key}`).innerHTML = `${value} <small>${s.unit}</small>`;
    const reader = state.reader?.settings[s.key];
    $(`#r-${s.key}`).textContent = reader === undefined ? '' : reader === value ? 'same as reader' : `reader: ${reader} ${s.unit}`;
  }
  const differs = state.reader && SETTINGS.some((s) => state.reader.settings[s.key] !== state.settings[s.key]);
  $('#device-write').disabled = !state.reader || !differs;
  $('#device-write').textContent = state.reader && !differs ? 'Reader is up to date' : 'Send to reader';
  document.querySelector('[data-preset="reader"]').disabled = !state.reader;
  renderDeviceSettings();
}

function setSettings(values) {
  for (const s of SETTINGS) if (values[s.key] !== undefined) state.settings[s.key] = clampSetting(Number(values[s.key]), s.limits);
  store.set('settings', state.settings);
  syncSliders();
  scheduleAnalyze();
}

// --------------------------------------------------------------------------------------------- device
function renderDeviceSettings() {
  const dl = $('#device-settings');
  if (!state.reader) { dl.hidden = true; return; }
  dl.hidden = false;
  dl.innerHTML = SETTINGS.map((s) => {
    const value = state.reader.settings[s.key];
    return `<dt>${s.label}</dt><dd class="${value !== state.settings[s.key] ? 'differs' : ''}">${value} ${s.unit}</dd>`;
  }).join('');
}

async function readDevice() {
  const status = $('#device-status');
  status.textContent = 'Connecting over USB…';
  $('#device-read').disabled = true;
  try {
    state.reader = await api('/api/device');
    const converted = state.reader.library.filter((b) => b.wordCount).length;
    status.textContent = `Connected · ${state.reader.info.firmwareVersion.split('+')[0]} · ${state.reader.library.length} books (${converted} converted)`;
    for (const r of state.reader.restored ?? []) status.textContent += ` · restored your place in ${r.title}`;
    if (state.reader.mode !== 'rsvp') status.textContent += ` · note: reader is in ${state.reader.mode} mode; timing here is for single words`;
    syncSliders();
    if (state.book) renderBookMeta();
  } catch (error) {
    status.textContent = error.message;
  } finally {
    $('#device-read').disabled = false;
  }
}

async function writeDevice() {
  const status = $('#device-status');
  $('#device-write').disabled = true;
  status.textContent = 'Sending…';
  try {
    const applied = await api('/api/device/settings', { method: 'PUT', body: JSON.stringify(state.settings) });
    state.reader.settings = applied;
    status.textContent = `Saved on the reader at ${new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}.`;
  } catch (error) {
    status.textContent = error.message;
  } finally {
    syncSliders();
  }
}

// ------------------------------------------------------------------------------------------ add books
// One upload at a time: the reader handles a single USB request.
let sending = Promise.resolve();
function addBooks(files) {
  for (const file of files) {
    const row = document.createElement('li');
    row.innerHTML = `<span class="name">${escapeHtml(file.name)}</span><span class="state">Waiting…</span>`;
    $('#drop-list').append(row);
    const state = row.querySelector('.state');
    sending = sending.then(async () => {
      state.textContent = 'Sending…';
      try {
        const sent = await api(`/api/device/books?name=${encodeURIComponent(file.name)}`, { method: 'POST', body: file });
        state.className = 'state done';
        state.textContent = sent.removedWords ? `Added · trimmed ${fmt(sent.removedWords)} words` : 'Added';
      } catch (error) {
        state.className = 'state failed';
        state.textContent = error.message;
      }
    });
  }
}

// --------------------------------------------------------------------------------------------- wiring
function wire() {
  $('#search').addEventListener('input', renderLibrary);
  $('#library').addEventListener('click', (e) => { const b = e.target.closest('.book-item'); if (b) openBook(b.dataset.id); });
  $('#sliders').addEventListener('input', (e) => { if (e.target.dataset.key) setSettings({ [e.target.dataset.key]: e.target.value }); });
  document.querySelector('.presets').addEventListener('click', (e) => {
    const preset = e.target.dataset.preset;
    if (preset === 'default') setSettings(DEFAULTS);
    if (preset === 'reader' && state.reader) setSettings(state.reader.settings);
    if (preset === 'flat') setSettings({ longWordDelayMs: 0, complexWordDelayMs: 0, punctuationDelayMs: 0 });
  });
  $('#device-read').addEventListener('click', readDevice);
  const drop = $('#drop');
  $('#drop-input').addEventListener('change', (e) => { addBooks(e.target.files); e.target.value = ''; });
  drop.addEventListener('dragover', (e) => { e.preventDefault(); drop.classList.add('over'); });
  drop.addEventListener('dragleave', () => drop.classList.remove('over'));
  drop.addEventListener('drop', (e) => { e.preventDefault(); drop.classList.remove('over'); addBooks(e.dataTransfer.files); });
  $('#trim-list').addEventListener('change', (e) => {
    const i = e.target.dataset.section;
    if (i === undefined) return;
    state.trim.keep[Number(i)] = e.target.checked;
    renderTrim();
    renderTimeline();
  });
  $('#trim-card').addEventListener('click', () => saveTrim('card'));
  $('#trim-downloads').addEventListener('click', () => saveTrim('downloads'));
  $('#device-write').addEventListener('click', writeDevice);
  $('#play').addEventListener('click', () => (state.playing ? stop() : play()));
  $('#screen').addEventListener('click', () => (state.playing ? stop() : play()));
  $('#back').addEventListener('click', previousSentence);
  $('#forward').addEventListener('click', nextSentence);
  $('#scrub').addEventListener('input', (e) => { stop(); seek(Number(e.target.value)); });
  $('#inspect-input').addEventListener('input', renderInspector);
  for (const list of ['#slowest', '#chapters']) {
    $(list).addEventListener('click', (e) => { const row = e.target.closest('[data-pos]'); if (row) { stop(); seek(Number(row.dataset.pos)); } });
  }
  const canvas = $('#timeline'), tip = $('#tooltip');
  canvas.addEventListener('mousemove', (e) => {
    if (!state.analysis) return;
    const bucket = bucketAt(e), chapter = chapterAt(bucket.start);
    tip.hidden = false;
    tip.style.left = `${e.offsetX}px`;
    tip.innerHTML = `${escapeHtml(chapter >= 0 ? state.book.chapters[chapter].title : 'Start')}<br><b>${fmt(bucket.wpm)}</b> <span class="muted">effective wpm</span>`;
  });
  canvas.addEventListener('mouseleave', () => { tip.hidden = true; });
  canvas.addEventListener('click', (e) => { if (state.analysis) { stop(); seek(bucketAt(e).start); } });
  document.addEventListener('keydown', (e) => {
    if (!state.book || e.target.matches('input[type=search], textarea')) return;
    if (e.key === ' ') { e.preventDefault(); state.playing ? stop() : play(); }
    else if (e.key === 'ArrowRight') { stop(); seek(state.pos + 1); }
    else if (e.key === 'ArrowLeft') { stop(); seek(state.pos - 1); }
    else if (e.key === ']') { stop(); nextSentence(); }
    else if (e.key === '[') { stop(); previousSentence(); }
  });
  new ResizeObserver(() => { if (state.analysis) { renderTimeline(); showWord(); } }).observe($('main'));
}

renderSliders();
wire();
loadLibrary();
