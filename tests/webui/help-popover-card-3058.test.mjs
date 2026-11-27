// TASK-3058 guard: the (i) help popover of a setting may cover its own card but never the card next to it.
// helpPlace() (data/v2.js) used to clamp the popover to the viewport only, so an icon near the right edge of a
// card (satroomsource, bench screenshot S22) put the 320 px popover over the labels of the neighbouring card.
// Now the popover is clamped to the .set-group card of its icon; an icon outside any card keeps the viewport clamp.
//
// Runs the REAL helpPlace() and hideHelp() cut out of data/v2.js against a small geometry stub (rectangles, no layout
// engine). Not covered: the real layout at 390 and 1400 px (needs a browser; the bench screenshots are the evidence).
//
// Red run: OTGW_V2_JS=<git show origin/dev:src/OTGW-firmware/data/v2.js saved to a file>.
// Run: node --test tests/webui/help-popover-card-3058.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const V2JS = process.env.OTGW_V2_JS || path.join(DATA, 'v2.js');
const SRC = fs.readFileSync(V2JS, 'utf8');

function fn(name) {
  const start = SRC.indexOf('function ' + name + '(');
  assert.ok(start >= 0, name + ' not found in ' + V2JS);
  let i = SRC.indexOf('{', start), depth = 0;
  for (; i < SRC.length; i++) {
    const c = SRC[i];
    if (c === "'" || c === '"') { for (i++; i < SRC.length && SRC[i] !== c; i++) if (SRC[i] === '\\') i++; }
    else if (c === '{') depth++; else if (c === '}' && --depth === 0) return SRC.slice(start, i + 1);
  }
  throw new Error('unbalanced');
}

// A popover of natural width 320 and height 60; like a fixed box it shrinks to its max-width when one is set.
function place({ vw = 1400, vh = 900, btn, card }) {
  const pop = {
    style: {}, classList: { remove() {} },
    get offsetWidth() { const mw = parseInt(this.style.maxWidth, 10); return Number.isFinite(mw) ? Math.min(320, mw) : 320; },
    get offsetHeight() { return 60; },
  };
  const btnEl = {
    isConnected: true, setAttribute() {}, removeAttribute() {},
    getBoundingClientRect: () => btn,
    closest: (sel) => (sel === '.set-group' ? (card ? { getBoundingClientRect: () => card } : null) : null),
  };
  const body = 'var helpPop = P, helpBtn = B, helpPinned = false, helpOverPop = false;\n' + fn('hideHelp') + '\n' + fn('helpPlace') + '\nhelpPlace(); return null;';
  new Function('P', 'B', 'document', body)(pop, btnEl, { documentElement: { clientWidth: vw, clientHeight: vh } });
  const left = parseInt(pop.style.left, 10), width = pop.offsetWidth;
  return { left, right: left + width, top: parseInt(pop.style.top, 10), width, pop };
}
const rect = (left, top, w, h) => ({ left, top, right: left + w, bottom: top + h });

test('an icon near the right edge of its card: the popover stays inside that card', () => {
  const card = rect(500, 80, 400, 300);           // the card, 500..900
  const r = place({ btn: rect(860, 100, 16, 16), card });
  assert.ok(r.left >= card.left, 'popover starts left of its card: ' + r.left + ' < ' + card.left);
  assert.ok(r.right <= card.right, 'popover ends right of its card (over the next card): ' + r.right + ' > ' + card.right);
});

test('a card narrower than the popover: the popover shrinks to the card, never crosses either side', () => {
  const card = rect(500, 80, 240, 300);           // 240 px card, a 320 px popover does not fit
  const r = place({ btn: rect(520, 100, 16, 16), card });
  assert.ok(r.left >= card.left, 'left edge ' + r.left);
  assert.ok(r.right <= card.right, 'right edge ' + r.right);
  assert.ok(r.width >= 160, 'a popover narrower than 160 px is unreadable: ' + r.width);
});

test('an icon with room inside its card keeps its place under the icon', () => {
  const card = rect(100, 80, 600, 300);
  const r = place({ btn: rect(140, 100, 16, 16), card });
  assert.equal(r.left, 140);
});

test('an icon outside any card keeps the viewport clamp of before', () => {
  const r = place({ vw: 390, btn: rect(360, 100, 16, 16), card: null });
  assert.equal(r.left, 390 - 320 - 8, 'ends 8 px before the viewport edge');
});

test('phone width: a card as wide as the viewport keeps the popover inside it', () => {
  const card = rect(8, 80, 374, 300);
  const r = place({ vw: 390, btn: rect(350, 100, 16, 16), card });
  assert.ok(r.left >= card.left && r.right <= card.right, r.left + '..' + r.right);
});
