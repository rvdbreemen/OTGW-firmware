// TASK-1768: a Settings select shows its whole option text in every web UI language.
// The Board mode select was capped at 220 px and cut 'Automatisch herkennen' (nl), 'Automatisch
// erkennen' (de) and 'Αυτόματη ανίχνευση' (el). Drives the REAL v2.html/v2.js/v2.css in headless
// Chrome against a fake device and measures the selected option's text with the select's own font.
// ONE Chrome for the whole file; the languages are switched in that page. Point OTGW_DATA_DIR at an
// older copy of data/ to see it red.
//
// Run: node --test tests/webui/settings-select-fits.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startPage, CHROME } from './lib/page-harness.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const skip = !CHROME && 'no Chrome';

const asset = (p, q, s) => {
  const m = /^\/i18n\/([a-z]+)\.json$/.exec(p);
  if (!m) return false;
  const f = path.join(ROOT, 'i18n', m[1] + '.json');
  if (!fs.existsSync(f)) { s.writeHead(404); s.end('nf'); return true; }
  s.writeHead(200, { 'content-type': 'application/json' });
  s.end(fs.readFileSync(f));
  return true;
};
const SETTINGS = { settings: {
  hostname:  { value: 'otgw-demo', type: 's', maxlen: 32 },
  boardmode: { value: 0, type: 'i', min: 0, max: 2 },
} };
const api = (m, p) => (p.endsWith('/v2/settings') ? SETTINGS : undefined);

// Free width inside the select (content box minus Chrome's native arrow, ~18 px; checked on a screenshot) against the width the
// selected option's text needs in the select's own font.
const MEASURE = `(function () {
  var sel = document.querySelector('#page-settings .srow select');
  if (!sel || !sel.options.length) return null;
  var cs = getComputedStyle(sel);
  var c = document.createElement('canvas').getContext('2d');
  c.font = cs.fontWeight + ' ' + cs.fontSize + ' ' + cs.fontFamily;
  var txt = sel.options[sel.selectedIndex].text;
  var need = Math.ceil(c.measureText(txt).width);
  var free = sel.clientWidth - parseFloat(cs.paddingLeft) - parseFloat(cs.paddingRight) - 18;
  return { txt: txt, need: need, free: Math.floor(free), w: sel.clientWidth };
})()`;

for (const [width, label] of [[1280, 'desktop'], [375, 'phone']]) {
  test(`Board mode select fits its option text in en/nl/de/el (${label})`, { skip }, async () => {
    const h = await startPage({ asset, api, width, height: 900,
      beforeScript: `try { localStorage.removeItem('otgw.lang'); } catch (e) {}` });
    try {
      assert.ok(await h.until(`!!document.querySelector('details.lang-menu')`));
      await h.evalJs(`document.querySelector('.switcher .seg[data-page="settings"]').click(); 0`);
      const bad = [];
      for (const lang of ['en', 'nl', 'de', 'el']) {
        await h.evalJs(`document.querySelector('details.lang-menu a[lang="${lang}"]').click(); 0`);
        assert.ok(await h.until(`document.documentElement.lang === '${lang}'`, 10000), `${lang} did not load`);
        assert.ok(await h.until(`!!document.querySelector('#page-settings .srow select')`, 10000), 'no select rendered');
        await h.sleep(300);
        const m = await h.evalJs(MEASURE);
        assert.ok(m, 'measure failed');
        if (m.need > m.free) bad.push(`${lang}: "${m.txt}" needs ${m.need} px, has ${m.free} px (select ${m.w} px)`);
      }
      assert.deepEqual(bad, [], 'option text cut off');
      assert.deepEqual(h.exceptions, []);
    } finally { await h.close(); }
  });
}
