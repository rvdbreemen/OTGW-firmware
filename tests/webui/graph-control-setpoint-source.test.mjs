// TASK-1271 guard: the Monitor > Graph "control setpoint" line must show what the
// boiler was sent: MsgID 1 Write-Data (T or R frames), as the firmware does in
// is_value_valid_for_master_topic(). A PIC exchange is T (thermostat's own value),
// R (gateway to boiler, overwrites T), B (boiler ack), A (echo to the thermostat).
// Write-Ack data is undefined (some boilers answer 0), so type 5 frames are ignored.
// The old applyFrame() took every write-type frame, so the A echo (last in the
// exchange) left the line on the thermostat's 10 while the boiler had received 45.
//
// Runs the REAL shipped rawFromLine(), applyFrame(), pushSample(), pushGraphSlot()
// and applyOtmonToModel(), cut out of data/v2.js by brace matching and evaluated
// against stubs. Full WS log lines go through rawFromLine() -> applyFrame() and the
// assertions read the graph history slots (gHist), the series the graph and CSV use.
// Date.now() is driven by a fake clock so slots are deterministic.
// Point OTGW_V2_JS at an older copy of v2.js (git show HEAD:... > file) to see it red.
//
// Not covered: the browser rendering of the graph, and a real device (the replay
// fixture tests/webui/fixtures/msgid1-pic-exchange.txt is for the bench check).
//
// Run: node --test tests/webui/graph-control-setpoint-source.test.mjs
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

// Fresh model + graph history per call; returns the shipped functions bound to it.
function load() {
  const body =
    'var clock = 0, fakeDate = { now: function () { return clock; } };\n' +
    'var model = { flow: null, ret: null, dhw: null, outside: null, room: null, roomSet: null, chSet: null,' +
    ' mod: null, pressure: null, flame: false, ch: false, dhw_on: false, fault: false, fresh: false, wxValid: false };\n' +
    'var samples = [], SAMPLES_MAX = 1800, GSLOT = 5000, GHIST_MAX = 86400000 / GSLOT, gHist = [], gDallas = null, gLastRender = 0;\n' +
    'function scheduleRender() {}\nfunction isMonitorVisible() { return false; }\nfunction renderGraph() {}\n' +
    [extractFunction(SRC, 'f88'), extractFunction(SRC, 'applyFrame'), extractFunction(SRC, 'rawFromLine'),
     extractFunction(SRC, 'pushSample'), extractFunction(SRC, 'pushGraphSlot'), extractFunction(SRC, 'applyOtmonToModel')]
      .join('\n').replace(/\bDate\.now\b/g, 'fakeDate.now') + '\n' +
    'return { model: model, gHist: function () { return gHist; }, setClock: function (t) { clock = t; },' +
    ' feed: function (line) { var raw = rawFromLine(line); if (raw) applyFrame(raw); return raw; },' +
    ' seed: function (o) { applyOtmonToModel(o); } };';
  return new Function(body)();
}

// A WS log line as the firmware sends it: timestamp, 18-char source field, space, frame.
const SRCNAME = { T: 'Thermostat', R: 'Request Boiler', B: 'Boiler', A: 'Answer Thermostat' };
const wsLine = (frame, ts = '11:00:00.000000') => ts + ' ' + SRCNAME[frame[0]].padEnd(18) + ' ' + frame + '   1 Write-Data';

const BASE = 1_700_000_000_000 - (1_700_000_000_000 % 5000);   // slot aligned
const slotValues = (h) => h.gHist().map((r) => r.chSet);

// One exchange per 5 s slot, frames 100 ms apart.
function replayExchanges(h, exchanges, n) {
  for (let i = 0; i < n; i++) {
    exchanges.forEach((fr, k) => { h.setClock(BASE + i * 5000 + k * 100); h.feed(wsLine(fr)); });
  }
}

test('a) PIC exchange T10 R45 B45 A10 repeated: graph slots show 45', () => {
  const h = load();
  replayExchanges(h, ['T10010A00', 'R10012D00', 'BD0012D00', 'AD0010A00'], 6);
  assert.deepEqual(slotValues(h), [45, 45, 45, 45, 45, 45]);
});

test('b) monitor mode T45 / B45: graph slots show 45', () => {
  const h = load();
  replayExchanges(h, ['T10012D00', 'BD0012D00'], 4);
  assert.deepEqual(slotValues(h), [45, 45, 45, 45]);
});

test('c) OT-Direct R45 / B45 shows 45; T10 overridden by R45 shows the R value', () => {
  const h = load();
  replayExchanges(h, ['R10012D00', 'BD0012D00'], 4);
  assert.deepEqual(slotValues(h), [45, 45, 45, 45]);
  const g = load();
  replayExchanges(g, ['T10010A00', 'R10012D00'], 3);
  assert.deepEqual(slotValues(g), [45, 45, 45]);
});

