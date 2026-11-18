// TASK-2025 (Robert's option A, DM 1572600155788546051, supersedes the TASK-1693 warning flow): on a board without
// PSRAM BLE cannot be switched on. The firmware refuses it (bleActive() needs PSRAM, the SATbleenable setter keeps it
// off, POST answers 409); the web UI does not post the switch, shows a red "BLE needs PSRAM" toast and a note in the
// BLE card, and no longer offers "Enable anyway". On .61 (beta.370) the accepted warning let the scanner exhaust the
// heap and hang the unit. PSRAM boards switch BLE on as before.
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. OTGW_DATA_DIR at an older copy of data/
// shows it red (the old page opens the "Enable anyway" warning instead).
//
// Run: node --test tests/webui/ble-nopsram-warning.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';

function fakeDevice({ psram, bleEnable, riskAck }) {
  const st = { satbleenable: bleEnable, satbleriskack: riskAck };
  const posts = [];
  const api = (method, p, search, body) => {
    if (p.endsWith('/v2/settings') && method === 'POST') {
      const j = JSON.parse(body || '{}');
      posts.push(j.name + '=' + j.value);
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
const SWITCH = `document.querySelector('#setCols .srow[data-key="satbleenable"] input.sw')`;
const TOAST = `(function(){ var t = document.getElementById('toast'); return t && t.classList.contains('show') ? (t.classList.contains('err') ? 'ERR:' : '') + t.textContent : ''; })()`;
const switchOnAndSave = `(function(){ var i = document.querySelector('#setCols .srow[data-key="satbleenable"] input.sw');
  i.checked = true; i.dispatchEvent(new Event('change')); document.getElementById('btnSave').click(); })()`;

test('no PSRAM: the BLE card says BLE needs PSRAM; no modal, nothing saved', { skip }, async () => {
  const dev = fakeDevice({ psram: 0, bleEnable: false, riskAck: true });   // .61: an old ack was stored
  const h = await startPage({ api: dev.api });
  try {
    await openSensors(h);
    assert.ok(await h.until(`/needs PSRAM/.test((document.getElementById('setcard-ble') || {}).textContent || '')`, 8000),
      'the BLE card says BLE needs PSRAM');
    // Robert, DM 1572613627528085586: "Niet verbergen maar disablen. Geen PSRAM dan mag je de schakelaar niet aanraken."
    assert.ok(await h.until(`${SWITCH}.disabled === true`, 8000), 'the BLE switch is disabled without PSRAM');
    assert.match(await h.evalJs(`(${SWITCH}.closest('.srow') || {}).title || ''`), /needs PSRAM/, 'the row says why');
    assert.equal(await h.evalJs(MODAL), false, 'no warning modal');
    assert.deepEqual(dev.posts, [], 'nothing was saved');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('no PSRAM: switching BLE on and saving posts nothing and shows a red "BLE needs PSRAM"', { skip }, async () => {
  const dev = fakeDevice({ psram: 0, bleEnable: false, riskAck: true });
  const h = await startPage({ api: dev.api });
  try {
    await openSensors(h);
    await h.evalJs(switchOnAndSave);
    assert.ok(await h.until(`/^ERR:BLE needs PSRAM/.test(${TOAST})`, 5000), 'red needs-PSRAM toast: ' + await h.evalJs(TOAST));
    await h.sleep(1000);
    assert.ok(!dev.posts.includes('satbleenable=true'), 'BLE is not stored: ' + dev.posts);
    assert.equal(await h.evalJs(MODAL), false, 'no "Enable anyway" warning any more');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('PSRAM board: switching BLE on saves without a warning or note', { skip }, async () => {
  const dev = fakeDevice({ psram: 1, bleEnable: false, riskAck: false });
  const h = await startPage({ api: dev.api });
  try {
    await openSensors(h);
    await h.sleep(1500);   // let the BLE poll answer: a PSRAM board must keep the switch usable
    assert.equal(await h.evalJs(`${SWITCH}.disabled`), false, 'the BLE switch is usable with PSRAM');
    await h.evalJs(switchOnAndSave);
    for (let i = 0; i < 40 && !dev.posts.length; i++) await h.sleep(100);
    assert.deepEqual(dev.posts, ['satbleenable=true']);
    assert.equal(await h.evalJs(MODAL), false, 'no warning on a PSRAM board');
    assert.doesNotMatch(await h.evalJs(`(document.getElementById('setcard-ble') || {}).textContent || ''`), /needs PSRAM/);
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});
