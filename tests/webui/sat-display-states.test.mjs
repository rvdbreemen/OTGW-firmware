// TASK-2957 (spec T2, docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T2-sat-display-states.md):
// the SAT page derives one display state per status poll and shows the pill, headline, sub-line
// and card treatment of that state. Before the fix the headline chain never read ot_link_lost or
// outside_temp_missing, so the .63 case (enabled, OpenTherm lost, boiler idle) read
// "At target · idle". Each case feeds one /api/v2/sat/status object to the REAL page.
// OTGW_DATA_DIR=<dir> runs it against another tree (old-vs-fix).
//
// Run: node --test tests/webui/sat-display-states.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage } from './lib/page-harness.mjs';

const HEALTH_OK = { health: { otcommandinterface: 'PIC', boilerconnected: true, thermostatconnected: true, boiler_age_s: 1, thermostat_age_s: 1 } };
const HEALTH_BOILER_DOWN = { health: { otcommandinterface: 'PIC', boilerconnected: false, thermostatconnected: true, boiler_age_s: -1, thermostat_age_s: 1 } };
const BASE = { enabled: true, active: true, boiler_status: 'idle', target_temp: 20, room_temp: 19.5, outside_temp: 8, control_mode: 1,
  pid_output: 42.5, error: 0.5, pid_p: 1.25, pid_i: 2.5, pid_d: 0.1, current_modulation: 100 };

async function openSat(health) {
  const ref = { sat: BASE };
  const h = await startPage({
    api: (m, p) => (p.endsWith('/v2/sat/status') ? ref.sat : p.endsWith('/v2/health') ? health : undefined),
    beforeScript: "try { localStorage.setItem('otgw-theme', 'light'); } catch (e) {}",
  });
  await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
  await h.until(`getComputedStyle(document.getElementById('page-sat')).display !== 'none'`);
  return { h, ref };
}

// Reads what the page shows; null when the element is missing.
const READ = `(function () {
  function tx(id) { var e = document.getElementById(id); return e ? e.textContent.trim() : null; }
  function kv(id, label) {
    var rows = document.querySelectorAll('#' + id + ' .kv-row');
    for (var i = 0; i < rows.length; i++) if (rows[i].children[0].textContent === label) return rows[i].children[1].textContent;
    return null;
  }
  function card(id) { var e = document.getElementById(id); var c = e && e.closest('.card'); return c ? c : null; }
  var sub = document.getElementById('satStateSub'), ctrl = card('satControlStatus');
  var btn = document.getElementById('satMsgBtn');
  return {
    big: tx('satStatusBig'), pill: tx('satPill'), pillCls: (document.getElementById('satPill') || {}).className,
    sub: sub && !sub.hidden ? sub.textContent.trim() : '', subLink: sub && !sub.hidden && sub.querySelector('button') ? sub.querySelector('button').textContent : '',
    enLbl: tx('satEnableLbl'), loop: tx('satLoopState'),
    ctrlStale: !!(ctrl && ctrl.classList.contains('is-stale')), ctrlTitle: ctrl ? ctrl.querySelector('h4').textContent.trim() : null,
    mod: kv('satControlStatus', 'Modulation sent'), modOld: kv('satControlStatus', 'Modulation'),
    pidOut: kv('satControlStatus', 'PID output'), err: kv('satControlStatus', 'Error'), pTerm: kv('satPidKv', 'P term'),
    cycle: tx('satCycleMode'), cycleSub: tx('satCycleSub'),
    msgTxt: tx('satMsgCount'), msgAria: btn ? btn.getAttribute('aria-label') : null, msgHidden: btn ? btn.hidden : null
  };
})()`;

async function show(env, sat, bigExpected) {
  env.ref.sat = Object.assign({}, BASE, sat);
  const ok = await env.h.until(`document.getElementById('satStatusBig').textContent.trim() === ${JSON.stringify(bigExpected)}`, 12000);
  const r = await env.h.evalJs(READ);
  assert.ok(ok, `headline should read "${bigExpected}" but reads "${r.big}" for ${JSON.stringify(sat)}`);
  return r;
}

