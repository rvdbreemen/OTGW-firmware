// TASK-2959 F11 (spec T4): on a plain-HTTP page the weather card hides "Detect location" and its
// hint no longer says "HTTPS"; the words "SAT settings › Weather" open that group on the SAT page
// (TASK-1211) with Latitude focused. The harness serves 127.0.0.1, which browsers treat as secure,
// so the page is told it is not (window.isSecureContext = false), as on the gateway's own address.
// OTGW_DATA_DIR=<dir> runs it against another tree (old-vs-fix).
//
// Run: node --test tests/webui/sat-weather-hint-link.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage } from './lib/page-harness.mjs';

const S = (value, type) => ({ value, type });
const SETTINGS = { satweatherenable: S('true', 'b'), satweatherlat: S('0', 'f'), satweatherlon: S('0', 'f'), satweatherinterval: S('900', 'i') };
const SAT = { enabled: true, active: true, boiler_status: 'idle', target_temp: 20, room_temp: 19.5, sun_elevation_source: 'calc' };

test('HTTP page: no Detect location, no HTTPS in the hint, the link focuses Latitude on the SAT page', async () => {
  const h = await startPage({
    api: (m, p) => (p.endsWith('/v2/sat/status') ? SAT : (p.endsWith('/v2/settings') && m === 'GET') ? SETTINGS
      : p.endsWith('/v2/sat/weather') ? { enabled: true, latitude: 0, longitude: 0 } : undefined),
    beforeScript: "try { Object.defineProperty(window, 'isSecureContext', { value: false }); localStorage.setItem('otgw-v2-satview', 'control'); } catch (e) {}",
  });
  try {
    await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
    const shown = await h.until(`(function(){ var e = document.getElementById('satLocHint'); return !!e && !e.hidden && e.textContent.length > 0; })()`, 12000);
    const r = await h.evalJs(`({ hint: document.getElementById('satLocHint').textContent,
      link: (document.querySelector('#satLocHint button') || {}).textContent || '', dl: document.getElementById('satDetectLoc').hidden })`);
    assert.ok(shown, 'the location hint shows');
    assert.equal(r.dl, true, 'Detect location is hidden on HTTP');
    assert.doesNotMatch(r.hint, /HTTPS/);
    assert.equal(r.link, 'SAT settings › Weather');
    await h.until(`!!document.querySelector('#satSetL2 .srow[data-key="satweatherlat"]')`, 8000);
    await h.evalJs(`document.querySelector('#satLocHint button').click()`);
    const f = await h.evalJs(`(function(){ var a = document.activeElement, row = a && a.closest('.srow'); return row ? row.dataset.key : (a || {}).tagName; })()`);
    assert.equal(f, 'satweatherlat');
  } finally { await h.close(); }
});
