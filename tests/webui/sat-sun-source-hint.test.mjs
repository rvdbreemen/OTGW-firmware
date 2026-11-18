// TASK-2015 (thaghostnl, #alpha-testing 2026-11-17; Robert 2026-11-18: "Niet iedereen gebruikt Home Assistant (HA).
// Dus dat is geen given, maar als HA de zonnestand doorgeeft, dan mag je dat gebruiken natuurlijk."): the SAT weather
// card asked for Latitude/Longitude ("Location detection needs HTTPS ...") also while Home Assistant published a fresh
// sat/sun_elevation. Now the SAT status carries sun_elevation_source (the firmware's 1 h rule, TASK-1410):
//   1. external, weather fetch off: the card names Home Assistant as the source and asks for no location;
//   2. external, weather fetch on without a location: the source is named, and the fetch still asks for one;
//   3. none (no HA, plain HTTP): the card asks for Latitude/Longitude and names sat/sun_elevation as the alternative;
//   4. the hint follows the polled status: HA stops publishing, the location ask comes back.
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device; navigator.geolocation is removed, as on the
// device's plain HTTP. OTGW_DATA_DIR at an older copy of data/ shows all four red (the old card always asks
// for Latitude/Longitude and never names sat/sun_elevation). OTGW_SHOT_DIR=<dir> writes the two card screenshots for AC3.
//
// Run: node --test tests/webui/sat-sun-source-hint.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { startPage, CHROME } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';
const HEALTH = { health: { otgwconnected: true, mqttconnected: true, boiler_age_s: 3, wifirssi: -60 } };
const NO_GEO = `(function () {
  Object.defineProperty(Navigator.prototype, 'geolocation', { configurable: true, get: function () { return undefined; } });
})();`;
// Page timers run 10x faster, so the 5 s SAT status poll comes every 0.5 s real.
const FAST = `(function () {
  var S = 10, st = window.setTimeout, si = window.setInterval;
  window.setTimeout = function (fn, d) { var a = [].slice.call(arguments, 2); return st.apply(window, [fn, (+d || 0) / S].concat(a)); };
  window.setInterval = function (fn, d) { var a = [].slice.call(arguments, 2); return si.apply(window, [fn, (+d || 0) / S].concat(a)); };
})();`;

function device(state) {
  return (m, p) => {
    if (p.endsWith('/v2/health')) return HEALTH;
    if (p.endsWith('/v2/sat/status')) {
      const s = { enabled: true, active: true, solar_gain_active: false, sun_elevation_source: state.source };
      if (state.source !== 'none') s.sun_elevation = state.elev;
      return s;
    }
    if (p.endsWith('/v2/sat/weather')) {
      return { enabled: state.weather, valid: false, latitude: state.lat, longitude: state.lon, fetch_errors: 0 };
    }
    return undefined;
  };
}

async function openWeatherCard(h) {
  assert.ok(await h.until(`!!document.querySelector('.connsum')`, 20000), 'v2.js started');
  await h.evalJs(`document.querySelector('button.seg[data-page="sat"]').click()`);
  await h.evalJs(`(function(){ var b = document.querySelector('.satview-btn[data-sv="technical"]'); if (b) b.click(); })()`);
  assert.ok(await h.until(`!!document.getElementById('satLocHint')`, 15000), 'the weather card is on the page');
}
const HINT = `(function(){ var e = document.getElementById('satLocHint'); return e && !e.hidden ? e.textContent : ''; })()`;
const hintHas = (s) => `(${HINT}).indexOf(${JSON.stringify(s)}) >= 0`;

async function shot(h, name) {
  const dir = process.env.OTGW_SHOT_DIR;
  if (!dir) return;
  await h.evalJs(`document.getElementById('satWeatherKv').closest('.card').scrollIntoView({ block: 'center' })`);
  await h.sleep(300);
  await h.shot(path.join(dir, name));
}

test('HA publishes sat/sun_elevation, weather fetch off: the card names HA and asks for no location', { skip }, async () => {
  const st = { source: 'external', elev: 23.4, weather: false, lat: 0, lon: 0 };
  const h = await startPage({ api: device(st), beforeScript: NO_GEO + FAST });
  try {
    await openWeatherCard(h);
    assert.ok(await h.until(hintHas('Home Assistant (sat/sun_elevation): 23.4'), 10000), 'hint is: ' + (await h.evalJs(HINT)));
    const hint = await h.evalJs(HINT);
    assert.ok(hint.indexOf('No location needed') >= 0, hint);
    assert.equal(hint.indexOf('Latitude'), -1, 'no location ask: ' + hint);
    assert.equal(hint.indexOf('HTTPS'), -1, hint);
    assert.equal(await h.evalJs(`document.getElementById('satDetectLoc').hidden`), true, 'no Detect location button');
    await shot(h, 'task2015-ha-source.png');
  } finally { await h.close(); }
});

test('HA publishes sat/sun_elevation, weather fetch on without a location: the fetch still asks for one', { skip }, async () => {
  const st = { source: 'external', elev: 5.0, weather: true, lat: 0, lon: 0 };
  const h = await startPage({ api: device(st), beforeScript: NO_GEO + FAST });
  try {
    await openWeatherCard(h);
    assert.ok(await h.until(hintHas('The weather fetch still needs Latitude and Longitude'), 10000), 'hint is: ' + (await h.evalJs(HINT)));
    assert.ok((await h.evalJs(HINT)).indexOf('Home Assistant (sat/sun_elevation): 5.0') >= 0);
  } finally { await h.close(); }
});

test('no HA, plain HTTP: the card asks for Latitude/Longitude and names sat/sun_elevation as the alternative', { skip }, async () => {
  const st = { source: 'none', elev: 0, weather: false, lat: 0, lon: 0 };
  const h = await startPage({ api: device(st), beforeScript: NO_GEO + FAST });
  try {
    await openWeatherCard(h);
    assert.ok(await h.until(hintHas('Enter Latitude and Longitude'), 10000), 'hint is: ' + (await h.evalJs(HINT)));
    assert.ok((await h.evalJs(HINT)).indexOf('sat/sun_elevation') >= 0, 'the alternative is named');
    await shot(h, 'task2015-no-ha-asks-location.png');
  } finally { await h.close(); }
});

test('the hint follows the polled status: HA stops, the location ask comes back', { skip }, async () => {
  const st = { source: 'external', elev: 12.0, weather: false, lat: 0, lon: 0 };
  const h = await startPage({ api: device(st), beforeScript: NO_GEO + FAST });
  try {
    await openWeatherCard(h);
    assert.ok(await h.until(hintHas('Home Assistant'), 10000), 'hint is: ' + (await h.evalJs(HINT)));
    st.source = 'none';   // the firmware's 1 h rule ran out
    assert.ok(await h.until(hintHas('Enter Latitude and Longitude'), 10000), 'hint is: ' + (await h.evalJs(HINT)));
  } finally { await h.close(); }
});
