// TASK-1747: after TASK-1741 two visible tabs still got 1.9 REST 503s per minute, 26 of 38 on
// otdirect/overrides: one health poll fired simulate and overrides at the same time, so one tab
// filled the device's 2-request REST cap (ADR-165) on its own.
//   1. The chain behind a health poll (overrides, then otdirect/status on OT-Direct, then
//      simulate) keeps at most one request in flight.
//   2. A busy 503 on the overrides list leaves the shown override badge as it is (it used to
//      clear the list, so the '⛓ n injected' badge vanished on a busy answer).
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. A beforeScript holds
// each chain response for 300 ms and counts how many chain requests are open at once.
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/health-chain-one-in-flight.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const HEALTH = { health: { otgwconnected: true, mqttconnected: true, boiler_age_s: 3, wifirssi: -60,
  otcommandinterface: 'OT-Direct' } };
const OVR = { overrides: { write: [{ msgid: 1, value: 11520 }], response: [], modify: [], unknown: [] } };
const BUSY = { __status: 503, error: 'Server busy: too many concurrent requests, please retry' };

const CHAIN = "/v2\\/(simulate|otdirect\\/overrides|otdirect\\/status|otgw\\/overrides)(\\?|$)/";
const holdChain = `(function () {
  var f0 = window.fetch, rx = ${CHAIN};
  window.__chainOpen = 0; window.__chainMax = 0; window.__chainSeen = [];
  window.fetch = function (u) {
    var url = String(u && u.url || u), p = f0.apply(this, arguments);
    if (!rx.test(url)) return p;
    window.__chainOpen++; window.__chainSeen.push(url.replace(/^.*\\/api\\//, ''));
    if (window.__chainOpen > window.__chainMax) window.__chainMax = window.__chainOpen;
    return p.then(function (r) { return new Promise(function (res) { setTimeout(function () { window.__chainOpen--; res(r); }, 300); }); },
                  function (e) { window.__chainOpen--; throw e; });
  };
})();`;

test('the health chain keeps at most one request in flight', { skip: !CHROME && 'no Chrome' }, async () => {
  const h = await startPage({ beforeScript: holdChain, api: (m, p) => {
    if (p.endsWith('/v2/health')) return HEALTH;
    if (p.endsWith('/v2/otdirect/overrides')) return OVR;
    if (p.endsWith('/v2/simulate')) return { simulation: { active: false } };
    return undefined;
  } });
  try {
    // The startup health poll runs at 0.4 s; its whole chain is done well within 3 s.
    const seen = await h.until(`(window.__chainSeen || []).length >= 3 && window.__chainOpen === 0 && window.__chainSeen.join(' ')`, 15000);
    assert.ok(seen, 'the chain ran: ' + await h.evalJs('JSON.stringify(window.__chainSeen)'));
    assert.match(seen, /simulate/); assert.match(seen, /otdirect\/overrides/); assert.match(seen, /otdirect\/status/);
    const max = await h.evalJs('window.__chainMax');
    assert.equal(max, 1, max + ' chain requests in flight at once (' + seen + ')');
  } finally { await h.close(); }
});

test('a busy 503 on the overrides list keeps the injected badge', { skip: !CHROME && 'no Chrome' }, async () => {
  let busy = false;
  const h = await startPage({ api: (m, p) => {
    if (p.endsWith('/v2/health')) return HEALTH;
    if (p.endsWith('/v2/otdirect/overrides')) return busy ? BUSY : OVR;
    return undefined;
  } });
  try {
    const shown = await h.until(`(function(){ var g = document.getElementById('cnOvr'), t = document.getElementById('cnOvrTxt');
      return g && g.style.display !== 'none' && t && /1 injected/.test(t.textContent); })()`, 15000);
    assert.ok(shown, 'the badge shows 1 injected override');
    busy = true;
    const n0 = h.log.filter((l) => l.includes('/v2/otdirect/overrides')).length;
    // Monitor > Connection asks health (and its chain) and the overrides list at once.
    await h.evalJs(`(function(){ var s = document.querySelector('.seg[data-page="monitor"]'); if (s) s.click();
      var c = document.querySelector('.subtab[data-tab="mconn"]'); if (c) c.click(); })()`);
    const asked = await h.until(`true`, 100) && await (async () => {
      for (let i = 0; i < 50; i++) { if (h.log.filter((l) => l.includes('/v2/otdirect/overrides')).length > n0) return true; await h.sleep(100); }
      return false; })();
    assert.ok(asked, 'the overrides list was asked again and answered 503');
    await h.sleep(800);
    const still = await h.evalJs(`(function(){ var g = document.getElementById('cnOvr'), t = document.getElementById('cnOvrTxt');
      return !!(g && g.style.display !== 'none' && t && /1 injected/.test(t.textContent)); })()`);
    assert.ok(still, 'after a busy 503 the badge still shows 1 injected override');
  } finally { await h.close(); }
});
