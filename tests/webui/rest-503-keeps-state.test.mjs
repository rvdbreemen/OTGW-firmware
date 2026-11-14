// TASK-1741 (Robert 2026-11-08: R1 only): a busy 503 from the device's REST in-flight cap is
// not a state. Two open tabs get ~16 of them per minute (TASK-1736 run A), so the three
// handlers that turned one into something the user sees now treat it as "no change":
//   1. SAT page: after good data, a 503 keeps the page and its data (no 'Couldn't load' banner).
//   2. SAT setup wizard, source health check: a failed request shows 'Unknown' in grey, not a
//      false 'Offline' / 'Not connected' warning.
//   3. SIMULATION header badge: a 503 leaves it as it is (a running simulation stays flagged). Since TASK-1915
//      the badge follows simulation_active in the health answer, so the 503 that matters is on /health.
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device.
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/rest-503-keeps-state.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const BUSY = { __status: 503, error: 'Server busy: too many concurrent requests, please retry' };
// sat/status is a flat object on the wire (SATcontrol.ino:4251 onward).
const SAT = { enabled: true, active: true, fallback_active: false, target_temp: 20.5, room_temp: 20.1,
  outside_temp: 8.2, heating_source: 1, heating_source_detected: 1 };
const HEALTH = { health: { otgwconnected: true, mqttconnected: true, boiler_age_s: 3, wifirssi: -60 } };

test('SAT page keeps its last data when a poll gets 503', { skip: !CHROME && 'no Chrome' }, async () => {
  let busy = false;
  const h = await startPage({ api: (m, p) => {
    if (p.endsWith('/v2/sat/status')) return busy ? BUSY : SAT;
    return undefined;
  } });
  try {
    await h.until(`!!document.querySelector('.seg[data-page="sat"]')`);
    await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
    // Good data has rendered: the status pill is filled by renderSatPage (not the static default).
    const shown = await h.until(`(function(){ var b = document.getElementById('satBody'), p = document.getElementById('satPill');
      return b && b.style.display !== 'none' && p && /Heating|Idle|Off|Takeover/.test(p.textContent); })()`);
    assert.ok(shown, 'the SAT page shows its body with good data');
    busy = true;
    const n0 = h.log.filter((l) => l.includes('/v2/sat/status')).length;
    // Upper bound for one full failing fetchSatPage: the next 5 s poll plus fetchWithRetry's
    // 400 + 800 ms backoff, plus margin. Other callers of sat/status also hit the log, so a
    // request count alone does not mark the end of the page's retries.
    await h.sleep(8500);
    const st = await h.evalJs(`(function(){ var b = document.getElementById('satBody'), nd = document.getElementById('satNoData');
      return { body: b ? b.style.display : null, banner: nd ? nd.style.display : null, text: nd ? nd.textContent : null }; })()`);
    assert.ok(h.log.filter((l) => l.includes('/v2/sat/status')).length >= n0 + 3, 'a poll and its retries ran while busy');
    assert.notEqual(st.body, 'none', 'the SAT body stays visible after a 503: ' + JSON.stringify(st));
    assert.equal(st.banner, 'none', 'no "Couldn\'t load" banner over good data: ' + JSON.stringify(st));
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('SAT wizard health check shows Unknown, not a false warning, when /health gets 503', { skip: !CHROME && 'no Chrome' }, async () => {
  const h = await startPage({ api: (m, p) => {
    if (p.endsWith('/v2/sat/status')) return SAT;
    if (p.endsWith('/v2/health')) return BUSY;
    return undefined;
  } });
  try {
    await h.until(`!!document.querySelector('.seg[data-page="sat"]')`);
    await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
    // The Re-run button is wired when the SAT page renders (v2.js rb._wired), so click until the wizard opens.
    assert.ok(await h.until(`(function(){ if (document.getElementById('satOnbBack')) return true;
      var b = document.getElementById('satRerunSetup'); if (b) b.click(); return !!document.getElementById('satOnbBack'); })()`),
      'the setup wizard opens from Re-run setup');
    for (const a of ['begin', 'to-mfr', 'to-tuning', 'to-sources']) {
      const found = await h.until(`!!document.querySelector('#satOnbBack [data-sa="${a}"]')`, 8000);
      const why = found ? '' : await h.evalJs(`(function(){ var b = document.getElementById('satOnbBack');
        return b ? 'wizard: ' + Array.prototype.map.call(b.querySelectorAll('[data-sa]'), function (x) { return x.getAttribute('data-sa'); }).join(',')
                 : 'no wizard; rerun button ' + (document.getElementById('satRerunSetup') ? 'present' : 'absent'); })()`);
      assert.ok(found, 'wizard button ' + a + ' (' + why + ')');
      await h.evalJs(`document.querySelector('#satOnbBack [data-sa="${a}"]').click()`);
    }
    const rows = await h.until(`(function(){ var e = document.getElementById('satOnbHealth');
      if (!e || /Checking sources/.test(e.textContent)) return null;
      return Array.prototype.map.call(e.children, function (r) { return r.textContent; }); })()`, 8000);
    assert.ok(Array.isArray(rows) && rows.length === 4, 'four source rows: ' + JSON.stringify(rows));
    const ot = rows.find((r) => /OpenTherm bus/.test(r)), mq = rows.find((r) => /MQTT/.test(r));
    assert.match(ot, /Unknown/, 'OT row says Unknown: ' + ot);
    assert.match(mq, /Unknown/, 'MQTT row says Unknown: ' + mq);
    assert.doesNotMatch(ot + mq, /Offline|Not connected/, 'no false warning: ' + JSON.stringify(rows));
    assert.match(rows.find((r) => /Room temperature/.test(r)), /20\.1/, 'the sat/status rows still show their values');
    const dot = await h.evalJs(`(function(){ var r = document.getElementById('satOnbHealth').children[0];
      return r.querySelector('span').style.background; })()`);
    assert.match(dot || '', /--muted/, 'the unknown dot is grey (var(--muted)): ' + dot);
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('SIMULATION badge stays while a simulation runs and a poll gets 503', { skip: !CHROME && 'no Chrome' }, async () => {
  let busy = false;
  const h = await startPage({ api: (m, p) => {
    if (p.endsWith('/v2/health')) return busy ? BUSY : { health: { ...HEALTH.health, simulation_active: true } };
    return undefined;
  } });
  try {
    assert.ok(await h.until(`!document.getElementById('hdrSim').classList.contains('hidden')`), 'badge shows while simulating');
    busy = true;
    const n0 = h.log.filter((l) => l.includes('/v2/health')).length;
    // Monitor > Connection runs a health poll at once (v2.js: the mconn tab calls fetchConn()).
    await h.evalJs(`(function(){ var s = document.querySelector('.seg[data-page="monitor"]'); if (s) s.click();
      var c = document.querySelector('.subtab[data-tab="mconn"]'); if (c) c.click(); })()`);
    for (let i = 0; i < 300 && h.log.filter((l) => l.includes('/v2/health')).length < n0 + 1; i++) await h.sleep(100);
    await h.sleep(500);
    assert.ok(h.log.filter((l) => l.includes('/v2/health')).length >= n0 + 1, 'a health poll ran while busy');
    const hidden = await h.evalJs(`document.getElementById('hdrSim').classList.contains('hidden')`);
    assert.equal(hidden, false, 'a 503 must not hide the SIMULATION badge');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});
