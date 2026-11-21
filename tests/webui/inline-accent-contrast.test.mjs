// TASK-2998: contrast of inline styles, which the CSS-rule probes of contrast-tokens.test.mjs do not
// see. TASK-2956 darkened the light --accent so white text passes; six inline styles in v2.js kept
// color:#04222c on background:var(--accent) (wizard primary buttons and Done check marks, 2.84:1 in
// light), and the injected-values chip on the connection map drew its text with --zone-warn on a
// 16 % warn tint (1.91:1).
//   1. gate: every inline style in v2.js / v2.html that sets background:var(--accent) and a text
//      colour uses color:var(--on-accent);
//   2. measure: each of those colours on --accent, and the chip text on its own fill, in light and
//      dark, at least 4.5:1 (colours normalised through a 1x1 canvas, as in contrast-tokens).
// OTGW_DATA_DIR=<dir> runs it against another tree (old-vs-fix).
//
// Run: node --test tests/webui/inline-accent-contrast.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startPage } from './lib/page-harness.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');

// Inline style texts (style="..." in HTML, and the style strings v2.js builds) that set
// background:var(--accent) together with a text colour.
function accentStyles() {
  const out = [];
  for (const f of ['v2.js', 'v2.html']) {
    const lines = fs.readFileSync(path.join(DATA, f), 'utf8').split(/\r?\n/);
    lines.forEach((ln, i) => {
      const re = /background:\s*var\(--accent\)[^'"`]*/g; let m;
      while ((m = re.exec(ln))) {
        const seg = m[0];
        const col = /(?:^|;)\s*color:\s*([^;'"`]+)/.exec(seg);
        if (col) out.push({ where: `${f}:${i + 1}`, color: col[1].trim() });
      }
    });
  }
  return out;
}

test('gate: inline text on background:var(--accent) uses color:var(--on-accent)', () => {
  const all = accentStyles();
  assert.ok(all.length >= 6, `expected the wizard buttons among the inline accent styles, found ${all.length}`);
  const bad = all.filter((s) => s.color.replace(/\s+/g, '') !== 'var(--on-accent)');
  assert.deepEqual(bad, [], 'inline styles with a hard-coded text colour on --accent:\n' + bad.map((b) => `  ${b.where} color:${b.color}`).join('\n'));
});

const LIB = `
  function rgba(css) {
    var c = document.createElement('canvas'); c.width = c.height = 1;
    var x = c.getContext('2d', { willReadFrequently: true });
    x.clearRect(0, 0, 1, 1); x.fillStyle = 'rgba(0,0,0,0)'; x.fillStyle = css; x.fillRect(0, 0, 1, 1);
    var d = x.getImageData(0, 0, 1, 1).data; return [d[0], d[1], d[2], d[3] / 255];
  }
  function lum(c) { var v = c.slice(0, 3).map(function (u) { u /= 255; return u <= 0.04045 ? u / 12.92 : Math.pow((u + 0.055) / 1.055, 2.4); });
    return 0.2126 * v[0] + 0.7152 * v[1] + 0.0722 * v[2]; }
  function ratio(f, b) { var l1 = lum(f), l2 = lum(b); return (Math.max(l1, l2) + 0.05) / (Math.min(l1, l2) + 0.05); }
  // Resolve a CSS colour expression (var(), color-mix()) through a probe element.
  function resolve(expr) { var p = document.createElement('div'); p.style.color = expr; document.body.appendChild(p);
    var v = getComputedStyle(p).color; p.remove(); return rgba(v); }
`;

for (const theme of ['light', 'dark']) {
  test(`measure (${theme}): accent text and the injected-values chip reach 4.5:1`, async () => {
    const pairs = accentStyles().map((s) => ({ where: s.where, fg: s.color, bg: 'var(--accent)' }));
    const h = await startPage({ beforeScript: `try { localStorage.setItem('otgw-theme', '${theme}'); } catch (e) {}` });
    try {
      await h.until(`document.documentElement.getAttribute('data-theme') === '${theme}'`);
      const res = await h.evalJs(`(function(){ ${LIB}
        var pairs = ${JSON.stringify(pairs)};
        var rect = document.querySelector('#cnOvr rect'), txtEl = document.getElementById('cnOvrTxt');
        if (rect && txtEl) pairs.push({ where: 'v2.html #cnOvrTxt', fg: txtEl.getAttribute('fill'), bg: rect.getAttribute('fill') });
        else pairs.push({ where: 'v2.html #cnOvrTxt', missing: true });
        return pairs.map(function (p) { if (p.missing) return p; p.ratio = Math.round(ratio(resolve(p.fg), resolve(p.bg)) * 100) / 100; return p; });
      })()`);
      for (const r of res) console.log(`  ${theme} ${r.where} ${r.fg} on ${r.bg}: ${r.ratio}`);
      const bad = res.filter((r) => r.missing || !(r.ratio >= 4.5));
      assert.deepEqual(bad, [], `${theme}: pairs below 4.5:1`);
    } finally { await h.close(); }
  });
}
