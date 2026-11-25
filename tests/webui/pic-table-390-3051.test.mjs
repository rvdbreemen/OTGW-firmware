// TASK-3051: on Advanced > PIC firmware the file table must stay readable at 390 px. Before the fix the six fixed
// columns (62 + 76 + 3 x 40 px + gaps) left the name column about 30 px, so FIRMWARE NAME showed one letter and an
// ellipsis. Now a narrow screen stacks the name over version, size and the three buttons: the name is whole and
// every button stays inside the table.
//
// Drives the REAL v2.html/v2.js/v2.css in headless Chrome against a fake device. OTGW_DATA_DIR=<dir> runs it against an
// older copy of data/ (old-vs-fix). OTGW_SHOT_DIR=<dir> saves screenshots (synthetic data).
//
// Run: node --test tests/webui/pic-table-390-3051.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { startPage, CHROME } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';
const FILES = [
  { name: 'gateway-5.8.hex', version: '5.8', size: 14336 },
  { name: 'interface-6.4.hex', version: '6.4', size: 9216 },
  { name: 'diagnose-1.3.hex', version: '1.3', size: 8192 },
];
function api(method, p) {
  if (p.endsWith('/v2/settings')) return method === 'POST' ? { status: 'ok' } : { hostname: { value: 'otgw', type: 's', maxlen: 30 } };
  if (p.endsWith('/v2/device/info')) return { device: { otcommandinterface: 'PIC', picavailable: true, picfwversion: '5.8', picfwtype: 'gateway', picdeviceid: '16F88' } };
  if (p.endsWith('/v2/firmware/files')) return FILES;
  if (p.endsWith('/v2/pic/update-check')) return { pic_update: { status: 'ready', current: '5.8', latest: '5.8', update_available: false } };
  if (p.endsWith('/v2/sat/status')) return { sat: { enabled: false, active: false, fallback_active: false, target_temp: 20 } };
  return undefined;
}
const shotDir = process.env.OTGW_SHOT_DIR;
// per file row: is the name cut (scrollWidth > clientWidth), how wide is it, and does every child lie inside the table
const MEASURE = `(function () {
  var t = document.querySelector('.pictable').getBoundingClientRect();
  return Array.prototype.map.call(document.querySelectorAll('#picFiles .picrow:not(.head)'), function (r) {
    var n = r.querySelector('.fname'), nr = n.getBoundingClientRect();
    var out = []; Array.prototype.forEach.call(r.children, function (c) { var b = c.getBoundingClientRect();
      if (b.width > 0 && (b.left < t.left - 1 || b.right > t.right + 1)) out.push(c.className || c.tagName); });
    return { name: n.textContent, cut: n.scrollWidth > n.clientWidth + 1, nameW: Math.round(nr.width), outside: out,
      buttons: r.querySelectorAll('.picbtn').length, page: document.documentElement.scrollWidth - document.documentElement.clientWidth }; }); })()`;

for (const theme of ['light', 'dark']) {
  for (const width of [390, 1400]) {
    test(`${width} px: PIC firmware file names are whole and every button is inside the table (${theme})`, { skip }, async () => {
      const h = await startPage({ width, api, beforeScript: `try { localStorage.setItem('otgw-theme', '${theme}'); } catch (e) {}` });
      try {
        await h.evalJs(`document.querySelector('.seg[data-page="advanced"]').click()`);
        assert.ok(await h.until(`document.querySelectorAll('#picFiles .picrow:not(.head)').length === 3`, 20000), 'three file rows drawn');
        const rows = await h.evalJs(MEASURE);
        if (shotDir) await h.shot(path.join(shotDir, `pic-${width}-${theme}.png`));
        for (const r of rows) {
          assert.equal(r.cut, false, `${r.name}: the name is cut (name column ${r.nameW} px)`);
          assert.ok(r.nameW >= 120, `${r.name}: the name column is only ${r.nameW} px`);
          assert.deepEqual(r.outside, [], `${r.name}: parts outside the table`);
          assert.equal(r.buttons, 3, `${r.name}: refresh, flash and delete are present`);
          assert.ok(r.page <= 1, `the page scrolls sideways by ${r.page} px`);
        }
        assert.deepEqual(h.exceptions, []);
      } finally { await h.close(); }
    });
  }
}
