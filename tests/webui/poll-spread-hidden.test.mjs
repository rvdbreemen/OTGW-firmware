// TASK-1741 (Robert 2026-11-09: "spread + pause hidden"): two open tabs got ~16 REST 503s per
// minute (TASK-1736 run A), because every tab registered its periodic pollers in the same init()
// tick: every 30 s one tab fired health (+ its follow-ups), sat/status and device/time at once,
// and two tabs opened together fired in step, against a server cap of 2 REST requests in flight
// (ADR-165) and 4 connections holding TX (ADR-188).
//   1. Within one tab, the slow pollers (health 15 s, sat/status 30 s, weather 60 s) never
//      dispatch together.
//   2. Each tab has its own phase: the first periodic health poll moves with Math.random.
//   3. A hidden tab sends no health, sat/status, weather or discovery polls, and refreshes once
//      when it is shown again.
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. A beforeScript runs
// every page timer SCALE times faster and stamps each fetch dispatch in virtual ms since the
// page registered its first timer (about init()), so 150 s of polling take ~3 s.
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/poll-spread-hidden.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const SCALE = 50;
const HEALTH = { health: { otgwconnected: true, mqttconnected: true, boiler_age_s: 3, wifirssi: -60 } };

function instrument(random, scale = SCALE) {
  return `(function () {
    var S = ${scale}, st = window.setTimeout, si = window.setInterval, f0 = window.fetch;
    var t0 = null, dn = Date.now.bind(Date), base = dn(), p0 = performance.now();
    function sc(d) { if (t0 === null) t0 = performance.now(); return (+d || 0) / S; }
    // Date.now runs S times faster too, so time-based checks in v2.js see virtual time.
    Date.now = function () { return Math.round(base + (performance.now() - p0) * S); };
    // init() registers setTimeout(fetchConn, 400): the anchor for per-tab phases.
    window.__anchor = null;
    window.setTimeout = function (fn, d) {
      if (window.__anchor === null && fn && fn.name === 'fetchConn') window.__anchor = (performance.now() - (t0 || 0)) * S;
      var a = [].slice.call(arguments, 2); return st.apply(window, [fn, sc(d)].concat(a)); };
    window.setInterval = function (fn, d) { var a = [].slice.call(arguments, 2); return si.apply(window, [fn, sc(d)].concat(a)); };
    window.__req = [];
    window.fetch = function (u) {
      window.__req.push({ u: String(u && u.url || u), t: (performance.now() - (t0 || 0)) * S });
      return f0.apply(this, arguments);
    };
    ${random === undefined ? '' : 'Math.random = function () { return ' + random + '; };'}
    window.__hidden = false;
    Object.defineProperty(Document.prototype, 'hidden', { configurable: true, get: function () { return window.__hidden; } });
    Object.defineProperty(Document.prototype, 'visibilityState', { configurable: true, get: function () { return window.__hidden ? 'hidden' : 'visible'; } });
  })();`;
}

const api = (m, p) => (p.endsWith('/v2/health') ? HEALTH : undefined);
const times = (reqs, tail) => reqs.filter((r) => r.u.endsWith(tail)).map((r) => r.t);
const reqs = (h) => h.evalJs('JSON.stringify(window.__req)').then((s) => JSON.parse(s || '[]'));

