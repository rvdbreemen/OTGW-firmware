// TASK-1837 (Robert 2026-11-12): the Home "Boiler | Heat pump" pair was a switch (a click POSTed satsource)
// that read as two labels that can never both be true. Home now shows the appliance type as a label:
// Boiler for a gas boiler, Heat pump for a heat pump (also the on/off heat pump), both for hybrid; nothing
// until sat/status has answered. The appliance type is set in Settings/onboarding, not on Home.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const SRC = fs.readFileSync(path.join(DATA, 'v2.js'), 'utf8');
const HTML = fs.readFileSync(path.join(DATA, 'v2.html'), 'utf8');
function fn(name) {
  const start = SRC.indexOf('function ' + name + '(');
  assert.ok(start >= 0, name + ' not found in v2.js');
  let i = SRC.indexOf('{', start), depth = 0;
  for (; i < SRC.length; i++) { if (SRC[i] === '{') depth++; else if (SRC[i] === '}' && --depth === 0) return SRC.slice(start, i + 1); }
  throw new Error('unbalanced');
}

test('appliance labels per type: gas, heat pump, on/off heat pump, hybrid; nothing while unknown', () => {
  const tags = new Function(fn('applianceTags') + '\nreturn applianceTags;')();
  assert.deepEqual(tags(1, true), { gas: true, hp: false, hpDrawing: false });
  assert.deepEqual(tags(2, true), { gas: false, hp: true, hpDrawing: true });
  assert.deepEqual(tags(4, true), { gas: false, hp: true, hpDrawing: true });
  assert.deepEqual(tags(3, true), { gas: true, hp: true, hpDrawing: false });
  assert.deepEqual(tags(1, false), { gas: false, hp: false, hpDrawing: false }, 'unknown until sat/status answered');
});

test('Home shows labels, not buttons, and no longer writes satsource', () => {
  const m = /<span class="appl-tags" id="applTags">([\s\S]*?)<\/span>\s*\n\s*<!--/.exec(HTML);
  assert.ok(m, '#applTags present');
  assert.doesNotMatch(m[1], /<button/, 'no buttons in the appliance labels');
  assert.match(m[1], /data-appl="gas"[^>]*hidden/); assert.match(m[1], /data-appl="hp"[^>]*hidden/);
  assert.doesNotMatch(HTML, /id="applToggle"/);
  assert.doesNotMatch(SRC, /function setSource\(/, 'Home has no satsource writer any more');
});
