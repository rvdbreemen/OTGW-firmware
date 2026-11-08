// TASK-1737 (Robert, #dev-sat-mqtt 2026-11-08 17:48): 'No setting should be editable when SAT is
// turned off. It should be grayed out. ... Only when it's enabled it should be editable.' and 17:50:
// 'The SAT switch should always be usable.' So with SAT off every SAT setting on the SAT page (the
// settingRow fields under Control/Technical and the page's own controls) is disabled and cannot be
// saved; the Enable switch and Re-run setup stay usable; non-SAT settings stay editable.
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device.
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/sat-settings-locked-when-off.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage } from './lib/page-harness.mjs';

const B = (value) => ({ value, type: 'b' });
function settings(satOn, onboarded = true) {
  return {
    satenabled: B(satOn), sat_onboarded: B(onboarded),
    satpushsetpoint: B(false), satfollowthermostat: B(true),
    mqttbroker: { value: 'broker.local', type: 's', maxlen: 30 },
  };
}
// Flat, as the device sends it (SATcontrol.ino: GET /api/v2/sat/status is one top-level object).
const status = (dev) => ({ enabled: dev.enabled, active: dev.enabled || dev.fallback, fallback_active: dev.fallback,
  target_temp: 20, dhw_enabled: false, simulation: false, sim_available: true });

const SAT_KEYS = ['satpushsetpoint', 'satfollowthermostat'];
const PAGE_IDS = ['satUp', 'satDown', 'satMaxSlider', 'satModSlider', 'satDhwEnabled', 'satDetectLoc'];

async function open(satOn, onboarded = true, fallback = false) {
  // A small stateful device: the Enable switch (POST sat/enable) changes what sat/status and settings report.
  const dev = { enabled: satOn, fallback };
  const h = await startPage({ api: (m, p, q, body) => {
    if (p.endsWith('/v2/settings')) return m === 'POST' ? { status: 'ok' } : settings(dev.enabled, onboarded);
    if (p.endsWith('/v2/sat/enable') && m === 'POST') { dev.enabled = ('' + body).trim() === '1'; return { status: 'ok' }; }
    if (p.endsWith('/v2/sat/status')) return status(dev);
    return undefined;
  } });
  h.dev = dev;
  assert.ok(await h.until(`!!document.querySelector('.seg[data-page="sat"]')`, 30000), 'page loaded');
  for (let i = 0; i < 100 && !h.log.some((l) => l.startsWith('GET /api/v2/settings')); i++) await h.sleep(100);
  await h.sleep(500);
  await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
  await h.evalJs(`document.querySelector('.satview-btn[data-sv="control"]').click()`);
  assert.ok(await h.until(`!!document.querySelector('#satSetL2 .srow[data-key="satpushsetpoint"] input')`, 20000), 'SAT form rendered');
  for (let i = 0; i < 100 && !h.log.some((l) => l.startsWith('GET /api/v2/sat/status')); i++) await h.sleep(100);
  await h.sleep(800);
  return h;
}
const disabledOf = (h, sel) => h.evalJs(`(function(){ var e = document.querySelector(${JSON.stringify(sel)}); return e ? e.disabled : null; })()`);

