// TASK-3120 / TASK-3121: on an OTGW32 the S0 counter pin starts as "not set". The firmware sends it as the integer -1
// (GET /api/v2/settings: s0counterpin value -1, range min -1, max 48) and accepts -1 in a POST as "not set". The web UI shows an
// EMPTY field with a "not set" hint (never "-1"), a typed number is sent as typed, and a cleared field is sent as -1.
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. OTGW_DATA_DIR=<older data dir> shows it red.
// Run: node --test tests/webui/s0-pin-not-set-3120.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME, sleep } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';

function fake(pinValue) {
  const settings = {
    hostname: { value: 'otgw', type: 's', maxlen: 30 },
    s0counterenabled: { value: false, type: 'b' },
    s0counterpin: { value: pinValue, type: 'i', min: -1, max: 48 },
  };
  const device = { hostname: 'otgw', otdirectavailable: false, otcommandinterface: 'PIC' };
  return (method, p) => {
    if (p.endsWith('/v2/settings')) return method === 'POST' ? { status: 'ok' } : settings;
    if (p.endsWith('/v2/device/info')) return { device };
    if (p.endsWith('/v2/health')) return { health: { otcommandinterface: 'PIC', otgwconnected: true } };
    return undefined;
  };
}

async function openS0(h) {
  assert.ok(await h.until(`!!document.querySelector('.connsum')`, 40000), 'v2.js started');
  await h.evalJs(`document.querySelector('button.seg[data-page="settings"]').click()`);
  assert.ok(await h.until(`!!document.querySelector('#setRail .rail-item[data-cat="s0"]')`, 40000), 'S0 category in the rail');
  await h.evalJs(`document.querySelector('#setRail .rail-item[data-cat="s0"]').click()`);
  assert.ok(await h.until(`!!document.querySelector('.srow[data-key="s0counterpin"] input')`, 20000), 'S0 pin field');
}
const field = (h) => h.evalJs(`(function(){ var i = document.querySelector('.srow[data-key="s0counterpin"] input'); return { value: i.value, placeholder: i.placeholder }; })()`);

test('pin -1 shows an empty field with a "not set" hint, never -1', { skip }, async () => {
  const h = await startPage({ api: fake(-1) });
  try {
    await openS0(h);
    const f = await field(h);
    assert.equal(f.value, '', 'the field is empty');
    assert.equal(f.placeholder, 'not set');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('a pin number is shown as is and a cleared field is saved as -1', { skip }, async () => {
  const h = await startPage({ api: fake(12) });
  try {
    await openS0(h);
    assert.equal((await field(h)).value, '12');
    await h.evalJs(`(function(){ var i = document.querySelector('.srow[data-key="s0counterpin"] input'); i.value = ''; i.dispatchEvent(new Event('input', { bubbles: true })); })()`);
    await h.evalJs(`document.getElementById('btnSave').click()`);
    let post = null;
    for (let i = 0; i < 60 && !post; i++) { post = h.log.find((l) => l.startsWith('POST /api/v2/settings') && /s0counterpin/.test(l)); if (!post) await sleep(100); }
    assert.ok(post, 'a POST with the pin was sent: ' + JSON.stringify(h.log.filter((l) => l.startsWith('POST'))));
    assert.match(post, /"value":"?-1"?/, 'a cleared field is sent as -1: ' + post);
  } finally { await h.close(); }
});

test('typing a pin into the empty field sends that number', { skip }, async () => {
  const h = await startPage({ api: fake(-1) });
  try {
    await openS0(h);
    await h.evalJs(`(function(){ var i = document.querySelector('.srow[data-key="s0counterpin"] input'); i.value = '5'; i.dispatchEvent(new Event('input', { bubbles: true })); })()`);
    await h.evalJs(`document.getElementById('btnSave').click()`);
    let post = null;
    for (let i = 0; i < 60 && !post; i++) { post = h.log.find((l) => l.startsWith('POST /api/v2/settings') && /s0counterpin/.test(l)); if (!post) await sleep(100); }
    assert.ok(post, 'a POST with the pin was sent');
    assert.match(post, /"value":"?5"?/, post);
  } finally { await h.close(); }
});
