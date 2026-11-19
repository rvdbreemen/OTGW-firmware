// TASK-2037: the Monitor graph draws the setpoint and target series (control setpoint, thermostat setpoint,
// SAT target) as dashed lines, but their legend swatch was a solid block. The swatch must show the dash, on
// screen and in the PNG export legend.
//   1. Real v2.html/v2.js in headless Chrome, SAT enabled with a satTarget history: the SAT target swatch is a
//      dashed top border in the line's colour; a solid series (boiler) keeps its solid block.
//   2. legendEntries() + layoutLegend() cut out of data/v2.js: a dashed swatch reports its colour (from the
//      border) and dash: true, so the PNG legend can draw it dashed.
// Point OTGW_DATA_DIR (test 1) or OTGW_V2_JS (test 2) at an older copy to see it red.
//
// Run: node --test tests/webui/graph-legend-dashed.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startPage, CHROME } from './lib/page-harness.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const V2JS = process.env.OTGW_V2_JS || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data', 'v2.js');
const SRC = fs.readFileSync(V2JS, 'utf8');

function extractFunction(src, name) {
  const start = src.indexOf('function ' + name + '(');
  if (start < 0) throw new Error(name + ' not found in ' + V2JS);
  let i = src.indexOf('{', start), depth = 0;
  for (; i < src.length; i++) {
    if (src[i] === '{') depth++;
    else if (src[i] === '}' && --depth === 0) return src.slice(start, i + 1);
  }
  throw new Error('unbalanced ' + name);
}

const n = 24, sc = 10;
const col = (f) => Array.from({ length: n }, (_, i) => f(i));
const HIST = { count: n, interval_s: 300, newest_age_ms: 2000, scale: sc,
  flow: col(() => 400), ret: col(() => 330), chSet: col(() => 420), roomSet: col(() => 200), satTarget: col(() => 207),
  room: col(() => 199), outside: col(() => 70), mod: col(() => 30), flame: col(() => 1), dhw_on: col(() => 0), ch: col(() => 1) };
const SAT = { enabled: true, active: true, fallback_active: false, target_temp: 20.7, room_temp: 19.9, outside_temp: 7,
  heating_source: 1, heating_source_detected: 1 };

test('the SAT target legend swatch is dashed like its line; a solid series keeps a solid swatch', { skip: !CHROME && 'no Chrome' }, async () => {
  const h = await startPage({ api: (m, p) => {
    if (p.includes('/v2/graph/history')) return HIST;
    if (p.endsWith('/v2/sat/status')) return SAT;
    return undefined;
  } });
  try {
    await h.until(`!!document.querySelector('.seg[data-page="monitor"]')`);
    await h.evalJs(`document.querySelector('.seg[data-page="monitor"]').click()`);
    await h.evalJs(`document.querySelector('.subtab[data-tab="mgraph"]').click()`);
    const sw = (label) => `(function(){ var lg = document.getElementById('gLegend'); if (!lg) return null;
      for (var i = 0; i < lg.children.length; i++) { var sp = lg.children[i];
        if (sp.textContent.trim() === ${JSON.stringify(label)}) { var c = getComputedStyle(sp.firstChild);
          return { bg: c.backgroundColor, bts: c.borderTopStyle, btc: c.borderTopColor }; } }
      return null; })()`;
    assert.ok(await h.until(`!!${sw('SAT target')}`, 20000), 'the legend has a SAT target entry');
    const sat = await h.evalJs(sw('SAT target'));
    const line = await h.evalJs(`(function(){ var p = document.querySelector('#gstrip [data-k="satTarget"]');
      return p ? { dash: p.getAttribute('stroke-dasharray'), stroke: p.getAttribute('stroke') } : null; })()`);
    assert.equal(line && line.dash, '5 4', 'the SAT target line itself is dashed: ' + JSON.stringify(line));
    assert.equal(sat.bts, 'dashed', 'SAT target swatch is dashed: ' + JSON.stringify(sat));
    assert.equal(sat.bg, 'rgba(0, 0, 0, 0)', 'no solid block behind the dash: ' + JSON.stringify(sat));
    const boiler = await h.evalJs(sw('boiler'));
    assert.ok(boiler, 'the legend has a boiler entry');
    assert.notEqual(boiler.bg, 'rgba(0, 0, 0, 0)', 'boiler swatch stays a solid block: ' + JSON.stringify(boiler));
    assert.notEqual(boiler.bts, 'dashed', 'boiler swatch is not dashed: ' + JSON.stringify(boiler));
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('legendEntries reports a dashed swatch with its colour and dash, and layoutLegend keeps the dash', () => {
  const fns = new Function(extractFunction(SRC, 'legendEntries') + '\n' + extractFunction(SRC, 'layoutLegend') +
    '\nreturn { legendEntries: legendEntries, layoutLegend: layoutLegend };')();
  const lg = { children: [
    { style: { opacity: '1' }, firstChild: { style: { background: 'rgb(248, 81, 73)', borderTopStyle: '', borderTopColor: '' } }, textContent: 'boiler' },
    { style: { opacity: '1' }, firstChild: { style: { background: 'none', borderTopStyle: 'dashed', borderTopColor: 'rgb(224, 195, 65)' } }, textContent: 'SAT target' } ] };
  global.document = { getElementById: () => lg };
  try {
    const e = fns.legendEntries();
    assert.deepEqual(e.map((x) => [x.label, x.color, !!x.dash]),
      [['boiler', 'rgb(248, 81, 73)', false], ['SAT target', 'rgb(224, 195, 65)', true]]);
    const lay = fns.layoutLegend(e, 780, (s) => s.length * 6);
    assert.deepEqual(lay.items.map((x) => !!x.dash), [false, true], 'the layout carries the dash to the PNG painter');
  } finally { delete global.document; }
});
