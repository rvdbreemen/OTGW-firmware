// TASK-1779: the hot-water settings appear once, on the SAT Hot water card, not again as a
// "Hot water" group in the SAT settings forms (Robert 2026-11-10, Sergeant D's suggestion).
// Static: reads v2.js and v2.html. OTGW_V2_JS / OTGW_V2_HTML point at older copies to see it red.
//
// Run: node --test tests/webui/sat-hot-water-one-card.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const JS = fs.readFileSync(process.env.OTGW_V2_JS || path.join(DATA, 'v2.js'), 'utf8');
const HTML = fs.readFileSync(process.env.OTGW_V2_HTML || path.join(DATA, 'v2.html'), 'utf8');
const KEYS = ['satdhwenabled', 'satdhwenable', 'satdhwsetpoint'];

test('no SAT settings form group repeats the hot-water settings', () => {
  const groups = /var SAT_L2 = \[([\s\S]*?)\n  \];[\s\S]*?var SAT_L3 = \[([\s\S]*?)\n  \];/.exec(JS);
  assert.ok(groups, 'SAT_L2 / SAT_L3 not found');
  for (const k of KEYS) assert.ok(!groups[1].includes("'" + k + "'") && !groups[2].includes("'" + k + "'"), k + ' is still in a SAT settings group');
});

test('the hot-water keys are excluded from the forms, so they do not fall into "Other"', () => {
  const m = /var SAT_NOT_IN_FORMS = \{([^}]*)\}/.exec(JS);
  assert.ok(m, 'SAT_NOT_IN_FORMS not found');
  for (const k of KEYS) assert.match(m[1], new RegExp('(^|[ ,{])' + k + ': 1'), k + ' not in SAT_NOT_IN_FORMS');
});

test('the SAT Hot water card still has all three controls', () => {
  for (const id of ['satDhwEnabled', 'satDhwSlider', 'satDhwEnable']) assert.match(HTML, new RegExp('id="' + id + '"'), id + ' missing on the card');
});
