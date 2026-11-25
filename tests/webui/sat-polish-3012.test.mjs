// TASK-3012 (follow-up of TASK-2957/2959, ux critique of beta.413): the SAT page repeated the same news three times, drew a
// 100 % modulation band while the loop was not running, put the chart note over the y-axis labels and asked to toggle a switch
// that is off by design. Each case feeds /api/v2/sat/status to the REAL page. OTGW_DATA_DIR=<dir> runs it against an older
// tree (old-vs-fix); OTGW_SHOT_DIR=<dir> saves screenshots of the paused chart in both themes (synthetic data).
//
// Run: node --test tests/webui/sat-polish-3012.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { startPage, CHROME } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';
const BASE = { enabled: true, active: true, boiler_status: 'heating', target_temp: 20, room_temp: 19.5, outside_temp: 8,
  final_setpoint: 45, flow_temp: 41, heating_curve: 44, current_modulation: 40, pid_output: 42.5, coefficient: 1.8, heating_system: 1 };
const MOD = '#00bffe';   // the modulation series colour in satDrawHistory()

async function openSat(ref, theme) {
  const h = await startPage({ api: (m, p) => (p.endsWith('/v2/sat/status') ? ref.sat : undefined),
    beforeScript: `try { localStorage.setItem('otgw-v2-satview', 'control'); localStorage.setItem('otgw-theme', '${theme || 'light'}'); } catch (e) {}` });
  await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
  await h.until(`getComputedStyle(document.getElementById('page-sat')).display !== 'none'`);
  return h;
}
const hidden = (id) => `(function () { var e = document.getElementById('${id}'); return !e || e.hidden; })()`;
const modPaths = `document.querySelectorAll('#satHistChart path[fill="${MOD}"]').length`;

test('PAUSED_OT and PAUSED_OUTSIDE do not repeat the headline in the messages panel; STOPPED still shows the hint', { skip }, async () => {
  const ref = { sat: Object.assign({}, BASE, { active: false, ot_link_lost: true }) };
  const h = await openSat(ref);
  try {
    assert.ok(await h.until(`document.getElementById('satStatusBig').textContent.trim() === 'Paused: no boiler connection'`, 15000), 'paused_ot headline');
    assert.ok(await h.evalJs(hidden('satOtLost')), 'PAUSED_OT: the OpenTherm hint is not shown');
    ref.sat = Object.assign({}, BASE, { active: false, outside_temp_missing: true });
    assert.ok(await h.until(`/outside/i.test(document.getElementById('satStatusBig').textContent)`, 15000), 'paused_outside headline');
    assert.ok(await h.evalJs(hidden('satOutsideMissing')), 'PAUSED_OUTSIDE: the outside-temperature hint is not shown');
    ref.sat = Object.assign({}, BASE, { active: false, ot_link_lost: true, safety_tripped: true });
    assert.ok(await h.until(`document.querySelector('#satPill') && /stopped/i.test(document.getElementById('satPill').textContent)`, 15000), 'stopped state');
    assert.equal(await h.evalJs(hidden('satOtLost')), false, 'STOPPED does not cover the OpenTherm loss, so the hint shows');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('no modulation or PID is drawn while SAT is inactive; they are drawn while it runs', { skip }, async () => {
  const ref = { sat: Object.assign({}, BASE, { active: false, current_modulation: 100, pid_output: 42.5 }) };
  const h = await openSat(ref);
  try {
    assert.ok(await h.until(`document.getElementById('satHistChart') && document.getElementById('satHistChart').children.length > 3`, 15000), 'chart drawn');
    await h.sleep(7000);   // a few polls with active=false and 100 % modulation
    assert.equal(await h.evalJs(modPaths), 0, 'inactive: no modulation band');
    ref.sat = Object.assign({}, BASE);
    assert.ok(await h.until(`${modPaths} > 0`, 20000), 'active: the modulation series is drawn');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

for (const theme of ['light', 'dark']) {
  test(`the paused chart note does not overlap the axis labels (${theme})`, { skip }, async () => {
    const ref = { sat: Object.assign({}, BASE) };
    const h = await openSat(ref, theme);
    try {
      assert.ok(await h.until(`document.getElementById('satHistChart').children.length > 3`, 15000));
      await h.sleep(7000);   // two or more points, so the note takes the corner position
      ref.sat = Object.assign({}, BASE, { active: false, ot_link_lost: true });
      assert.ok(await h.until(`(function () { var e = document.getElementById('satHistEmpty'); return e && !e.hidden && e.classList.contains('corner'); })()`, 20000), 'corner note shown');
      const r = await h.evalJs(`(function () {
        var n = document.getElementById('satHistEmpty').getBoundingClientRect(), hit = [];
        Array.prototype.forEach.call(document.querySelectorAll('#satHistChart text'), function (t) {
          var b = t.getBoundingClientRect();
          if (b.width && b.left < n.right && b.right > n.left && b.top < n.bottom && b.bottom > n.top) hit.push(t.textContent);
        });
        return { hit: hit, note: document.getElementById('satHistEmpty').textContent };
      })()`);
      assert.deepEqual(r.hit, [], `the note "${r.note}" covers chart text`);
      if (process.env.OTGW_SHOT_DIR) await h.shot(path.join(process.env.OTGW_SHOT_DIR, `paused-chart-${theme}.png`));
      assert.deepEqual(h.exceptions, []);
    } finally { await h.close(); }
  });
}

test('STOPPED during a takeover does not ask to switch SAT off and on', { skip }, async () => {
  const ref = { sat: Object.assign({}, BASE, { enabled: false, fallback_active: true, active: false, safety_tripped: true }) };
  const h = await openSat(ref);
  try {
    assert.ok(await h.until(`(document.getElementById('satStateSub') || {}).hidden === false`, 15000), 'sub-line shown');
    const take = await h.evalJs(`document.getElementById('satStateSub').textContent`);
    assert.doesNotMatch(take, /switch SAT off and on/i, `takeover wording: ${take}`);
    assert.match(take, /Check the connection/);
    ref.sat = Object.assign({}, BASE, { active: false, safety_tripped: true });
    assert.ok(await h.until(`/switch SAT off and on/i.test(document.getElementById('satStateSub').textContent)`, 15000), 'a plain STOPPED keeps the restart instruction');
  } finally { await h.close(); }
});
