// TASK-1963 (ADR-245, Robert's option A, DM 1571457707725168736): on a board with PSRAM the device graph history
// holds 7 days; the Monitor graph gets a 7-day window that asks for it (?hours=168, served thinned to 10 min
// slots), keeps the device records of the whole week and its live 5 s records for 24 h (trimmed by time, not by
// count), and shows the chip only when the device's ring holds days (interval_s of 2 min or more).
//
// Runs the REAL shipped graphTrim(), mergeGraphHistory() and loadGraphHistory() cut out of data/v2.js and
// evaluated against stubs (fetch, DOM and clock). Point OTGW_V2_JS at an older copy of v2.js to see it red.
//
// Not covered: the browser rendering of the 7-day window and a real device (bench on .63).
//
// Run: node --test tests/webui/graph-7day-window.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const DATA = path.join(ROOT, 'src', 'OTGW-firmware', 'data');
const V2JS = process.env.OTGW_V2_JS || path.join(DATA, 'v2.js');
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

const DAY = 86400000, GSLOT = 5000;
const NOW = 1_700_000_000_000 - (1_700_000_000_000 % GSLOT);

function load() {
  const body =
    'var clock = 0, fakeDate = { now: function () { return clock; } };\n' +
    'var GSLOT = 5000, GHIST_MAX = 86400000 / GSLOT, GHIST_LIVE_MS = 86400000, GHIST_SPAN_MS = 7 * 86400000, gTrimN = 0;\n' +
    'var gHist = [], graphWindowMs = 3600000, APIGW = "/api/", urls = [], chips = [{ hidden: true }], rendered = 0;\n' +
    'var gHistState = "idle", gHistLoadedAt = 0, GHIST_RELOAD_MS = 600000, GHIST_TIMEOUT_MS = 20000, gHistHours = 0;\n' +
    'var document = { querySelectorAll: function () { return chips; } };\n' +
    'function renderGraph() { rendered++; }\n' +
    'var nextBody = null;\n' +
    'function fetchWithRetry(u) { urls.push(u); var b = nextBody; return Promise.resolve({ status: 200, ok: true, json: function () { return Promise.resolve(b); } }); }\n' +
    [extractFunction(SRC, 'graphTrim'), extractFunction(SRC, 'mergeGraphHistory'), extractFunction(SRC, 'loadGraphHistory')]
      .join('\n').replace(/\bDate\.now\b/g, 'fakeDate.now') + '\n' +
    'return { trim: graphTrim, merge: mergeGraphHistory, setClock: function (t) { clock = t; },' +
    ' setWindow: function (w) { graphWindowMs = w; }, setBody: function (b) { nextBody = b; },' +
    ' setHist: function (h) { gHist = h; }, hist: function () { return gHist; }, urls: urls, chips: chips,' +
    ' state: function () { return gHistState; }, loadNow: function () { gHistState = "idle"; loadGraphHistory(); } };';
  return new Function(body)();
}

// A device response: n slots of iv seconds, the newest ending ageMs before the request.
function deviceBody(n, iv, ageMs = 0) {
  const col = (v) => Array.from({ length: n }, () => v);
  return { interval_s: iv, newest_age_ms: ageMs, count: n, scale: 10, flow: col(500), ret: col(400), chSet: col(null),
           roomSet: col(null), room: col(null), outside: col(null), mod: col(20), flame: col(1), dhw_on: col(0), ch: col(1) };
}
const liveDay = () => Array.from({ length: DAY / GSLOT }, (_, i) => ({ t: NOW - DAY + (i + 1) * GSLOT, flow: 50, flame: 0, dhw_on: 0, ch: 0 }));

test('a) graphTrim keeps device records for 7 days and live records for 24 h', () => {
  const h = load();
  const arr = [{ t: NOW - 8 * DAY, h: 1 }, { t: NOW - 6 * DAY, h: 1 }, { t: NOW - 2 * DAY }, { t: NOW - 1000 }];
  assert.deepEqual(h.trim(arr, NOW).map((r) => r.t), [NOW - 6 * DAY, NOW - 1000]);
});

test('b) a 7-day device response survives a full day of live 5 s records in the merge', () => {
  const h = load();
  const merged = h.merge(liveDay(), deviceBody(1008, 600), NOW);
  const oldest = merged[0];
  assert.ok(oldest.h === 1 && NOW - oldest.t > 6.9 * DAY, 'the oldest record is a device record about 7 days back');
  assert.ok(merged.filter((r) => r.h).length > 800, 'the device records older than the live day are kept');
});

test('c) the 7-day window asks for ?hours=168; the default window does not', async () => {
  const h = load();
  h.setClock(NOW);
  h.setBody(deviceBody(720, 120));
  h.loadNow();
  await new Promise((r) => setTimeout(r, 0));
  h.setWindow(7 * DAY);
  h.loadNow();
  await new Promise((r) => setTimeout(r, 0));
  assert.deepEqual(h.urls, ['/api/v2/graph/history', '/api/v2/graph/history?hours=168']);
});

test('d) the 7-day chip is shown for a 2 min ring and hidden for the 30 s ring (no PSRAM)', async () => {
  const h = load();
  h.setClock(NOW);
  h.setBody(deviceBody(120, 30));
  h.loadNow();
  await new Promise((r) => setTimeout(r, 0));
  assert.equal(h.chips[0].hidden, true, 'no PSRAM (30 s slots): hidden');
  h.setBody(deviceBody(720, 120));
  h.loadNow();
  await new Promise((r) => setTimeout(r, 0));
  assert.equal(h.chips[0].hidden, false, 'PSRAM (2 min slots): shown');
});

test('e) the chip, its label in every pack and the hidden rule ship', () => {
  const html = fs.readFileSync(path.join(DATA, 'v2.html'), 'utf8');
  assert.match(html, /<span class="tchip" data-win-ms="604800000" data-i18n="h_mon\.graph_win_7d" data-psram-only hidden>/);
  assert.match(fs.readFileSync(path.join(DATA, 'v2.css'), 'utf8'), /\.tchip\[hidden\]\{display:none\}/,
    '.tchip sets display:inline-flex, so the hidden attribute needs its own rule');
  assert.match(SRC, /"graph_win_7d": "7 days"/);
  for (const lang of ['nl', 'de', 'el']) {
    const pack = JSON.parse(fs.readFileSync(path.join(ROOT, 'i18n', lang + '.json'), 'utf8'));
    assert.ok(pack.h_mon && pack.h_mon.graph_win_7d, lang + ': h_mon.graph_win_7d');
  }
});
