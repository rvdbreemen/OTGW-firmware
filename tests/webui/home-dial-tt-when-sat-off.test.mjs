// TASK-1717: the home dial (+/-, design b) always POSTed /api/v2/sat/target and said "target set",
// also with SAT off, when that changes nothing on the boiler (tjfs, 2026-11-08). Robert's decision:
// with SAT off the dial sends a TT= room setpoint override and says it lasts until the thermostat's
// next program change; with SAT on it keeps setting the SAT target.
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device.
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/home-dial-tt-when-sat-off.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage } from './lib/page-harness.mjs';

// Opens the page with SAT on or off, switches to design b and presses + once.
// Returns the POSTs it caused and the command line text.
async function pressUp(satEnabled, presses = 1) {
  const SAT = { sat: { enabled: satEnabled, active: satEnabled, fallback_active: false, target_temp: 20 } };
  const h = await startPage({ api: (m, p) => {
    if (p.endsWith('/v2/sat/status')) return SAT;
    if (p.endsWith('/v2/sat/target')) return { status: 'ok' };
    if (p.endsWith('/v2/otgw/commands')) return { status: 'queued' };
    return undefined;
  } });
  try {
    assert.ok(await h.until(`!!document.querySelector('.vp-item[data-design="b"]')`, 30000), 'page loaded');
    // fetchSatStatus runs 2.8 s after start; wait until it has been asked.
    for (let i = 0; i < 100 && !h.log.some((l) => l.startsWith('GET /api/v2/sat/status')); i++) await h.sleep(100);
    await h.sleep(500);
    await h.evalJs(`document.querySelector('.vp-item[data-design="b"]').click()`);
    await h.until(`!!document.getElementById('bUp') && document.getElementById('bUp').offsetParent !== null`, 10000);
    const n = h.log.length;
    const isPost = (l) => l.startsWith('POST /api/v2/sat/target') || l.startsWith('POST /api/v2/otgw/commands');
    for (let k = 1; k <= presses; k++) {
      await h.evalJs(`document.getElementById('bUp').click()`);
      for (let i = 0; i < 100 && h.log.slice(n).filter(isPost).length < k; i++) await h.sleep(100);
      await h.sleep(500);
    }
    const posts = h.log.slice(n).filter(isPost).map((l) => ({ path: l.split(' ')[1], body: JSON.parse(l.slice(l.indexOf('{'))) }));
    const cmd = await h.evalJs(`document.getElementById('bCmd').textContent`);
    assert.deepEqual(h.exceptions, []);
    return { posts, cmd };
  } finally { await h.close(); }
}

test('with SAT off, + sends a TT= room setpoint override and says how long it lasts', async () => {
  const { posts, cmd } = await pressUp(false);
  assert.deepEqual(posts, [{ path: '/api/v2/otgw/commands', body: { command: 'TT=20.5' } }], 'one TT= override, no SAT target');
  assert.match(cmd, /thermostat override 20\.5°/, `the command line names the override (${cmd})`);
  assert.match(cmd, /next program change/, `and says it ends at the thermostat's next program change (${cmd})`);
});

test('with SAT on, + keeps setting the SAT target', async () => {
  const { posts, cmd } = await pressUp(true);
  assert.deepEqual(posts, [{ path: '/api/v2/sat/target', body: { value: '20.5' } }], 'the SAT target, no TT=');
  assert.match(cmd, /target set 20\.5°/, `(${cmd})`);
});

test('with SAT off, a second + steps on from the override it just sent', async () => {
  const { posts } = await pressUp(false, 2);
  assert.deepEqual(posts.map((p) => p.body.command), ['TT=20.5', 'TT=21.0'], 'the thermostat setpoint has not caught up yet');
});
