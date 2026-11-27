// TASK-3007 (ADR-291, Proposed): Monitor > Graph and its CSV export show the room SAT regulates on as its own
// series, next to the thermostat's room (MsgID 24). The device graph history carries it as the optional satRoom
// column (18-byte slot, tenths, absent = empty); the browser's own 5 s slots take it from the SAT status it already
// polls, with the firmware's rule: only while SAT is enabled and its room is not lost. The CSV column order is
// ...,satTarget,satRoom,room,... (scripts/otgw_capture.py GRAPH_CSV_COLUMNS follows it).
//
// Runs the REAL shipped pushGraphSlot(), graphRoomLost(), graphTrim(), mergeGraphHistory(), windowedSamples(),
// graphKeys(), gVal() and exportCsv(), plus the GSER series table and GSER_LABEL_KEY, cut out of data/v2.js and
// evaluated against stubs (as graph-sat-target.test.mjs). OTGW_V2_JS=<older v2.js> shows it red.
//
// Not covered: the drawn line in a browser (bench screenshot) and the firmware sampler (otgw-dev's lane).
//
// Run: node --test tests/webui/graph-sat-room.test.mjs
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
    'var sat = { roomLost: false, target: null, room: null }, satEnabledLive = null, GROOM_STALE_MS = 270000, graphWindowMs = 3600000, csvOut = null;\n' +
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

test('the series table has a SAT room temperature series right after the SAT target, with its own label', () => {
  const h = load();
  const i = h.gser.findIndex((x) => x[0] === 'satRoom');
  assert.ok(i >= 0, 'no satRoom in GSER');
  assert.equal(h.gser[i][3], 't');
  assert.equal(h.gser[i - 1][0], 'satTarget');
  assert.equal(h.gser[i + 1][0], 'room', 'the thermostat room follows, so the two rooms sit side by side');
  assert.equal(h.labels.satRoom, 'graph.series_sat_room');
  assert.match(SRC, /"series_sat_room": "SAT room"/, 'English default label missing');
  const colours = h.gser.filter((x) => x[3] === 't').map((x) => x[2]);
  assert.equal(new Set(colours).size, colours.length, 'every temperature series has its own colour');
});

test('a live slot takes SAT\'s room only while SAT is enabled and its room is not lost; room stays MsgID 24', () => {
  const h = load();
  h.sat.room = 21.4;
  h.setEnabled(true); h.setClock(BASE); h.slot();
  assert.equal(h.gHist()[0].satRoom, 21.4);
  assert.equal(h.gHist()[0].room, 20.1, 'the thermostat room is still MsgID 24');
  h.sat.roomLost = true; h.setClock(BASE + 5000); h.slot();
  assert.equal(h.gHist()[1].satRoom, null, 'SAT room lost: no value');
  h.sat.roomLost = false; h.setEnabled(false); h.setClock(BASE + 10000); h.slot();
  assert.equal(h.gHist()[2].satRoom, null, 'SAT off: no value');
  h.setEnabled(null); h.setClock(BASE + 15000); h.slot();
  assert.equal(h.gHist()[3].satRoom, null, 'SAT status not known yet: no value');
});

test('a device history with satRoom fills the series; absent or of the wrong length is empty', () => {
  const h = load();
  const out = h.merge([], history(3, { satRoom: [214, null, 216] }), BASE);
  assert.equal(out.length, 3);
  assert.deepEqual(out.map((r) => r.satRoom), [21.4, null, 21.6]);
  assert.deepEqual(out.map((r) => r.room), [20.1, 20.1, 20.1]);
  assert.deepEqual(h.merge([], history(3), BASE).map((r) => r.satRoom), [null, null, null]);
  assert.deepEqual(h.merge([], history(3, { satRoom: [214] }), BASE).map((r) => r.satRoom), [null, null, null]);
});

test('the CSV export has the satRoom column between satTarget and room', async () => {
  const h = load();
  h.setHist(h.merge([], history(2, { satTarget: [205, 210], satRoom: [214, 216] }), BASE));
  h.setClock(BASE);
  const text = await h.csv().blob.text();
  const [head, ...rows] = text.split('\n');
  const cols = head.split(',');
  assert.equal(cols.indexOf('satRoom'), cols.indexOf('satTarget') + 1, head);
  assert.equal(cols.indexOf('room'), cols.indexOf('satRoom') + 1, head);
  const i = cols.indexOf('satRoom');
  assert.deepEqual(rows.map((r) => r.split(',')[i]), ['21.4', '21.6']);
});
