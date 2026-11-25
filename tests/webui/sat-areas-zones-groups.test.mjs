// TASK-3014 (gruijter's suggestion, Robert approved the design proposal 2026-11-21): the SAT settings show
// 'Averaged room sensors' (formerly Areas / Multi-area, sensors numbered 1-4) and 'Zones' (zone count, timeout
// and headroom together) as two groups in the Control view, each with a one-line explanation. Setting keys,
// MQTT and REST stay as they are. Before the fix Zone count sat under Areas and the zone timeout and headroom
// under Technical > Sensors, the areas were numbered 0-3, and NL called both counts 'Aantal zones'.
// OTGW_DATA_DIR=<dir> runs it against another tree (old-vs-fix).
//
// Run: node --test tests/webui/sat-areas-zones-groups.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startPage } from './lib/page-harness.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const S = (value, type) => ({ value, type });
const SETTINGS = {
  satmultiarea: S('true', 'b'), satmultiareacount: S('4', 'i'),
  satsensorarea0: S('', 's'), satsensorarea1: S('', 's'), satsensorarea2: S('', 's'), satsensorarea3: S('', 's'),
  satareaweight0: S('1', 'f'), satareaweight1: S('1', 'f'), satareaweight2: S('1', 'f'), satareaweight3: S('1', 'f'),
  satzonecount: S('2', 'i'), satzonetimeout: S('300', 'i'), saterrormon: S('true', 'b'),
};
const GROUPS = `(function () {
  function groups(id) {
    return Array.prototype.map.call(document.querySelectorAll('#' + id + ' .set-group'), function (g) {
      var d = g.querySelector('.set-group-desc');
      return { title: g.querySelector('h3').textContent, desc: d ? d.textContent : '',
        rows: Array.prototype.map.call(g.querySelectorAll('.srow'), function (r) {
          return { key: r.dataset.key, label: r.querySelector('.slbl').firstChild.textContent.trim() }; }) };
    });
  }
  return { l2: groups('satSetL2'), l3: groups('satSetL3') };
})()`;

test('Control view: Averaged room sensors and Zones side by side, sensors numbered 1-4, Technical keeps error monitoring', async () => {
  const h = await startPage({
    width: 1400,
    api: (m, p) => ((p.endsWith('/v2/settings') && m === 'GET') ? SETTINGS : p.endsWith('/v2/sat/status') ? { enabled: true, active: true } : undefined),
    beforeScript: "try { localStorage.setItem('otgw-v2-satview', 'technical'); } catch (e) {}",
  });
  try {
    await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
    await h.until(`!!document.querySelector('#satSetL2 .srow[data-key="satzonecount"]')`, 12000);
    const r = await h.evalJs(GROUPS);
    const avg = r.l2.find((g) => g.title === 'Averaged room sensors');
    const zones = r.l2.find((g) => g.title === 'Zones');
    assert.ok(avg, 'no Averaged room sensors group: ' + JSON.stringify(r.l2.map((g) => g.title)));
    assert.ok(zones, 'no Zones group: ' + JSON.stringify(r.l2.map((g) => g.title)));
    assert.equal(r.l2.indexOf(zones), r.l2.indexOf(avg) + 1, 'the two groups sit next to each other');
    assert.match(avg.desc, /weighted average of up to 4 room sensors/);
    assert.match(zones.desc, /can only raise the main room's heat demand/);
    const lbl = Object.fromEntries(avg.rows.map((x) => [x.key, x.label]));
    assert.equal(lbl.satmultiarea, 'Average several room sensors');
    assert.equal(lbl.satmultiareacount, 'Number of sensors');
    assert.deepEqual(['satsensorarea0', 'satsensorarea1', 'satsensorarea2', 'satsensorarea3'].map((k) => lbl[k]), ['Sensor 1', 'Sensor 2', 'Sensor 3', 'Sensor 4']);
    assert.deepEqual(['satareaweight0', 'satareaweight1', 'satareaweight2', 'satareaweight3'].map((k) => lbl[k]), ['Weight sensor 1', 'Weight sensor 2', 'Weight sensor 3', 'Weight sensor 4']);
    assert.deepEqual(zones.rows.map((x) => x.key), ['satzonecount', 'satzonetimeout']);
    assert.equal(zones.rows[0].label, 'Number of zones');
    const l3keys = r.l3.flatMap((g) => g.rows.map((x) => x.key));
    assert.ok(l3keys.includes('saterrormon'), 'error monitoring stays in Technical');
    assert.ok(!l3keys.some((k) => /^satzone/.test(k)), 'zone settings left Technical: ' + JSON.stringify(l3keys));
  } finally { await h.close(); }
});

test('NL and DE give the sensor count and the zone count different names', () => {
  for (const l of ['nl', 'de']) {
    const j = JSON.parse(fs.readFileSync(path.join(ROOT, 'i18n', l + '.json'), 'utf8'));
    assert.notEqual(j.settings.label.satmultiareacount, j.settings.label.satzonecount, l + ': both counts read ' + j.settings.label.satzonecount);
  }
});
