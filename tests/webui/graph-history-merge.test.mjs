// TASK-1296 guard: the Monitor > Graph merges the device history
// (GET /api/v2/graph/history) into the live samples by slot time.
//
// Runs the REAL shipped mergeGraphHistory(), cut out of data/v2.js by brace matching
// and evaluated with GSLOT / GHIST_MAX as v2.js defines them. The old v2.js has no such
// function, so extraction fails there: point OTGW_V2_JS at an older copy of v2.js
// (git show HEAD:... > file) to see it red.
//
// Timestamp convention under test: a device slot is stamped at the START of its interval,
// like a live record: x = now - newest_age_ms - (count - i) * interval, snapped to GSLOT.
//
// Not covered: the fetch/state handling in loadGraphHistory() and the browser rendering
// (bench check with a screenshot), and the device side (tests/test_graph_history.cpp).
//
// Run: node --test tests/webui/graph-history-merge.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const V2JS = process.env.OTGW_V2_JS || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data', 'v2.js');
const SRC = fs.readFileSync(V2JS, 'utf8');

function extractFunction(src, name) {
  const start = src.indexOf('function ' + name + '(');
  if (start < 0) throw new Error(name + ' not found in ' + V2JS);
  let i = src.indexOf('{', start), depth = 0;
  for (; i < src.length; i++) {
    const c = src[i];
    if (c === "'" || c === '"' || c === '`') {
      for (i++; i < src.length && src[i] !== c; i++) if (src[i] === '\\') i++;
    } else if (c === '/' && src[i + 1] === '/') {
      i = src.indexOf('\n', i);
    } else if (c === '/' && src[i + 1] === '*') {
      i = src.indexOf('*/', i) + 1;
    } else if (c === '{') depth++;
    else if (c === '}' && --depth === 0) return src.slice(start, i + 1);
  }
  throw new Error('unbalanced braces in ' + name);
}

const GSLOT = 5000;
const GHIST_MAX = 86400000 / GSLOT;
// GBREAK_MS is read from v2.js itself, so the seam test follows the shipped value.
const m = SRC.match(/\bGBREAK_MS\s*=\s*(\d+)/);
const GBREAK_MS = m ? Number(m[1]) : undefined;
const merge = new Function('GSLOT', 'GHIST_MAX',
  extractFunction(SRC, 'mergeGraphHistory') + '\nreturn mergeGraphHistory;')(GSLOT, GHIST_MAX);

const NOW = 1_800_000_000_000;   // a multiple of 5000
const IV = 120000;

function col(n, f) { const a = []; for (let i = 0; i < n; i++) a.push(f(i)); return a; }
// n slots, 120 s apart, newest slot ENDED 30 s ago. Every column has distinct per-slot values.
function body(n, over) {
  return Object.assign({
    interval_s: 120, newest_age_ms: 30000, count: n, scale: 10,
    flow: col(n, i => 600 + i), ret: col(n, i => 300 + 2 * i), chSet: col(n, i => 700 + 3 * i),
    roomSet: col(n, i => 200 + 4 * i), room: col(n, i => 190 + 5 * i), outside: col(n, i => -50 + 7 * i),
    mod: col(n, i => 10 + i), flame: col(n, i => i % 2), dhw_on: col(n, i => (i % 3 === 0 ? 1 : 0)), ch: col(n, i => (i % 2 === 0 ? 1 : 0))
  }, over || {});
}
function live(t, extra) { return Object.assign({ t: t, flow: 55, ret: 30, chSet: 70, roomSet: 20, room: 19.5, outside: 3, mod: 10, flame: 0, dhw_on: 0, ch: 1, d: null }, extra || {}); }
function assertSortedUnique(a) {
  for (let i = 1; i < a.length; i++) assert.ok(a[i].t > a[i - 1].t, 'sorted ascending, unique t at ' + i);
}
const floor5 = x => x - x % GSLOT;

test('empty live history: slots are stamped at the START of their interval', () => {
  const r = merge([], body(5), NOW);
  assert.equal(r.length, 5);
  assertSortedUnique(r);
  assert.equal(r[4].t, NOW - 30000 - IV, 'newest slot ended 30 s ago, started 150 s ago');
  assert.equal(r[0].t, NOW - 30000 - 5 * IV);
});

