// TASK-3011 UI part (Robert's decision Q3 C; firmware contract from otgw-dev's lane/d3-blesrc 399624013):
//   - a one-time banner on the SAT page while /api/v2/sat/status ble_pin_notice is true: "SAT uses BLE sensor
//     <name or MAC>"; Confirm re-POSTs the same MAC to /api/v2/sat/settings/ble_mac (the firmware clears the
//     notice on any ble_mac write), Change opens Settings > Sensors;
//   - the room tile names where SAT's room temperature comes from (room_temp_source: ble, areas, external,
//     thermostat, simulation, estimate, none);
//   - the primary source setting satroomsource (0 auto, 1 thermostat, 2 external, 3 ble, 4 areas) as a select in
//     the SAT settings, group Indoor temperature.
// OTGW_DATA_DIR=<dir> runs it against another tree (old-vs-fix).
//
// Run: node --test tests/webui/sat-ble-pin-room-source.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage } from './lib/page-harness.mjs';

const MAC = 'A4:C1:38:00:11:22';
const BASE = { enabled: true, active: true, boiler_status: 'idle', target_temp: 20, room_temp: 20.2, outside_temp: 8,
  room_temp_source: 'ble', room_source: 'auto', ble_enable: true, ble_mac: MAC, ble_pin_notice: true };
const S = (value, type, extra) => Object.assign({ value, type }, extra || {});
const SETTINGS = { satroomsource: S('0', 'i', { min: 0, max: 4 }), satexternaltemp: S('false', 'b'), satsensormaxage: S('300', 'i') };

async function openSat(sat, posts) {
  const ref = { sat };
  const h = await startPage({
    api: (m, p, q, body) => {
      if (p.endsWith('/v2/sat/settings/ble_mac') && m === 'POST') { posts.push(body); ref.sat = Object.assign({}, ref.sat, { ble_pin_notice: false }); return { ok: true }; }
      if (p.endsWith('/v2/sat/status')) return ref.sat;
      if (p.endsWith('/v2/settings') && m === 'GET') return SETTINGS;
      return undefined;
    },
    beforeScript: "try { localStorage.setItem('otgw-v2-satview', 'control'); } catch (e) {}",
  });
  await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
  return { h, ref };
}
const BANNER = `(function () { var b = document.getElementById('satBlePin'); if (!b || b.hidden) return null;
  return { text: b.textContent, buttons: Array.prototype.map.call(b.querySelectorAll('button'), function (x) { return x.textContent; }) }; })()`;

test('ble_pin_notice shows the banner; Confirm re-posts the same MAC and the banner goes', async () => {
  const posts = [];
  const { h } = await openSat(BASE, posts);
  try {
    assert.ok(await h.until(`!!${BANNER}`, 12000), 'the banner shows');
    const b = await h.evalJs(BANNER);
    assert.match(b.text, /SAT uses BLE sensor A4:C1:38:00:11:22/);
    assert.deepEqual(b.buttons, ['Confirm', 'Change']);
    await h.evalJs(`document.querySelector('#satBlePin button').click()`);
    assert.ok(await h.until(`!${BANNER}`, 8000), 'the banner goes after Confirm');
    for (let i = 0; i < 40 && !posts.length; i++) await h.sleep(100);   // the banner hides at the click, the POST lands a moment later
    assert.deepEqual(posts, [MAC], 'Confirm posts the same MAC to /api/v2/sat/settings/ble_mac');
  } finally { await h.close(); }
});

test('no notice, no banner; Change opens Settings > Sensors', async () => {
  const posts = [];
  const { h, ref } = await openSat(Object.assign({}, BASE, { ble_pin_notice: false }), posts);
  try {
    await h.until(`document.getElementById('satRoom').textContent !== '—'`, 12000);
    assert.equal(await h.evalJs(BANNER), null);
    ref.sat = Object.assign({}, BASE);
    assert.ok(await h.until(`!!${BANNER}`, 12000));
    await h.evalJs(`document.querySelectorAll('#satBlePin button')[1].click()`);
    assert.ok(await h.until(`document.getElementById('page-settings').classList.contains('active')`, 5000), 'Change opens Settings');
    assert.deepEqual(posts, []);
  } finally { await h.close(); }
});

