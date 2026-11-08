// TASK-1719: in PIC mode (board mode 1 or 3, or a live PIC) the web UI shows no OT-Direct element.
// - Settings: no OT-Direct category in the rail. The firmware leaves the otd* keys out of
//   /api/v2/settings in PIC mode (tests/test_otd_settings_hidden_in_pic_mode.py), and the rail
//   builds itself from those keys.
// - Advanced > Debug Information: no "OT-Direct (otd*)" group. In PIC mode device/info still
//   carries otdirectavailable=false, which rendered that group with one row.
// - Monitor: the OT-Direct status and override panels stay hidden (regression check of the
//   TASK-964 / TASK-1223.16 gate on /health otcommandinterface).
// The OT-Direct case is the control: the same three elements are shown there.
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. OTGW_DATA_DIR at an
// older copy of data/ shows it red.
//
// Run: node --test tests/webui/otd-hidden-in-pic-mode.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME, sleep } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';
const S = (value, maxlen = 30) => ({ value, type: 's', maxlen });
const OTD_SETTINGS = {
  otdmode: { value: 1, type: 'i', min: 0, max: 4 },
  otdautodetect: { value: true, type: 'b' },
  otdfailsafe: { value: false, type: 'b' },
};

// Payload shapes as restAPI.ino sends them in each mode.
function fakeDevice(pic) {
  const settings = Object.assign({ hostname: S('otgw'), mqttbroker: S('broker.local') }, pic ? {} : OTD_SETTINGS);
  const device = pic
    ? { hostname: 'otgw', otdirectavailable: false, otcommandinterface: 'PIC', picfwversion: '6.6', picfwtype: 'gateway' }
    : { hostname: 'otgw', otdirectavailable: true, otcommandinterface: 'OT-Direct', otdmode: 'gateway', otdbypass: false };
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

async function look(h) {
  assert.ok(await h.until(`!!document.querySelector('.connsum')`, 40000), 'v2.js started');
  // Monitor panels follow the first /health poll.
  assert.ok(await h.until(`!!window.__otgwHealthSeen || performance.getEntriesByType('resource').some(function (e) { return /\\/v2\\/health/.test(e.name); })`, 40000), '/health fetched');
  await sleep(500);
  const panels = await h.evalJs(`(function(){ var d = function (id) { var e = document.getElementById(id); return e ? e.style.display !== 'none' : null; };
    return { status: d('otdStatus'), ovr: d('otdOvr') }; })()`);
  await h.evalJs(`document.querySelector('button.seg[data-page="settings"]').click()`);
  assert.ok(await h.until(`document.querySelectorAll('#setRail .rail-item').length > 0`, 40000), 'settings rail built');
  const rail = await h.evalJs(`!!document.querySelector('#setRail .rail-item[data-cat="otd"]')`);
  await h.evalJs(`document.querySelector('button.seg[data-page="advanced"]').click()`);
  await h.evalJs(`document.querySelector('.subtab[data-adv="debug"]').click()`);
  assert.ok(await h.until(`document.querySelectorAll('#dbgGroups .devinfo-group').length > 0`, 40000), 'device info groups rendered');
  const group = await h.evalJs(`[].some.call(document.querySelectorAll('#dbgGroups h4'), function (e) { return /OT-Direct/.test(e.textContent); })`);
  return { rail, group, panels };
}

test('PIC mode: no OT-Direct settings category, device-info group or Monitor panel', { skip }, async () => {
  const h = await startPage({ api: fakeDevice(true) });
  try {
    const r = await look(h);
    if (process.env.OTGW_SHOT) await h.shot(process.env.OTGW_SHOT + '-pic.png');
    assert.equal(r.rail, false, 'Settings rail has no OT-Direct category');
    assert.equal(r.group, false, 'Device info has no OT-Direct group');
    assert.deepEqual(r.panels, { status: false, ovr: false }, 'Monitor OT-Direct panels hidden');
  } finally { await h.close(); }
});

test('OT-Direct mode (control): the same elements are shown', { skip }, async () => {
  const h = await startPage({ api: fakeDevice(false) });
  try {
    const r = await look(h);
    if (process.env.OTGW_SHOT) await h.shot(process.env.OTGW_SHOT + '-otd.png');
    assert.equal(r.rail, true, 'Settings rail has the OT-Direct category');
    assert.equal(r.group, true, 'Device info has the OT-Direct group');
    assert.deepEqual(r.panels, { status: true, ovr: true }, 'Monitor OT-Direct panels shown');
  } finally { await h.close(); }
});
