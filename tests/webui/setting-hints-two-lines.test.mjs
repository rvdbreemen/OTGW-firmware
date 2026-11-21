// TASK-2959 F10 (spec T4): a hint under a setting label renders as at most 2 lines at 1400 px; longer
// help lives in the (i) popover (SET_META[k].help). Before the fix the Appliance type hint ran to 13
// lines in the narrow label column. The test serves every SET_META key that has a hint, opens every
// Settings category and the SAT page (Technical view shows L2 and L3), and measures each .shint against
// the height of one line. OTGW_DATA_DIR=<dir> runs it against another tree (old-vs-fix).
//
// Run: node --test tests/webui/setting-hints-two-lines.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startPage } from './lib/page-harness.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const SRC = fs.readFileSync(path.join(DATA, 'v2.js'), 'utf8');
const KEYS = [...SRC.matchAll(/^\s+(\w+):\s*\{ cat: '\w+'[^\n]*?hint: '/gm)].map((m) => m[1]);
const SETTINGS = Object.fromEntries(KEYS.map((k) => [k, { value: '', type: 's', maxlen: 32 }]));

const MEASURE = `(function () {
  var out = [];
  document.querySelectorAll('.srow .shint').forEach(function (s) {
    if (!s.offsetParent) return;
    var probe = s.cloneNode(false); probe.textContent = 'X'; probe.style.position = 'absolute'; probe.style.visibility = 'hidden';
    s.parentNode.appendChild(probe); var one = probe.getBoundingClientRect().height; probe.remove();
    var lines = Math.round(s.getBoundingClientRect().height / one);
    out.push({ key: s.closest('.srow').dataset.key, lines: lines });
  });
  return out;
})()`;

test('every setting hint renders in at most 2 lines at 1400 px', async () => {
  const h = await startPage({
    width: 1400, height: 1000,
    api: (m, p) => ((p.endsWith('/v2/settings') && m === 'GET') ? SETTINGS : p.endsWith('/v2/sat/status') ? { enabled: true, active: true } : undefined),
    beforeScript: "try { localStorage.setItem('otgw-v2-satview', 'technical'); } catch (e) {}",
  });
  const seen = {};
  try {
    await h.evalJs(`document.querySelector('.seg[data-page="settings"]').click()`);
    await h.until(`document.querySelectorAll('#page-settings .rail-item').length > 0`, 10000);
    const n = await h.evalJs(`document.querySelectorAll('#page-settings .rail-item').length`);
    for (let i = 0; i < n; i++) {
      await h.evalJs(`document.querySelectorAll('#page-settings .rail-item')[${i}].click()`);
      await h.sleep(300);
      for (const r of await h.evalJs(MEASURE)) seen[r.key] = Math.max(seen[r.key] || 0, r.lines);
    }
    await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
    await h.until(`!!document.querySelector('#satSetL3 .srow')`, 10000);
    await h.sleep(500);
    for (const r of await h.evalJs(MEASURE)) seen[r.key] = Math.max(seen[r.key] || 0, r.lines);
  } finally { await h.close(); }
  console.log('  measured', Object.keys(seen).length, 'hints; over 1 line:', JSON.stringify(Object.entries(seen).filter(([, v]) => v > 1)));
  assert.ok(seen.satmanufacturer !== undefined, 'the Manufacturer hint was measured');
  assert.ok(seen.satsource !== undefined && seen.satsource <= 2, 'the Appliance type hint is short while the experimental type is not chosen');
  // While the experimental type is chosen, entry 204 keeps its warning inline (sergeant-d-corrections.md:5470).
  const long = Object.entries(seen).filter(([, v]) => v > 2);
  assert.deepEqual(long, [], 'hints longer than 2 lines');
});