test('every display state renders its pill, headline, sub-line and card treatment', async () => {
  const env = await openSat(HEALTH_OK);
  try {
    let r = await show(env, { enabled: false, active: false }, 'SAT is off');
    assert.equal(r.pill, 'Off'); assert.match(r.pillCls, /\boff\b/); assert.equal(r.sub, ''); assert.equal(r.ctrlStale, false);
    assert.equal(r.loop, 'off');

    r = await show(env, { active: false, safety_tripped: true }, 'SAT stopped: no OpenTherm interface');
    assert.equal(r.pill, 'Stopped'); assert.match(r.pillCls, /\bwarn\b/);
    assert.match(r.sub, /^SAT lost contact with the OpenTherm interface and stopped itself\./);
    assert.equal(r.ctrlStale, true); assert.match(r.ctrlTitle, /\(stopped\)$/); assert.equal(r.loop, 'stopped');

    // The .63 case: AC3, never "At target" or "idle" in the headline.
    r = await show(env, { active: false, ot_link_lost: true, boiler_status: 'idle', room_temp: 25.5, target_temp: 19.8 }, 'Paused: no boiler connection');
    assert.doesNotMatch(r.big, /at target|idle/i);
    assert.equal(r.pill, 'Paused'); assert.match(r.pillCls, /\bwarn\b/);
    assert.match(r.sub, /the boiler runs on its own control/); assert.match(r.ctrlTitle, /\(paused\)$/); assert.equal(r.loop, 'paused');

    // Both pause flags: PAUSED_OT wins.
    r = await show(env, { active: false, ot_link_lost: true, outside_temp_missing: true }, 'Paused: no boiler connection');

    r = await show(env, { active: false, outside_temp_missing: true }, 'Paused: no outside temperature');
    assert.equal(r.pill, 'Paused'); assert.equal(r.subLink, 'Set up a source');

    r = await show(env, { enabled: false, fallback_active: true, active: true, boiler_status: 'heating' }, 'Takeover (MQTT lost)');
    assert.equal(r.pill, 'Heating'); assert.match(r.pillCls, /\bwarn\b/); assert.equal(r.enLbl, 'Off · takeover active');
    assert.match(r.sub, /^Home Assistant has been unreachable over MQTT/); assert.equal(r.ctrlStale, false); assert.equal(r.loop, 'live');

    r = await show(env, { active: false }, 'Waiting to take over');
    assert.equal(r.pill, 'Waiting'); assert.match(r.pillCls, /\bidle\b/); assert.equal(r.subLink, '');
    assert.match(r.ctrlTitle, /\(waiting\)$/); assert.equal(r.enLbl, 'Enabled');

    r = await show(env, { window_open: true }, 'Window open');
    assert.equal(r.sub, ''); assert.equal(r.ctrlStale, false);
    r = await show(env, { summer_active: true }, 'Summer mode');
    r = await show(env, { boiler_status: 'heating' }, 'Heating to 20.0°');
    assert.equal(r.pill, 'Heating'); assert.doesNotMatch(r.pillCls, /\b(idle|warn|off)\b/);

    // AC4: the IDLE headline uses the band (+ 0.1) and the display step (+ 1.0).
    r = await show(env, { room_temp: 25.5, target_temp: 19.8 }, 'Idle · room above target');
    assert.equal(r.pill, 'Idle');
    r = await show(env, { room_temp: 20.0, target_temp: 19.8 }, 'Idle · room at target');
    r = await show(env, { room_temp: 19.0, target_temp: 19.8, boiler_status: 'anti_cycling' }, 'Idle · anti-cycling pause');   // TASK-3012: spec T2 3.1 wording
    r = await show(env, { room_temp: null }, 'Idle');
  } finally { await env.h.close(); }
});

test('not-live values show a dash while active is false; Modulation sent; burner cycle ignores control_mode', async () => {
  const env = await openSat(HEALTH_OK);
  try {
    let r = await show(env, { active: false, ot_link_lost: true, room_temp_lost: true, control_mode: 1 }, 'Paused: no boiler connection');   // TASK-3012: the OT hint is not repeated in PAUSED_OT, so the badge needs another message (room lost)
    assert.equal(r.modOld, null, 'the Modulation row is renamed');
    assert.equal(r.mod, '—'); assert.equal(r.pidOut, '—'); assert.equal(r.err, '—'); assert.equal(r.pTerm, '—');
    assert.equal(r.cycle, '—'); assert.equal(r.cycleSub, 'SAT is not actively controlling.');
    // AC6: the messages badge shows visible text and keeps its aria-label.
    assert.equal(r.msgHidden, false); assert.match(r.msgTxt, /^\d+ messages?$/); assert.equal(r.msgAria, r.msgTxt, 'TASK-3012: the accessible name is the visible text (WCAG 2.5.3)');

    for (const s of [{ active: false, safety_tripped: true, control_mode: 2 }, { active: false, outside_temp_missing: true, control_mode: 2 }, { active: false, control_mode: 1 }]) {
      env.ref.sat = Object.assign({}, BASE, s);
      const ok = await env.h.until(`document.getElementById('satCycleMode').textContent === '—' && document.getElementById('satStatusBig').textContent.indexOf('Idle') < 0 && document.getElementById('satStatusBig').textContent !== 'Paused: no boiler connection'`, 12000);
      assert.ok(ok, 'burner cycle shows a dash for ' + JSON.stringify(s));
    }

    r = await show(env, { active: true, control_mode: 1, boiler_status: 'heating' }, 'Heating to 20.0°');
    assert.equal(r.mod, '100 %'); assert.equal(r.pidOut, '42.5 °C'); assert.equal(r.cycle, 'Continuous');
    assert.equal(r.cycleSub, 'PID drives the burner continuously. SAT chooses the mode itself.');
  } finally { await env.h.close(); }
});

