// TASK-2956: contrast regression test (spec T1b, docs/reviews/2026-11-20_ux-heuristic-v2-webui/).
// Layer A resolves every token pair of spec T1 section 2 through the REAL v2.css/ds-tokens.css in
// headless Chrome, in light and dark, and checks the WCAG 2.x ratio. Layer B scans every visible text
// on Home, SAT (three views) and Settings in both themes, with fixtures that show the SAT pill in
// heating / idle / off, the SAT source tag, the beta warn banner and the reboot badge.
// Colours are normalised through a 1x1 canvas, so color(srgb ...) and color-mix() results read right;
// backgrounds are composited bottom-up over the first opaque ancestor.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';

// [row, theme, fg, bg, min] -- the rows of spec T1 section 2. When that table changes, change this one.
const ROWS = [
  ['A1', 'light', 'var(--accent)', 'var(--panel)', 4.5],
  ['A2', 'light', 'var(--accent)', 'var(--bg)', 4.5],
  ['A3', 'light', 'var(--accent)', 'var(--panel-2)', 4.5],
  ['A4', 'dark', 'var(--accent)', 'var(--panel)', 4.5],
  ['A5', 'dark', 'var(--accent)', 'var(--bg)', 4.5],
  ['A6', 'dark', 'var(--accent)', 'var(--bg-surface-alt)', 4.5],
  ['B1', 'light', 'var(--nav-accent)', 'var(--nav)', 4.5],
  ['B2', 'dark', 'var(--nav-accent)', 'var(--nav)', 4.5],
  ['C1', 'light', 'var(--on-accent)', 'var(--accent)', 4.5],
  ['C2', 'dark', 'var(--on-accent)', 'var(--accent)', 4.5],
  ['D1', 'light', 'var(--status-warn-text)', 'color-mix(in srgb, var(--zone-warn) 16%, var(--bg))', 4.5],
  ['D2', 'light', 'var(--status-warn-text)', 'var(--panel)', 4.5],
  ['D2b', 'light', 'var(--status-warn-text)', 'color-mix(in srgb, var(--zone-warn) 20%, var(--bg))', 4.5],
  ['D3', 'dark', 'var(--status-warn-text)', 'color-mix(in srgb, var(--zone-warn) 16%, var(--bg))', 4.5],
  ['D4', 'dark', 'var(--status-warn-text)', 'color-mix(in srgb, var(--zone-warn) 20%, var(--panel))', 4.5],
  ['E1', 'light', 'var(--on-hot)', 'var(--hot-fill)', 4.5],
  ['E2', 'dark', 'var(--on-hot)', 'var(--hot-fill)', 4.5],
  ['E3', 'light', '#ffffff', 'var(--pill-off)', 4.5],
  ['E4', 'dark', '#ffffff', 'var(--pill-off)', 4.5],
  ['F1', 'light', 'var(--hot-text)', 'color-mix(in srgb, var(--hot) 16%, var(--panel))', 4.5],
  ['F2', 'dark', 'var(--hot-text)', 'color-mix(in srgb, var(--hot) 16%, var(--panel))', 4.5],
];

// In-page helpers: canvas colour normalisation, compositing, WCAG ratio.
const LIB = `
  function rgba(css) {
    var c = document.createElement('canvas'); c.width = c.height = 1;
    var x = c.getContext('2d', { willReadFrequently: true });
    x.clearRect(0, 0, 1, 1); x.fillStyle = 'rgba(0,0,0,0)'; x.fillStyle = css; x.fillRect(0, 0, 1, 1);
    var d = x.getImageData(0, 0, 1, 1).data; return [d[0], d[1], d[2], d[3] / 255];
  }
  function over(top, base) { var a = top[3]; return [0, 1, 2].map(function (i) { return top[i] * a + base[i] * (1 - a); }).concat([1]); }
  function lum(c) { var v = c.slice(0, 3).map(function (u) { u /= 255; return u <= 0.04045 ? u / 12.92 : Math.pow((u + 0.055) / 1.055, 2.4); });
    return 0.2126 * v[0] + 0.7152 * v[1] + 0.0722 * v[2]; }
  function ratio(f, b) { var l1 = lum(f), l2 = lum(b); return (Math.max(l1, l2) + 0.05) / (Math.min(l1, l2) + 0.05); }
  function hex(c) { return '#' + c.slice(0, 3).map(function (u) { return ('0' + Math.round(u).toString(16)).slice(-2); }).join(''); }
`;

