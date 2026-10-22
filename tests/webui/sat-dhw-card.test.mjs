// TASK-1279 guard: the SAT "Hot water" card shows the standalone dependency and an inactive
// setpoint, and its slider range is the boiler's MsgID 48 range cut to what SAT can send.
//
// Runs the REAL satRenderDhw(), fetchDhwBounds(), satNum() and txt() cut out of data/v2.js by
// brace matching, against a small DOM stub, so inverted conditions or a wrong status field
// fail here (a substring check on the source would not see them).
// Point OTGW_V2_JS at an older copy (git show HEAD:... > file) to see it red.
//
// Run: node --test tests/webui/sat-dhw-card.test.mjs
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

const RANGE = (SRC.match(/var SAT_DHW_SETPOINT_MIN = \d+, SAT_DHW_SETPOINT_MAX = \d+;/) || [''])[0];

// Fresh card: slider (default 30-60 as in v2.html), readout, both toggles, tank row.
function makeCard() {
  const el = {
    satDhwSlider: { min: '30', max: '60', value: '30', disabled: true, title: '' },
    satDhwR: { textContent: '' },
    satDhwEnabled: { checked: false },
    satDhwEnable: { checked: false },
    satDhwEnableRow: { style: { display: 'none' } },
  };
  return el;
}

function load(el, opts = {}) {
  const body = RANGE + '\n' +
    'var satDhwDirty = ' + !!opts.dirty + ', satDhwEnabledPending = ' + !!opts.pending + ', satDhwEnPending = false, satLastData = null;\n' +
    'var APIGW = "/api/";\n' +
    'var document = { getElementById: function (id) { return el[id] || null; } };\n' +
    extractFunction(SRC, 'txt') + '\n' + extractFunction(SRC, 'satNum') + '\n' +
    extractFunction(SRC, 'satRenderDhw') + '\n' + extractFunction(SRC, 'fetchDhwBounds') + '\n' +
    'return { satRenderDhw: satRenderDhw, fetchDhwBounds: fetchDhwBounds };';
  return new Function('el', 'fetch', body)(el, opts.fetch || (() => Promise.reject(new Error('no fetch'))));
}

test('standalone DHW off: toggle off, slider disabled; on: toggle on, slider enabled', () => {
  const el = makeCard(), f = load(el);
  f.satRenderDhw({ dhw_enabled: false, dhw_setpoint: 50 });
  assert.equal(el.satDhwEnabled.checked, false);
  assert.equal(el.satDhwSlider.disabled, true, 'slider must be disabled while standalone DHW is off');
  f.satRenderDhw({ dhw_enabled: true, dhw_setpoint: 50 });
  assert.equal(el.satDhwEnabled.checked, true);
  assert.equal(el.satDhwSlider.disabled, false, 'slider must be usable while standalone DHW is on');
});

test('the standalone toggle follows dhw_enabled, not dhw_enable (the HW= master enable)', () => {
  const el = makeCard(), f = load(el);
  f.satRenderDhw({ dhw_enabled: false, dhw_enable: true, dhw_setpoint: 50 });
  assert.equal(el.satDhwEnabled.checked, false);
  assert.equal(el.satDhwSlider.disabled, true);
  assert.equal(el.satDhwEnable.checked, true, 'the HW= checkbox keeps following dhw_enable');
});

test('an inactive setpoint (0, or below 30) shows "off" with the thumb at the minimum', () => {
  for (const sp of [0, 10, 29.9]) {
    const el = makeCard(), f = load(el);
    el.satDhwSlider.value = '50';
    f.satRenderDhw({ dhw_enabled: true, dhw_setpoint: sp });
    assert.equal(el.satDhwR.textContent, 'off', 'setpoint ' + sp);
    assert.equal(String(el.satDhwSlider.value), el.satDhwSlider.min, 'thumb at min for ' + sp);
  }
  for (const [sp, shown] of [[30, '30°C'], [45.4, '45°C'], [60, '60°C']]) {
    const el = makeCard(), f = load(el);
    f.satRenderDhw({ dhw_enabled: true, dhw_setpoint: sp });
    assert.equal(el.satDhwR.textContent, shown);
  }
});

test('a pending toggle POST is not overwritten by the next status poll', () => {
  const el = makeCard(), f = load(el, { pending: true });
  el.satDhwEnabled.checked = true;                       // the user just switched it on
  f.satRenderDhw({ dhw_enabled: false, dhw_setpoint: 50 });
  assert.equal(el.satDhwEnabled.checked, true);
});

test('a firmware without the dhw_enabled field (older than this UI) leaves the slider usable', () => {
  const el = makeCard(), f = load(el);
  el.satDhwEnabled.checked = false;
  f.satRenderDhw({ dhw_setpoint: 50 });
  assert.equal(el.satDhwSlider.disabled, false);
  assert.equal(el.satDhwR.textContent, '50°C');
});

async function boundsFor(lb, ub) {
  const el = makeCard();
  const value = (ub << 8) | lb;
  const f = load(el, { fetch: () => Promise.resolve({ ok: true, json: () => Promise.resolve({ value: String(value) }) }) });
  await f.fetchDhwBounds();
  return [+el.satDhwSlider.min, +el.satDhwSlider.max];
}

test('fetchDhwBounds cuts the boiler MsgID 48 range to 30-60 and falls back to 30-60', async () => {
  assert.deepEqual(await boundsFor(20, 80), [30, 60], 'wide boiler range');
  assert.deepEqual(await boundsFor(35, 50), [35, 50], 'boiler range inside 30-60');
  assert.deepEqual(await boundsFor(40, 55), [40, 55], 'the bench boiler range');
  assert.deepEqual(await boundsFor(65, 80), [30, 60], 'no overlap with 30-60');
  assert.deepEqual(await boundsFor(0, 0), [30, 60], 'no MsgID 48 from the boiler');
});
