// TASK-2997 (impeccable critique of 2026-11-21): (1) no skipped heading levels on SAT, Settings, Advanced and Diagnose
// (h2 followed by h4 read as a gap in the outline); (2) the Settings rail focus ring sits inside its item, so it never
// overlaps a neighbour's active tint; (3) the MQTT onboarding step no longer uses the undefined tokens --ok and --warn.
// The colour pairs are in contrast-tokens.test.mjs (rows G to K).
//
// Drives the REAL v2.html/v2.js/v2.css in headless Chrome. OTGW_DATA_DIR=<dir> runs it against an older copy of data/.
//
// Run: node --test tests/webui/heading-levels-and-rail-focus-2997.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startPage, CHROME } from './lib/page-harness.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const skip = !CHROME && 'no Chrome';
const S = (value) => ({ value, type: 's', maxlen: 30 });
const SETTINGS = { hostname: S('otgw'), mqttbroker: S('broker.local') };

test('the static page has no skipped heading level', () => {
  const html = fs.readFileSync(path.join(DATA, 'v2.html'), 'utf8');
  const seq = [...html.matchAll(/<h([1-6])\b[^>]*>/g)].map((m) => +m[1]);
  const bad = [];
  seq.forEach((l, i) => { if (i && l > seq[i - 1] + 1) bad.push(`h${seq[i - 1]} then h${l} at heading ${i + 1}`); });
  assert.ok(seq.length > 10, 'headings found: ' + seq.length);
  assert.deepEqual(bad, [], 'skipped levels: ' + bad.join('; '));
});

test('rendered headings never skip a level, on every page; no heading is created as an h4', { skip }, async () => {
  const h = await startPage({ api: (m, p) => (p.endsWith('/v2/settings') ? SETTINGS : undefined) });
  try {
    for (const page of ['home', 'sat', 'monitor', 'settings', 'advanced']) {
      await h.evalJs(`document.querySelector('.seg[data-page="${page}"]').click()`);
      await h.sleep(900);
    }
    const r = await h.evalJs(`(function () { var hs = Array.prototype.slice.call(document.querySelectorAll('h1, h2, h3, h4, h5, h6')), bad = [], prev = 0;
      hs.forEach(function (e) { var l = +e.tagName.charAt(1); if (prev && l > prev + 1) bad.push('h' + prev + ' then h' + l + ' "' + e.textContent.trim().slice(0, 40) + '"'); prev = l; });
      return { n: hs.length, bad: bad, h4: document.querySelectorAll('h4').length }; })()`);
    assert.ok(r.n > 10, 'headings: ' + r.n);
    assert.deepEqual(r.bad, [], 'skipped levels: ' + r.bad.join('; '));
    assert.equal(r.h4, 0, 'no h4 left under the h2 pages');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('the rail focus ring is inset; the wizard uses defined tokens only', { skip }, async () => {
  const h = await startPage({ api: (m, p) => (p.endsWith('/v2/settings') ? SETTINGS : undefined) });
  try {
    const r = await h.evalJs(`(function () { var off = null, inline = 0;
      Array.prototype.forEach.call(document.styleSheets, function (s) { try { Array.prototype.forEach.call(s.cssRules, function (c) { if (c.selectorText === '.rail-item:focus-visible') off = c.style.outlineOffset; }); } catch (e) { } });
      return { off: off }; })()`);
    assert.equal(r.off, '-2px', 'the rail focus ring offset is inside the item');
    const js = fs.readFileSync(path.join(DATA, 'v2.js'), 'utf8');
    assert.doesNotMatch(js, /var\(--(ok|warn)[,)]/, 'v2.js uses the undefined tokens --ok / --warn');
  } finally { await h.close(); }
});
