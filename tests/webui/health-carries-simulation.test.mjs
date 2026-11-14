// TASK-1915 (Robert, #alpha-testing 1570947181945299066: "Yes add the simulation into there health api"):
// the SIMULATION header badge follows simulation_active in the /api/v2/health answer, and the poll chain
// behind a health poll no longer asks /api/v2/simulate (the web UI asked it after every health poll, about
// every 15 s per tab; gruijter's capture had 7,668 such calls in 18.5 h).
//   1. No /api/v2/simulate request goes out while health polls run; the badge shows from health alone.
//   2. The badge follows the health answer: shown on true, hidden on false and when the field is missing
//      (a firmware from before TASK-1915).
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. A health poll is forced by
// opening Monitor > Connection (v2.js: the mconn tab calls fetchConn()).
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/health-carries-simulation.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const base = { otgwconnected: true, mqttconnected: true, boiler_age_s: 3, wifirssi: -60, otcommandinterface: 'PIC' };
const BADGE_HIDDEN = `document.getElementById('hdrSim').classList.contains('hidden')`;
const healthPolls = (h) => h.log.filter((l) => l.includes('/v2/health')).length;
const simCalls = (h) => h.log.filter((l) => l.includes('/v2/simulate')).length;

// open Monitor > Connection again (via another sub-tab), which runs one health poll
async function forceHealthPoll(h) {
  const n0 = healthPolls(h);
  await h.evalJs(`(function(){ var s = document.querySelector('.seg[data-page="monitor"]'); if (s) s.click();
    var o = document.querySelector('.subtab[data-tab="mlog"]'); if (o) o.click();
    var c = document.querySelector('.subtab[data-tab="mconn"]'); if (c) c.click(); })()`);
  for (let i = 0; i < 50 && healthPolls(h) <= n0; i++) await h.sleep(100);
  await h.sleep(600);   // the answer is applied and its chain (overrides) has run
  return healthPolls(h) > n0;
}

test('the badge comes from /health and no /simulate request goes out', { skip: !CHROME && 'no Chrome' }, async () => {
  const h = await startPage({ api: (m, p) => {
    if (p.endsWith('/v2/health')) return { health: { ...base, simulation_active: true, simulation_source: 'builtin' } };
    if (p.endsWith('/v2/simulate')) return { simulation: { active: true } };
    return undefined;
  } });
  try {
    assert.ok(await h.until(`!${BADGE_HIDDEN}`, 8000), 'the badge shows from the health answer');
    assert.ok(await forceHealthPoll(h), 'a second health poll ran');
    assert.ok(await forceHealthPoll(h), 'a third health poll ran');
    assert.equal(simCalls(h), 0, 'the poll chain asked /api/v2/simulate: ' +
      JSON.stringify(h.log.filter((l) => l.includes('/v2/simulate'))));
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('the badge follows the health answer: true, false, missing', { skip: !CHROME && 'no Chrome' }, async () => {
  let sim = true;   // true, false, or undefined (field missing)
  const h = await startPage({ api: (m, p) => {
    if (p.endsWith('/v2/health')) {
      const health = { ...base };
      if (sim !== undefined) health.simulation_active = sim;
      return { health };
    }
    return undefined;
  } });
  try {
    assert.ok(await h.until(`!${BADGE_HIDDEN}`, 8000), 'shown while simulation_active is true');
    sim = false;
    assert.ok(await forceHealthPoll(h));
    assert.equal(await h.evalJs(BADGE_HIDDEN), true, 'hidden once simulation_active is false');
    sim = true;
    assert.ok(await forceHealthPoll(h));
    assert.equal(await h.evalJs(BADGE_HIDDEN), false, 'shown again on true');
    sim = undefined;
    assert.ok(await forceHealthPoll(h));
    assert.equal(await h.evalJs(BADGE_HIDDEN), true, 'hidden when the field is missing (older firmware)');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});
