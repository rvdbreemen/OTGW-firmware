// TASK-2946 (will_85903, #beta-testing 2026-11-20): while SAT owns the temperature, the Home room readouts show the
// room temperature SAT controls on (sat/status room_temp, from satGetRoomTemp(): BLE, areas, an external MQTT/REST
// push or MsgID 24), not only the thermostat's MsgID 24. The setpoint beside it already follows SAT (TASK-1342) and so
// does the HA climate entity (ADR-248). The thermostat's own MsgID 24 stays in the tooltip. SAT off, or SAT without a
// valid room temperature (null or room_temp_lost): MsgID 24 as before (TASK-1735).
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device; one browser, one page load per case.
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/home-room-follows-sat.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage } from './lib/page-harness.mjs';

const ON = { enabled: true, active: true, fallback_active: false, target_temp: 20.5, room_temp: 21.3, room_temp_lost: false };
const CASES = {
  on: ON,
  takeover: Object.assign({}, ON, { enabled: false, fallback_active: true }),
  off: Object.assign({}, ON, { enabled: false, active: false }),
  nullRoom: Object.assign({}, ON, { room_temp: null }),
  lost: Object.assign({}, ON, { room_temp_lost: true }),
};

// Thermostat (MsgID 24) 19.8; SAT's room temperature (an external push) 21.3.
test('Home room readouts follow SAT while SAT owns the temperature', async (t) => {
  let satBody = ON;
  const h = await startPage({ api: (m, p) => {
    if (p.endsWith('/v2/sat/status')) return { sat: satBody };
    if (p.endsWith('/v2/otgw/otmonitor')) return { otmonitor: {
      roomtemperature: { value: 19.8, unit: '°C', epoch: 100 }, roomsetpoint: { value: 20.0, unit: '°C', epoch: 100 } } };
    return undefined;
  } });
  const R = {};
  try {
    for (const [name, body] of Object.entries(CASES)) {
      satBody = body;
      const asked = h.log.filter((l) => l.startsWith('GET /api/v2/sat/status')).length;
      if (name !== 'on') await h.send('Page.navigate', { url: `http://127.0.0.1:${h.PORT}/v2.html` });
      assert.ok(await h.until(`!!document.querySelector('.vp-item[data-design="b"]') && document.getElementById('aRoom').textContent !== '—'`, 30000), `${name}: page loaded, room filled`);
      // fetchSatStatus runs 2.8 s after start; wait until this load has asked it, then let the answer render.
      for (let i = 0; i < 150 && h.log.filter((l) => l.startsWith('GET /api/v2/sat/status')).length <= (name === 'on' ? 0 : asked); i++) await h.sleep(100);
      await h.sleep(800);
      const a = await h.evalJs(`(function () { var e = document.getElementById('aRoom'); return [e.textContent, e.getAttribute('title')]; })()`);
      const chip = await h.evalJs(`document.getElementById('aStrip').textContent`);
      await h.evalJs(`document.querySelector('.vp-item[data-design="b"]').click()`);
      assert.ok(await h.until(`document.getElementById('bRoom').textContent !== '—'`, 20000), `${name}: design b filled`);
      await h.sleep(400);
      const b = await h.evalJs(`(function () { var e = document.getElementById('bRoom'); return [e.textContent, e.getAttribute('title')]; })()`);
      const tick = await h.evalJs(`(function () { var e = document.getElementById('bTick'); return [+e.getAttribute('x2'), +e.getAttribute('y2')]; })()`);
      R[name] = { a, b, chip, tick };
      await h.evalJs(`document.querySelector('.vp-item[data-design="a"]').click()`);   // the design choice persists
    }
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }

  await t.test('SAT on: SAT room temperature shown, thermostat value in the tooltip', () => {
    const { a, b, chip } = R.on;
    assert.equal(a[0], '21.3°', `design a big number (${a[0]})`);
    assert.match(a[1] || '', /thermostat reports 19\.8°/, `design a tooltip (${a[1]})`);
    assert.equal(b[0], '21.3°', `design b big number (${b[0]})`);
    assert.match(b[1] || '', /thermostat reports 19\.8°/, `design b tooltip (${b[1]})`);
    assert.match(chip, /21\.3°/, `mobile chip strip (${chip})`);
  });
  await t.test('SAT takeover (fallback active, switch off): SAT room temperature shown', () => {
    assert.equal(R.takeover.a[0], '21.3°', `(${R.takeover.a[0]})`);
  });
  await t.test('SAT off: MsgID 24 exactly as before, no tooltip', () => {
    assert.deepEqual(R.off.a, ['19.8°', null], 'design a');
    assert.deepEqual(R.off.b, ['19.8°', null], 'design b');
  });
  await t.test('SAT on without a valid room temperature: MsgID 24, no stale SAT value', () => {
    assert.deepEqual(R.nullRoom.a, ['19.8°', null], 'room_temp null');
    assert.deepEqual(R.lost.a, ['19.8°', null], 'room_temp_lost');
  });
  await t.test('the dial tick sits at the shown room temperature', () => {
    assert.notDeepEqual(R.on.tick, R.off.tick, `the tick moves with the value shown (${R.on.tick} vs ${R.off.tick})`);
  });
});
