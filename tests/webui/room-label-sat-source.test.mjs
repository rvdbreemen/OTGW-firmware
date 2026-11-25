// TASK-3035 (will_85903, beta.410; ux proposal docs/reviews/2026-11-25_room-temperature-labels/README.md): while SAT is on,
// the Home tile showed SAT's room temperature and the Monitor graph the thermostat's MsgID 24, both called "room". The
// qualifier now appears only while the two can differ (SAT on with a source other than the thermostat):
//   Home tile     ROOM · SAT + a visible line "thermostat 19.8° · via <source>" (tooltip stays)
//   SAT chart     panel and legend "Room (SAT)" (always: the SAT page is SAT's view)
//   Monitor graph series "room (thermostat)" + one live line "SAT uses 20.5° (<source>). The graph shows the thermostat reading."
// SAT off, SAT on with the thermostat as source, SAT room lost: plain ROOM, plain "room", no line.
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. OTGW_DATA_DIR=<dir> runs it against an older copy
// of data/ (old-vs-fix). OTGW_SHOT_DIR=<dir> also saves screenshots of the external-source state at 1400 and 390 px, light
// and dark (synthetic data).
//
// Run: node --test tests/webui/room-label-sat-source.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { startPage, CHROME } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';
const n = 24, sc = 10;
const col = (f) => Array.from({ length: n }, (_, i) => f(i));
const HIST = { count: n, interval_s: 300, newest_age_ms: 2000, scale: sc,
  flow: col(() => 400), ret: col(() => 330), chSet: col(() => 420), roomSet: col(() => 200), satTarget: col(() => 207),
  room: col(() => 198), outside: col(() => 70), mod: col(() => 30), flame: col(() => 1), dhw_on: col(() => 0), ch: col(() => 1) };
const ON = { enabled: true, active: true, fallback_active: false, target_temp: 20.7, room_temp: 20.5, room_temp_lost: false,
  room_temp_source: 'external', heating_source: 1, heating_source_detected: 1 };
const STATES = {
  off: { sat: Object.assign({}, ON, { enabled: false, active: false }), tr: true },
  thermostat: { sat: Object.assign({}, ON, { room_temp: 19.8, room_temp_source: 'thermostat' }), tr: true },
  mqtt: { sat: ON, tr: true },
  lost: { sat: Object.assign({}, ON, { room_temp_lost: true }), tr: true },
  noTr: { sat: ON, tr: false },
};

async function load(state, opts = {}) {
  const h = await startPage({ width: opts.width || 1280,
    beforeScript: `try { localStorage.setItem('otgw-theme', '${opts.theme || 'light'}'); localStorage.setItem('otgw-v2-satview', 'control'); } catch (e) {}`,
    api: (m, p) => {
      if (p.includes('/v2/graph/history')) return HIST;
      if (p.endsWith('/v2/sat/status')) return { sat: state.sat };
      if (p.endsWith('/v2/otgw/otmonitor')) return { otmonitor: state.tr ? { roomtemperature: { value: 19.8, unit: '°C', epoch: 100 }, roomsetpoint: { value: 20.0, unit: '°C', epoch: 100 } } : {} };
      return undefined;
    } });
  return h;
}
const shotDir = process.env.OTGW_SHOT_DIR;

