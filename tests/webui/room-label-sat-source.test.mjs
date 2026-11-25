// TASK-3035 (will_85903, beta.410): while SAT is on, the overview card showed SAT's room temperature (sat/status room_temp,
// TASK-2946) and the graph tooltip showed the thermostat's MsgID 24, both called "room". The tester read them as one value.
// The ROOM tag now says whose value it is (ROOM · SAT while SAT's room is shown), the tooltip names SAT's source, and the
// graph series that is always MsgID 24 is called "thermostat room"; while SAT regulates on a room temperature its label also
// names the source SAT uses. SAT off: the tag stays ROOM and the series is plain "thermostat room".
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. Point OTGW_DATA_DIR at an older copy of data/ to
// see it red. OTGW_SHOT_DIR=<dir> also saves screenshots of each case (synthetic data).
//
// Run: node --test tests/webui/room-label-sat-source.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { startPage, CHROME } from './lib/page-harness.mjs';

const n = 24, sc = 10;
const col = (f) => Array.from({ length: n }, (_, i) => f(i));
const HIST = { count: n, interval_s: 300, newest_age_ms: 2000, scale: sc,
  flow: col(() => 400), ret: col(() => 330), chSet: col(() => 420), roomSet: col(() => 200), satTarget: col(() => 207),
  room: col(() => 198), outside: col(() => 70), mod: col(() => 30), flame: col(() => 1), dhw_on: col(() => 0), ch: col(() => 1) };
const ON = { enabled: true, active: true, fallback_active: false, target_temp: 20.7, room_temp: 21.3, room_temp_lost: false,
  room_temp_source: 'ble', heating_source: 1, heating_source_detected: 1 };
const CASES = { sat: ON, off: Object.assign({}, ON, { enabled: false, active: false }) };

test('the room labels name whose room temperature they show', { skip: !CHROME && 'no Chrome' }, async (t) => {
  let satBody = ON;
  const h = await startPage({ api: (m, p) => {
    if (p.includes('/v2/graph/history')) return HIST;
    if (p.endsWith('/v2/sat/status')) return { sat: satBody };
    if (p.endsWith('/v2/otgw/otmonitor')) return { otmonitor: { roomtemperature: { value: 19.8, unit: '°C', epoch: 100 }, roomsetpoint: { value: 20.0, unit: '°C', epoch: 100 } } };
    return undefined;
  } });
  const R = {};
  try {
    for (const [name, body] of Object.entries(CASES)) {
      satBody = body;
      const asked = h.log.filter((l) => l.startsWith('GET /api/v2/sat/status')).length;
      if (name !== 'sat') await h.send('Page.navigate', { url: `http://127.0.0.1:${h.PORT}/v2.html` });
      assert.ok(await h.until(`!!document.getElementById('aRoom') && document.getElementById('aRoom').textContent !== '—'`, 30000), `${name}: Home room filled`);
      for (let i = 0; i < 150 && h.log.filter((l) => l.startsWith('GET /api/v2/sat/status')).length <= (name === 'sat' ? 0 : asked); i++) await h.sleep(100);
      await h.sleep(800);
      const home = await h.evalJs(`(function () { var e = document.getElementById('aRoom'); return { tag: document.getElementById('aRoomTag').textContent, value: e.textContent, tip: e.getAttribute('title') }; })()`);
      if (process.env.OTGW_SHOT_DIR) await h.shot(path.join(process.env.OTGW_SHOT_DIR, `home-${name}.png`));
      await h.evalJs(`document.querySelector('.seg[data-page="monitor"]').click()`);
      await h.evalJs(`document.querySelector('.subtab[data-tab="mgraph"]').click()`);
      assert.ok(await h.until(`!!document.getElementById('gLegend') && document.getElementById('gLegend').children.length > 3`, 20000), `${name}: graph legend drawn`);
      await h.sleep(500);
      const legend = await h.evalJs(`Array.prototype.map.call(document.getElementById('gLegend').children, function (s) { return s.textContent.trim(); })`);
      if (process.env.OTGW_SHOT_DIR) await h.shot(path.join(process.env.OTGW_SHOT_DIR, `graph-${name}.png`));
      R[name] = { home, legend };
      await h.evalJs(`document.querySelector('.seg[data-page="home"]').click()`);
    }
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }

  await t.test('SAT on: the ROOM tag says SAT, the tooltip names the source and the thermostat value', () => {
    const { home } = R.sat;
    assert.equal(home.value, '21.3°', `SAT's room is the big number (${home.value})`);
    assert.equal(home.tag, 'ROOM · SAT', `tag (${home.tag})`);
    assert.match(home.tip || '', /\(BLE sensor\)/, `tooltip names SAT's source (${home.tip})`);
    assert.match(home.tip || '', /thermostat reports 19\.8°/, `tooltip keeps the thermostat's value (${home.tip})`);
  });
  await t.test('SAT on: the graph series is the thermostat room and names the source SAT uses', () => {
    assert.ok(R.sat.legend.includes('thermostat room (SAT uses BLE sensor)'), `legend: ${JSON.stringify(R.sat.legend)}`);
    assert.ok(!R.sat.legend.includes('room'), 'no bare "room" series left');
  });
  await t.test('SAT off: the plain ROOM tag and the plain thermostat room series, as MsgID 24 is the only room', () => {
    assert.equal(R.off.home.tag, 'ROOM');
    assert.equal(R.off.home.value, '19.8°');
    assert.ok(R.off.legend.includes('thermostat room'), `legend: ${JSON.stringify(R.off.legend)}`);
  });
});
