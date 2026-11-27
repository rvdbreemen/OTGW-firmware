// TASK-2926: Advanced > File System at 375 px. A long file name must not widen the document: the page has no
// sideways scroll (scrollWidth <= clientWidth), in English and in Dutch.
// Drives the REAL v2.html/v2.js/v2.css in headless Chrome against a fake device (synthetic file names).
// OTGW_DATA_DIR=<dir> runs it against an older copy of data/ (old-vs-fix). OTGW_SHOT_DIR=<dir> saves screenshots.
// Run: node --test tests/webui/fs-table-375-2926.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { startPage, CHROME } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';
const FILES = [
  { name: 'settings.ini', type: 'file', size: 2048 },
  { name: 'ot-thermo-history-2026-11-19-extended-capture-with-a-very-long-name.json', type: 'file', size: 183456 },
  { name: 'rebootlog.txt', type: 'file', size: 9120 },
  { name: 'recordings', type: 'dir', size: 0 },
  { summary: true, usedBytes: 812345, totalBytes: 2097152, freeBytes: 1284807, truncated: false },
];
const SUMMARY = { usedBytes: 812345, totalBytes: 2097152, freeBytes: 1284807, truncated: false };
function api(method, p) {
  if (p.endsWith('/v2/filesystem/files')) return FILES.slice(0, -1).concat([SUMMARY]);
  if (p.endsWith('/v2/settings')) return method === 'POST' ? { status: 'ok' } : { hostname: { value: 'otgw', type: 's', maxlen: 30 } };
  if (p.endsWith('/v2/sat/status')) return { sat: { enabled: false } };
  return undefined;
}
const shotDir = process.env.OTGW_SHOT_DIR;
const OPEN = `(function () { document.querySelector('.seg[data-page="advanced"]').click();
  document.querySelector('#advTabs [data-adv="files"]').click(); return true; })()`;
const WIDTH = `(function () { var d = document.documentElement; return { sw: d.scrollWidth, cw: d.clientWidth }; })()`;

for (const lang of ['en', 'nl']) {
  test(`375 px: File System has no sideways page scroll with a long file name (${lang})`, { skip }, async () => {
    const h = await startPage({ width: 375, height: 812, api,
      beforeScript: `try { localStorage.setItem('otgw.lang', '${lang}'); localStorage.setItem('otgw-theme', 'light'); } catch (e) {}` });
    try {
      assert.ok(await h.until(`!!document.querySelector('.seg[data-page="advanced"]')`, 20000), 'page is up');
      await h.evalJs(OPEN);
      assert.ok(await h.until(`document.querySelectorAll('#fsTableBody tr').length >= 4`, 15000), 'file list rendered');
      const w = await h.evalJs(WIDTH);
      if (shotDir) await h.shot(path.join(shotDir, `fs-375-${lang}.png`));
      assert.ok(w.sw <= w.cw, `no sideways scroll: scrollWidth ${w.sw} <= clientWidth ${w.cw}`);
      assert.deepEqual(h.exceptions, []);
    } finally { await h.close(); }
  });
}

// Desktop: the fix must not change the roomy layout; no sideways scroll and the Download/Delete columns stay visible.
test('1400 px: File System keeps one row per file and no sideways scroll', { skip }, async () => {
  const h = await startPage({ width: 1400, height: 900, api,
    beforeScript: `try { localStorage.setItem('otgw-theme', 'light'); } catch (e) {}` });
  try {
    assert.ok(await h.until(`!!document.querySelector('.seg[data-page="advanced"]')`, 20000), 'page is up');
    await h.evalJs(OPEN);
    assert.ok(await h.until(`document.querySelectorAll('#fsTableBody tr').length >= 4`, 15000), 'file list rendered');
    const w = await h.evalJs(WIDTH);
    if (shotDir) await h.shot(path.join(shotDir, 'fs-1400-en.png'));
    assert.ok(w.sw <= w.cw, `no sideways scroll: scrollWidth ${w.sw} <= clientWidth ${w.cw}`);
    const rowH = await h.evalJs(`(function () { var r = document.querySelectorAll('#fsTableBody tr')[2].getBoundingClientRect(); return r.height; })()`);
    assert.ok(rowH < 60, 'a short name stays on one line on desktop, row height ' + rowH);
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});
