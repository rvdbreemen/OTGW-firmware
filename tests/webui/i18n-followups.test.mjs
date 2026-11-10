// TASK-1775: three translation leftovers of TASK-1764. Static (reads v2.js, v2.html and the packs; no Chrome).
// OTGW_V2_JS / OTGW_V2_HTML point at older copies to see it red.
//
// Run: node --test tests/webui/i18n-followups.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { i18nEn } from './lib/i18n-en.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const DATA = path.join(ROOT, 'src', 'OTGW-firmware', 'data');
const JS = fs.readFileSync(process.env.OTGW_V2_JS || path.join(DATA, 'v2.js'), 'utf8');
const HTML = fs.readFileSync(process.env.OTGW_V2_HTML || path.join(DATA, 'v2.html'), 'utf8');
const PACKS = Object.fromEntries(['nl', 'de', 'el'].map((l) => [l, JSON.parse(fs.readFileSync(path.join(ROOT, 'i18n', l + '.json'), 'utf8'))]));
const get = (o, k) => k.split('.').reduce((a, x) => (a && a[x] !== undefined ? a[x] : undefined), o);

test('the webhook test texts get the translated on/off word, not the English ON/OFF', () => {
  assert.doesNotMatch(JS, /state:\s*st\.toUpperCase\(\)/, 'webhook state is still st.toUpperCase()');
  assert.match(JS, /t\('settings\.wh_send_test', \{ state: t\(st === 'on' \? 'home\.on' : 'home\.off'\) \}\)/);
});

test('the Home LCD line uses translated abbreviations for CH, HP and DHW', () => {
  assert.doesNotMatch(JS, /var lead = isHP \? 'HP' : 'CH';/, 'LCD lead is hard-coded English');
  assert.doesNotMatch(JS, /\('DHW ' \+/, 'LCD DHW prefix is hard-coded English');
  const en = i18nEn(JS);
  for (const k of ['home.lcd_lead_ch', 'home.lcd_lead_hp', 'home.lcd_dhw']) {
    assert.ok(en.i18nString(k), k + ' missing in I18N_EN');
    for (const [l, p] of Object.entries(PACKS)) {
      const v = get(p, k);
      assert.ok(v, `${l} ${k} missing`);
      assert.ok(v.replace('{{temp}}', '52').length <= 8, `${l} ${k} "${v}" too long for the LCD`);
    }
  }
});

test('change and override counts use _one/_other plural keys in every language', () => {
  const en = i18nEn(JS);
  for (const k of ['h_set.unsaved_suffix', 'h_mon.sat_unsaved', 'h_home.ovr_badge_suffix']) {
    assert.ok(en.i18nString(k + '_one') && en.i18nString(k + '_other'), k + ' has no _one/_other in I18N_EN');
    assert.equal(en.t(k, { count: 1 }), en.i18nString(k + '_one'));
    for (const [l, p] of Object.entries(PACKS)) {
      for (const f of ['_one', '_other']) {
        const v = get(p, k + f);
        assert.ok(v, `${l} ${k}${f} missing`);
        assert.doesNotMatch(v, /\(s\)|\(en\)|\(-/, `${l} ${k}${f} still uses a (s)/(en)/(-ές) form: ${v}`);
      }
    }
  }
  for (const id of ['dirtyCountSfx', 'satDirtyCountSfx', 'aOvrCountSfx']) assert.match(HTML, new RegExp(`id="${id}"`), id + ' span missing');
  assert.match(JS, /txt\(ids\[1\] \+ 'Sfx', t\(/, 'updateSaveBar does not set the plural suffix');
  assert.match(JS, /txt\('aOvrCountSfx', t\('h_home\.ovr_badge_suffix', \{ count: no \}\)\)/);
});

test('the Home chips and cells label hot water through a key, not a hard-coded DHW', () => {
  assert.doesNotMatch(JS, /\['DHW', model\.dhw/, 'Home chip or cell DHW is hard-coded English');
  const en = i18nEn(JS);
  for (const k of ['home.chip_dhw', 'home.cell_dhw']) {
    assert.equal(en.i18nString(k), 'DHW');
    for (const [l, p] of Object.entries(PACKS)) assert.ok(get(p, k), `${l} ${k} missing`);
  }
});