test('all ten fields map from their own column, scaled, flags 0/1, d null', () => {
  const r = merge([], body(4), NOW);
  const s = r[2];   // i = 2
  assert.equal(s.flow, 60.2);
  assert.equal(s.ret, 30.4);
  assert.equal(s.chSet, 70.6);
  assert.equal(s.roomSet, 20.8);
  assert.equal(s.room, 20);
  assert.equal(s.outside, -3.6);
  assert.equal(s.mod, 12);
  assert.equal(s.flame, 0);
  assert.equal(s.dhw_on, 0);
  assert.equal(s.ch, 1);
  assert.equal(s.d, null);
  assert.equal(r[3].flame, 1);
  assert.equal(r[3].dhw_on, 1);
  assert.equal(r[3].ch, 0);
  assert.equal(r[1].mod, 11);
});

test('scale 100 divides by 100', () => {
  const r = merge([], body(2, { scale: 100, flow: [6550, 6612] }), NOW);
  assert.equal(r[0].flow, 65.5);
  assert.equal(r[1].flow, 66.12);
});

test('null stays null, also for mod; a slot with some data is kept', () => {
  const j = body(3, { flow: [null, 600, null], mod: [null, 7, null], outside: [null, null, null] });
  const r = merge([], j, NOW);
  assert.equal(r.length, 3);
  assert.equal(r[0].flow, null);
  assert.equal(r[1].flow, 60);
  assert.equal(r[0].mod, null);
  assert.equal(r[1].mod, 7);
  assert.equal(r[2].outside, null);
});

test('a non-grid newest_age_ms is floored to the 5 s grid', () => {
  const r = merge([], body(3, { newest_age_ms: 31234 }), NOW);
  const x2 = NOW - 31234 - IV;
  assert.equal(r[2].t, floor5(x2));
  assert.notEqual(x2 % GSLOT, 0, 'the raw start is off-grid, so the floor matters');
  assert.equal(r[1].t, floor5(x2 - IV));
  assert.equal(r[0].t, floor5(x2 - 2 * IV));
});

test('a device slot that snaps exactly onto a live t: live wins, t stays unique', () => {
  const tSlot = NOW - 30000 - IV;                 // where the newest device slot starts
  const hist = [live(tSlot, { flow: 99 })];
  const r = merge(hist, body(3), NOW);
  assertSortedUnique(r);
  assert.equal(r.filter(s => s.t === tSlot).length, 1);
  assert.equal(r.find(s => s.t === tSlot).flow, 99);
  assert.equal(r.length, 3, 'two older device slots plus the live record');
});

test('a device slot is dropped when any live record falls inside its interval', () => {
  const tNewest = NOW - 30000 - IV;
  const hist = [live(tNewest + 60000)];            // inside the newest slot
  const r = merge(hist, body(3), NOW);
  assertSortedUnique(r);
  assert.equal(r.length, 3);
  assert.equal(r[2].t, tNewest + 60000);
  assert.ok(!r.some(s => s.h && s.t === tNewest), 'overlapped device slot is gone');
  assert.ok(r[0].h && r[1].h && !r[2].h);
});

test('device records carry span = interval in ms; live records have none', () => {
  const r = merge([live(NOW)], body(2), NOW);
  assert.ok(r.filter(s => s.h).every(s => s.span === IV));
  assert.equal(r.find(s => !s.h).span, undefined);
  const r30 = merge([], body(2, { interval_s: 30, newest_age_ms: 0 }), NOW);
  assert.ok(r30.every(s => s.span === 30000));
});

test('120 s slots: the history-to-live seam (end of the last device slot to the first live record) never exceeds GBREAK_MS', () => {
  // Live records every 5 s from page load P to NOW; sweep P and newest_age_ms over the slot.
  // renderGraph breaks where next.t - (prev.t + prev.span) > GBREAK_MS, so that is the measure.
  let worst = 0, cases = 0;
  for (let age = 0; age < IV; age += GSLOT) {
    for (let back = 5000; back <= 600000; back += GSLOT) {
      const P = NOW - back, hist = [];
      for (let t = P; t <= NOW; t += GSLOT) hist.push(live(t));
      const r = merge(hist, body(30, { newest_age_ms: age }), NOW);
      const firstLive = r.findIndex(s => !s.h);
      if (firstLive > 0) worst = Math.max(worst, r[firstLive].t - (r[firstLive - 1].t + r[firstLive - 1].span));
      cases++;
    }
  }
  assert.ok(cases > 1000);
  assert.ok(worst < GBREAK_MS, 'worst seam ' + worst + ' ms must stay under GBREAK_MS ' + GBREAK_MS);
});

