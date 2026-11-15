// TASK-1939: the PIC panel showed a raw value such as 't20.50' for the setpoint override (PR=O). The PIC
// (PrintOverride, other-projects/otgw-6.6/gateway.asm:2198-2222) prints T/C for an override the thermostat has
// accepted, t/c in lowercase while it has not accepted it yet, n while a cleared override waits to be released,
// and N when there is none. fmtPicSetting() now renders t, c and n as text too.
//
// Runs the REAL shipped fmtPicSetting() cut out of data/v2.js with a t() stub that returns the key and its
// parameters, so the case is checked without the language pack. Point OTGW_V2_JS at an older copy of v2.js to
// see it red. The English defaults in v2.js and every pack under i18n/ must carry the three new keys.
//
// Run: node --test tests/webui/pic-setpoint-override-text.test.mjs
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

const fmt = new Function(
  'function t(k, p) { return p && p.temp !== undefined ? k + "|" + p.temp : k; }\n' +
  'function picLetters() { return ""; }\nfunction picCode() { return ""; }\nvar PIC_LED = {}, PIC_GPIO = {};\n' +
  extractFunction(SRC, 'fmtPicSetting') + '\nreturn fmtPicSetting;')();

test('accepted overrides and no override render as before', () => {
  assert.equal(fmt('setpoint_override', 'N'), 'adv.pic_ovr_none');
  assert.equal(fmt('setpoint_override', 'T20.50'), 'adv.pic_ovr_temp|20.50');
  assert.equal(fmt('setpoint_override', 'C18.00'), 'adv.pic_ovr_const|18.00');
});

test('lowercase t and c render as an override the thermostat has not accepted yet', () => {
  assert.equal(fmt('setpoint_override', 't20.50'), 'adv.pic_ovr_temp_pending|20.50');
  assert.equal(fmt('setpoint_override', 'c18.00'), 'adv.pic_ovr_const_pending|18.00');
});

test('n renders as an override being cleared', () => {
  assert.equal(fmt('setpoint_override', 'n'), 'adv.pic_ovr_clearing');
});

test('the English defaults and every language pack carry the three keys', () => {
  for (const k of ['pic_ovr_temp_pending', 'pic_ovr_const_pending', 'pic_ovr_clearing']) {
    assert.match(SRC, new RegExp('"' + k + '": "'), 'English default for adv.' + k);
    for (const lang of ['nl', 'de', 'el']) {
      const pack = JSON.parse(fs.readFileSync(path.join(ROOT, 'i18n', lang + '.json'), 'utf8'));
      const v = pack.adv && pack.adv[k];
      assert.ok(typeof v === 'string' && v.length > 0, lang + ': adv.' + k);
      if (k !== 'pic_ovr_clearing') assert.ok(v.includes('{{temp}}'), lang + ': adv.' + k + ' keeps {{temp}}');
    }
  }
});