async function read(h, state, name) {
  // Home: wait for the sat/status answer to be rendered (the tag follows it)
  for (let i = 0; i < 150 && !h.log.some((l) => l.startsWith('GET /api/v2/sat/status')); i++) await h.sleep(100);
  const expectSat = name === 'mqtt' || name === 'noTr';
  assert.ok(await h.until(`document.getElementById('aRoom') && document.getElementById('aRoom').textContent !== '—' && (${expectSat} ? document.getElementById('aRoomTag').textContent !== 'ROOM' : true)`, 30000), `${name}: Home rendered`);
  await h.sleep(600);
  const home = await h.evalJs(`(function () { var e = document.getElementById('aRoom'); return { tag: document.getElementById('aRoomTag').textContent,
    value: e.textContent, tip: e.getAttribute('title'), sub: document.getElementById('aRoomSub').textContent }; })()`);
  if (shotDir && name === 'mqtt') await h.shot(path.join(shotDir, `home-${h.__tag}.png`));
  await h.evalJs(`document.querySelector('.seg[data-page="monitor"]').click()`);
  await h.evalJs(`document.querySelector('.subtab[data-tab="mgraph"]').click()`);
  assert.ok(await h.until(`!!document.getElementById('gLegend') && document.getElementById('gLegend').children.length > 3`, 20000), `${name}: graph legend drawn`);
  await h.sleep(500);
  const graph = await h.evalJs(`(function () { var nt = document.getElementById('gSatNote');
    return { legend: Array.prototype.map.call(document.getElementById('gLegend').children, function (s) { return s.textContent.trim(); }),
      note: nt && !nt.hidden ? nt.textContent : '' }; })()`);
  if (shotDir && name === 'mqtt') await h.shot(path.join(shotDir, `graph-${h.__tag}.png`));
  await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
  assert.ok(await h.until(`!!document.getElementById('satHistLegend') && document.getElementById('satHistLegend').children.length > 1`, 20000), `${name}: SAT chart legend drawn`);
  const satLegend = await h.evalJs(`Array.prototype.map.call(document.getElementById('satHistLegend').children, function (s) { return s.textContent.trim(); })`);
  assert.deepEqual(h.exceptions, [], `${name}: no page errors`);
  return { home, graph, satLegend };
}

const R = {};
test('the room labels name whose room temperature they show, one state per page load', { skip }, async () => {
  for (const [name, state] of Object.entries(STATES)) {
    const h = await load(state); h.__tag = name;
    try { R[name] = await read(h, state, name); } finally { await h.close(); }
  }
});

test('SAT on with an MQTT source: ROOM · SAT, visible thermostat line, qualified graph series and live line', { skip }, () => {
  const { home, graph, satLegend } = R.mqtt;
  assert.equal(home.value, '20.5°');
  assert.equal(home.tag, 'ROOM · SAT');
  assert.equal(home.sub, 'thermostat 19.8° · via MQTT/REST sensor');
  assert.match(home.tip || '', /\(MQTT\/REST sensor\)/);
  assert.ok(graph.legend.includes('room (thermostat)'), JSON.stringify(graph.legend));
  assert.equal(graph.note, 'SAT uses 20.5° (MQTT/REST sensor). The graph shows the thermostat reading.');
  assert.ok(satLegend.includes('Room (SAT)'), JSON.stringify(satLegend));
});
test('SAT off: the tile and the graph are as before, nothing about SAT', { skip }, () => {
  const { home, graph, satLegend } = R.off;
  assert.deepEqual([home.value, home.tag, home.sub, home.tip], ['19.8°', 'ROOM', '', null]);
  assert.ok(graph.legend.includes('room') && !graph.legend.includes('room (thermostat)'), JSON.stringify(graph.legend));
  assert.equal(graph.note, '');
  assert.ok(satLegend.includes('Room (SAT)'), 'the SAT page chart always says Room (SAT): ' + JSON.stringify(satLegend));
});
test('SAT on with the thermostat as its source: nothing to qualify', { skip }, () => {
  const { home, graph } = R.thermostat;
  assert.deepEqual([home.value, home.tag, home.sub, home.tip], ['19.8°', 'ROOM', '', null]);
  assert.ok(graph.legend.includes('room') && !graph.legend.includes('room (thermostat)'));
  assert.equal(graph.note, '');
});
test('SAT room lost: the thermostat value with plain ROOM and no line', { skip }, () => {
  const { home, graph } = R.lost;
  assert.deepEqual([home.value, home.tag, home.sub], ['19.8°', 'ROOM', '']);
  assert.equal(graph.note, '');
});
test('no MsgID 24 reading while SAT uses another source: the line says so', { skip }, () => {
  const { home } = R.noTr;
  assert.equal(home.tag, 'ROOM · SAT');
  assert.equal(home.sub, 'thermostat: no reading');
});

if (shotDir) {
  for (const width of [1400, 390]) for (const theme of ['light', 'dark']) {
    test(`screenshots of the MQTT state at ${width} px, ${theme}`, { skip }, async () => {
      const h = await load(STATES.mqtt, { width, theme }); h.__tag = `${width}-${theme}`;
      try { await read(h, STATES.mqtt, 'mqtt'); } finally { await h.close(); }
    });
  }
}
