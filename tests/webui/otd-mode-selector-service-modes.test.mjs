// TASK-1716 (small variant, Robert 2026-11-08): the OT-Direct mode selector offers gateway,
// monitor and master only. Bypass and loopback are service modes (REST/command only): they are
// not offered, but a stored bypass or loopback is still shown, selected, as "(service mode)",
// so a unit that runs one does not silently show another mode.
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. OTGW_DATA_DIR at an
// older copy of data/ shows it red.
//
// Run: node --test tests/webui/otd-mode-selector-service-modes.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';

function fakeDevice(stored) {
  const settings = {
    hostname: { value: 'otgw', type: 's', maxlen: 30 },
    otdmode: { value: stored, type: 'i', min: 0, max: 4 },
    otdautodetect: { value: true, type: 'b' },
  };
  return (method, p) => {
    if (p.endsWith('/v2/settings')) return method === 'POST' ? { status: 'ok' } : settings;
    if (p.endsWith('/v2/device/info')) return { device: { otdirectavailable: true, otcommandinterface: 'OT-Direct' } };
    if (p.endsWith('/v2/health')) return { health: { otcommandinterface: 'OT-Direct', otgwconnected: true } };
    return undefined;
  };
}

async function selector(stored) {
  const h = await startPage({ api: fakeDevice(stored) });
  try {
    assert.ok(await h.until(`!!document.querySelector('.connsum')`, 40000), 'v2.js started');
    await h.evalJs(`document.querySelector('button.seg[data-page="settings"]').click()`);
    assert.ok(await h.until(`!!document.querySelector('#setRail .rail-item[data-cat="otd"]')`, 40000), 'OT-Direct category');
    await h.evalJs(`document.querySelector('#setRail .rail-item[data-cat="otd"]').click()`);
    assert.ok(await h.until(`!!document.querySelector('#setCols .srow[data-key="otdmode"] select')`, 40000), 'otdmode select');
    return await h.evalJs(`(function(){ var s = document.querySelector('#setCols .srow[data-key="otdmode"] select');
      return { values: [].map.call(s.options, function (o) { return o.value; }),
               labels: [].map.call(s.options, function (o) { return o.textContent; }), selected: s.value }; })()`);
  } finally { await h.close(); }
}

test('stored gateway: only gateway, monitor and master are offered', { skip }, async () => {
  const s = await selector(1);
  assert.deepEqual(s.values, ['1', '2', '3'], JSON.stringify(s));
  assert.equal(s.selected, '1');
});

test('stored master: still offered and selected', { skip }, async () => {
  const s = await selector(3);
  assert.deepEqual(s.values, ['1', '2', '3'], JSON.stringify(s));
  assert.equal(s.selected, '3');
});

test('stored bypass: shown selected as a service mode, loopback not offered', { skip }, async () => {
  const s = await selector(0);
  assert.deepEqual(s.values, ['0', '1', '2', '3'], JSON.stringify(s));
  assert.equal(s.selected, '0');
  assert.equal(s.labels[0], 'Bypass (service mode)');
});

test('stored loopback: shown selected as a service mode, bypass not offered', { skip }, async () => {
  const s = await selector(4);
  assert.deepEqual(s.values, ['1', '2', '3', '4'], JSON.stringify(s));
  assert.equal(s.selected, '4');
  assert.equal(s.labels[3], 'Loopback (service mode)');
});
