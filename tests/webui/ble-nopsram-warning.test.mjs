// TASK-1693 (Robert 2026-11-08): on a board without PSRAM, BLE is off by default and no dialog
// appears after boot or when the Sensors page opens. The stability warning shows once, when the
// user switches BLE on and saves: "Enable anyway" stores satbleenable=true, then satbleriskack=true
// (one call after the other); "Keep BLE off" stores nothing. PSRAM boards get no warning.
// A unit that already has BLE on without the ack sees a hint in the BLE card, not a modal.
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. OTGW_DATA_DIR at an older
// copy of data/ shows it red (the old page opens the modal on the Sensors page by itself).
//
// Run: node --test tests/webui/ble-nopsram-warning.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';

function fakeDevice({ psram, bleEnable, riskAck, failPost }) {
  const st = { satbleenable: bleEnable, satbleriskack: riskAck };
  const posts = [];
  const api = (method, p, search, body) => {
    if (p.endsWith('/v2/settings') && method === 'POST') {
      const j = JSON.parse(body || '{}');
      posts.push(j.name + '=' + j.value);
      if (failPost === j.name) return { __status: 500 };
      if (j.name in st) st[j.name] = ('' + j.value) === 'true';
      return { ok: true };
    }
    if (p.endsWith('/v2/settings')) {
      return { settings: {
        hostname: { value: 'otgw', type: 's', maxlen: 32 },
        satbleenable: { value: st.satbleenable, type: 'b' },
        satbleriskack: { value: st.satbleriskack, type: 'b' },
      } };
    }
    if (p.endsWith('/v2/sat/ble/discovery')) {
      return { psram, ble_enable: st.satbleenable, risk_ack: st.satbleriskack, populated_slots: 0, sensors: [] };
    }
    return undefined;
  };
  return { api, posts, st };
}

async function openSensors(h) {
  assert.ok(await h.until(`!!document.querySelector('.connsum')`, 20000), 'v2.js started');
  await h.evalJs(`document.querySelector('button.seg[data-page="settings"]').click()`);
  assert.ok(await h.until(`!!document.querySelector('.rail-item')`, 15000), 'settings rail rendered');
  await h.evalJs(`(function(){ var r = Array.from(document.querySelectorAll('.rail-item'))
    .find(function (x) { return /sensor/i.test(x.textContent); }); if (r) r.click(); return !!r; })()`);
  assert.ok(await h.until(`!!document.querySelector('#setCols .srow[data-key="satbleenable"] input.sw')`, 15000),
    'the Enable BLE sensors row is shown');
}
const MODAL = `!!document.getElementById('bleRiskBackdrop')`;

