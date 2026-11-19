// TASK-2914: since TASK-2910 (beta.389) the device's busy path closes the connection without a response
// when its heap is starved, instead of answering 503 + Retry-After. In the browser that is a fetch that
// rejects (TypeError, connection reset). This test drives the REAL v2.html/v2.js in headless Chrome against
// a fake device that resets the connection (and, like the device, closes every API connection after its
// answer), and checks that every handler that keeps its state on a busy
// 503 (TASK-1741, TASK-1747) also keeps it on a reset:
//   1. SAT page: after good data, a reset keeps the page and its data (fetchWithRetry retries, then keeps).
//   2. SIMULATION header badge: a reset on /health leaves it as it is.
//   3. Injected-override badge on the connection map: a reset on otgw/overrides keeps the rows
//      (red before TASK-2914: the catch emptied the rows, so the badge vanished).
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/rest-reset-keeps-state.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const RESET = { __reset: true };
const SAT = { enabled: true, active: true, fallback_active: false, target_temp: 20.5, room_temp: 20.1,
  outside_temp: 8.2, heating_source: 1, heating_source_detected: 1 };
const HEALTH = { health: { otgwconnected: true, mqttconnected: true, boiler_age_s: 3, wifirssi: -60,
  otcommandinterface: 'PIC', thermostatconnected: true, boilerconnected: true } };
const OVR = { overrides: [{ id: 1, label: 'Control setpoint', kind: 'answer', value: 45.0, age_s: 3 }] };

function count(h, s) { return h.log.filter((l) => l.includes(s)).length; }
async function openConnTab(h) {
  // Monitor > Connection runs a health poll at once (v2.js: the mconn tab calls fetchConn()).
  await h.evalJs(`(function(){ var s = document.querySelector('.seg[data-page="monitor"]'); if (s) s.click();
    var c = document.querySelector('.subtab[data-tab="mconn"]'); if (c) c.click(); })()`);
}

test('SAT page keeps its last data when a poll gets a connection reset', { skip: !CHROME && 'no Chrome' }, async () => {
  let reset = false;
  const h = await startPage({ apiConnClose: true, api: (m, p) => {
    if (p.endsWith('/v2/sat/status')) return reset ? RESET : SAT;
    return undefined;
  } });
  try {
    await h.until(`!!document.querySelector('.seg[data-page="sat"]')`);
    await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
    const shown = await h.until(`(function(){ var b = document.getElementById('satBody'), p = document.getElementById('satPill');
      return b && b.style.display !== 'none' && p && /Heating|Idle|Off|Takeover/.test(p.textContent); })()`);
    assert.ok(shown, 'the SAT page shows its body with good data');
    reset = true;
    const n0 = count(h, '/v2/sat/status');
    // One full failing fetchSatPage: the next 5 s poll plus fetchWithRetry's 400 + 800 ms backoff, plus margin.
    await h.sleep(8500);
    const st = await h.evalJs(`(function(){ var b = document.getElementById('satBody'), nd = document.getElementById('satNoData');
      return { body: b ? b.style.display : null, banner: nd ? nd.style.display : null }; })()`);
    const n = count(h, '/v2/sat/status') - n0;
    assert.ok(n >= 3, 'a poll and its retries ran while resetting (' + n + ' requests)');
    assert.ok(n <= 12, 'no retry storm: ' + n + ' sat/status requests in 8.5 s');
    assert.notEqual(st.body, 'none', 'the SAT body stays visible after a reset: ' + JSON.stringify(st));
    assert.equal(st.banner, 'none', 'no "Couldn\'t load" banner over good data: ' + JSON.stringify(st));
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('SIMULATION badge stays while a simulation runs and a health poll gets a reset', { skip: !CHROME && 'no Chrome' }, async () => {
  let reset = false;
  const h = await startPage({ apiConnClose: true, api: (m, p) => {
    if (p.endsWith('/v2/health')) return reset ? RESET : { health: { ...HEALTH.health, simulation_active: true } };
    return undefined;
  } });
  try {
    assert.ok(await h.until(`!document.getElementById('hdrSim').classList.contains('hidden')`), 'badge shows while simulating');
    reset = true;
    const n0 = count(h, '/v2/health');
    await openConnTab(h);
    for (let i = 0; i < 300 && count(h, '/v2/health') < n0 + 1; i++) await h.sleep(100);
    await h.sleep(500);
    assert.ok(count(h, '/v2/health') >= n0 + 1, 'a health poll ran while resetting');
    assert.equal(await h.evalJs(`document.getElementById('hdrSim').classList.contains('hidden')`), false,
      'a reset must not hide the SIMULATION badge');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('Injected-override badge stays when otgw/overrides gets a reset', { skip: !CHROME && 'no Chrome' }, async () => {
  let reset = false;
  const h = await startPage({ apiConnClose: true, api: (m, p) => {
    if (p.endsWith('/v2/health')) return HEALTH;
    if (p.endsWith('/v2/otgw/overrides')) return reset ? RESET : OVR;
    return undefined;
  } });
  try {
    await openConnTab(h);
    const shown = await h.until(`(function(){ var g = document.getElementById('cnOvr'), tx = document.getElementById('cnOvrTxt');
      return !!(g && g.style.display !== 'none' && tx && /1/.test(tx.textContent)); })()`, 20000);
    assert.ok(shown, 'the override badge shows one injected override');
    reset = true;
    const n0 = count(h, '/v2/otgw/overrides'), h0 = count(h, '/v2/health');
    await openConnTab(h);
    for (let i = 0; i < 300 && count(h, '/v2/otgw/overrides') < n0 + 1; i++) await h.sleep(100);
    await h.sleep(2500);   // room for one retry (fetchWithRetry 400 ms) and the render
    const n = count(h, '/v2/otgw/overrides') - n0;
    assert.ok(n >= 1, 'an overrides poll ran while resetting');
    // Each health poll chains one overrides fetch (fetchConn), which tries at most twice (fetchWithRetry, tries 2).
    // With Connection: close (apiConnClose) Chrome has no reused socket to resend on, so the count is exact.
    const polls = count(h, '/v2/health') - h0;
    assert.ok(n <= 2 * (polls + 1), 'no retry storm: ' + n + ' overrides requests for ' + polls + ' health polls; log: ' + JSON.stringify(h.log.slice(-24)));
    const st = await h.evalJs(`(function(){ var g = document.getElementById('cnOvr'), tx = document.getElementById('cnOvrTxt');
      return { display: g ? g.style.display : null, text: tx ? tx.textContent : null }; })()`);
    assert.notEqual(st.display, 'none', 'a reset must not hide the override badge: ' + JSON.stringify(st));
    assert.match(st.text || '', /1/, 'the badge still counts the override: ' + JSON.stringify(st));
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});