function layerA(theme, rows) {
  return `(function(){ ${LIB}
    var rows = ${JSON.stringify(rows)}, out = [];
    document.documentElement.setAttribute('data-theme', ${JSON.stringify(theme)});
    var root = getComputedStyle(document.documentElement);
    rows.forEach(function (r) {
      var missing = ((r[2] + ' ' + r[3]).match(/--[a-z0-9-]+/g) || []).filter(function (n) { return !root.getPropertyValue(n).trim(); });
      if (missing.length) { out.push({ row: r[0], theme: r[1], missing: missing }); return; }
      var p = document.createElement('div'); p.style.color = r[2]; p.style.backgroundColor = r[3];
      document.body.appendChild(p); var cs = getComputedStyle(p);
      var bg = over(rgba(cs.backgroundColor), rgba(root.getPropertyValue('--bg').trim()));
      var fg = over(rgba(cs.color), bg); p.remove();
      out.push({ row: r[0], theme: r[1], fg: hex(fg), bg: hex(bg), ratio: Math.round(ratio(fg, bg) * 100) / 100, min: r[4] });
    });
    return out; })()`;
}

test('layer A: every token pair of spec T1 meets its ratio in light and dark', { skip }, async () => {
  const h = await startPage({ api: () => undefined });
  const res = [];
  try {
    await h.until(`!!document.querySelector('.seg')`);
    for (const theme of ['light', 'dark']) res.push(...await h.evalJs(layerA(theme, ROWS.filter((r) => r[1] === theme))));
  } finally { await h.close(); }
  const bad = [];
  for (const r of res) {
    if (r.missing) { bad.push(`${r.row} ${r.theme}: token ${r.missing.join(', ')} not defined`); console.log(`  ${r.row} ${r.theme}: MISSING ${r.missing.join(', ')}`); continue; }
    const ok = r.ratio >= r.min;
    console.log(`  ${r.row} ${r.theme}: ${r.fg} on ${r.bg} = ${r.ratio} (min ${r.min}) ${ok ? 'ok' : 'FAIL'}`);
    if (!ok) bad.push(`${r.row} ${r.theme}: ${r.fg} on ${r.bg} = ${r.ratio} < ${r.min}`);
  }
  assert.equal(res.length, ROWS.length, 'every row measured');
  assert.deepEqual(bad, [], 'token pairs below their minimum:\n' + bad.join('\n'));
});

// ---- Layer B: rendered scan ----
const S = (value, maxlen = 15) => ({ value, type: 's', maxlen });
const SETTINGS = { wifistaticip: S(''), wifisubnet: S(''), wifigateway: S(''), wifidns1: S(''), wifidns2: S(''), hostname: S('otgw', 32) };
const SAT = {
  heating: { enabled: true, active: true, boiler_status: 'heating', target_temp: 20, room_temp: 19.5, final_setpoint: 45 },
  idle: { enabled: true, active: true, boiler_status: 'idle', target_temp: 20, room_temp: 20.5, final_setpoint: 10 },
  off: { enabled: false },
};

const SCAN = (view) => `(function(){ ${LIB}
  var bgBase = rgba(getComputedStyle(document.documentElement).getPropertyValue('--bg').trim());
  var fails = [], gaps = [], seen = {}, n = 0;
  function visible(el) { var r = el.getBoundingClientRect(); if (r.width <= 1 || r.height <= 1) return false;
    for (var e = el; e && e.nodeType === 1; e = e.parentElement) { var s = getComputedStyle(e);
      if (s.display === 'none' || s.visibility === 'hidden') return false; } return true; }
  var all = document.querySelectorAll('body *');
  for (var i = 0; i < all.length; i++) {
    var el = all[i]; if (el.closest('svg, canvas, script, style, [aria-hidden="true"] svg')) continue;
    var txt = ''; for (var k = 0; k < el.childNodes.length; k++) if (el.childNodes[k].nodeType === 3) txt += el.childNodes[k].nodeValue;
    txt = txt.trim(); if (!txt || !visible(el)) continue;
    if (el.closest(':disabled, [aria-disabled="true"]')) continue;
    var cs = getComputedStyle(el), layers = [], op = 1, img = false, base = null;
    for (var e = el; e && e.nodeType === 1; e = e.parentElement) { var s = getComputedStyle(e);
      op *= parseFloat(s.opacity); if (s.backgroundImage !== 'none') img = true;
      var c = rgba(s.backgroundColor); if (c[3] >= 0.999) { base = c; break; } if (c[3] > 0) layers.push(c); }
    var sel = el.tagName.toLowerCase() + (el.id ? '#' + el.id : '') + (el.className && typeof el.className === 'string' ? '.' + el.className.trim().split(/\\s+/).join('.') : '');
    if (img || op < 0.999) { gaps.push(sel + ' "' + txt.slice(0, 30) + '"' + (img ? ' (image)' : ' (opacity ' + op.toFixed(2) + ')')); continue; }
    var bg = base || bgBase; for (var j = layers.length - 1; j >= 0; j--) bg = over(layers[j], bg);
    var fg = over(rgba(cs.color), bg), px = parseFloat(cs.fontSize), w = parseInt(cs.fontWeight, 10) || 400;
    var min = (px >= 24 || (px >= 18.66 && w >= 700)) ? 3 : 4.5, rt = ratio(fg, bg); n++;
    var key = txt.slice(0, 30) + '|' + hex(fg) + '|' + hex(bg); if (seen[key]) continue; seen[key] = 1;
    if (rt < min) fails.push(${JSON.stringify(view)} + ' ' + document.documentElement.getAttribute('data-theme') + ' ' + sel + ' "' + txt.slice(0, 30) + '" ' + hex(fg) + ' on ' + hex(bg) + ' = ' + rt.toFixed(2) + ' < ' + min);
  }
  return { fails: fails, gaps: gaps, n: n }; })()`;

