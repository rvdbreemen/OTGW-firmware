// TASK-3151.3 (ADR-306, Robert 2026-11-28): SAT has ONE margin setting, satpwmswitchmargin (default 3.0, as the reference SAT).
// The setting satovershootmargin is gone from the firmware, so the web UI must not show, label, group or document it any more.
// Static check of the shipped files (no browser): v2.js, the English help (settingsHelp.h) and the nl, de and el packs.
//
// Run: node --test tests/webui/sat-one-margin-3151.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const read = (...p) => fs.readFileSync(path.join(ROOT, ...p), 'utf8');
const v2 = read('src', 'OTGW-firmware', 'data', 'v2.js');
const hdr = read('src', 'OTGW-firmware', 'settingsHelp.h');
const EN = JSON.parse(hdr.slice(hdr.indexOf('R"json(') + 7, hdr.indexOf(')json"')));

test('v2.js no longer mentions satovershootmargin (label, SET_META row, Curve group)', () => {
  assert.equal(v2.includes('satovershootmargin'), false);
});

test('v2.js keeps the one margin: label, row and a place in the Curve group', () => {
  assert.match(v2, /"satpwmswitchmargin": "PWM switch margin"/);
  assert.match(v2, /satpwmswitchmargin:\s+\{ cat: 'sat', label: 'PWM switch margin', hint: '°C' \}/);
  assert.match(v2, /\['Curve', \[[^\]]*'satpwmswitchmargin'\]\]/);
});

test('English help: no satovershootmargin, the one margin says default 3.0', () => {
  assert.equal('satovershootmargin' in EN, false);
  assert.match(EN.satpwmswitchmargin, /default 3\.0/);
  assert.doesNotMatch(EN.satpwmswitchmargin, /default 2/);
});

for (const l of ['nl', 'de', 'el']) {
  test(`${l} pack: no satovershootmargin label or help, the one margin keeps its label`, () => {
    const raw = read('i18n', l + '.json');
    assert.equal(raw.includes('satovershootmargin'), false);
    assert.equal(raw.includes('"satpwmswitchmargin"'), true);
  });
}