test('the room tile names the active source', async () => {
  const { h, ref } = await openSat(Object.assign({}, BASE, { ble_pin_notice: false }), []);
  try {
    const want = { ble: 'Source: BLE sensor', areas: 'Source: averaged room sensors', external: 'Source: MQTT/REST sensor',
      thermostat: 'Source: thermostat', simulation: 'Source: simulation', estimate: 'Source: estimate', none: 'Source: none' };
    for (const [src, txt] of Object.entries(want)) {
      ref.sat = Object.assign({}, BASE, { ble_pin_notice: false, room_temp_source: src });
      const ok = await h.until(`(document.getElementById('satRoomSrc') || {}).textContent === ${JSON.stringify(txt)}`, 12000);
      assert.ok(ok, src + ': ' + await h.evalJs(`(document.getElementById('satRoomSrc') || {}).textContent`));
    }
  } finally { await h.close(); }
});

test('the primary source is a select in Indoor temperature: Auto, Thermostat, MQTT/REST sensor, BLE sensor, Averaged room sensors', async () => {
  const { h } = await openSat(Object.assign({}, BASE, { ble_pin_notice: false }), []);
  try {
    assert.ok(await h.until(`!!document.querySelector('#satSetL2 .srow[data-key="satroomsource"] select')`, 12000), 'select present');
    const r = await h.evalJs(`(function () { var row = document.querySelector('#satSetL2 .srow[data-key="satroomsource"]');
      var sel = row.querySelector('select');
      return { label: row.querySelector('.slbl').firstChild.textContent.trim(), group: row.closest('.set-group').querySelector('h3').textContent,
        opts: Array.prototype.map.call(sel.options, function (o) { return o.value + ':' + o.textContent; }), value: sel.value }; })()`);
    assert.equal(r.label, 'Room temperature source');
    assert.equal(r.group, 'Indoor temperature');
    assert.deepEqual(r.opts, ['0:Auto', '1:Thermostat', '2:MQTT/REST sensor', '3:BLE sensor', '4:Averaged room sensors']);
    assert.equal(r.value, '0');
  } finally { await h.close(); }
});

// Coordinator add-on: a chosen source that is not enabled gets a one-line hint under the select.
test('a chosen room source that is not enabled shows a hint; an enabled one does not', async () => {
  const cases = [
    [{ satroomsource: S('2', 'i', { min: 0, max: 4 }), satexternaltemp: S('false', 'b') }, true],
    [{ satroomsource: S('2', 'i', { min: 0, max: 4 }), satexternaltemp: S('true', 'b') }, false],
    [{ satroomsource: S('3', 'i', { min: 0, max: 4 }), satbleenable: S('true', 'b'), satblemac: S('', 's') }, true],
    [{ satroomsource: S('3', 'i', { min: 0, max: 4 }), satbleenable: S('true', 'b'), satblemac: S(MAC, 's') }, false],
    [{ satroomsource: S('4', 'i', { min: 0, max: 4 }), satmultiarea: S('false', 'b') }, true],
    [{ satroomsource: S('1', 'i', { min: 0, max: 4 }) }, false],
    [{ satroomsource: S('0', 'i', { min: 0, max: 4 }) }, false],
  ];
  for (const [set, want] of cases) {
    const h = await startPage({
      api: (m, p) => (p.endsWith('/v2/sat/status') ? Object.assign({}, BASE, { ble_pin_notice: false }) : (p.endsWith('/v2/settings') && m === 'GET') ? Object.assign({ satsensormaxage: S('300', 'i') }, set) : undefined),
      beforeScript: "try { localStorage.setItem('otgw-v2-satview', 'control'); } catch (e) {}",
    });
    try {
      await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
      await h.until(`!!document.querySelector('#satSetL2 .srow[data-key="satroomsource"]')`, 12000);
      const hint = await h.evalJs(`(document.querySelector('#satSetL2 .srow[data-key="satroomsource"] .shint') || {}).textContent || ''`);
      if (want) assert.match(hint, /This source is not enabled; SAT uses the next source in the auto order\./, JSON.stringify(set));
      else assert.equal(hint, '', JSON.stringify(set));
    } finally { await h.close(); }
  }
});