async function scanTheme(theme) {
  let sat = SAT.heating;
  const api = (m, p) => {
    if (p.endsWith('/v2/sat/status')) return sat;
    if (p.endsWith('/v2/device/info')) return { device: { fwversion: '2.0.0-beta.1' } };   // the firmware nests it under device
    if (p.endsWith('/v2/settings') && m === 'GET') return SETTINGS;
    return undefined;
  };
  const h = await startPage({ api, beforeScript: `try { localStorage.setItem('otgw-theme', '${theme}'); sessionStorage.removeItem('otgw-v2-satbeta-dismissed'); } catch (e) {}` });
  const out = { fails: [], gaps: [], n: 0 };
  const add = (r, what) => { assert.ok(r, `${theme} ${what}: scan returned nothing`); out.fails.push(...r.fails); out.gaps.push(...r.gaps.map((g) => `${what} ${theme} ${g}`)); out.n += r.n; };
  const page = async (p) => {
    await h.evalJs(`document.querySelector('.seg[data-page="${p}"]').click()`);
    assert.ok(await h.until(`(function(){ var e = document.getElementById('page-${p}'); return !!e && getComputedStyle(e).display !== 'none'; })()`), `page ${p} shown`);
  };
  try {
    assert.ok(await h.until(`document.documentElement.getAttribute('data-theme') === '${theme}'`), `theme ${theme} applied`);
    await page('home');
    add(await h.evalJs(SCAN('home')), 'home');
    await page('sat');
    for (const st of ['heating', 'idle', 'off']) {
      sat = SAT[st];
      const cls = st === 'heating' ? 'sat-pill' : 'sat-pill ' + st;
      assert.ok(await h.until(`document.getElementById('satPill').className === '${cls}'`, 12000), `SAT pill reaches ${st}`);
      for (const v of ['simple', 'control', 'technical']) {
        await h.evalJs(`document.querySelector('.satview-btn[data-sv="${v}"]').click()`);
        await h.until(`document.querySelector('.satview-btn[data-sv="${v}"]').classList.contains('active')`);
        add(await h.evalJs(SCAN(`sat-${v}-${st}`)), `sat-${v}-${st}`);
      }
    }
    await page('settings');
    assert.ok(await h.until(`document.querySelectorAll('#page-settings .rail-item').length > 0`), 'settings rail rendered');
    add(await h.evalJs(SCAN('settings')), 'settings');
  } finally { await h.close(); }
  return out;
}

for (const theme of ['light', 'dark']) {
  test(`layer B: every visible text on Home, SAT and Settings meets WCAG 1.4.3 (${theme})`, { skip, timeout: 170000 }, async () => {
    const r = await scanTheme(theme);
    console.log(`  ${theme}: ${r.n} text elements measured, ${r.fails.length} below the minimum, ${r.gaps.length} not measured`);
    for (const g of [...new Set(r.gaps)]) console.log('  not measured: ' + g);
    for (const f of r.fails) console.log('  FAIL ' + f);
    assert.ok(r.n > 50, `scan measured ${r.n} elements`);
    assert.deepEqual(r.fails, [], 'texts below their contrast minimum:\n' + r.fails.join('\n'));
  });
}
