// TASK-1821: Monitor > Graph in the 1.x OTGraph layout: five stacked panels on one time axis
// (flame, DHW, CH as on/off areas, modulation 0-100 %, temperatures in °C), and dashed
// disconnect (red) / reconnect (green) markers that can be switched off (remembered per browser).
// Reads the shipped v2.html and runs the shipped functions cut out of data/v2.js.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const SRC = fs.readFileSync(path.join(DATA, 'v2.js'), 'utf8');
const HTML = fs.readFileSync(path.join(DATA, 'v2.html'), 'utf8');

function balanced(src, start, what) {
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
  throw new Error('unbalanced braces in ' + what);
}
const fn = (n) => {
  const s = SRC.indexOf('function ' + n + '(');
  assert.ok(s >= 0, n + ' not found in v2.js');
  return balanced(SRC, s, n);
};
const varObj = (n) => {
  const s = SRC.indexOf('var ' + n + ' = {');
  assert.ok(s >= 0, 'var ' + n + ' not found in v2.js');
  return balanced(SRC, s, n) + ';';
};
const svg = (() => { const m = /<svg[^>]*id="gstrip"[\s\S]*?<\/svg>/.exec(HTML); assert.ok(m, '#gstrip'); return m[0]; })();

test('five stacked panels top to bottom: flame, DHW, CH, modulation, temperatures', () => {
  const P = new Function(varObj('GPANEL') + '\nreturn GPANEL;')();
  const order = ['flame', 'dhw_on', 'ch', 'mod', 'temp'];
  assert.deepEqual(Object.keys(P), order);
  for (let i = 1; i < order.length; i++) {
    const a = P[order[i - 1]], b = P[order[i]];
    assert.ok(a.top + a.h < b.top, order[i - 1] + ' and ' + order[i] + ' must not overlap');
  }
  assert.ok(P.mod.h > P.flame.h, 'modulation panel taller than an on/off lane');
  assert.ok(P.temp.h > P.mod.h, 'temperature panel is the largest');
  const vbH = +/viewBox="0 0 780 (\d+)"/.exec(svg)[1];
  assert.ok(P.temp.top + P.temp.h + 12 < vbH, 'time labels fit below the temperature panel');
});

test('each panel has a translated title and its own y axis labels', () => {
  const titles = [...svg.matchAll(/class="axis-txt gtitle"[^>]*data-i18n="([^"]+)"/g)].map((m) => m[1]);
  assert.deepEqual(titles, ['h_mon.graph_flame', 'h_mon.graph_dhw', 'h_mon.graph_ch', 'graph.series_mod', 'h_mon.graph_temps']);
  assert.equal((svg.match(/data-i18n="graph\.on"/g) || []).length, 3, 'on label per lane');
  assert.equal((svg.match(/data-i18n="graph\.off"/g) || []).length, 3, 'off label per lane');
  for (const pct of ['100%', '50%', '0%']) assert.ok(svg.includes('>' + pct + '<'), 'modulation axis ' + pct);
  for (const id of ['gAxHi', 'gAxMid', 'gAxLo']) assert.ok(svg.includes('id="' + id + '"'), id);
  // the marker group sits between the time grid and the series
  const iT = svg.indexOf('id="gTime"'), iM = svg.indexOf('id="gMarks"'), iS = svg.indexOf('id="gSer"');
  assert.ok(iT > 0 && iT < iM && iM < iS, 'gTime < gMarks < gSer');
});

test('disconnect markers: a toolbar chip, translated', () => {
  const m = /<span class="tchip[^"]*" id="chipMarks"[^>]*>/.exec(HTML);
  assert.ok(m, '#chipMarks in the graph toolbar');
  assert.match(m[0], /data-i18n="h_mon\.graph_marks"/);
  assert.match(m[0], /data-i18n-title="h_mon\.graph_marks_title"/);
});

test('connection marks: a drop only after an open socket, a return only after a drop', () => {
  const body = 'var gMarks = [], gWsUp = false, gWsDown = false, now = 1000;\n' +
    'Date = { now: function () { return now; } };\n' +
    'var renders = 0; function isMonitorVisible(id) { return id === "mgraph"; } function renderGraph() { renders++; }\n' + fn('noteWsMark') +
    '\nreturn { note: noteWsMark, marks: function () { return gMarks; }, at: function (t) { now = t; }, renders: function () { return renders; } };';
  const h = new Function(body)();
  h.note(false); h.note(false);              // failed connects before the first open: no marks
  assert.deepEqual(h.marks(), []);
  h.at(2000); h.note(true);                  // first open: nothing came back, nothing to mark
  assert.deepEqual(h.marks(), []);
  h.at(3000); h.note(false);                 // drop
  h.at(3500); h.note(false);                 // failed reconnect: same outage
  const r0 = h.renders();
  assert.ok(r0 > 0, 'a visible graph redraws on a drop, also while no data arrives');
  h.at(5000); h.note(true);                  // back
  assert.deepEqual(h.marks(), [{ t: 3000, up: 0 }, { t: 5000, up: 1 }]);
  assert.ok(h.renders() > r0, 'and on the return');
});

test('marks map to x inside the window only', () => {
  const body = 'var C_X0 = 34, C_X1 = 772;\n' + fn('graphMarkLines') + '\nreturn graphMarkLines;';
  const lines = new Function(body)();
  const t0 = 100000, win = 10000;
  const r = lines([{ t: 90000, up: 0 }, { t: 100000, up: 0 }, { t: 105000, up: 1 }, { t: 111000, up: 0 }], t0, win);
  assert.deepEqual(r.map((x) => x.up), [0, 1]);
  assert.equal(r[0].x, 34);
  assert.equal(r[1].x, 34 + (772 - 34) / 2);
});

test('the marker switch is remembered per browser, default on', () => {
  const store = {};
  const ls = { getItem: (k) => (k in store ? store[k] : null), setItem: (k, v) => { store[k] = String(v); } };
  const body = 'var localStorage = LS, gMarksOn = true, rendered = 0;\n' +
    'function renderGraph() { rendered++; }\n' +
    'var chip = { cls: {}, classList: { toggle: function (c, on) { chip.cls[c] = on; } } };\n' +
    'var document = { getElementById: function (id) { return id === "chipMarks" ? chip : null; } };\n' +
    fn('graphMarksPref') + '\n' + fn('setGraphMarksOn') +
    '\nreturn { pref: graphMarksPref, set: setGraphMarksOn, on: function () { return gMarksOn; }, chip: chip, n: function () { return rendered; } };';
  const h = new Function('LS', body)(ls);
  assert.equal(h.pref(), true, 'default on');
  h.set(false);
  assert.equal(h.on(), false); assert.equal(store['otgw-v2-gmarks'], '0'); assert.equal(h.chip.cls.on, false); assert.ok(h.n() > 0);
  assert.equal(h.pref(), false, 'read back off');
  h.set(true);
  assert.equal(store['otgw-v2-gmarks'], '1'); assert.equal(h.pref(), true); assert.equal(h.chip.cls.on, true);
});
