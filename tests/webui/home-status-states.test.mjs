// TASK-2959 F4 (spec T4, docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T4-copy-and-states.md):
// the Home status line says nothing healthy without a healthy bus. Before the fix it read
// "Standby · everything quiet" with no data and no boiler, and the header said
// "All systems connected" before the first health answer.
// OTGW_DATA_DIR=<dir> runs it against another tree (old-vs-fix).
//
// Run: node --test tests/webui/home-status-states.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage } from './lib/page-harness.mjs';

const OTMON = { otmonitor: { flamestatus: { value: 'Off', epoch: 1 }, chmodus: { value: 'Off', epoch: 1 }, dhwmode: { value: 'Off', epoch: 1 }, faultindicator: { value: 'Off', epoch: 1 } } };
const health = (boiler) => ({ health: { otcommandinterface: 'PIC', boilerconnected: boiler, thermostatconnected: true, boiler_age_s: boiler ? 1 : -1, thermostat_age_s: 1 } });
const READ = `({ status: (document.getElementById('aStatusTxt') || {}).textContent,
  link: (function () { var b = document.getElementById('aStatusLink'); return b && !b.hidden ? b.textContent : ''; })(),
  hdr: (function () { var b = document.querySelector('#connStrip .connsum'); return b ? b.textContent : ''; })() })`;

async function open(api) {
  const h = await startPage({ api });
  await h.until(`document.getElementById('aStatusTxt') && document.getElementById('aStatusTxt').textContent !== '—'`, 8000);
  return h;
}

test('no OpenTherm data yet: waiting line, header checking', async () => {
  const h = await open((m, p) => (p.endsWith('/v2/otgw/otmonitor') || p.endsWith('/v2/health') ? { __status: 503 } : undefined));
  try {
    await h.sleep(2500);
    const r = await h.evalJs(READ);
    assert.equal(r.status, 'Waiting for data from the gateway…');
    assert.match(r.hdr, /checking…/);
    assert.doesNotMatch(r.hdr, /All systems connected/);
  } finally { await h.close(); }
});

test('boiler link down: No boiler connected with a working Check connection link', async () => {
  const h = await open((m, p) => (p.endsWith('/v2/otgw/otmonitor') ? OTMON : p.endsWith('/v2/health') ? health(false) : undefined));
  try {
    const ok = await h.until(`document.getElementById('aStatusTxt').textContent === 'No boiler connected'`, 10000);
    const r = await h.evalJs(READ);
    assert.ok(ok, 'status reads ' + r.status);
    assert.equal(r.link, 'Check connection ›');
    await h.evalJs(`document.getElementById('aStatusLink').click()`);
    const mon = await h.until(`document.getElementById('page-monitor').classList.contains('active')`, 4000);
    assert.ok(mon, 'the link opens Monitor');
  } finally { await h.close(); }
});

test('boiler link up and nothing running: everything quiet, no link', async () => {
  const h = await open((m, p) => (p.endsWith('/v2/otgw/otmonitor') ? OTMON : p.endsWith('/v2/health') ? health(true) : undefined));
  try {
    const ok = await h.until(`document.getElementById('aStatusTxt').textContent === 'Standby · everything quiet' && document.querySelector('#connStrip .connsum').textContent.indexOf('checking') < 0`, 10000);
    const r = await h.evalJs(READ);
    assert.ok(ok, 'status reads ' + r.status);
    assert.equal(r.link, '');
    assert.doesNotMatch(r.hdr, /checking…/);
  } finally { await h.close(); }
});

// A socket that opens and stays silent: the old code only re-seeded while the socket was closed.
const OPEN_WS = "window.WebSocket = function () { var s = this; s.readyState = 0; setTimeout(function () { s.readyState = 1; if (s.onopen) s.onopen({}); }, 50); };" +
  " window.WebSocket.prototype.send = function () {}; window.WebSocket.prototype.close = function () { this.readyState = 3; }; window.WebSocket.OPEN = 1;";

test('a failed first seed is retried while the socket is open: waiting goes away without visiting Monitor', async () => {
  let n = 0;
  const api = (m, p) => (p.endsWith('/v2/otgw/otmonitor') ? (++n <= 2 ? { __status: 503 } : OTMON) : p.endsWith('/v2/health') ? health(true) : undefined);
  const h = await startPage({ api, beforeScript: OPEN_WS });
  try {
    const ok = await h.until(`document.getElementById('aStatusTxt').textContent === 'Standby · everything quiet'`, 40000);
    const r = await h.evalJs(READ);
    assert.ok(ok, 'still ' + r.status + ' after ' + n + ' otmonitor requests');
  } finally { await h.close(); }
});

test('OpenTherm data but no health answer yet: still waiting, never everything quiet', async () => {
  const h = await open((m, p) => (p.endsWith('/v2/otgw/otmonitor') ? OTMON : p.endsWith('/v2/health') ? { __status: 503 } : undefined));
  try {
    await h.sleep(5000);
    const r = await h.evalJs(READ);
    assert.equal(r.status, 'Waiting for data from the gateway…');
  } finally { await h.close(); }
});

// ux check of beta.413 (2026-11-21): live data was on the tiles while Home still said "Waiting for data",
// because the waiting line also held until the first /v2/health answer. With data, Home shows what the
// data says; only "No boiler connected" and "everything quiet" need the health answer.
test('live data before the first health answer: Home shows the data, not the waiting line', async () => {
  const FLAME = { otmonitor: Object.assign({}, OTMON.otmonitor, { flamestatus: { value: 'On', epoch: 1 }, chmodus: { value: 'On', epoch: 1 } }) };
  const h = await open((m, p) => (p.endsWith('/v2/otgw/otmonitor') ? FLAME : p.endsWith('/v2/health') ? { __status: 503 } : undefined));
  try {
    const ok = await h.until(`/flame on/i.test(document.getElementById('aStatusTxt').textContent)`, 10000);
    const r = await h.evalJs(READ);
    assert.ok(ok, 'status reads ' + r.status);
  } finally { await h.close(); }
});
