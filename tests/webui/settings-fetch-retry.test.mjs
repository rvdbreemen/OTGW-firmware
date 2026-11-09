// TASK-1743 (found on the bench, .61 beta.157, 2026-11-08): fetchSettings() runs once at page
// start and again only when the user opens Settings. One busy 503 on that startup call left
// setData empty, so the SAT page's 'SAT settings' section stayed blank. The fix retries a 503 or
// network error with fetchWithRetry (sequential, doubling backoff, so the 2-in-flight cap holds).
//   1. First GET /api/v2/settings answers 503, later ones 200: the SAT page's Control view shows
//      the SAT settings rows without the user ever opening Settings.
//   2. The device stays busy: the retries stop (bounded count, no tight loop).
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device.
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/settings-fetch-retry.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const BUSY = { __status: 503, error: 'Server busy: too many concurrent requests, please retry' };
const B = (value) => ({ value, type: 'b' });
const SETTINGS = {
  satenabled: B(true), sat_onboarded: B(true), satpushsetpoint: B(false), satfollowthermostat: B(true),
  mqttbroker: { value: 'broker.local', type: 's', maxlen: 30 },
};
const SAT = { enabled: true, active: true, fallback_active: false, target_temp: 20, dhw_enabled: false,
  simulation: false, sim_available: true };
const settingsGets = (h) => h.log.filter((l) => l.startsWith('GET /api/v2/settings')).length;

test('a busy 503 on the startup settings fetch is retried, so the SAT page shows its SAT settings', { skip: !CHROME && 'no Chrome' }, async () => {
  let gets = 0;
  const h = await startPage({ api: (m, p) => {
    if (p.endsWith('/v2/settings') && m === 'GET') return ++gets === 1 ? BUSY : SETTINGS;
    if (p.endsWith('/v2/sat/status')) return SAT;
    return undefined;
  } });
  try {
    assert.ok(await h.until(`!!document.querySelector('.seg[data-page="sat"]')`, 30000), 'page loaded');
    await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
    await h.evalJs(`document.querySelector('.satview-btn[data-sv="control"]').click()`);
    // The user never opens Settings: only the startup fetch (and its retries) can fill the form.
    const rows = await h.until(`!!document.querySelector('#satSetL2 .srow[data-key="satpushsetpoint"] input')`, 10000);
    assert.ok(rows, 'SAT settings rows render after a 503 on the first settings GET (GETs: ' + settingsGets(h) + ')');
    assert.ok(settingsGets(h) >= 2, 'the settings GET was retried');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('while the device stays busy the settings retries back off and stop after 5 tries', { skip: !CHROME && 'no Chrome' }, async () => {
  const at = [];   // arrival time of every settings GET at the fake device
  const h = await startPage({ api: (m, p) => {
    if (p.endsWith('/v2/settings') && m === 'GET') { at.push(Date.now()); return BUSY; }
    if (p.endsWith('/v2/sat/status')) return SAT;
    return undefined;
  } });
  try {
    assert.ok(await h.until(`!!document.querySelector('.seg[data-page="sat"]')`, 30000), 'page loaded');
    // 400 + 800 + 1600 + 3200 ms of backoff is 6 s; a 6th try would come 6.4 s after the 5th.
    await h.sleep(16000);
    assert.equal(at.length, 5, 'exactly 5 settings GETs while busy (no unbounded retry), got ' + at.length);
    const gaps = at.slice(1).map((t, i) => t - at[i]);
    // Doubling backoff: every gap is at least 300 ms and clearly longer than the one before it.
    assert.ok(gaps[0] >= 300, 'first retry waits (backoff), gaps ' + JSON.stringify(gaps));
    for (let i = 1; i < gaps.length; i++) assert.ok(gaps[i] >= gaps[i - 1] * 1.5, 'backoff grows, gaps ' + JSON.stringify(gaps));
  } finally { await h.close(); }
});
