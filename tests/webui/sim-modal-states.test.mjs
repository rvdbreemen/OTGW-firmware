// TASK-2959 F6 (spec T4): the simulation modal waits for the enable POST and shows a failure.
// Before the fix it fired the POST, swallowed every error and closed at once. The modal opens 12 s
// after load on a PIC bench with nothing on the bus (maybePromptSimulation).
// OTGW_DATA_DIR=<dir> runs it against another tree (old-vs-fix).
//
// Run: node --test tests/webui/sim-modal-states.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage } from './lib/page-harness.mjs';

const INFO = { device: { otcommandinterface: 'PIC', otgwsimulation: false, boilerconnected: false, thermostatconnected: false, otgwconnected: false, fwversion: '2.0.0-beta.1' } };

async function openModal(post) {
  const ref = { post };
  const h = await startPage({
    api: (m, p) => {
      if (p.endsWith('/v2/device/info')) return INFO;
      if (p.endsWith('/v2/settings') && m === 'POST') return ref.post();
      return undefined;
    },
    beforeScript: "try { localStorage.removeItem('otgw-sim-prompted'); } catch (e) {}",
  });
  const open = await h.until(`!!document.querySelector('#simBackdrop .sim-modal')`, 25000);
  assert.ok(open, 'the simulation modal opens');
  return { h, ref };
}
const READ = `(function () {
  var bd = document.getElementById('simBackdrop'); if (!bd) return { open: false };
  var b = bd.querySelectorAll('.acts button'); var err = bd.querySelector('.sim-err');
  var m = bd.querySelector('.sim-modal'), link = bd.querySelector('.sim-err-link');
  return { open: true, no: b[0].textContent, noDis: b[0].disabled, yes: b[1].textContent, yesDis: b[1].disabled,
    err: err && !err.hidden ? err.textContent : '', role: err ? err.getAttribute('role') : null,
    dlg: m ? m.getAttribute('role') + '/' + m.getAttribute('aria-modal') : '', link: link && !link.hidden ? link.textContent : '',
    focusYes: document.activeElement === b[1] };
})()`;
const clickYes = `document.querySelectorAll('#simBackdrop .acts button')[1].click()`;

test('HTTP 500: the modal stays open, names HTTP 500 and offers Try again', async () => {
  const { h } = await openModal(() => ({ __status: 500 }));
  try {
    await h.evalJs(clickYes);
    const ok = await h.until(`(function(){ var e = document.querySelector('#simBackdrop .sim-err'); return !!e && !e.hidden; })()`, 5000);
    const r = await h.evalJs(READ);
    assert.ok(ok && r.open, 'error line shows and the modal stays open: ' + JSON.stringify(r));
    assert.match(r.err, /HTTP 500/); assert.equal(r.role, 'alert'); assert.doesNotMatch(r.err, /Settings ›/);
    assert.equal(r.link, 'Open the simulation setting');
    assert.equal(r.yes, 'Try again'); assert.equal(r.yesDis, false); assert.equal(r.noDis, false);
    assert.equal(r.dlg, 'dialog/true'); assert.equal(r.focusYes, true, 'focus returns to Try again');
  } finally { await h.close(); }
});

test('network error: the reason reads no answer from the gateway', async () => {
  const { h } = await openModal(() => ({ __reset: true }));
  try {
    await h.evalJs(clickYes);
    const ok = await h.until(`(function(){ var e = document.querySelector('#simBackdrop .sim-err'); return !!e && !e.hidden; })()`, 8000);
    const r = await h.evalJs(READ);
    assert.ok(ok, 'error line shows: ' + JSON.stringify(r));
    assert.match(r.err, /no answer from the gateway/);
  } finally { await h.close(); }
});

test('pending disables both buttons; HTTP 200 closes the modal and shows the toast', async () => {
  const env = await openModal(() => ({ ok: true }));
  const { h } = env;
  try {
    // Hold the POST in the page so the pending state can be read, then release it.
    await h.evalJs(`window.__simFetch = window.fetch; window.fetch = function (u, o) { if (o && o.method === 'POST') return new Promise(function (res) { window.__simRelease = function () { window.__simFetch(u, o).then(res); }; }); return window.__simFetch(u, o); }`);
    await h.evalJs(clickYes);
    const pend = await h.evalJs(READ);
    assert.equal(pend.yes, 'Enabling…'); assert.equal(pend.yesDis, true); assert.equal(pend.noDis, true);
    await h.evalJs(`window.__simRelease()`);
    const closed = await h.until(`!document.getElementById('simBackdrop')`, 5000);
    assert.ok(closed, 'the modal closes on HTTP 200');
    const toast = await h.evalJs(`(document.getElementById('toast') || {}).textContent`);
    assert.equal(toast, 'Simulation enabled');
  } finally { await h.close(); }
});

test('HTTP 409 with a reason: the reason shows and the button closes (no Try again)', async () => {
  const { h } = await openModal(() => ({ __status: 409, error: { status: 409, message: 'simulation unavailable: boiler hardware detected' } }));
  try {
    await h.evalJs(clickYes);
    const ok = await h.until(`(function(){ var e = document.querySelector('#simBackdrop .sim-err'); return !!e && !e.hidden; })()`, 5000);
    const r = await h.evalJs(READ);
    assert.ok(ok, 'error line shows: ' + JSON.stringify(r));
    assert.match(r.err, /simulation unavailable: boiler hardware detected/);
    assert.equal(r.yes, 'Close');
    await h.evalJs(clickYes);
    assert.ok(await h.until(`!document.getElementById('simBackdrop')`, 3000), 'Close closes the modal');
  } finally { await h.close(); }
});

test('Escape closes the modal when nothing is pending', async () => {
  const { h } = await openModal(() => ({ ok: true }));
  try {
    await h.evalJs(`document.querySelector('#simBackdrop .sim-modal').dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }))`);
    assert.ok(await h.until(`!document.getElementById('simBackdrop')`, 3000), 'Escape closes');
  } finally { await h.close(); }
});
