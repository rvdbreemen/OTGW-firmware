// TASK-1814: the SAT Settings card no longer shows the target temperature (sattargettemp); the Home page
// dial and POST /api/v2/sat/target still set it (Robert 2026-11-11, Sergeant D's proposal, entry 311).
// Static: reads v2.js. OTGW_V2_JS points at an older copy to see it red.
//
// Run: node --test tests/webui/sat-target-not-in-settings.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const JS = fs.readFileSync(process.env.OTGW_V2_JS || path.join(DATA, 'v2.js'), 'utf8');

test('no SAT settings form group lists sattargettemp', () => {
  const groups = /var SAT_L2 = \[([\s\S]*?)\n  \];[\s\S]*?var SAT_L3 = \[([\s\S]*?)\n  \];/.exec(JS);
  assert.ok(groups, 'SAT_L2 / SAT_L3 not found');
  assert.ok(!groups[1].includes("'sattargettemp'") && !groups[2].includes("'sattargettemp'"), 'sattargettemp is still in a SAT settings group');
});

test('sattargettemp is excluded from the forms, so it does not fall into "Other"', () => {
  const m = /var SAT_NOT_IN_FORMS = \{([^}]*)\}/.exec(JS);
  assert.ok(m, 'SAT_NOT_IN_FORMS not found');
  assert.match(m[1], /(^|[ ,{\n])sattargettemp: 1/, 'sattargettemp not in SAT_NOT_IN_FORMS');
});

test('the Home dial still sets the target through /api/v2/sat/target', () => {
  assert.match(JS, /\/api\/v2\/sat\/target/, 'the Home dial endpoint is gone');
});