test('with SAT off, SAT settings are locked and cannot be saved; the switch and other settings stay usable', async () => {
  const h = await open(false);
  try {
    for (const k of SAT_KEYS) assert.equal(await disabledOf(h, `#satSetL2 .srow[data-key="${k}"] input`), true, `${k} is locked`);
    for (const id of PAGE_IDS) assert.equal(await disabledOf(h, `#${id}`), true, `#${id} is locked`);
    assert.equal(await disabledOf(h, '#satEnable'), false, 'the Enable switch stays usable');
    assert.equal(await disabledOf(h, '#satRerunSetup'), false, 'Re-run setup (the wizard) stays usable');
    assert.match(await h.evalJs(`(document.getElementById('satSetL2') || {}).textContent || ''`), /SAT is off/, 'the form says why');
    // A forced change (script) on a locked field must still not be saved.
    const n = h.log.length;
    await h.evalJs(`(function(){ var i = document.querySelector('#satSetL2 .srow[data-key="satpushsetpoint"] input');
      i.checked = true; i.dispatchEvent(new Event('change')); var b = document.getElementById('satBtnSave'); if (b) b.click(); })()`);
    await h.sleep(1500);
    const posts = h.log.slice(n).filter((l) => l.startsWith('POST /api/v2/settings') && /satpushsetpoint/.test(l));
    assert.deepEqual(posts, [], 'no SAT setting is POSTed while SAT is off');
    // A non-SAT setting stays editable.
    await h.evalJs(`document.querySelector('.seg[data-page="settings"]').click()`);
    await h.until(`(function(){ var r = document.querySelector('.rail-item[data-cat="mqtt"]'); if (!r) return false; r.click(); return true; })()`, 10000);
    assert.ok(await h.until(`!!document.querySelector('.srow[data-key="mqttbroker"] input')`, 10000), 'MQTT form rendered');
    assert.equal(await disabledOf(h, '.srow[data-key="mqttbroker"] input'), false, 'a non-SAT setting stays editable');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('with SAT on, SAT settings are editable', async () => {
  const h = await open(true);
  try {
    // Under load each CDP round trip can stall: wait for the end state once, then read it in one call.
    const sels = [...SAT_KEYS.map((k) => `#satSetL2 .srow[data-key="${k}"] input`), ...PAGE_IDS.map((id) => `#${id}`)];
    const read = `(function(){ var s = ${JSON.stringify(sels)}; return { dis: s.map(function (q) { var e = document.querySelector(q); return e ? e.disabled : null; }),
      note: ((document.getElementById('satSetL2') || {}).textContent || '').indexOf('SAT is off') >= 0 }; })()`;
    await h.until(`(function(){ var r = ${read}; return !r.note && r.dis.every(function (d) { return d === false; }); })()`, 30000);
    const r = await h.evalJs(read);
    sels.forEach((q, i) => assert.equal(r.dis[i], false, `${q} is editable`));
    assert.equal(r.note, false, 'no "SAT is off" note');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('with SAT off and not set up, switching SAT on still starts the setup wizard', async () => {
  const h = await open(false, false);
  try {
    await h.evalJs(`document.getElementById('satEnable').click()`);
    assert.ok(await h.until(`!!document.getElementById('satOnbBack')`, 10000), 'the wizard opens');
    assert.equal(await disabledOf(h, '#satOnbBetaOk'), false, 'its first step is usable (the lock does not touch the wizard)');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

const ROW = (k) => `#satSetL2 .srow[data-key="${k}"] input`;

test('during a SAT takeover (fallback active, switch off) the SAT settings stay locked: they follow the switch', async () => {
  const h = await open(false, true, true);
  try {
    for (const k of SAT_KEYS) assert.equal(await disabledOf(h, ROW(k)), true, `${k} is locked during a takeover`);
    for (const id of PAGE_IDS) assert.equal(await disabledOf(h, `#${id}`), true, `#${id} is locked during a takeover`);
    assert.equal(await disabledOf(h, '#satEnable'), false, 'the Enable switch stays usable');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('switching SAT on unlocks the SAT settings without a reload', async () => {
  const h = await open(false);
  try {
    assert.equal(await disabledOf(h, ROW('satpushsetpoint')), true, 'locked while off');
    await h.evalJs(`(function(){ var e = document.getElementById('satEnable'); e.checked = true; e.dispatchEvent(new Event('change')); })()`);
    assert.ok(await h.until(`(function(){ var i = document.querySelector(${JSON.stringify(ROW('satpushsetpoint'))}); return !!i && i.disabled === false; })()`, 15000), 'the row unlocks once the switch has saved');
    for (const id of PAGE_IDS) assert.equal(await disabledOf(h, `#${id}`), false, `#${id} is usable again`);
    assert.doesNotMatch(await h.evalJs(`(document.getElementById('satSetL2') || {}).textContent || ''`), /SAT is off/, 'the note is gone');
    assert.ok(h.log.some((l) => l.startsWith('POST /api/v2/sat/enable')), 'the switch saved');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('an edit made while SAT was on is not saved after SAT went off, and the user is told', async () => {
  const h = await open(true);
  try {
    await h.evalJs(`(function(){ var i = document.querySelector(${JSON.stringify(ROW('satpushsetpoint'))}); i.checked = true; i.dispatchEvent(new Event('change')); })()`);
    assert.ok(await h.until(`(document.getElementById('satDirtyCount') || {}).textContent === '1'`, 5000), 'the edit is dirty');
    h.dev.enabled = false;   // SAT switched off elsewhere (another tab, MQTT)
    assert.ok(await h.until(`document.querySelector(${JSON.stringify(ROW('satpushsetpoint'))}).disabled === true`, 15000), 'the next SAT poll locks the row');
    const n = h.log.length;
    await h.evalJs(`document.getElementById('satBtnSave').click()`);
    await h.sleep(1500);
    assert.deepEqual(h.log.slice(n).filter((l) => l.startsWith('POST /api/v2/settings')), [], 'nothing is POSTed');
    assert.match(await h.evalJs(`document.getElementById('toast').textContent`), /SAT is off: not saved: .+/, 'the dropped edit is named');
    assert.equal(await h.evalJs(`(document.getElementById('satDirtyCount') || {}).textContent`), '0', 'nothing left to save');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});
