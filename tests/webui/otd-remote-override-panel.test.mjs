// TASK-1749: in gateway mode TT=/TC= are answer state (ADR-223), no longer a write slot, so the
// OT-Direct overrides panel lost the room setpoint override. GET /api/v2/otdirect/overrides now
// carries overrides.remote ({mode, value, msgid9_answered, accepted, mismatches} or null); the panel
// shows one row for it with a Clear that sends TT=0, and the SAT rule of the other rows applies.
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake OT-Direct device.
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/otd-remote-override-panel.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage } from './lib/page-harness.mjs';

const HEALTH = { health: { otcommandinterface: 'OT-Direct', wifirssi: -55 } };
const DEVICE = { device: { otcommandinterface: 'OT-Direct', hardwaremode: 'OT-Direct', otdirectavailable: true } };
const REMOTE = { mode: 'temporary', value: 21.5, msgid9_answered: false, accepted: false, mismatches: 0 };

// state.remote: undefined = key absent (older firmware), null, or the object. state.status = HTTP status of the list.
async function openPanel(state) {
  const h = await startPage({ api: (m, p) => {
    if (p.endsWith('/v2/otdirect/overrides')) {
      if (state.status) return { __status: state.status };
      const o = { write: [], response: [], modify: [], unknown: [], sat_owns: !!state.satOwns };
      if (state.remote !== undefined) o.remote = state.remote;
      return { overrides: o, queue: { depth: 0, highWater: 0, capacity: 12 } };
    }
    if (p.endsWith('/v2/health')) return HEALTH;
    if (p.endsWith('/v2/device/info')) return DEVICE;
    if (p.endsWith('/v2/otgw/commands')) return { status: 'queued' };
    return undefined;
  } });
  assert.ok(await h.until(`!!document.querySelector('.seg[data-page="monitor"]')`, 20000), 'v2.js started');
  await h.evalJs(`document.querySelector('.seg[data-page="monitor"]').click()`);
  await h.until(`(function(){ var t = document.querySelector('.subtab[data-tab="mconn"]'); if (!t) return false; t.click(); return true; })()`, 20000);
  assert.ok(await h.until(`(function(){ var el = document.getElementById('otdOvrList'); return !!el && el.children.length > 0; })()`, 30000), 'override list rendered');
  return h;
}
const rowsOf = (h) => h.evalJs(`[].slice.call(document.getElementById('otdOvrList').children).map(function (r) {
  var b = r.querySelector('button'); return { text: r.textContent, clear: b ? b.textContent : '' }; })`);
const posts = (h, n = 0) => h.log.slice(n).filter((l) => l.startsWith('POST /api/v2/otgw/commands')).map((l) => JSON.parse(l.slice(l.indexOf('{'))).command);

test('an active TT= remote override shows mode, value and status, and Clear sends TT=0', async () => {
  const h = await openPanel({ remote: REMOTE });
  try {
    const rows = await rowsOf(h);
    assert.equal(rows.length, 1, JSON.stringify(rows));
    assert.match(rows[0].text, /Room setpoint override \(TT=, temporary\)/);
    assert.match(rows[0].text, /21\.5 °C/);
    assert.match(rows[0].text, /waiting for the thermostat to read it/);
    assert.equal(rows[0].clear, 'Clear');
    const n = h.log.length;
    await h.evalJs(`document.querySelector('#otdOvrList button').click()`);
    for (let i = 0; i < 50 && !posts(h, n).length; i++) await h.sleep(100);
    assert.deepEqual(posts(h, n), ['TT=0']);
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('the status follows msgid9_answered and accepted; TC= reads as constant', async () => {
  for (const [r, re] of [
    [{ ...REMOTE, msgid9_answered: true }, /offered to the thermostat/],
    [{ ...REMOTE, mode: 'constant', msgid9_answered: true, accepted: true }, /Room setpoint override \(TC=, constant\).*accepted by the thermostat/],
  ]) {
    const h = await openPanel({ remote: r });
    try { assert.match((await rowsOf(h))[0].text, re); assert.deepEqual(h.exceptions, []); } finally { await h.close(); }
  }
});

test('remote null or absent: no row and no error', async () => {
  for (const remote of [null, undefined]) {
    const h = await openPanel({ remote });
    try {
      const rows = await rowsOf(h);
      assert.equal(rows.length, 1, JSON.stringify(rows));
      assert.equal(rows[0].text, 'No active overrides');
      assert.deepEqual(h.exceptions, []);
    } finally { await h.close(); }
  }
});

test('while SAT owns the heating commands the remote row says so and offers no Clear', async () => {
  const h = await openPanel({ remote: REMOTE, satOwns: true });
  try {
    const rows = await rowsOf(h);
    assert.equal(rows.length, 1, JSON.stringify(rows));
    assert.match(rows[0].text, /TT=/);
    assert.equal(rows[0].clear, '', 'the firmware refuses TT= while SAT is enabled (TASK-1695)');
    assert.match(rows[0].text, /SAT controls this/);
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('a busy 503 on the list after good data keeps the remote row (TASK-1747)', async () => {
  const state = { remote: REMOTE };
  const h = await openPanel(state);
  try {
    state.status = 503;
    // Clear schedules two list refetches (400 ms, 2 s); both now get 503.
    const n = h.log.length;
    await h.evalJs(`document.querySelector('#otdOvrList button').click()`);
    for (let i = 0; i < 100 && h.log.slice(n).filter((l) => l.startsWith('GET /api/v2/otdirect/overrides')).length < 2; i++) await h.sleep(100);
    assert.ok(h.log.slice(n).filter((l) => l.startsWith('GET /api/v2/otdirect/overrides')).length >= 2, 'the refetches ran');
    const rows = await rowsOf(h);
    assert.equal(rows.length, 1);
    assert.match(rows[0].text, /Room setpoint override \(TT=/);
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});