test('c2) a dropped R frame does not bring back the thermostat value; T counts again after 30 s without R', () => {
  const h = load();
  h.setClock(BASE);          h.feed(wsLine('T10010A00'));
  h.setClock(BASE + 100);    h.feed(wsLine('R10012D00'));   // gateway sends 45
  h.setClock(BASE + 10000);  h.feed(wsLine('T10010A00'));   // R of this exchange lost: still 45
  assert.equal(h.model.chSet, 45);
  h.setClock(BASE + 40200);  h.feed(wsLine('T10010A00'));   // no R for > 30 s: gateway no longer overrides
  h.setClock(BASE + 41000);  h.feed(wsLine('BD0010A00'));   // TASK-1522: the T counts once 500 ms passed without R
  assert.equal(h.model.chSet, 10);
});

test('i) TASK-1522 gruijter 09:47: T 29.7 at the end of a slot, R 10.0 just after the boundary: no slot shows 29.7', () => {
  const h = load();
  // Thermostat and SAT agree on 10.0 for > 30 s: T frames only, the PIC sends no R.
  for (let i = 0; i < 8; i++) { h.setClock(BASE + i * 5000); h.feed(wsLine('T10010A00')); h.setClock(BASE + i * 5000 + 900); h.feed(wsLine('BD0010A00')); }
  // The thermostat asks 29.7 at 4.96 s into a slot; the PIC replaces it, the R lands 90 ms later in the next slot.
  const s = BASE + 8 * 5000;
  h.setClock(s + 4960); h.feed(wsLine('T90011DB3'));
  h.setClock(s + 5050); h.feed(wsLine('R10010A00'));
  h.setClock(s + 5060); h.feed(wsLine('BD0010A00'));
  h.setClock(s + 5900); h.feed(wsLine('A50011DB3'));
  assert.ok(slotValues(h).every((v) => v === 10), 'slots: ' + JSON.stringify(slotValues(h)));
});

test('d) non-echo boiler: T/R 27.22 then Write-Ack data 0 keeps 27.22', () => {
  const h = load();
  replayExchanges(h, ['T10011B38', 'R10011B38', 'BD0010000', 'AD0010000'], 3);   // 0x1B38 = 27.21875
  assert.deepEqual(slotValues(h), [27.21875, 27.21875, 27.21875]);
});

test('e) MsgID 16 non-echo: T 20 then Write-Ack data 0 keeps 20', () => {
  const h = load();
  replayExchanges(h, ['T10101400', 'BD0100000'], 3);
  assert.deepEqual(h.gHist().map((r) => r.roomSet), [20, 20, 20]);
});

test('f) REST seed sets chSet when no frames arrive (PS=1 / WS down)', () => {
  const h = load();
  h.seed({ controlsetpoint: { value: '45.00', epoch: 1 } });
  assert.equal(h.model.chSet, 45);
  assert.equal(h.gHist().at(-1).chSet, 45);
});

test('h) boiler answers data-invalid (6) or unknown-dataid (7): chSet unchanged', () => {
  const h = load();
  replayExchanges(h, ['T10012D00', 'BD0012D00'], 1);
  h.setClock(BASE + 5000); h.feed(wsLine('B60010A00'));
  h.setClock(BASE + 5100); h.feed(wsLine('B70010A00'));
  assert.equal(h.model.chSet, 45);
});

test('graph legend labels MsgID 16 as the thermostat setpoint', () => {
  assert.match(SRC, /\['roomSet', 'thermostat setpoint'/);
});

test('g) field capture 1138 (PIC Classic + Honeywell): graph follows what the boiler was sent', () => {
  const lines = fs.readFileSync(path.join(HERE, 'fixtures', 'msgid1-field-cap-1138.txt'), 'utf8').split('\n').filter(Boolean);
  assert.ok(lines.length > 300);
  const h = load();
  // Independent expectation per slot: the last accepted Write-Data value, where an R frame always
  // counts and a T frame counts only when no R frame came in the 30 s before it.
  const slotWrite = new Map();
  let lastR = -Infinity;
  for (const line of lines) {
    const m = line.match(/^(\d\d):(\d\d):(\d\d)\.(\d{3})/);
    const t = BASE + ((+m[1] * 60 + +m[2]) * 60 + +m[3]) * 1000 + +m[4];
    h.setClock(t);
    const raw = h.feed(line);
    assert.ok(raw, 'rawFromLine must accept: ' + line);
    if ('TR'.includes(raw[0]) && ((parseInt(raw.substr(1, 2), 16) >> 4) & 7) === 1) {
      if (raw[0] === 'R') lastR = t;
      else if (t - lastR < 30000) continue;
      slotWrite.set(t - (t % 5000), parseInt(raw.substr(5, 4), 16) / 256);
    }
  }
  const slots = h.gHist();
  const first45 = slots.findIndex((r) => r.chSet === 45);
  assert.ok(first45 >= 0, 'a slot with 45 must exist');
  const after = slots.slice(first45);
  const at45 = after.filter((r) => r.chSet === 45).length;
  console.log('field capture 1138: 45 in ' + at45 + ' of ' + after.length + ' slots after the first 45');
  assert.ok(at45 / after.length >= 0.8, '45 in ' + at45 + ' of ' + after.length + ' slots after the first 45');
  for (const r of slots) {
    if (slotWrite.has(r.t)) assert.equal(r.chSet, slotWrite.get(r.t), 'slot ' + r.t);
  }
});
