// TASK-3121 (otgw-ux-designer's OTGW32 newcomer check): on an OT-Direct board the web UI stops showing PIC-only things and says what is
// not available; on a PIC board nothing changes.
//  - Settings > OpenTherm: no "PIC temperature sensor" row on OT-Direct (PIC Classic only); the row stays on a PIC board.
//  - Advanced nav subtitle: "debug · files · system" on OT-Direct, "pic · debug · files · system" on a PIC board.
//  - Settings > OT-Direct subtitle names what is not available on this board (PIC firmware updates, Bluetooth sensors).
//  - The two ventilation bypass rows say "ventilation" (they are not the thermostat bypass relay).
// An unknown board answer hides nothing (checked by the PIC case before the answer: the row is present in the first render).
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. OTGW_DATA_DIR=<older data dir> shows it red.
// OTGW_SHOT=<prefix> saves screenshots (synthetic data).
//
// Run: node --test tests/webui/otd-board-hints-3121.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME, sleep } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';
const S = (value, maxlen = 30) => ({ value, type: 's', maxlen });
const B = (value) => ({ value, type: 'b' });

function fakeDevice(pic, extra = {}) {
  const settings = Object.assign({ hostname: S('otgw'), pictempsensor: B(false), ledblink: B(true), gpiooutputsenabled: B(false), otgwcommandenable: B(false), otgwcommands: S('PS=0;GW=1', 128), gpiooutputspin: { value: 11, type: 'i', min: 0, max: 48 }, s0counterenabled: B(false) },
    pic ? {} : { otdmode: { value: 1, type: 'i', min: 0, max: 4 }, otdopenbypass: B(false), otdautobypass: B(false), otdhasbypassrelay: B(false) });
  const device = pic
    ? { hostname: 'otgw', otdirectavailable: false, otcommandinterface: 'PIC', picfwversion: '6.6', picfwtype: 'gateway' }
    : Object.assign({ hostname: 'otgw', otdirectavailable: true, otcommandinterface: 'OT-Direct', otdmode: 'gateway', otdbypass: false }, extra);
  const health = { otcommandinterface: pic ? 'PIC' : 'OT-Direct', otgwconnected: true, wifirssi: -60 };
  return (method, p) => {
    if (p.endsWith('/v2/settings')) return method === 'POST' ? { status: 'ok' } : settings;
    if (p.endsWith('/v2/device/info')) return { device };
    if (p.endsWith('/v2/health')) return { health };
    if (p.endsWith('/v2/otdirect/status')) return { otdirect_status: { mode: 'gateway' } };
    if (p.endsWith('/v2/otdirect/overrides')) return { overrides: [] };
    return undefined;
  };
}

async function look(h, pic) {
  assert.ok(await h.until(`!!document.querySelector('.connsum')`, 40000), 'v2.js started');
  await sleep(1500);   // the device answer (applyPicTabGate) and the first settings render
  await h.evalJs(`document.querySelector('button.seg[data-page="settings"]').click()`);
  assert.ok(await h.until(`document.querySelectorAll('#setRail .rail-item').length > 0`, 40000), 'settings rail built');
  await sleep(500);
  const nav = await h.evalJs(`(document.querySelector('.seg[data-page="advanced"] small') || {}).textContent`);
  await h.evalJs(`(function(){ var r = document.querySelector('#setRail .rail-item[data-cat="otgw"]'); if (r) r.click(); })()`);
  await sleep(400);
  const picRow = await h.evalJs(`!!document.querySelector('.srow[data-key="pictempsensor"]')`);
  const bootRows = await h.evalJs(`!!document.querySelector('.srow[data-key="otgwcommands"]') || !!document.querySelector('.srow[data-key="otgwcommandenable"]')`);
  let otd = null;
  if (!pic) {
    await h.evalJs(`document.querySelector('#setRail .rail-item[data-cat="otd"]').click()`);
    await sleep(500);
    otd = await h.evalJs(`(function(){ var labels = {};
      ['otdopenbypass', 'otdautobypass', 'otdhasbypassrelay'].forEach(function (k) { var r = document.querySelector('.srow[data-key="' + k + '"]'); labels[k] = r ? r.textContent : null; });
      var d = document.querySelector('#setCols .set-group .gdesc, #setCols .cat-desc, #setCols .set-desc');
      return { labels: labels, page: document.getElementById('setCols').textContent }; })()`);
  }
  return { nav: (nav || '').trim(), picRow, bootRows, otd };
}

test('OT-Direct board: no PIC temperature sensor row, nav subtitle without pic, not-available line, bypass rows hidden', { skip }, async () => {
  const h = await startPage({ api: fakeDevice(false) });
  try {
    const r = await look(h, false);
    if (process.env.OTGW_SHOT) await h.shot(process.env.OTGW_SHOT + '-otd.png');
    assert.equal(r.picRow, false, 'the PIC temperature sensor row is hidden on OT-Direct');
    assert.equal(r.bootRows, false, 'the boot-commands rows do nothing without a PIC and are hidden on OT-Direct');
    assert.equal(r.nav, 'debug · files · system', 'Advanced subtitle has no pic');
    assert.match(r.otd.page, /Not available on this board: PIC firmware updates and Bluetooth sensors\./, 'the not-available line');
    // nothing reads the three bypass settings in this release (TASK-3121, coordinator decision): no row promises a function
    assert.deepEqual(r.otd.labels, { otdopenbypass: null, otdautobypass: null, otdhasbypassrelay: null }, 'the three bypass rows are hidden');
    assert.doesNotMatch(r.otd.page, /bypass/i, 'no bypass text in the OT-Direct group');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('PIC board: the PIC temperature sensor row stays and the Advanced subtitle keeps pic', { skip }, async () => {
  const h = await startPage({ api: fakeDevice(true) });
  try {
    const r = await look(h, true);
    if (process.env.OTGW_SHOT) await h.shot(process.env.OTGW_SHOT + '-pic.png');
    assert.equal(r.picRow, true, 'the PIC temperature sensor row is shown on a PIC board');
    assert.equal(r.bootRows, true, 'the boot-commands rows stay on a PIC board');
    assert.equal(r.nav, 'pic · debug · files · system');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

// TASK-3120 (Robert, Q32): the OTGW32 has no pad for the GPIO output, the firmware says so with device/info has_output_pin: false.
// S0 stays visible (a promise of 1.0); absent or true = shown, so older firmware keeps today's UI.
async function railCats(extra) {
  const h = await startPage({ api: fakeDevice(false, extra) });
  try {
    assert.ok(await h.until(`!!document.querySelector('.connsum')`, 40000), 'v2.js started');
    await sleep(1500);
    await h.evalJs(`document.querySelector('button.seg[data-page="settings"]').click()`);
    assert.ok(await h.until(`document.querySelectorAll('#setRail .rail-item').length > 0`, 40000), 'settings rail built');
    await sleep(500);
    return await h.evalJs(`[].map.call(document.querySelectorAll('#setRail .rail-item'), function (r) { return r.dataset.cat; })`);
  } finally { await h.close(); }
}
test('has_output_pin false: no GPIO outputs group in Settings, S0 stays', { skip }, async () => {
  const cats = await railCats({ has_output_pin: false });
  assert.ok(!cats.includes('outputs'), 'GPIO outputs hidden: ' + cats);
  assert.ok(cats.includes('s0'), 'S0 stays: ' + cats);
});
test('has_output_pin true or absent: the GPIO outputs group is shown', { skip }, async () => {
  assert.ok((await railCats({ has_output_pin: true })).includes('outputs'));
  assert.ok((await railCats({})).includes('outputs'), 'absent (older firmware) = shown');
});
