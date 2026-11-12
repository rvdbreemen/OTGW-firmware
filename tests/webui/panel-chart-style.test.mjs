// TASK-1841 (Robert 2026-11-12): Home > Mission control and SAT > SAT control loop use the stacked-panel
// style of Monitor > Graph (TASK-1821): panels on one time axis, a title per panel, an own y axis with
// its unit, on/off lanes as areas. Runs the shipped drawPanelChart() (cut out of data/v2.js) against a
// fake SVG, and checks that both charts use it with the panels chosen for each screen.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const SRC = fs.readFileSync(path.join(DATA, 'v2.js'), 'utf8');
function fn(name) {
  const start = SRC.indexOf('function ' + name + '(');
  assert.ok(start >= 0, name + ' not found in v2.js');
  let i = SRC.indexOf('{', start), depth = 0;
  for (; i < SRC.length; i++) {
    const c = SRC[i];
    if (c === "'" || c === '"') { for (i++; i < SRC.length && SRC[i] !== c; i++) if (SRC[i] === '\\') i++; }
    else if (c === '{') depth++; else if (c === '}' && --depth === 0) return SRC.slice(start, i + 1);
  }
  throw new Error('unbalanced');
}
const draw = () => {
  // esc() is v2.js's HTML escaper; its regex literals contain quotes, which this brace matcher does not parse, so it
  // is restated here (same three replacements). drawPanelChart() is the real one.
  const esc = 'function esc(s) { return ("" + (s == null ? "" : s)).replace(/&/g, "&amp;").replace(/\\x22/g, "&quot;").replace(/</g, "&lt;"); }';
  const body = 'var C_X0 = 34, C_X1 = 772;\nfunction t(k) { return k === "h_mon.graph_lane_on" ? "on" : "off"; }\n' + esc + '\n' + fn('drawPanelChart') + '\nreturn drawPanelChart;';
  return new Function(body)();
};
const fakeSvg = () => ({ attrs: {}, innerHTML: '', setAttribute(k, v) { this.attrs[k] = String(v); } });

test('stacked panels: titles in order, own axes with units, on/off as areas, viewBox fits', () => {
  const svg = fakeSvg();
  draw()(svg, { n: 4, panels: [
    { title: 'flame', kind: 'l', series: [{ vals: [0, 1, 1, 0], color: '#f80' }] },
    { title: 'modulation', kind: 'm', series: [{ vals: [0, 40, 80, 0], color: '#0bf' }] },
    { title: 'temperatures', kind: 't', unit: '°C', series: [{ vals: [30, 45, null, 50], color: '#f00' }, { vals: [28, 30, 31, 32], color: '#00f', dash: '5 4' }] },
  ] });
  const h = svg.innerHTML;
  const titles = [...h.matchAll(/class="axis-txt gtitle"[^>]*>([^<]*)</g)].map((m) => m[1]);
  assert.deepEqual(titles, ['flame', 'modulation', 'temperatures']);
  assert.match(h, />100%</); assert.match(h, />0%</);
  assert.match(h, />50°C</); assert.match(h, />20°C</);   // 28..50 rounds to a 20..50 axis, as Monitor > Graph
  assert.match(h, /fill-opacity="\.3"/, 'on/off lane drawn as an area');
  assert.match(h, /stroke-dasharray="5 4"/);
  const paths = [...h.matchAll(/<path d="([^"]*)" fill="none" stroke="#f00"/g)];
  assert.equal(paths.length, 1); assert.equal((paths[0][1].match(/M/g) || []).length, 2, 'a gap breaks the line');
  const vb = /^0 0 780 (\d+(\.\d+)?)$/.exec(svg.attrs.viewBox); assert.ok(vb, 'viewBox ' + svg.attrs.viewBox);
  const ys = [...h.matchAll(/y2?="(\d+(\.\d+)?)"/g)].map((m) => +m[1]);
  assert.ok(Math.max(...ys) <= +vb[1], 'everything inside the viewBox');
});

test('Mission control and SAT control loop draw with it, with the panels picked per screen', () => {
  const c = fn('renderC'), s = fn('satDrawHistory');
  assert.match(c, /drawPanelChart\(/); assert.match(s, /drawPanelChart\(/);
  // Mission control: the boiler loop live: flame, modulation, temperatures (flow, return, setpoint)
  assert.match(c, /h_mon\.graph_flame[\s\S]*graph\.series_mod[\s\S]*h_mon\.graph_temps/);
  // SAT control loop: room/outside, flow (setpoint, heating curve), PID output, each with its own scale
  assert.match(s, /legend_room[\s\S]*legend_setpoint[\s\S]*legend_pid/);
});
