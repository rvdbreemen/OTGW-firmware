// TASK-2018 (ADR-256, Robert's option A, DM 1572585729098383494): Monitor > Graph and its CSV export show the SAT
// target as its own series while SAT is enabled. The device graph history carries it as the optional satTarget
// column (16-byte slot); the browser's own 5 s slots take it from the SAT status it already polls, with the
// firmware's rule (only while SAT is enabled). A history response without the column (a window without SAT, or
// firmware before ADR-256) must still load: absent means empty.
//
// Runs the REAL shipped pushGraphSlot(), graphRoomLost(), graphTrim(), mergeGraphHistory(), windowedSamples(),
// graphKeys(), gVal() and exportCsv(), plus the GSER series table and GSER_LABEL_KEY, cut out of data/v2.js and
// evaluated against stubs. Point OTGW_V2_JS at an older copy of v2.js to see it red.
//
// Not covered: the drawn line in a browser (bench screenshot) and the firmware sampler (tests/test_graph_history.py).
//
// Run: node --test tests/webui/graph-sat-target.test.mjs
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
function extractVar(src, name, end) {
  const a = src.indexOf('var ' + name + ' = ');
  if (a < 0) throw new Error(name + ' not found in ' + V2JS);
  return src.slice(a, src.indexOf(end, a) + end.length);
}

function load() {
  const body =
    'var clock = 0, fakeDate = { now: function () { return clock; } };\n' +
    'var model = { flow: 50, ret: 35, dhw: null, outside: 8, room: 20.1, roomAt: 0, roomSet: 19.0, chSet: 45,' +
    ' mod: 30, pressure: null, flame: false, ch: false, dhw_on: false };\n' +
    'var GSLOT = 5000, gHist = [], gDallas = null, gLastRender = 0, gTrimN = 0;\n' +
    'var GHIST_LIVE_MS = 86400000, GHIST_SPAN_MS = 7 * 86400000;\n' +
    'var sat = { roomLost: false, target: null }, satEnabledLive = null, GROOM_STALE_MS = 270000, graphWindowMs = 3600000, csvOut = null;\n' +
    'function isMonitorVisible() { return false; }\nfunction renderGraph() {}\n' +
    'function ts() { return "T"; }\nfunction downloadBlob(blob, name) { csvOut = { blob: blob, name: name }; }\n' +
    extractVar(SRC, 'GSER', '];') + '\n' + extractVar(SRC, 'GSER_LABEL_KEY', '};') + '\n' +
    [extractFunction(SRC, 'graphRoomLost'), extractFunction(SRC, 'pushGraphSlot'), extractFunction(SRC, 'graphTrim'),
     extractFunction(SRC, 'mergeGraphHistory'), extractFunction(SRC, 'windowedSamples'), extractFunction(SRC, 'graphKeys'),
     extractFunction(SRC, 'gVal'), extractFunction(SRC, 'exportCsv')]
      .join('\n').replace(/\bDate\.now\b/g, 'fakeDate.now') + '\n' +
    'return { sat: sat, setEnabled: function (v) { satEnabledLive = v; }, gHist: function () { return gHist; },' +
    ' setHist: function (h) { gHist = h; }, setClock: function (t) { clock = t; }, gser: GSER, labels: GSER_LABEL_KEY,' +
    ' merge: function (h, j, now) { return mergeGraphHistory(h, j, now); }, slot: function () { pushGraphSlot(); },' +
    ' csv: function () { exportCsv(); return csvOut; } };';
  return new Function(body)();
}

const BASE = 1_700_000_000_000 - (1_700_000_000_000 % 5000);
const col = (n, v) => Array.from({ length: n }, () => v);
function history(n, extra) {
  return Object.assign({ interval_s: 120, newest_age_ms: 0, count: n, scale: 10, flow: col(n, 500), ret: col(n, 350),
    chSet: col(n, 450), roomSet: col(n, 190), room: col(n, 201), outside: col(n, 80), mod: col(n, 30),
    flame: col(n, 0), dhw_on: col(n, 0), ch: col(n, 1) }, extra || {});
}

test('the series table has a SAT target temperature series with its own translated label', () => {
  const h = load();
  const s = h.gser.find((x) => x[0] === 'satTarget');
  assert.ok(s, 'no satTarget in GSER');
  assert.equal(s[3], 't');
  assert.equal(h.labels.satTarget, 'graph.series_sat_target');
  assert.match(SRC, /"series_sat_target": "SAT target"/, 'English default label missing');
  assert.ok(h.gser.find((x) => x[0] === 'roomSet' && x[1] === 'thermostat setpoint'), 'roomSet stays the thermostat setpoint');
});

test('a live slot takes the SAT target only while SAT is enabled', () => {
  const h = load();
  h.sat.target = 20.5;
  h.setEnabled(true); h.setClock(BASE); h.slot();
  assert.equal(h.gHist()[0].satTarget, 20.5);
  assert.equal(h.gHist()[0].roomSet, 19.0, 'the thermostat setpoint is still MsgID 16');
  h.setEnabled(false); h.setClock(BASE + 5000); h.slot();
  assert.equal(h.gHist()[1].satTarget, null, 'SAT off: no SAT target');
  h.setEnabled(null); h.setClock(BASE + 10000); h.slot();
  assert.equal(h.gHist()[2].satTarget, null, 'SAT status not known yet: no SAT target');
});

test('a device history with satTarget fills the series, scaled like the other temperatures', () => {
  const h = load();
  const out = h.merge([], history(3, { satTarget: [205, null, 210] }), BASE);
  assert.equal(out.length, 3);
  assert.deepEqual(out.map((r) => r.satTarget), [20.5, null, 21.0]);
  assert.deepEqual(out.map((r) => r.roomSet), [19, 19, 19]);
});

test('a device history without satTarget still loads (window without SAT, or older firmware)', () => {
  const h = load();
  const out = h.merge([], history(3), BASE);
  assert.equal(out.length, 3, 'the history was rejected');
  assert.deepEqual(out.map((r) => r.satTarget), [null, null, null]);
  assert.deepEqual(out.map((r) => r.flow), [50, 50, 50]);
  const bad = h.merge([], history(3, { satTarget: [205] }), BASE);   // wrong length: ignored, not fatal
  assert.equal(bad.length, 3);
  assert.deepEqual(bad.map((r) => r.satTarget), [null, null, null]);
});

test('the CSV export has a satTarget column after roomSet', async () => {
  const h = load();
  h.setHist(h.merge([], history(2, { satTarget: [205, 210] }), BASE));
  h.setClock(BASE);
  const csv = h.csv();
  const text = await csv.blob.text();
  const [head, ...rows] = text.split('\n');
  const cols = head.split(',');
  assert.ok(cols.indexOf('satTarget') === cols.indexOf('roomSet') + 1, head);
  const i = cols.indexOf('satTarget');
  assert.deepEqual(rows.map((r) => r.split(',')[i]), ['20.5', '21']);
});
