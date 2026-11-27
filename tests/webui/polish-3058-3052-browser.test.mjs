// TASK-3058 + TASK-3052 in a real browser (headless Chrome, real layout, the real v2.css and ds-tokens.css; the real
// drawPanelChart(), helpPlace() and hideHelp() cut out of v2.js and run in the page). The page itself is a small synthetic
// one (three .set-group cards in a row, one stacked-panel chart), not the whole app: the full SAT page needs a long
// fake device. Synthetic values only.
//
//  3058: the popover of an icon at the right edge of the middle card must stay inside that card (before: it ran over the
//        card on the right). Checked at 375 px (cards stacked: the popover stays inside its card and the viewport)
//        and at 1400 px (cards side by side).
//  3052: every y label of the chart ends at least 3 px before the axis line, measured with getBBox() of the rendered text
//        (before: "-10 C" ended at the line).
//
// OTGW_V2_JS=<older v2.js> runs the old code (red). OTGW_SHOT_DIR=<dir> saves screenshots.
// Run: node --test tests/webui/polish-3058-3052-browser.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startPage, CHROME } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';
const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const V2JS = process.env.OTGW_V2_JS || path.join(DATA, 'v2.js');
const SRC = fs.readFileSync(V2JS, 'utf8');
const shotDir = process.env.OTGW_SHOT_DIR;

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
const ESC = 'function esc(s) { return ("" + (s == null ? "" : s)).replace(/&/g, "&amp;").replace(/\\x22/g, "&quot;").replace(/</g, "&lt;"); }';

const PAGE = `<!doctype html><html data-theme="light"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<link rel="stylesheet" href="ds-tokens.css"><link rel="stylesheet" href="v2.css">
<style>body{margin:0;padding:8px;background:var(--bg)} .row{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:12px}
.set-group{padding:10px} #chartbox{margin-top:16px;width:100%;max-width:780px} #chart{width:100%;height:auto;display:block}</style></head><body>
<div class="row"><section class="set-group" id="c1"><h3>Zones</h3><p>Zone one label text</p></section>
<section class="set-group" id="c2"><h3>SAT settings</h3><p>Indoor temperature</p><button type="button" class="sinfo" id="ic" style="float:right"></button></section>
<section class="set-group" id="c3"><h3>Preset</h3><p>Comfort preset text that must stay readable</p></section></div>
<div id="chartbox"><svg id="chart" xmlns="http://www.w3.org/2000/svg"></svg></div>
<div id="shelp" role="tooltip">Which room temperature SAT regulates on. A longer help text so that the popover is a real 320 px box.</div>
<script>
var C_X0 = 34, C_X1 = 772;
function t(k) { return k === 'h_mon.graph_lane_on' ? 'on' : 'off'; }
${ESC}
${fn('drawPanelChart')}
var helpPop = document.getElementById('shelp'), helpBtn = null, helpPinned = false, helpOverPop = false;
${fn('hideHelp')}
${fn('helpPlace')}
drawPanelChart(document.getElementById('chart'), { n: 4, panels: [
  { title: 'flame', kind: 'l', series: [{ vals: [0, 1, 1, 0], color: '#f80' }] },
  { title: 'modulation', kind: 'm', series: [{ vals: [0, 40, 80, 0], color: '#0bf' }] },
  { title: 'temperatures', kind: 't', unit: '\\u00b0C', series: [{ vals: [-10, 5, 12, 30], color: '#e5484d' }] } ] });
function showAt(id) { helpBtn = document.getElementById(id); helpPop.classList.add('show'); helpPlace(); }
</script></body></html>`;

const asset = (p, q, s) => {
  if (p !== '/polish.html') return false;
  s.writeHead(200, { 'content-type': 'text/html; charset=utf-8' }); s.end(PAGE); return true;
};

for (const width of [375, 1400]) {
  test(`${width} px: the help popover stays inside its own card (TASK-3058)`, { skip }, async () => {
    const h = await startPage({ width, page: '/polish.html', asset });
    try {
      assert.ok(await h.until(`!!document.getElementById('ic')`), 'page ready');
      const r = await h.evalJs(`(function () {
        var ic = document.getElementById('ic'); ic.parentNode.style.position = 'relative';
        showAt('ic');
        var p = document.getElementById('shelp').getBoundingClientRect(), c = document.getElementById('c2').getBoundingClientRect();
        return { pl: p.left, pr: p.right, cl: c.left, cr: c.right, vw: document.documentElement.clientWidth, pw: p.width };
      })()`);
      if (shotDir) await h.shot(path.join(shotDir, `popover-${width}.png`));
      assert.ok(r.pl >= r.cl - 0.5, `popover starts at ${r.pl}, its card at ${r.cl}`);
      assert.ok(r.pr <= r.cr + 0.5, `popover ends at ${r.pr}, past its card end ${r.cr} (over the next card)`);
      assert.ok(r.pr <= r.vw, 'popover inside the viewport');
      assert.ok(r.pw >= 160, `popover is ${r.pw} px wide`);
      assert.deepEqual(h.exceptions, []);
    } finally { await h.close(); }
  });

  test(`${width} px: no y label of the chart touches the axis line (TASK-3052)`, { skip }, async () => {
    const h = await startPage({ width, page: '/polish.html', asset });
    try {
      assert.ok(await h.until(`!!document.querySelector('#chart text')`), 'chart drawn');
      const labels = await h.evalJs(`Array.prototype.map.call(document.querySelectorAll('#chart text.axis-txt:not(.gtitle)'), function (e) {
        var b = e.getBBox(); return { text: e.textContent, left: b.x, right: b.x + b.width }; })`);
      if (shotDir) await h.shot(path.join(shotDir, `chart-${width}.png`));
      assert.equal(labels.length, 7);
      for (const l of labels) {
        assert.ok(l.right <= 34 - 3, `"${l.text}" ends at ${l.right.toFixed(1)}, the axis line is at 34`);
        assert.ok(l.left >= -0.5, `"${l.text}" starts at ${l.left.toFixed(1)}: clipped`);
      }
      assert.deepEqual(h.exceptions, []);
    } finally { await h.close(); }
  });
}
