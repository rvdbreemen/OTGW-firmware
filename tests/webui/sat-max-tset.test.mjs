// TASK-1373 guard: the SAT "Max flow setpoint" slider takes its range from the boiler's MsgID 49
// (20-80 without it), shows "default" while the user has not chosen a value, and posts
// sat/settings/max_setpoint on change.
//
// Runs the REAL satRenderMaxTSet(), fetchMaxTSetBounds(), satNum() and txt() cut out of data/v2.js by
// brace matching, against a small DOM stub, so inverted conditions or a wrong status field
// fail here (a substring check on the source would not see them).
// Point OTGW_V2_JS at an older copy (git show HEAD:... > file) to see it red.
//
// Run: node --test tests/webui/sat-max-tset.test.mjs
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


const HTML = fs.readFileSync(path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data', 'v2.html'), 'utf8');
const RANGE = (SRC.match(/var SAT_MAXTSET_MIN = \d+, SAT_MAXTSET_MAX = \d+;/) || [''])[0];

function makeCard() {
  return { satMaxSlider: { min: '20', max: '80', value: '62', title: '' }, satMaxR: { textContent: '' } };
}

function load(el, opts = {}) {
  const body = RANGE + '\n' +
    'var satMaxDirty = ' + !!opts.dirty + ', satLastData = null;\n' +
    'var APIGW = "/api/";\n' +
    'var document = { getElementById: function (id) { return el[id] || null; } };\n' +
    extractFunction(SRC, 'txt') + '\n' + extractFunction(SRC, 'satNum') + '\n' +
    extractFunction(SRC, 'satRenderMaxTSet') + '\n' + extractFunction(SRC, 'fetchMaxTSetBounds') + '\n' +
    'return { satRenderMaxTSet: satRenderMaxTSet, fetchMaxTSetBounds: fetchMaxTSetBounds };';
  return new Function('el', 'fetch', body)(el, opts.fetch || (() => Promise.reject(new Error('no fetch'))));
}

test('a chosen value moves the thumb and shows the value', () => {
  const el = makeCard(), f = load(el);
  f.satRenderMaxTSet({ max_setpoint_user: 48, max_setpoint_system: 48 });
  assert.equal(String(el.satMaxSlider.value), '48');
  assert.equal(el.satMaxR.textContent, '48°C');
});

test('a chosen value the firmware cut (heat pump 40 cap) shows the effective value and names the chosen one', () => {
  const el = makeCard(), f = load(el);
  f.satRenderMaxTSet({ max_setpoint_user: 55, max_setpoint_system: 40 });
  assert.equal(String(el.satMaxSlider.value), '40');
  assert.equal(el.satMaxR.textContent, '40°C (chosen 55°C)');
});

test('a chosen value above 65 is shown as chosen, the global 65 cap no longer cuts it (entry 152)', () => {
  const el = makeCard(), f = load(el);
  f.satRenderMaxTSet({ max_setpoint_user: 70, max_setpoint_system: 70 });
  assert.equal(String(el.satMaxSlider.value), '70');
  assert.equal(el.satMaxR.textContent, '70°C');
});

test('not chosen (0) shows "default" with the ceiling SAT works with', () => {
  const el = makeCard(), f = load(el);
  f.satRenderMaxTSet({ max_setpoint_user: 0, max_setpoint_system: 50 });
  assert.equal(String(el.satMaxSlider.value), '50');
  assert.equal(el.satMaxR.textContent, 'default (50°C)');
});

test('a firmware without max_setpoint_user shows a dash and leaves the thumb', () => {
  const el = makeCard(), f = load(el);
  f.satRenderMaxTSet({ max_setpoint_system: 62 });
  assert.equal(el.satMaxR.textContent, '—');
  assert.equal(String(el.satMaxSlider.value), '62');
});

test('a slider being dragged is not overwritten by the status poll', () => {
  const el = makeCard(), f = load(el, { dirty: true });
  f.satRenderMaxTSet({ max_setpoint_user: 40, max_setpoint_system: 40 });
  assert.equal(String(el.satMaxSlider.value), '62');
});

async function boundsFor(lb, ub, raw) {
  const el = makeCard();
  const value = raw !== undefined ? raw : (ub << 8) | lb;
  const f = load(el, { fetch: (u) => { el.url = u; return Promise.resolve({ ok: true, json: () => Promise.resolve({ value: String(value) }) }); } });
  await f.fetchMaxTSetBounds();
  return [+el.satMaxSlider.min, +el.satMaxSlider.max, el.url];
}

test('fetchMaxTSetBounds takes MsgID 49 and falls back to 20-80', async () => {
  assert.deepEqual((await boundsFor(30, 70)).slice(0, 2), [30, 70], 'boiler range');
  assert.deepEqual((await boundsFor(40, 55)).slice(0, 2), [40, 55], 'narrow boiler range');
  assert.equal((await boundsFor(30, 70))[2], '/api/v2/otgw/messages/49');
  assert.deepEqual((await boundsFor(0, 0)).slice(0, 2), [20, 80], 'no MsgID 49 reported');
  assert.deepEqual((await boundsFor(70, 30)).slice(0, 2), [20, 80], 'upper below lower is not valid');
  assert.deepEqual((await boundsFor(30, 110)).slice(0, 2), [20, 80], 'upper above 100 is not valid');
});

test('the slider row is on the SAT page and a change posts sat/settings/max_setpoint', () => {
  assert.match(HTML, /id="satMaxSlider"[^>]*class="sat-slider"/);
  assert.match(HTML, /id="satMaxR"/);
  assert.match(SRC, /getElementById\('satMaxSlider'\)[\s\S]{0,400}satPost\('settings\/max_setpoint', ms\.value\)/);
  assert.match(SRC, /mk\.then\(fetchDhwBounds\)\.then\(fetchMaxTSetBounds\)/, 'bounds are fetched after the DHW bounds, one at a time');
});
