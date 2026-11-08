// TASK-1735 (supersedes TASK-1717): the web UI may only control temperatures while SAT is enabled.
// Robert 2026-11-08: 'SAT enabled: the UI is responsive, the set temperature is for SAT, and SAT
// sets the boiler values. SAT disabled: the UI only shows what the real thermostat has set.'
// So with SAT off the home dial (+/-, design b) is hidden, sends nothing (no TT=) and says why;
// with SAT on it keeps setting the SAT target.
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device.
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/home-dial-sat-only.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage } from './lib/page-harness.mjs';

// Opens the page with SAT on or off, switches to design b and presses + once.
// Returns the POSTs it caused and the command line text.
async function pressUp(satEnabled, presses = 1, fallback = false) {
  const owns = satEnabled || fallback;
  const SAT = { sat: { enabled: satEnabled, active: owns, fallback_active: fallback, target_temp: 20 } };
  const h = await startPage({ api: (m, p) => {
    if (p.endsWith('/v2/sat/status')) return SAT;
    if (p.endsWith('/v2/sat/target')) return { status: 'ok' };
    if (p.endsWith('/v2/otgw/commands')) return { status: 'queued' };
    // The thermostat's own values (firmware: roomtemperature = MsgID 24, roomsetpoint = MsgID 16).
    if (p.endsWith('/v2/otgw/otmonitor')) return { otmonitor: {
      roomtemperature: { value: 19.8, unit: '°C', epoch: 100 }, roomsetpoint: { value: 20.0, unit: '°C', epoch: 100 } } };
    return undefined;
  } });
  try {
    assert.ok(await h.until(`!!document.querySelector('.vp-item[data-design="b"]')`, 30000), 'page loaded');
    // fetchSatStatus runs 2.8 s after start; wait until it has been asked.
    for (let i = 0; i < 100 && !h.log.some((l) => l.startsWith('GET /api/v2/sat/status')); i++) await h.sleep(100);
    await h.sleep(500);
    await h.evalJs(`document.querySelector('.vp-item[data-design="b"]').click()`);
    // Under load design b can take a while; a zero-width dial would read as "hidden".
    assert.ok(await h.until(`!!document.getElementById('bUp') && document.getElementById('bCmd').offsetParent !== null`, 30000), 'design b shown');
    // With SAT not owning, wait until the sat/status answer is applied (old code never hides: stays red).
    if (!owns) await h.until(`document.getElementById('bUp').style.display === 'none'`, 20000);
    await h.sleep(300);
    const n = h.log.length;
    const isPost = (l) => l.startsWith('POST /api/v2/sat/target') || l.startsWith('POST /api/v2/otgw/commands');
    for (let k = 1; k <= presses; k++) {
      await h.evalJs(`document.getElementById('bUp').click()`);
      for (let i = 0; i < 100 && h.log.slice(n).filter(isPost).length < k; i++) await h.sleep(100);
      await h.sleep(500);
    }
    const posts = h.log.slice(n).filter(isPost).map((l) => ({ path: l.split(' ')[1], body: JSON.parse(l.slice(l.indexOf('{'))) }));
    const cmd = await h.evalJs(`document.getElementById('bCmd').textContent`);
    const values = await h.evalJs(`[document.getElementById('bRoom').textContent, document.getElementById('bSet').textContent]`);
    const shown = await h.evalJs(`['bUp', 'bDown'].map(function (id) { var e = document.getElementById(id); return getComputedStyle(e).display !== 'none' && e.getBoundingClientRect().width > 0; })`);
    assert.deepEqual(h.exceptions, []);
    return { posts, cmd, shown, values };
  } finally { await h.close(); }
}

test('with SAT off, the dial has no controls, sends nothing and says the thermostat controls it', async () => {
  const { posts, cmd, shown } = await pressUp(false);
  assert.deepEqual(shown, [false, false], '+ and - are hidden');
  assert.deepEqual(posts, [], 'a click (forced) sends no TT= and no SAT target');
  assert.match(cmd, /SAT is off: the thermostat controls the temperature/, `the dial says why (${cmd})`);
});

test('with SAT off, the dial shows the thermostat values: room temperature (24) and its setpoint (16)', async () => {
  const { values } = await pressUp(false);
  assert.deepEqual(values, ['19.8°', 'target 20.0°'], 'nothing derived: MsgID 24 as the big number, MsgID 16 as the target');
});

test('with SAT on, + keeps setting the SAT target', async () => {
  const { posts, cmd, shown } = await pressUp(true);
  assert.deepEqual(shown, [true, true], '+ and - are shown');
  assert.deepEqual(posts, [{ path: '/api/v2/sat/target', body: { value: '20.5' } }], 'the SAT target, no TT=');
  assert.match(cmd, /target set 20\.5°/, `(${cmd})`);
});

test('during a SAT takeover (fallback active, switch off), + sets the SAT target: SAT really controls', async () => {
  const { posts, shown } = await pressUp(false, 1, true);
  assert.deepEqual(shown, [true, true], '+ and - are shown while SAT owns the setpoint');
  assert.deepEqual(posts, [{ path: '/api/v2/sat/target', body: { value: '20.5' } }], 'the SAT target, no TT=');
});

test('presses before SAT status is known ask the device once (at most 2 requests in flight)', async () => {
  // The page sees sat/status 1.5 s late, and the device has no answer yet (null), so sat.owns stays unknown.
  const slow = `(function () { var f = window.fetch; window.fetch = function (u, o) {
    var p = f.apply(this, arguments);
    return ('' + u).indexOf('v2/sat/status') >= 0 ? p.then(function (r) { return new Promise(function (ok) { setTimeout(function () { ok(r); }, 1500); }); }) : p; }; })();`;
  const h = await startPage({ beforeScript: slow, api: (m, p) => (p.endsWith('/v2/sat/status') ? null : undefined) });
  try {
    assert.ok(await h.until(`!!document.querySelector('.vp-item[data-design="b"]')`, 30000), 'page loaded');
    for (let i = 0; i < 100 && !h.log.some((l) => l.startsWith('GET /api/v2/sat/status')); i++) await h.sleep(100);
    await h.sleep(2500);
    await h.evalJs(`document.querySelector('.vp-item[data-design="b"]').click()`);
    assert.ok(await h.until(`document.getElementById('bCmd').offsetParent !== null && document.getElementById('bUp').getBoundingClientRect().width > 0`, 30000), 'design b shown, + visible');
    const n = h.log.length;
    await h.evalJs(`for (var i = 0; i < 4; i++) document.getElementById('bUp').click()`);
    await h.sleep(3000);
    const asks = h.log.slice(n).filter((l) => l.startsWith('GET /api/v2/sat/status'));
    assert.equal(asks.length, 1, `four presses, one sat/status ask (${asks.length})`);
    assert.deepEqual(h.log.slice(n).filter((l) => l.startsWith('POST ')), [], 'SAT not on: nothing is sent');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});
