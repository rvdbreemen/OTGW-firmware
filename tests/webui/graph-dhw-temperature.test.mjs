// TASK-1776 (Robert's option B, DM 1571431149795024929): Monitor > Graph and its CSV export show the DHW
// temperature (MsgID 26, Tdhw) as a live-only series. The browser already decodes MsgID 26 into model.dhw
// (applyFrame); the graph keeps it per 5 s slot from page load. The device graph history (ADR-193, 14-byte
// slot) is not changed, so after a reload the earlier part of the window has no DHW value: no backfill.
//
// Runs the REAL shipped f88(), applyFrame(), rawFromLine(), graphRoomLost(), pushGraphSlot(), graphKeys(),
// gVal(), windowedSamples() and exportCsv(), plus the GSER series table, cut out of data/v2.js and
// evaluated against stubs. Date.now() is a fake clock so slots are deterministic.
// Point OTGW_V2_JS at an older copy of v2.js (git show <rev>:src/OTGW-firmware/data/v2.js > file) to see it red.
//
// Not covered: the browser rendering of the line, and a real boiler (the bench check is a screenshot).
//
// Run: node --test tests/webui/graph-dhw-temperature.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const V2JS = process.env.OTGW_V2_JS || path.join(ROOT, 'src', 'OTGW-firmware', 'data', 'v2.js');
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

// The series table: 'var GSER = [ ... ];' up to its closing '];'.
function extractGser(src) {
  const a = src.indexOf('var GSER = ');
  if (a < 0) throw new Error('GSER not found in ' + V2JS);
  return src.slice(a, src.indexOf('];', a) + 2);
}

function load() {
  const body =
    'var clock = 0, fakeDate = { now: function () { return clock; } };\n' +
    'var model = { flow: null, ret: null, dhw: null, outside: null, room: null, roomSet: null, chSet: null,' +
    ' mod: null, pressure: null, flame: false, ch: false, dhw_on: false, fault: false, fresh: false, wxValid: false };\n' +
    'var samples = [], SAMPLES_MAX = 1800, GSLOT = 5000, GHIST_MAX = 86400000 / GSLOT, gHist = [], gDallas = null, gLastRender = 0;\n' +
    'var GHIST_LIVE_MS = 86400000, GHIST_SPAN_MS = 7 * 86400000, gTrimN = 0;\n' +
    'var sat = { roomLost: false }, satEnabledLive = null, GROOM_STALE_MS = 270000, graphWindowMs = 3600000, csvOut = null;\n' +
    'function scheduleRender() {}\nfunction isMonitorVisible() { return false; }\nfunction renderGraph() {}\n' +
    'function ts() { return "T"; }\nfunction downloadBlob(blob, name) { csvOut = { blob: blob, name: name }; }\n' +
    extractGser(SRC) + '\n' +
    [extractFunction(SRC, 'f88'), extractFunction(SRC, 'applyFrame'), extractFunction(SRC, 'rawFromLine'),
     extractFunction(SRC, 'pushSample'), extractFunction(SRC, 'graphRoomLost'), extractFunction(SRC, 'pushGraphSlot'), extractFunction(SRC, 'graphTrim'),
     extractFunction(SRC, 'windowedSamples'), extractFunction(SRC, 'graphKeys'), extractFunction(SRC, 'gVal'),
     extractFunction(SRC, 'exportCsv')]
      .join('\n').replace(/\bDate\.now\b/g, 'fakeDate.now') + '\n' +
    'return { model: model, gHist: function () { return gHist; }, setHist: function (h) { gHist = h; },' +
    ' setClock: function (t) { clock = t; }, keys: function () { return graphKeys(gHist); },' +
    ' feed: function (line) { var raw = rawFromLine(line); if (raw) applyFrame(raw); return raw; },' +
    ' slot: function () { pushGraphSlot(); }, csv: function () { exportCsv(); return csvOut; } };';
  return new Function(body)();
}

// A WS log line as the firmware sends it: timestamp, 18-char source field, space, frame.
const wsLine = (frame, ts = '11:00:00.000000') => ts + ' ' + 'Boiler'.padEnd(18) + ' ' + frame + '   1 Read-Ack';
// MsgID 26 Read-Ack with Tdhw = 58.00 °C (f8.8 0x3A00); parity bit clear (8 bits set).
const DHW58 = 'B401A3A00';
const BASE = 1_700_000_000_000 - (1_700_000_000_000 % 5000);   // slot aligned

test('a) a boiler MsgID 26 answer lands in the live graph slot as dhw', () => {
  const h = load();
  h.setClock(BASE);
  assert.ok(h.feed(wsLine(DHW58)), 'the line parses');
  assert.equal(h.model.dhw, 58, 'applyFrame decodes MsgID 26 (unchanged, already shipped)');
  h.slot();
  assert.equal(h.gHist().length, 1);
  assert.equal(h.gHist()[0].dhw, 58, 'the graph slot carries the DHW temperature');
});

test('b) the series table has a DHW temperature line in the temperature panel', () => {
  const h = load();
  assert.ok(h.keys().includes('dhw'), 'graphKeys() includes dhw, so the graph draws it and the CSV exports it');
  const m = SRC.match(/\['dhw', '([^']+)', '(#[0-9a-f]{6})', '(\w)'\]/);
  assert.ok(m, 'GSER entry for dhw');
  assert.equal(m[3], 't', 'drawn on the temperature panel');
  assert.match(SRC, /dhw: 'graph\.series_dhw'/, 'the label is translated');
});

test('c) CSV: a dhw column, empty in a slot without MsgID 26 and in a device-history slot (no backfill)', async () => {
  const h = load();
  // A device-history record from before the page load: no dhw field (ADR-193 slot unchanged).
  const dev = { t: BASE - 10000, h: 1, span: 5000, flow: 50, ret: 40, flame: 1, dhw_on: 0, ch: 1 };
  h.setHist([dev]);
  h.setClock(BASE);       // a live slot before any MsgID 26 arrived
  h.slot();
  h.setClock(BASE + 5000);
  h.feed(wsLine(DHW58));
  h.slot();
  const out = h.csv();
  const lines = (await out.blob.text()).split('\n');
  const head = lines[0].split(','), col = head.indexOf('dhw');
  assert.ok(col > 0, 'the CSV header has a dhw column: ' + lines[0]);
  assert.deepEqual(lines.slice(1).map((l) => l.split(',')[col]), ['', '', '58'],
    'device slot empty (no backfill), live slot before MsgID 26 empty, then 58');
});

test('d) every language pack translates graph.series_dhw', () => {
  for (const lang of ['nl', 'de', 'el']) {
    const pack = JSON.parse(fs.readFileSync(path.join(ROOT, 'i18n', lang + '.json'), 'utf8'));
    const v = pack.graph && pack.graph.series_dhw;
    assert.ok(typeof v === 'string' && v.length > 0, lang + ': graph.series_dhw present');
  }
  assert.match(SRC, /"series_dhw": "DHW temperature"/, 'English default built into v2.js');
});
