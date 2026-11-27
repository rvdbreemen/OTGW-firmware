// TASK-3052 guard: no y-axis label of the stacked-panel chart (SAT control loop, Monitor > Graph, Home) touches the axis line.
// The value labels were left-aligned at x=2; a 5-character label such as "-10°C" in the 10 px mono font is about 30 px wide,
// so it ended at x=32 with the axis at C_X0=34 (bench screenshot S15). Now every y label is right-aligned
// (text-anchor="end", class yl) and ends 4 px left of the axis, whatever its length.
//
// Runs the REAL drawPanelChart() cut out of data/v2.js against a fake SVG. Not covered: real glyph widths (the estimate is
// 6 px per character for the 10 px mono font) and the dark theme (colour only; the position does not change).
//
// Red run: OTGW_V2_JS=<git show origin/dev:src/OTGW-firmware/data/v2.js saved to a file>.
// Run: node --test tests/webui/chart-ylabel-axis-3052.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const V2JS = process.env.OTGW_V2_JS || path.join(DATA, 'v2.js');
const SRC = fs.readFileSync(V2JS, 'utf8');
const C_X0 = 34, CHAR_W = 6;

function fn(name) {
  const start = SRC.indexOf('function ' + name + '(');
  assert.ok(start >= 0, name + ' not found in ' + V2JS);
  let i = SRC.indexOf('{', start), depth = 0;
  for (; i < SRC.length; i++) {
    const c = SRC[i];
    if (c === "'" || c === '"') { for (i++; i < SRC.length && SRC[i] !== c; i++) if (SRC[i] === '\\') i++; }
    else if (c === '{') depth++; else if (c === '}' && --depth === 0) return SRC.slice(start, i + 1);
  }
  throw new Error('unbalanced');
}
const draw = () => {
  // esc() is v2.js's HTML escaper (its regex literals defeat the brace matcher); drawPanelChart() is the real one.
  const esc = 'function esc(s) { return ("" + (s == null ? "" : s)).replace(/&/g, "&amp;").replace(/\\x22/g, "&quot;").replace(/</g, "&lt;"); }';
  const body = 'var C_X0 = 34, C_X1 = 772;\nfunction t(k) { return k === "h_mon.graph_lane_on" ? "on" : "off"; }\n' + esc + '\n' + fn('drawPanelChart') + '\nreturn drawPanelChart;';
  return new Function(body)();
};
const render = (panels) => {
  const svg = { attrs: {}, innerHTML: '', setAttribute(k, v) { this.attrs[k] = String(v); } };
  draw()(svg, { n: 4, panels });
  return svg.innerHTML;
};
const labels = (html) => [...html.matchAll(/<text class="([^"]*)" x="([-\d.]+)" y="[-\d.]+"([^>]*)>([^<]*)<\/text>/g)]
  .map((m) => ({ cls: m[1], x: Number(m[2]), extra: m[3], text: m[4] }))
  .filter((l) => !/\bgtitle\b/.test(l.cls));

const panels = [
  { title: 'flame', kind: 'l', series: [{ vals: [0, 1, 1, 0], color: '#f80' }] },
  { title: 'modulation', kind: 'm', series: [{ vals: [0, 40, 80, 0], color: '#0bf' }] },
  { title: 'temperatures', kind: 't', unit: '°C', series: [{ vals: [-10, 5, 12, 30], color: '#f00' }] },
];

test('every y label is right-aligned and ends before the axis (temperature, percent and lane labels)', () => {
  const ls = labels(render(panels));
  assert.equal(ls.length, 7, 'expected 2 lane + 2 percent + 3 temperature labels, got ' + ls.length);
  for (const l of ls) {
    assert.match(l.cls, /\byl\b/, 'label "' + l.text + '" has no yl class');
    assert.match(l.extra, /text-anchor="end"/, 'label "' + l.text + '" is not end-anchored (works without the CSS, also in the PNG export)');
    assert.ok(l.x <= C_X0 - 3, 'label "' + l.text + '" ends at x=' + l.x + ', too close to the axis at ' + C_X0);
  }
});

test('the longest label (-10°C) starts inside the viewBox and ends 3 px or more before the axis', () => {
  const l = labels(render(panels)).find((x) => x.text === '-10°C');
  assert.ok(l, '-10°C label not drawn');
  const startX = l.x - l.text.length * CHAR_W;
  assert.ok(startX >= 0, 'label would start at x=' + startX + ' (clipped)');
  assert.ok(l.x <= C_X0 - 3);
});

test('panel titles keep their place at x=38 (not a y label)', () => {
  const html = render(panels);
  assert.match(html, /class="axis-txt gtitle" x="38"/);
});