test('no PSRAM, BLE already on without ack: no modal on the Sensors page, a hint in the BLE card', { skip }, async () => {
  const dev = fakeDevice({ psram: 0, bleEnable: true, riskAck: false });
  const h = await startPage({ api: dev.api });
  try {
    await openSensors(h);
    await h.sleep(5000);   // the BLE card polls discovery every 4 s; the old page opened the modal here
    assert.equal(await h.evalJs(MODAL), false, 'no warning modal without a user action');
    assert.ok(await h.until(`/PSRAM/.test((document.getElementById('setcard-ble') || {}).textContent || '')`, 8000),
      'the BLE card says BLE waits for the PSRAM warning to be accepted');
    assert.deepEqual(dev.posts, [], 'nothing was saved');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('no PSRAM: switching BLE on and saving shows the warning; Enable anyway stores enable, then the ack', { skip }, async () => {
  const dev = fakeDevice({ psram: 0, bleEnable: false, riskAck: false });
  const h = await startPage({ api: dev.api });
  try {
    await openSensors(h);
    assert.equal(await h.evalJs(MODAL), false, 'no modal before the user acts');
    await h.evalJs(`(function(){ var i = document.querySelector('#setCols .srow[data-key="satbleenable"] input.sw');
      i.checked = true; i.dispatchEvent(new Event('change')); document.getElementById('btnSave').click(); })()`);
    assert.ok(await h.until(MODAL, 8000), 'the stability warning opened on save');
    assert.ok(!dev.posts.includes('satbleenable=true'), 'BLE is not stored before the user accepts: ' + dev.posts);
    // TASK-1732: nothing was posted, so no "Settings saved" toast while the warning waits
    const toast = await h.evalJs(`(function(){ var t = document.getElementById('toast'); return t && t.classList.contains('show') ? t.textContent : ''; })()`);
    assert.doesNotMatch(toast, /Settings saved/, 'no "Settings saved" toast when nothing was saved: ' + toast);
    await h.evalJs(`Array.from(document.querySelectorAll('#bleRiskBackdrop button')).find(function (b) { return /anyway/i.test(b.textContent); }).click()`);
    assert.ok(await h.until(`!document.getElementById('bleRiskBackdrop')`, 5000), 'the warning closed');
    for (let i = 0; i < 40 && dev.posts.length < 2; i++) await h.sleep(100);
    assert.deepEqual(dev.posts, ['satbleenable=true', 'satbleriskack=true'], 'enable first, then the ack');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('no PSRAM: Enable anyway whose save fails says so (red "Save failed"), not silence', { skip }, async () => {
  // TASK-1732 review (agent 3): before, a failed satbleenable/satbleriskack POST on the consent
  // path was swallowed; with the "Settings saved" toast gone the user saw nothing at all.
  const dev = fakeDevice({ psram: 0, bleEnable: false, riskAck: false, failPost: 'satbleriskack' });
  const h = await startPage({ api: dev.api });
  try {
    await openSensors(h);
    await h.evalJs(`(function(){ var i = document.querySelector('#setCols .srow[data-key="satbleenable"] input.sw');
      i.checked = true; i.dispatchEvent(new Event('change')); document.getElementById('btnSave').click(); })()`);
    assert.ok(await h.until(MODAL, 8000), 'the stability warning opened on save');
    await h.evalJs(`Array.from(document.querySelectorAll('#bleRiskBackdrop button')).find(function (b) { return /anyway/i.test(b.textContent); }).click()`);
    const TOAST = `(function(){ var t = document.getElementById('toast'); return t && t.classList.contains('show') ? (t.classList.contains('err') ? 'ERR:' : '') + t.textContent : ''; })()`;
    assert.ok(await h.until(`/^ERR:Save failed/.test(${TOAST})`, 5000), 'red "Save failed" toast: ' + await h.evalJs(TOAST));
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('no PSRAM: Keep BLE off stores nothing', { skip }, async () => {
  const dev = fakeDevice({ psram: 0, bleEnable: false, riskAck: false });
  const h = await startPage({ api: dev.api });
  try {
    await openSensors(h);
    await h.evalJs(`(function(){ var i = document.querySelector('#setCols .srow[data-key="satbleenable"] input.sw');
      i.checked = true; i.dispatchEvent(new Event('change')); document.getElementById('btnSave').click(); })()`);
    assert.ok(await h.until(MODAL, 8000), 'the stability warning opened on save');
    await h.evalJs(`Array.from(document.querySelectorAll('#bleRiskBackdrop button')).find(function (b) { return /keep/i.test(b.textContent); }).click()`);
    await h.sleep(1500);
    assert.ok(!dev.posts.includes('satbleenable=true') && !dev.posts.includes('satbleriskack=true'), 'nothing stored: ' + dev.posts);
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('PSRAM board: switching BLE on saves without a warning', { skip }, async () => {
  const dev = fakeDevice({ psram: 1, bleEnable: false, riskAck: false });
  const h = await startPage({ api: dev.api });
  try {
    await openSensors(h);
    await h.evalJs(`(function(){ var i = document.querySelector('#setCols .srow[data-key="satbleenable"] input.sw');
      i.checked = true; i.dispatchEvent(new Event('change')); document.getElementById('btnSave').click(); })()`);
    for (let i = 0; i < 40 && !dev.posts.length; i++) await h.sleep(100);
    assert.deepEqual(dev.posts, ['satbleenable=true']);
    assert.equal(await h.evalJs(MODAL), false, 'no warning on a PSRAM board');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});
