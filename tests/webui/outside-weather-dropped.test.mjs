// TASK-1875: gruijter's graph export showed the outside column flat for hours while SAT's weather
// input moved. The tab was hidden: fetchWeather() skips a hidden tab (TASK-1741) and the
// visibilitychange handler only cleared model.wxValid, so model.outside kept the last weather
// value and pushGraphSlot() (v2.js, which copies model.outside into every graph slot) recorded it.
//   1. A hidden tab drops the weather value: model.outside becomes null (a gap in the graph).
//   2. Shown again, even after a short hide, the weather is fetched at once and OUTSIDE is back.
//   3. A weather answer that turns invalid drops the old weather value too.
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. Point OTGW_DATA_DIR at an
// older copy of data/ to see it red.
//
// Run: node --test tests/webui/outside-weather-dropped.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const HEALTH = { health: { otgwconnected: true, mqttconnected: true, boiler_age_s: 3, wifirssi: -60 } };
const HIDDEN = `(function () {
  window.__hidden = false;
  Object.defineProperty(Document.prototype, 'hidden', { configurable: true, get: function () { return window.__hidden; } });
  Object.defineProperty(Document.prototype, 'visibilityState', { configurable: true, get: function () { return window.__hidden ? 'hidden' : 'visible'; } });
})();`;
const setHidden = (h, v) => h.evalJs(`window.__hidden = ${v}; document.dispatchEvent(new Event("visibilitychange"))`);
const outside = (h) => h.evalJs('window.OTGWv2 && window.OTGWv2.model.outside');

function apiWith(state) {
  return (m, p) => {
    if (p.endsWith('/v2/health')) return HEALTH;
    if (p.endsWith('/v2/sat/weather')) return { enabled: true, valid: state.valid, temperature: state.temp };
    return undefined;
  };
}

test('hidden tab drops the weather OUTSIDE value; shown again it is fetched at once', { skip: !CHROME && 'no Chrome' }, async () => {
  const state = { valid: true, temp: 8.3 };
  const h = await startPage({ api: apiWith(state), beforeScript: HIDDEN });
  try {
    assert.ok(await h.until('window.OTGWv2 && window.OTGWv2.model.outside === 8.3', 15000), 'weather OUTSIDE 8.3 after start-up');
    await setHidden(h, true);
    assert.ok(await h.until('window.OTGWv2.model.outside === null', 2000), 'hidden: OUTSIDE is ' + (await outside(h)) + ', not dropped');
    state.temp = 2.0;   // the weather moved while the tab was hidden
    await setHidden(h, false);   // a short hide (< 15 s): before the fix nothing was refetched
    assert.ok(await h.until('window.OTGWv2.model.outside === 2', 5000), 'shown: OUTSIDE is ' + (await outside(h)) + ', expected the fresh 2.0');
  } finally { await h.close(); }
});

// Counts the requests that really reach the network, under the v2 API queue (ADR-184), which
// wraps window.fetch when v2.js loads and so calls this wrapper as its underlying fetch.
const NET = `(function () {
  var f0 = window.fetch; window.__net = { now: 0, peak: 0, wx: 0 };
  window.fetch = function (u) {
    var n = window.__net; n.now++; if (n.now > n.peak) n.peak = n.now;
    if (String(u && u.url || u).indexOf('v2/sat/weather') >= 0) n.wx++;
    return f0.apply(this, arguments).finally(function () { n.now--; });
  };
})();`;

test('a hide/show storm keeps 2 requests in flight and one weather request at a time', { skip: !CHROME && 'no Chrome' }, async () => {
  const state = { valid: true, temp: 7.1 };
  const h = await startPage({ api: apiWith(state), beforeScript: HIDDEN + NET });
  try {
    assert.ok(await h.until('window.OTGWv2 && window.OTGWv2.model.outside === 7.1', 15000), 'weather OUTSIDE after start-up');
    await h.sleep(4000);   // let the start-up burst finish
    const wx0 = await h.evalJs('window.__net.wx');
    await h.evalJs(`(function () { for (var i = 0; i < 20; i++) { window.__hidden = true; document.dispatchEvent(new Event('visibilitychange'));
      window.__hidden = false; document.dispatchEvent(new Event('visibilitychange')); } })()`);
    await h.sleep(3000);
    const net = JSON.parse(await h.evalJs('JSON.stringify(window.__net)'));
    assert.ok(net.peak <= 2, 'peak ' + net.peak + ' requests in flight');
    assert.ok(net.wx - wx0 <= 2, (net.wx - wx0) + ' weather requests for 20 hide/show toggles');
    assert.ok(await h.until('window.OTGWv2.model.outside === 7.1', 5000), 'OUTSIDE back after the storm');
  } finally { await h.close(); }
});

// Page timers run 30x faster here, so the 60 s weather poll comes every 2 s real; the tab stays visible.
const FAST = `(function () {
  var S = 30, st = window.setTimeout, si = window.setInterval;
  window.setTimeout = function (fn, d) { var a = [].slice.call(arguments, 2); return st.apply(window, [fn, (+d || 0) / S].concat(a)); };
  window.setInterval = function (fn, d) { var a = [].slice.call(arguments, 2); return si.apply(window, [fn, (+d || 0) / S].concat(a)); };
})();`;

test('a weather answer that turns invalid drops the old weather value', { skip: !CHROME && 'no Chrome' }, async () => {
  const state = { valid: true, temp: 6.5 };
  const h = await startPage({ api: apiWith(state), beforeScript: HIDDEN + FAST });
  try {
    assert.ok(await h.until('window.OTGWv2 && window.OTGWv2.model.outside === 6.5', 15000), 'weather OUTSIDE 6.5 after start-up');
    state.valid = false;   // the next periodic weather poll answers valid:false
    assert.ok(await h.until('window.OTGWv2.model.outside === null', 8000),
      'an invalid weather answer left OUTSIDE at ' + (await outside(h)));
  } finally { await h.close(); }
});
