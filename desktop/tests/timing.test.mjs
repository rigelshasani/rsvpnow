// timing.js must reproduce the firmware's per-word durations for every golden token and setting variant.
// Run from desktop/: node --test 'tests/*.test.mjs'
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { scoreWords, durations, focusIndex } from '../rigels_studio/web/timing.js';

const golden = JSON.parse(readFileSync(new URL('./golden/golden.json', import.meta.url), 'utf8'));
const words = golden.tokens.map((token) => token.t);
const scores = scoreWords(words);

for (const [v, variant] of golden.variants.entries()) {
  test(`durations match firmware at ${JSON.stringify(variant)}`, () => {
    const ours = durations(scores, variant);
    for (let i = 0; i < words.length; ++i)
      assert.equal(ours[i], golden.tokens[i].ms[v], `word ${i} ${JSON.stringify(words[i])}`);
  });
}

test('focus letter follows the reader', () => {
  assert.equal(focusIndex('a'), 0);
  assert.equal(focusIndex('word'), 1);
  assert.equal(focusIndex('reading'), 2);
  assert.equal(focusIndex('"Quoted."'), 3); // skips the quote: Q(1) u(2) o(3)
  assert.equal(focusIndex('extraordinary'), 3);
  assert.equal(focusIndex('incomprehensibilities'), 4);
  assert.equal(focusIndex('—'), 0);
  assert.equal(focusIndex(''), -1);
});
