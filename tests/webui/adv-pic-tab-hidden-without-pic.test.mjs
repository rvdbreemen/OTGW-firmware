// TASK-1843 (Robert 2026-11-12): on a unit without a PIC (OT-Direct) Advanced opened on "PIC firmware"
// showing only "No PIC detected". The tab is now hidden there and Advanced opens on Debug Information;
// with a PIC nothing changes. Drives the REAL v2.html/v2.js in headless Chrome against a fake device.
// OTGW_DATA_DIR=<dir> serves an older data/ to see it red.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';
const dev = (iface) => (m, p) => (p === '/api/v2/device/info' ? { device: { otcommandinterface: iface, picavailable: iface === 'PIC' } } : undefined);
const STATE = `(function(){ var t = document.querySelector('#advTabs .subtab[data-adv="pic"]');
  var a = document.querySelector('#advTabs .subtab.active');
  return { picHidden: !!t && (t.hidden || getComputedStyle(t).display === 'none'), active: a ? a.dataset.adv : null,
    panel: (document.querySelector('[id^="adv-"].active') || {}).id || null }; })()`;

async function open(iface, storedTab) {
  const before = storedTab ? `try { localStorage.setItem('otgw-v2-advtab', '${storedTab}'); } catch (e) {}` : `try { localStorage.removeItem('otgw-v2-advtab'); } catch (e) {}`;
  const h = await startPage({ api: dev(iface), beforeScript: before });
  await h.until(`!!document.querySelector('.connsum')`);
  await h.evalJs(`document.querySelector('button.seg[data-page="advanced"]').click(); 1`);
  await h.sleep(1500);
  const s = await h.evalJs(STATE);
  const ex = h.exceptions.slice();
  await h.close();
  return { s, ex };
}

test('no PIC: the PIC firmware tab is hidden and Advanced opens on Debug Information', { skip }, async () => {
  const { s, ex } = await open('OTDirect');
  assert.equal(s.picHidden, true, 'PIC firmware tab hidden');
  assert.equal(s.active, 'debug'); assert.equal(s.panel, 'adv-debug');
  assert.deepEqual(ex, []);
});

test('no PIC, Debug remembered: the PIC tab is still hidden', { skip }, async () => {
  const { s } = await open('OTDirect', 'debug');
  assert.equal(s.picHidden, true); assert.equal(s.active, 'debug');
});

test('with a PIC nothing changes: the tab shows and opens first', { skip }, async () => {
  const { s, ex } = await open('PIC');
  assert.equal(s.picHidden, false); assert.equal(s.active, 'pic'); assert.equal(s.panel, 'adv-pic');
  assert.deepEqual(ex, []);
});