test('a hole inside the live range is filled from the device history', () => {
  // live records every 5 s except a hole of ~10 minutes
  const hole0 = NOW - 1800000, hole1 = NOW - 1200000;
  const hist = [];
  for (let t = NOW - 3600000; t < hole0; t += GSLOT) hist.push(live(t));
  for (let t = hole1; t <= NOW; t += GSLOT) hist.push(live(t));
  const before = hist.length;
  const r = merge(hist, body(30, { newest_age_ms: 0 }), NOW);
  assertSortedUnique(r);
  const filled = r.filter(s => s.h && s.t >= hole0 && s.t < hole1);
  assert.ok(filled.length >= 4 && filled.length <= 6, 'about 600 s / 120 s slots land in the hole, got ' + filled.length);
  assert.ok(r.length > before);
  assert.equal(r.filter(s => !s.h).length, before, 'live records untouched');
});

test('pure gap slots (all null, no flag) are dropped; a flag-only slot is kept', () => {
  const nul = n => col(n, () => null);
  const j = body(4, {
    flow: nul(4), ret: nul(4), chSet: nul(4), roomSet: nul(4), room: nul(4), outside: nul(4), mod: nul(4),
    flame: [0, 1, 0, 0], dhw_on: [0, 0, 0, 0], ch: [0, 0, 0, 0]
  });
  const r = merge([], j, NOW);
  assert.equal(r.length, 1);
  assert.equal(r[0].flame, 1);
  assert.equal(r[0].t, floor5(NOW - 30000 - 3 * IV));
});

test('a reload replaces the earlier history instead of duplicating it', () => {
  const live1 = [live(NOW - 20000), live(NOW - 15000)];
  const first = merge(live1, body(10), NOW);
  const again = merge(first, body(10, { newest_age_ms: 30000 }), NOW + 600000 + 25000 - 0);
  assertSortedUnique(again);
  assert.equal(again.filter(s => !s.h).length, 2);
  assert.ok(again.filter(s => s.h).length <= 10);
});

test('result is trimmed to GHIST_MAX, keeping the newest', () => {
  const hist = col(GHIST_MAX - 50, i => live(NOW + i * GSLOT));       // 50 below the cap
  const r = merge(hist, body(120, { newest_age_ms: 10000 }), NOW);
  assert.equal(r.length, GHIST_MAX, 'capped');
  assert.equal(r[r.length - 1].t, hist[hist.length - 1].t, 'newest live record kept');
  assertSortedUnique(r);
});

test('does not mutate its inputs', () => {
  const hist = [live(NOW - 200000)];
  const snapshot = JSON.stringify(hist);
  const j = body(4);
  const jsnap = JSON.stringify(j);
  merge(hist, j, NOW);
  assert.equal(JSON.stringify(hist), snapshot);
  assert.equal(JSON.stringify(j), jsnap);
});

test('malformed or empty payloads return the history unchanged', () => {
  const hist = [live(NOW)];
  const bad = [
    null, undefined, 'x', 5, {}, body(0),
    body(3, { count: '3' }),
    body(3, { interval_s: 0 }),
    body(3, { scale: 0 }),
    body(3, { newest_age_ms: -1 }),
    body(3, { newest_age_ms: 'a' }),
    body(3, { flow: [1, 2] }),
    body(3, { flame: null }),
    body(3, { mod: 'abc' })
  ];
  for (const j of bad) assert.equal(merge(hist, j, NOW), hist);
});

test('count 0 from a device that has no slot yet leaves the history alone', () => {
  const hist = [];
  assert.equal(merge(hist, body(0, { newest_age_ms: 0 }), NOW), hist);
});

test('interval_s 30 (no-PSRAM ring): slots are 30 s apart, stamped at their start', () => {
  const r = merge([], body(6, { interval_s: 30, newest_age_ms: 12000 }), NOW);
  assert.equal(r.length, 6);
  assertSortedUnique(r);
  assert.equal(r[5].t, floor5(NOW - 12000 - 30000));
  assert.equal(r[0].t, floor5(NOW - 12000 - 6 * 30000));
  assert.equal(r[5].t - r[4].t, 30000);
});

test('interval_s 30: the covered-by-live window is 30 s wide', () => {
  const tNewest = floor5(NOW - 12000 - 30000);                       // newest slot covers [tNewest, tNewest + 30 s)
  const inside = merge([live(tNewest + 25000)], body(4, { interval_s: 30, newest_age_ms: 12000 }), NOW);
  assert.ok(!inside.some(s => s.h && s.t === tNewest), 'live record inside the slot drops it');
  const outside = merge([live(tNewest + 30000)], body(4, { interval_s: 30, newest_age_ms: 12000 }), NOW);
  assert.ok(outside.some(s => s.h && s.t === tNewest), 'live record just after the slot keeps it');
  assertSortedUnique(inside);
  assertSortedUnique(outside);
});
