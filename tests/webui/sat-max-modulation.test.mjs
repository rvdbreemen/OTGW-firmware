// TASK-1514 guard: the SAT card that holds the Max flow setpoint slider is titled "Central Heating
// Settings" and carries a second slider, Max relative modulation (0-100 %, default 100 = no cap), that
// follows status field max_rel_modulation and posts sat/settings/max_modulation on change.
//
// Runs the REAL satRenderMaxMod(), satNum() and txt() cut out of data/v2.js by brace matching, against a
// small DOM stub. Point OTGW_V2_JS at an older copy to see it red.
//
// Run: node --test tests/webui/sat-max-modulation.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const V2JS = process.env.OTGW_V2_JS || path.join(DATA, 'v2.js');
const SRC = fs.readFileSync(V2JS, 'utf8');
const HTML = fs.readFileSync(path.join(DATA, 'v2.html'), 'utf8');

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

function load(el, dirty) {
  const body = 'var satModDirty = ' + !!dirty + ';\n' +
    'var document = { getElementById: function (id) { return el[id] || null; } };\n' +
    extractFunction(SRC, 'txt') + '\n' + extractFunction(SRC, 'satNum') + '\n' +
    extractFunction(SRC, 'satRenderMaxMod') + '\n' +
    'return { satRenderMaxMod: satRenderMaxMod };';
  return new Function('el', body)(el);
}
const card = () => ({ satModSlider: { value: '100' }, satModR: { textContent: '' } });

test('the card is titled "Central Heating Settings" and holds both sliders', () => {
  const m = HTML.match(/<details[^>]*id="satMaxGrp"[\s\S]*?<\/details>/);
  assert.ok(m, 'satMaxGrp card present');
  assert.match(m[0], /<summary>Central Heating Settings<\/summary>/);
  assert.match(m[0], /id="satMaxSlider"/);
  assert.match(m[0], /<input type="range" min="0" max="100" value="100" id="satModSlider" class="sat-slider"/);
  assert.match(m[0], /id="satModR"/);
  assert.doesNotMatch(HTML, /<summary>Max flow setpoint<\/summary>/);
});

test('a status value moves the thumb and shows it in percent', () => {
  const el = card(), f = load(el);
  f.satRenderMaxMod({ max_rel_modulation: 60 });
  assert.equal(String(el.satModSlider.value), '60');
  assert.equal(el.satModR.textContent, '60 %');
});

test('100 is shown as 100 %, there is no "not chosen" state', () => {
  const el = card(), f = load(el);
  el.satModSlider.value = '40';
  f.satRenderMaxMod({ max_rel_modulation: 100 });
  assert.equal(String(el.satModSlider.value), '100');
  assert.equal(el.satModR.textContent, '100 %');
});

test('0 is a valid value', () => {
  const el = card(), f = load(el);
  f.satRenderMaxMod({ max_rel_modulation: 0 });
  assert.equal(String(el.satModSlider.value), '0');
  assert.equal(el.satModR.textContent, '0 %');
});

test('a firmware without max_rel_modulation shows a dash and leaves the thumb', () => {
  const el = card(), f = load(el);
  f.satRenderMaxMod({});
  assert.equal(el.satModR.textContent, '—');
  assert.equal(String(el.satModSlider.value), '100');
});

test('a slider being dragged is not overwritten by the status poll', () => {
  const el = card(), f = load(el, true);
  f.satRenderMaxMod({ max_rel_modulation: 30 });
  assert.equal(String(el.satModSlider.value), '100');
});

test('a change posts sat/settings/max_modulation and the poll renders it', () => {
  assert.match(SRC, /getElementById\('satModSlider'\)[\s\S]{0,500}satPost\('settings\/max_modulation', mm\.value\)/);
  assert.match(SRC, /satRenderMaxTSet\(d\);\s*satRenderMaxMod\(d\);/);
});
