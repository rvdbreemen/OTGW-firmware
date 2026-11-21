// TASK-2959 F13 (spec T4): the SAT control-loop chart says why it is empty or no longer moving, and
// the heating curve card shows one line at about 120 px when SAT does not know its coefficient or
// target. Before the fix the chart showed empty axes with the label "live", and the curve card was a
// blank 400 px block. OTGW_DATA_DIR=<dir> runs it against another tree (old-vs-fix).
//
// Run: node --test tests/webui/sat-chart-empty-states.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage } from './lib/page-harness.mjs';

const BASE = { enabled: true, active: true, boiler_status: 'heating', target_temp: 20, room_temp: 19.5, outside_temp: 8,
  final_setpoint: 45, coefficient: 1.8, heating_system: 1 };
const READ = `(function () {
  var e = document.getElementById('satHistEmpty'), l = document.getElementById('satLoopState');
  var c = document.getElementById('satCurveEmpty'), wrap = document.querySelector('.sat-curve-hero .curve-wrap');
  return { note: e && !e.hidden ? e.textContent : '', corner: !!(e && e.classList.contains('corner')),
    label: l && !l.hidden ? l.textContent : '', curve: c && !c.hidden ? c.textContent : '',
    curveH: wrap ? Math.round(wrap.getBoundingClientRect().height) : null };
})()`;

test('control-loop chart notes per state, and the heating curve empty state', async () => {
  const ref = { sat: Object.assign({}, BASE, { enabled: false, active: false }) };
  const h = await startPage({ api: (m, p) => (p.endsWith('/v2/sat/status') ? ref.sat : undefined),
    beforeScript: "try { localStorage.setItem('otgw-v2-satview', 'control'); } catch (e) {}" });
  const wait = (cond) => h.until(`(function(){ var r = ${READ}; return (${cond}); })()`, 15000);
  try {
    await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
    assert.ok(await wait(`r.note === 'No control data: SAT is off.'`), 'off note: ' + JSON.stringify(await h.evalJs(READ)));
    assert.equal((await h.evalJs(READ)).label, 'off');

    ref.sat = Object.assign({}, BASE);
    // The page keeps the points of this visit; with fewer than 2 points it is collecting, then live.
    assert.ok(await wait(`r.note === '' && r.label === 'live'`), 'live with 2+ points: ' + JSON.stringify(await h.evalJs(READ)));

    ref.sat = Object.assign({}, BASE, { active: false, ot_link_lost: true });
    assert.ok(await wait(`r.note === 'No new control data: SAT is paused.'`), 'paused note: ' + JSON.stringify(await h.evalJs(READ)));
    assert.equal((await h.evalJs(READ)).corner, true, 'points drawn before the pause stay: the note sits in the corner');

    ref.sat = Object.assign({}, BASE, { active: false });
    assert.ok(await wait(`r.note === 'No control data yet: SAT is waiting to take over.'`), 'waiting note');

    ref.sat = Object.assign({}, BASE, { coefficient: null });
    assert.ok(await wait(`r.curve === 'The heating curve appears once SAT knows its coefficient and target.'`), 'curve empty line: ' + JSON.stringify(await h.evalJs(READ)));
    const r = await h.evalJs(READ);
    assert.ok(r.curveH <= 160, 'the empty curve card stays low, height ' + r.curveH);
  } finally { await h.close(); }
});

test('first poll with SAT on: collecting, label hidden', async () => {
  const h = await startPage({ api: (m, p) => (p.endsWith('/v2/sat/status') ? BASE : undefined),
    beforeScript: "try { localStorage.setItem('otgw-v2-satview', 'control'); } catch (e) {}" });
  try {
    await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
    const ok = await h.until(`(function(){ var r = ${READ}; return r.note === 'Collecting data…' && r.label === ''; })()`, 10000);
    assert.ok(ok, 'collecting on the first point: ' + JSON.stringify(await h.evalJs(READ)));
  } finally { await h.close(); }
});