test('one tab: health, sat/status and weather polls never dispatch together', { skip: !CHROME && 'no Chrome' }, async () => {
  const h = await startPage({ api, beforeScript: instrument() });
  try {
    await h.sleep(150000 / SCALE + 1500);   // 150 s virtual
    const r = await reqs(h);
    // Periodic polls only: the startup stagger (0.4-3.6 s) runs before 5 s.
    const periodic = (tail) => times(r, tail).filter((t) => t > 5000);
    const health = periodic('/v2/health'), sat = periodic('/v2/sat/status'), wx = periodic('/v2/sat/weather');
    assert.ok(health.length >= 6 && sat.length >= 3 && wx.length >= 1,
      'polls ran: health ' + health.length + ', sat ' + sat.length + ', weather ' + wx.length);
    const gap = (a, b) => Math.min.apply(null, a.map((x) => Math.min.apply(null, b.map((y) => Math.abs(x - y)))));
    // 2.5 s virtual = 50 ms real: far above timer jitter, far below the 7.5 s design spacing.
    assert.ok(gap(health, sat) >= 2500, 'health and sat/status ' + Math.round(gap(health, sat)) + ' ms apart');
    assert.ok(gap(health, wx) >= 2500, 'health and weather ' + Math.round(gap(health, wx)) + ' ms apart');
    assert.ok(gap(sat, wx) >= 2500, 'sat/status and weather ' + Math.round(gap(sat, wx)) + ' ms apart');
  } finally { await h.close(); }
});

// Median phase of a poller inside its period, in virtual ms.
function phase(ts, period) {
  const m = ts.map((t) => t % period).sort((a, b) => a - b);
  return m[m.length >> 1];
}

test('two tabs: each has its own poll phase (health and the Monitor > Values otmonitor poll)', { skip: !CHROME && 'no Chrome' }, async () => {
  const first = [], otmon = [];
  for (const rnd of [0.1, 0.7]) {
    // Both tabs open on Monitor > Values, the run A scenario.
    const open = "try { localStorage.setItem('otgw-v2-page', 'monitor'); localStorage.setItem('otgw-v2-montab', 'mvalues'); } catch (e) {}";
    // Scale 10 here: at 50 the 2 s otmonitor tick is 40 ms real, and Windows timer
    // granularity (~15.6 ms) drifts its phase.
    const h = await startPage({ api, beforeScript: instrument(rnd, 10) + open });
    try {
      await h.sleep(40000 / 10 + 1500);
      const r = await reqs(h), a = await h.evalJs('window.__anchor');
      assert.equal(typeof a, 'number', 'init() anchor recorded');
      // Times relative to init(), so page-load differences between the two tabs drop out.
      first.push(times(r, '/v2/health').map((t) => t - a).filter((t) => t > 5000)[0]);
      otmon.push(times(r, '/v2/otgw/otmonitor').map((t) => t - a).filter((t) => t > 5000));
    } finally { await h.close(); }
  }
  assert.ok(first.every((t) => typeof t === 'number'), 'a periodic health poll ran in both tabs: ' + first);
  // Same page, same timers: the two first periodic polls land within timer jitter unless the
  // phase differs. With Math.random 0.1 and 0.7 they are 9 s apart by design.
  assert.ok(Math.abs(first[0] - first[1]) >= 4000, 'first periodic health poll ' + first.map(Math.round).join(' vs ') + ' ms');
  assert.ok(otmon.every((t) => t.length >= 5), 'otmonitor polled in both tabs: ' + otmon.map((t) => t.length));
  // By design 1500 vs 500 ms inside the 2 s period: the furthest apart two phases can be.
  const p = otmon.map((t) => phase(t, 2000));
  const d = Math.abs(p[0] - p[1]), circ = Math.min(d, 2000 - d);
  assert.ok(circ >= 500, 'otmonitor phases ' + p.map(Math.round).join(' vs ') + ' ms in the 2 s period');
});

test('a hidden tab does not poll, and refreshes once when shown', { skip: !CHROME && 'no Chrome' }, async () => {
  const h = await startPage({ api, beforeScript: instrument() });
  const POLLED = ['/v2/health', '/v2/sat/status', '/v2/sat/weather', '/v2/sat/ble/discovery', '/v2/sensors'];
  const count = (r) => r.filter((x) => POLLED.some((p) => x.u.includes(p))).length;
  try {
    await h.sleep(20000 / SCALE + 1000);   // past the startup stagger
    await h.evalJs('window.__hidden = true; document.dispatchEvent(new Event("visibilitychange"))');
    const n0 = count(await reqs(h));
    await h.sleep(150000 / SCALE);   // 150 s virtual hidden
    const hidden = count(await reqs(h)) - n0;
    assert.equal(hidden, 0, hidden + ' poll requests from a hidden tab');
    const r0 = (await reqs(h)).length;
    await h.evalJs('window.__hidden = false; document.dispatchEvent(new Event("visibilitychange"))');
    await h.sleep(2000);   // the refresh is staggered over 1.4 s real
    const after = (await reqs(h)).slice(r0).map((x) => x.u);
    for (const p of ['/v2/health', '/v2/sat/status', '/v2/sat/weather']) {
      assert.ok(after.some((u) => u.endsWith(p)), p + ' refreshed on show: ' + after.join(', '));
    }
  } finally { await h.close(); }
});