test('WAITING with the boiler link down shows the no-boiler variant with a Check connection link', async () => {
  const env = await openSat(HEALTH_BOILER_DOWN);
  try {
    await env.h.sleep(1500);   // the first /v2/health poll runs 400 ms after load
    const r = await show(env, { active: false }, 'Waiting: no boiler connection');
    assert.equal(r.pill, 'Waiting'); assert.match(r.pillCls, /\bwarn\b/);
    assert.equal(r.subLink, 'Check connection ›'); assert.match(r.sub, /^SAT waits for a reply from the boiler/);
  } finally { await env.h.close(); }
});

test('Set up a source opens the SAT page, Control view, and focuses Latitude (SAT settings live on the SAT page)', async () => {
  const ref = { sat: Object.assign({}, BASE, { active: false, outside_temp_missing: true }) };
  const S = (value, type) => ({ value, type });
  const SETTINGS = { satweatherenable: S('true', 'b'), satweatherlat: S('52.1', 'f'), satweatherlon: S('5.1', 'f'), satweatherinterval: S('900', 'i') };
  const h = await startPage({
    api: (m, p) => (p.endsWith('/v2/sat/status') ? ref.sat : p.endsWith('/v2/health') ? HEALTH_OK : (p.endsWith('/v2/settings') && m === 'GET') ? SETTINGS : undefined),
    beforeScript: "try { localStorage.setItem('otgw-v2-satview', 'simple'); } catch (e) {}",
  });
  try {
    await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
    const shown = await h.until(`(function(){ var b = document.querySelector('#satStateSub button'); return !!b && b.textContent === 'Set up a source'; })()`, 12000);
    assert.ok(shown, 'the link shows in PAUSED_OUTSIDE');
    await h.until(`!!document.querySelector('#satSetL2 .srow[data-key="satweatherlat"]')`, 8000);
    await h.evalJs(`document.querySelector('#satStateSub button').click()`);
    const r = await h.evalJs(`({ view: document.getElementById('satBody').className, page: document.getElementById('page-sat').classList.contains('active'),
      focusKey: (document.activeElement && document.activeElement.closest('.srow')) ? document.activeElement.closest('.srow').dataset.key : (document.activeElement || {}).tagName })`);
    assert.equal(r.page, true); assert.equal(r.view, 'sv-control'); assert.equal(r.focusKey, 'satweatherlat');
  } finally { await h.close(); }
});

test('Check connection opens Monitor on the Connection tab even when Monitor was last on Log', async () => {
  const ref = { sat: Object.assign({}, BASE, { active: false }) };
  const h = await startPage({
    api: (m, p) => (p.endsWith('/v2/sat/status') ? ref.sat : p.endsWith('/v2/health') ? HEALTH_BOILER_DOWN : undefined),
  });
  try {
    await h.evalJs(`document.querySelector('.seg[data-page="monitor"]').click()`);
    await h.evalJs(`document.querySelector('#page-monitor .subtab[data-tab="mlog"]').click()`);
    await h.sleep(1500);
    await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
    const shown = await h.until(`(function(){ var b = document.querySelector('#satStateSub button'); return !!b && b.textContent === 'Check connection ›'; })()`, 15000);
    assert.ok(shown, 'the Check connection link shows in WAITING with the boiler link down');
    await h.evalJs(`document.querySelector('#satStateSub button').click()`);
    const r = await h.evalJs(`({ page: document.getElementById('page-monitor').classList.contains('active'),
      tab: (document.querySelector('#page-monitor .subtab.active') || {}).dataset.tab,
      mapVisible: !!document.getElementById('connMap').getBoundingClientRect().width })`);
    assert.equal(r.page, true); assert.equal(r.tab, 'mconn'); assert.equal(r.mapVisible, true);
  } finally { await h.close(); }
});
