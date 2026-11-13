// TASK-1885 (VTherm UI review U7/U8): the SAT control loop chart plots the boiler's ACTUAL flow (sat/status
// flow_temp) next to the setpoint, keeps the heating curve as its own labelled dashed line, and adds modulation and
// flame panels; the outside and room tiles say how old the reading is, in plain minutes. Before the task the line
// labelled "Flow" was the heating curve (satHistPush pushed d.heating_curve).
// Runs the shipped satHistPush(), satDrawHistory(), satRenderAge() and drawPanelChart(), cut out of data/v2.js, on a
// fixture payload against fake DOM elements. OTGW_DATA_DIR points at another data/ for old-vs-fix runs.
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
  if (start < 0) return null;
  let i = SRC.indexOf('{', start), depth = 0;
  for (; i < SRC.length; i++) {
    const c = SRC[i];
    if (c === "'" || c === '"') { for (i++; i < SRC.length && SRC[i] !== c; i++) if (SRC[i] === '\\') i++; }
    else if (c === '{') depth++; else if (c === '}' && --depth === 0) return SRC.slice(start, i + 1);
  }
  throw new Error('unbalanced');
}

const COLORS = { '--accent': '#acc', '--hot': '#f00', '--zone-ok': '#0f0', '--cold': '#00f', '--muted': '#999' };
function load() {
  const els = {};
  const el = (id) => els[id] || (els[id] = { id, attrs: {}, innerHTML: '', textContent: '', hidden: false, setAttribute(k, v) { this.attrs[k] = String(v); } });
  const env = {
    document: { getElementById: el },
    satCssVar: (k) => COLORS[k],
    t: (k, o) => (o && o.count !== undefined ? k + ':' + o.count : k),
  };
  const histDecl = SRC.match(/var satHist = \{[^}]*\};/)[0];
  const esc = 'function esc(s) { return ("" + (s == null ? "" : s)).replace(/&/g, "&amp;").replace(/\\x22/g, "&quot;").replace(/</g, "&lt;"); }';
  const body = [
    'var C_X0 = 34, C_X1 = 772, SAT_HIST_MAX = 720;', histDecl, esc,
    'function satNum(v) { return (v !== null && v !== undefined && !isNaN(v)) ? +v : null; }',
    fn('drawPanelChart'), fn('satHistPush'), fn('satDrawHistory'), fn('satRenderAge') || 'function satRenderAge() {}',
    'return { satHistPush: satHistPush, satDrawHistory: satDrawHistory, satRenderAge: satRenderAge, satHist: satHist };',
  ].join('\n');
  const api = new Function('document', 'satCssVar', 't', body)(env.document, env.satCssVar, env.t);
  return { api, els };
}

const payload = (i) => ({
  final_setpoint: 50, flow_temp: 40 + i, heating_curve: null, current_modulation: 20 * i, flame_on: i > 0,
  room_temp: 20.5, outside_temp: 8, pid_output: 1.5, room_temp_age_s: 125, outside_temp_age_s: -1,
});

test('the chart plots actual flow next to the setpoint and labels the heating curve as such', () => {
  const { api, els } = load();
  for (let i = 0; i < 4; i++) api.satHistPush(payload(i));
  api.satDrawHistory();
  const h = els.satHistChart.innerHTML;
  const titles = [...h.matchAll(/class="axis-txt gtitle"[^>]*>([^<]*)</g)].map((m) => m[1]);
  assert.deepEqual(titles, [
    'tools.legend_room · tools.legend_outside',
    'tools.legend_setpoint · tools.legend_flow · h_mon.sat_curve_title',
    'graph.series_mod', 'h_mon.graph_flame', 'tools.legend_pid',
  ]);
  // the hot line is the actual flow (flow_temp 40..43); the fixture has no heating curve, so on the old code
  // (Flow = heating_curve) there is no hot line at all
  const hot = [...h.matchAll(/<path d="([^"]*)" fill="none" stroke="#f00"/g)];
  assert.equal(hot.length, 1, 'actual flow line drawn');
  assert.equal((hot[0][1].match(/L/g) || []).length, 3, 'one point per sample');
  assert.match(h, /fill-opacity="\.18" stroke="#00bffe"/, 'modulation as an area');
  assert.match(h, /fill-opacity="\.3" stroke="#ff7b00"/, 'flame as an on/off lane');
  const lg = els.satHistLegend.innerHTML;
  assert.match(lg, /tools\.legend_flow[\s\S]*h_mon\.sat_curve_title/);
});

test('a heating curve value is drawn dashed, apart from the actual flow', () => {
  const { api, els } = load();
  for (let i = 0; i < 3; i++) api.satHistPush({ ...payload(i), heating_curve: 45 });
  api.satDrawHistory();
  assert.match(els.satHistChart.innerHTML, /fill="none" stroke="#999" stroke-width="1.8" stroke-dasharray="5 4"/);
});

test('tile ages: plain minutes, hidden without a known age', () => {
  const { api, els } = load();
  api.satRenderAge('satRoomAge', 125);
  api.satRenderAge('satOutAge', -1);
  assert.equal(els.satRoomAge.hidden, false);
  assert.equal(els.satRoomAge.textContent, 'tools.sat_age_min:2');
  assert.equal(els.satOutAge.hidden, true);
  api.satRenderAge('satRoomAge', 30);
  assert.equal(els.satRoomAge.textContent, 'tools.sat_age_now');
  api.satRenderAge('satRoomAge', undefined);   // firmware without the field
  assert.equal(els.satRoomAge.hidden, true);
  assert.ok(fn('satRenderAge'), 'satRenderAge() exists');
});

test('the SAT page markup has the two age lines and the page fills them', () => {
  const html = fs.readFileSync(path.join(DATA, 'v2.html'), 'utf8');
  assert.match(html, /id="satOut">—<\/div><div class="sub" id="satOutAge" hidden>/);
  assert.match(html, /id="satRoomTrend"><\/span><\/div><div class="sub" id="satRoomAge" hidden>/);
  assert.match(SRC, /satRenderAge\('satOutAge', d\.outside_temp_age_s\)/);
  assert.match(SRC, /satRenderAge\('satRoomAge', d\.room_temp_age_s\)/);
});