// Coordinator 2026-11-09: "pause hidden" covers the page pollers too. Per poller: it ran while
// the tab was visible, and a hidden tab sends none of its requests.
const SETTINGS = { settings: { hostname: { value: 'otgw', type: 's', maxlen: 32 },
  satbleenable: { value: true, type: 'b' }, satbleriskack: { value: true, type: 'b' } } };
const BLE = { psram: true, ble_enable: true, risk_ack: true, populated_slots: 0, sensors: [] };
const pageApi = (m, p) => {
  if (p.endsWith('/v2/health')) return HEALTH;
  if (p.endsWith('/v2/settings')) return SETTINGS;
  if (p.endsWith('/v2/sat/ble/discovery')) return BLE;
  return undefined;
};

async function hiddenSilence(h, tail) {
  const n = async () => times(await reqs(h), tail).length;
  const v0 = await n();
  await h.sleep(30000 / SCALE + 300);   // 30 s virtual visible
  const visible = (await n()) - v0;
  await h.evalJs('window.__hidden = true; document.dispatchEvent(new Event("visibilitychange"))');
  await h.sleep(200);
  const h0 = await n();
  await h.sleep(60000 / SCALE);   // 60 s virtual hidden
  return { visible, hidden: (await n()) - h0 };
}

test('a hidden tab pauses the SAT page poll (5 s)', { skip: !CHROME && 'no Chrome' }, async () => {
  const open = "try { localStorage.setItem('otgw-v2-page', 'sat'); } catch (e) {}";
  const h = await startPage({ api: pageApi, beforeScript: instrument() + open });
  try {
    assert.ok(await h.until(`document.getElementById('page-sat') && document.getElementById('page-sat').classList.contains('active')`, 15000), 'SAT page open');
    const r = await hiddenSilence(h, '/v2/sat/status');
    assert.ok(r.visible >= 4, r.visible + ' sat/status polls in 30 s visible');
    assert.equal(r.hidden, 0, r.hidden + ' sat/status polls from a hidden SAT page');
  } finally { await h.close(); }
});

test('a hidden tab pauses the Settings > Sensors BLE roster poll (4 s)', { skip: !CHROME && 'no Chrome' }, async () => {
  const h = await startPage({ api: pageApi, beforeScript: instrument() });
  try {
    assert.ok(await h.until(`!!document.querySelector('.connsum')`, 20000), 'v2.js started');
    await h.evalJs(`document.querySelector('button.seg[data-page="settings"]').click()`);
    assert.ok(await h.until(`!!document.querySelector('.rail-item')`, 15000), 'settings rail rendered');
    await h.evalJs(`(function(){ var r = Array.from(document.querySelectorAll('.rail-item'))
      .find(function (x) { return /sensor/i.test(x.textContent); }); if (r) r.click(); return !!r; })()`);
    assert.ok(await h.until(`!!document.getElementById('setcard-ble')`, 15000), 'Sensors category with the BLE card open');
    const r = await hiddenSilence(h, '/v2/sat/ble/discovery');
    assert.ok(r.visible >= 5, r.visible + ' roster polls in 30 s visible');
    assert.equal(r.hidden, 0, r.hidden + ' roster polls from a hidden Sensors page');
  } finally { await h.close(); }
});
