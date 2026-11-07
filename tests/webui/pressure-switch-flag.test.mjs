// TASK-1682 guard: a boiler with only a low-pressure switch (Ideal Logic) answers MsgID 18 with
// a fixed 0.1 bar. After 10 minutes at 0.1 bar the web UI marks the pressure as "switch?" (with
// a tooltip) and does not paint it as an alarm; any other value, or a value that moves, is shown
// as before.
//
// Runs the REAL fmt(), trackPressSwitch(), pressIsSwitch(), pressText(), setPress() and
// pressClass() cut out of data/v2.js by brace matching, and checks that every pressure readout
// goes through them. Point OTGW_V2_JS at an older copy (git show HEAD~1:... > file) to see it red.
//
// Run: node --test tests/webui/pressure-switch-flag.test.mjs
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

function constLine(name) {
  const m = SRC.match(new RegExp('var ' + name + ' = [^\\n]*;'));
  if (!m) throw new Error('var ' + name + ' not found in ' + V2JS);
  return m[0];
}

// One sandbox: the sliced functions share the "model" and "document" globals, as in v2.js.
function load() {
  const els = {};
  const document = { getElementById: (id) => els[id] || null };
  const model = { pressure: null, pressFlatSince: null };
  const body = [constLine('PRESS_SWITCH_BAR'), constLine('PRESS_SWITCH_TITLE'),
    ...['fmt', 'trackPressSwitch', 'pressIsSwitch', 'pressText', 'setPress', 'pressClass'].map((n) => extractFunction(SRC, n)),
    'return { trackPressSwitch, pressIsSwitch, pressText, setPress, pressClass };'].join('\n');
  const api = new Function('model', 'document', body)(model, document);
  function el(id) {
    const e = { textContent: '', attrs: {}, setAttribute(k, v) { this.attrs[k] = v; }, removeAttribute(k) { delete this.attrs[k]; } };
    els[id] = e; return e;
  }
  return { ...api, model, el };
}

const MIN = 60 * 1000;
const f88 = (u16) => u16 / 256;

test('fixed 0.1 bar is marked only after 10 minutes', () => {
  const t = load();
  t.model.pressure = 0.1; t.trackPressSwitch(t.model, 0);
  t.trackPressSwitch(t.model, 9 * MIN);
  assert.equal(t.pressText(t.model, 10 * MIN - 1), '0.10 bar');
  assert.equal(t.pressText(t.model, 10 * MIN), '0.10 bar · switch?');
});

test('both f8.8 encodings of 0.1 bar count (0x0019 and 0x001A)', () => {
  for (const u of [0x0019, 0x001A]) {
    const t = load();
    t.model.pressure = f88(u); t.trackPressSwitch(t.model, 0);
    assert.ok(t.pressIsSwitch(t.model, 10 * MIN), 'f8.8 0x' + u.toString(16) + ' = ' + f88(u));
  }
});

test('a pressure that moves resets the 10 minutes', () => {
  const t = load();
  t.model.pressure = 0.1; t.trackPressSwitch(t.model, 0);
  t.model.pressure = 1.5; t.trackPressSwitch(t.model, 5 * MIN);
  t.model.pressure = 0.1; t.trackPressSwitch(t.model, 6 * MIN);
  assert.ok(!t.pressIsSwitch(t.model, 15 * MIN), 'counted from the first 0.1 instead of the last');
  assert.ok(t.pressIsSwitch(t.model, 16 * MIN));
  t.model.pressure = 1.5; t.trackPressSwitch(t.model, 17 * MIN);
  assert.equal(t.pressText(t.model, 30 * MIN), '1.50 bar');
});

test('other low values and missing data are never marked', () => {
  const t = load();
  for (const p of [0.0, 0.2, 0.09]) {
    t.model.pressure = p; t.model.pressFlatSince = null; t.trackPressSwitch(t.model, 0);
    assert.ok(!t.pressIsSwitch(t.model, 60 * MIN), p + ' bar marked as a switch');
  }
  t.model.pressure = null; t.trackPressSwitch(t.model, 0);
  assert.equal(t.pressText(t.model, 60 * MIN), '—');
});

test('setPress sets the tooltip only while marked', () => {
  const t = load();
  const e = t.el('bPress');
  t.model.pressure = 0.1; t.trackPressSwitch(t.model, 0);
  t.setPress('bPress', MIN);
  assert.equal(e.textContent, '0.10 bar'); assert.equal(e.attrs.title, undefined);
  t.setPress('bPress', 10 * MIN);
  assert.equal(e.textContent, '0.10 bar · switch?');
  assert.match(e.attrs.title, /pressure switch, no sensor/);
  t.model.pressure = 1.8; t.trackPressSwitch(t.model, 11 * MIN); t.setPress('bPress', 11 * MIN);
  assert.equal(e.attrs.title, undefined);
});

test('every pressure readout goes through the switch-aware helpers', () => {
  assert.ok(!/fmt\(model\.pressure/.test(SRC), 'a readout still formats model.pressure directly');
  assert.ok(/setPress\('aPress'/.test(SRC) && /setPress\('bPress'/.test(SRC), '#aPress and #bPress use setPress');
  assert.ok(/\['Pressure', pressText\(model/.test(SRC), 'mobile chip uses pressText');
  assert.ok(/\['PRESSURE', pressText\(model, Date\.now\(\)\), pressIsSwitch\(model, Date\.now\(\)\) \? '' : pressClass/.test(SRC),
    'grid cell uses pressText and no alarm colour while marked');
  assert.ok(/case 18: if \(isAck\) \{ model\.pressure = f88\(u16\); trackPressSwitch\(model/.test(SRC), 'MsgID 18 frames feed the tracker');
  assert.ok(/trackPressSwitch\(model, Date\.now\(\)\);/.test(extractFunction(SRC, 'applyOtmonToModel')), 'the REST seed feeds the tracker');
});

test('a marked 0.1 bar is not an alarm colour, a real low pressure still is', () => {
  const t = load();
  assert.equal(t.pressClass(0.1), 'alert', 'pressClass itself keeps the alarm band');
});
