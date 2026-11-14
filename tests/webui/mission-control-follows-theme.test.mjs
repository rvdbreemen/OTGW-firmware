// TASK-1863 (Robert, choice A, DM 1571274595758248078): Home > Mission control follows the theme.
// The .mc block in v2.css carried its own fixed dark palette (--bg #0d1117, --panel #161b22, a #0a0e14
// ticker, #79f2c0 / #00bffe accents), so it stayed dark in the light theme. The test loads the REAL
// v2.html/v2.css in headless Chrome in the light and the dark theme and checks that the Mission control
// box, a cell and the frame ticker use the theme's own tokens. Red on the old v2.css, green on the fix.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const S = (value, maxlen = 15) => ({ value, type: 's', maxlen });
const SETTINGS = { settings: { hostname: S('otgw', 32), mqttbroker: S('broker.local', 30) } };
const api = (m, p) => (p.endsWith('/v2/settings') ? SETTINGS : undefined);
const skip = !CHROME && 'no Chrome';

// Resolve a token to a computed rgb() string through a probe element outside .mc.
const PROBE = `(function(){
  var mc = document.querySelector('.mc'), tk = document.getElementById('cTicker'), h = document.querySelector('.mc h2');
  if (!mc || !tk || !h) return null;
  function tok(name, prop) { var d = document.createElement('div'); d.style[prop] = 'var(' + name + ')';
    document.body.appendChild(d); var v = getComputedStyle(d)[prop]; d.remove(); return v; }
  var cell = document.createElement('div'); cell.className = 'mc-cell'; mc.appendChild(cell);
  var r = { theme: document.documentElement.getAttribute('data-theme'),
    mcBg: getComputedStyle(mc).backgroundColor, mcText: getComputedStyle(mc).color,
    cellBg: getComputedStyle(cell).backgroundColor, tickerBg: getComputedStyle(tk).backgroundColor,
    bg: tok('--bg', 'backgroundColor'), panel: tok('--panel', 'backgroundColor'), text: tok('--text', 'color') };
  cell.remove(); return r; })()`;

async function look(theme) {
  const h = await startPage({ api, beforeScript: `try { localStorage.setItem('otgw-theme', '${theme}'); } catch (e) {}` });
  try {
    await h.until(`(function(){ var r = ${PROBE}; return r && r.theme === '${theme}'; })()`);
    return await h.evalJs(PROBE);
  } finally {
    await h.close();
  }
}

for (const theme of ['light', 'dark']) {
  test(`${theme} theme: Mission control uses the theme tokens, not its own dark palette`, { skip }, async () => {
    const r = await look(theme);
    assert.equal(r.mcBg, r.bg, `.mc background ${r.mcBg}, theme --bg ${r.bg}`);
    assert.equal(r.mcText, r.text, `.mc text ${r.mcText}, theme --text ${r.text}`);
    assert.equal(r.cellBg, r.panel, `.mc-cell background ${r.cellBg}, theme --panel ${r.panel}`);
    assert.equal(r.tickerBg, r.panel, `ticker background ${r.tickerBg}, theme --panel ${r.panel}`);
  });
}
