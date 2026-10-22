// TASK-1267 guard: Monitor > Graph needs a time (X) axis, and the exported PNG must show
// readable Y-axis labels, the legend of the visible series and that time axis.
//
// Why the PNG was wrong: exportPng() drew the #gstrip SVG as an <img>. An SVG used as an
// image cannot load v2-bundle.css, so every <text class="axis-txt"> lost its fill and font
// (black on the #0d1117 canvas). #gLegend is an HTML div outside the SVG, so it was never
// exported at all. There was no time axis on screen either.
//
// Runs the REAL shipped graphTimeTicks(), renderGraphTime(), inlineSvgTextStyles(),
// legendEntries(), layoutLegend() and exportPng(), cut out of data/v2.js by brace matching
// and evaluated against small DOM / canvas stubs. The stubs record what the real code does
// (attributes set on the cloned SVG, canvas calls, the SVG handed to the <img>).
// Point OTGW_V2_JS / OTGW_V2_HTML at older copies (git show HEAD:... > file) to see it red.
//
// Not covered: how a real browser rasterises the SVG-as-image (needs the bench / Playwright
// check), and getComputedStyle on real CSS (stubbed with the values v2.css yields).
//
// Run: node --test tests/webui/graph-time-axis-and-png-export.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const V2JS = process.env.OTGW_V2_JS || path.join(DATA, 'v2.js');
const V2HTML = process.env.OTGW_V2_HTML || path.join(DATA, 'v2.html');
const SRC = fs.readFileSync(V2JS, 'utf8');
const HTML = fs.readFileSync(V2HTML, 'utf8');

function extractFunction(src, name, optional = false) {
  const start = src.indexOf('function ' + name + '(');
  if (start < 0) { if (optional) return ''; throw new Error(name + ' not found in ' + V2JS); }
  let i = src.indexOf('{', start), depth = 0;
  for (; i < src.length; i++) {
    const c = src[i];
    if (c === "'" || c === '"' || c === '`') {
      for (i++; i < src.length && src[i] !== c; i++) if (src[i] === '\\') i++;
    } else if (c === '/' && src[i + 1] === '/') {
      i = src.indexOf('\n', i);
    } else if (c === '/' && src[i + 1] === '*') {
      i = src.indexOf('*/', i) + 1;
    } else if (c === '{') depth++;
    else if (c === '}' && --depth === 0) return src.slice(start, i + 1);
  }
  throw new Error('unbalanced braces in ' + name);
}

const PLOT = SRC.match(/var C_X0 = \d+, C_X1 = \d+;/)[0];   // the shipped plot bounds
const { C_X0, C_X1 } = new Function(PLOT + ' return { C_X0: C_X0, C_X1: C_X1 };')();

const MIN = 60000, HOUR = 3600000;
const WINDOWS = [['10 min', 10 * MIN], ['1 h', HOUR], ['4 h', 4 * HOUR], ['6 h', 6 * HOUR], ['24 h', 24 * HOUR]];
const loadTicks = () => new Function(PLOT + '\n' + extractFunction(SRC, 'graphTimeTicks') + '\nreturn graphTimeTicks;')();

function setTz(tz) { process.env.TZ = tz; }

// Is every tick a local wall-clock multiple of the step between two neighbours?
function assertAligned(ticks, label) {
  const step = ticks[1].t - ticks[0].t;
  for (const k of ticks) {
    const d = new Date(k.t);
    if (step < HOUR) assert.equal(d.getMinutes() % (step / MIN), 0, label + ': minute ' + d.getMinutes() + ' not on a ' + step / MIN + ' min grid');
    else assert.equal(d.getHours() % (step / HOUR) + d.getMinutes(), 0, label + ': ' + d.toString() + ' not on a ' + step / HOUR + ' h grid');
    assert.equal(d.getSeconds() + d.getMilliseconds(), 0, label + ': seconds not zero');
  }
}

for (const tz of ['Europe/Amsterdam', 'Asia/Kolkata']) {
  test('graphTimeTicks (' + tz + '): 10 min, 1 h, 6 h, 24 h windows give 4..8 round-clock ticks inside the plot', () => {
    setTz(tz);
    const graphTimeTicks = loadTicks();
    const base = Date.UTC(2026, 9, 10, 0, 0, 0);       // 10 Oct 2026, before the Amsterdam DST change
    const counts = {};
    for (const [name, win] of WINDOWS) {
      for (let m = 0; m < 24 * 60; m += 7) {            // 206 different "now" phases over a day
        const now = base + m * MIN + 13000;             // not minute aligned
        const t0 = now - win;
        const ticks = graphTimeTicks(t0, win);
        const label = tz + ' ' + name + ' now=' + new Date(now).toISOString();
        assert.ok(ticks.length >= 4 && ticks.length <= 8, label + ': ' + ticks.length + ' ticks');
        counts[name] = counts[name] || new Set(); counts[name].add(ticks.length);
        for (const k of ticks) {
          assert.ok(k.x >= C_X0 && k.x <= C_X1, label + ': x ' + k.x + ' outside plot');
          assert.ok(k.t >= t0 && k.t <= now, label + ': tick outside window');
          assert.match(k.label, /^\d\d:\d\d$|^\d\d-\d\d$/);
        }
        for (let i = 1; i < ticks.length; i++) assert.equal(ticks[i].t - ticks[i - 1].t, ticks[1].t - ticks[0].t, label + ': uneven spacing');
        assertAligned(ticks, label);
      }
    }
    const summary = Object.entries(counts).map(([n, s]) => n + ' -> ' + [...s].sort().join('/') + ' ticks').join(', ');
    console.log(tz + ': ' + summary);
  });
}

test('graphTimeTicks: the step depends on the window only, so it does not flip while the graph scrolls', () => {
  setTz('Europe/Amsterdam');
  const graphTimeTicks = loadTicks();
  const base = Date.UTC(2026, 9, 10, 8, 0, 0);
  for (const [name, win] of WINDOWS) {
    const steps = new Set();
    for (let s = 0; s < 3600; s += 17) {
      const t = graphTimeTicks(base + s * 1000 - win, win);
      steps.add(t[1].t - t[0].t);
    }
    assert.equal(steps.size, 1, name + ': steps ' + [...steps]);
  }
});

test('graphTimeTicks: a 24 h window shows the date (dd-mm) on the midnight tick, and the grid survives the DST change', () => {
  setTz('Europe/Amsterdam');
  const graphTimeTicks = loadTicks();
  const now = Date.UTC(2026, 9, 25, 12, 0, 0);          // DST ends 25 Oct 2026 01:00 UTC, inside the window
  const ticks = graphTimeTicks(now - 24 * HOUR, 24 * HOUR);
  assert.ok(ticks.length >= 4 && ticks.length <= 8, ticks.length + ' ticks');
  assertAligned(ticks, 'DST window');
  assert.ok(ticks.some((k) => k.label === '25-10'), 'labels: ' + ticks.map((k) => k.label));
  assert.ok(!ticks.some((k) => k.label === '00:00'), 'midnight must show the date');
  const short = loadTicks()(Date.UTC(2026, 9, 10, 21, 30) - HOUR, HOUR);   // 1 h window: no date labels
  assert.ok(short.every((k) => /^\d\d:\d\d$/.test(k.label)));
});

// Review finding (TASK-1267): the first version built ticks from the UTC offset at t0 and patched
// it per tick, so a window over the spring-forward change got repeated, out-of-order ticks
// (10 min window at 2026-03-29 01:03Z: 01:54 01:56 01:58 01:54 ...) and a window over the
// fall-back change lost the repeated hour. Sweep "now" over both changes for every chip window.
test('graphTimeTicks: over both DST changes the ticks stay strictly increasing, on round clock times, 4..8 per window', () => {
  setTz('Europe/Amsterdam');
  const graphTimeTicks = loadTicks();
  const STEPS = [1, 2, 5, 10, 15, 30, 60, 120, 180, 360, 720, 1440].map((m) => m * MIN);
  let windows = 0;
  for (const change of [Date.UTC(2026, 2, 29, 1, 0, 0), Date.UTC(2026, 9, 25, 1, 0, 0)]) {
    for (const [name, win] of [['10 min', 10 * MIN], ['1 h', HOUR], ['4 h', 4 * HOUR], ['24 h', 24 * HOUR]]) {
      const step = STEPS.find((s) => win / s <= 8);
      for (let now = change - 60 * MIN; now <= change + win + 60 * MIN; now += 3 * MIN + 13000) {
        const t0 = now - win, ticks = graphTimeTicks(t0, win), label = name + ' now=' + new Date(now).toISOString();
        windows++;
        // 9 is right once a year: 24 real hours over spring-forward hold 25 wall-clock hours, so a
        // ninth 3 h tick fits (one gap is then 2 h; nothing overlaps).
        assert.ok(ticks.length >= 4 && ticks.length <= 9, label + ': ' + ticks.length + ' ticks ' + ticks.map((k) => k.label));
        assert.equal(new Set(ticks.map((k) => k.t)).size, ticks.length, label + ': repeated tick');
        for (let i = 1; i < ticks.length; i++) {
          assert.ok(ticks[i].t > ticks[i - 1].t && ticks[i].x > ticks[i - 1].x, label + ': not increasing ' + ticks.map((k) => k.label));
          assert.ok(ticks[i].t - ticks[i - 1].t <= 2 * step, label + ': gap in the axis ' + ticks.map((k) => k.label));
        }
        for (const k of ticks) {
          const d = new Date(k.t);
          assert.ok(k.t >= t0 && k.t <= now, label + ': tick outside window');
          assert.equal(d.getSeconds() + d.getMilliseconds(), 0, label);
          if (step < HOUR) assert.equal(d.getMinutes() % (step / MIN), 0, label + ': ' + k.label);
          else assert.equal(d.getHours() % (step / HOUR) + d.getMinutes(), 0, label + ': ' + k.label);
        }
      }
    }
  }
  console.log('DST sweep: ' + windows + ' windows checked');
});

// ---- renderGraphTime: cheap per pass --------------------------------------------------------

function fakeSvgEl(tag) {
  return { tag, attrs: {}, children: [], _text: '',
    setAttribute(k, v) { this.attrs[k] = String(v); },
    appendChild(c) { this.children.push(c); return c; },
    get childNodes() { return this.children; },
    set textContent(v) { this._text = v; if (v === '') this.children.length = 0; },
    get textContent() { return this._text; } };
}

test('renderGraphTime rebuilds #gTime only when the tick set changes, otherwise just moves the x positions', () => {
  setTz('Europe/Amsterdam');
  const g = fakeSvgEl('g');
  let created = 0;
  const body = PLOT + '\n' +
    'var document = { getElementById: function (id) { return id === "gTime" ? g : null; },' +
    ' createElementNS: function (ns, tag) { return mk(tag); } };\n' +
    SRC.match(/var gTimeSig = '';/)[0] + '\n' +
    extractFunction(SRC, 'graphTimeTicks') + '\n' + extractFunction(SRC, 'renderGraphTime') + '\n' +
    'return renderGraphTime;';
  const render = new Function('g', 'mk', body)(g, (tag) => { created++; return fakeSvgEl(tag); });
  const win = HOUR, now = Date.UTC(2026, 9, 10, 10, 3, 20);
  render(now - win, win);
  const first = created;
  assert.ok(first >= 8 && first % 2 === 0, 'created ' + first + ' elements');
  const x0 = g.children.filter((c) => c.tag === 'line').map((c) => +c.attrs.x1);
  render(now + 2000 - win, win);                         // 2 s later: same ticks, shifted left
  assert.equal(created, first, 'no new elements within the same tick set');
  const x1 = g.children.filter((c) => c.tag === 'line').map((c) => +c.attrs.x1);
  assert.ok(x1.every((x, i) => x < x0[i]), 'positions move left as time passes');
  for (const l of g.children.filter((c) => c.tag === 'line')) assert.equal(l.attrs.x1, l.attrs.x2);
  const labels = g.children.filter((c) => c.tag === 'text').map((c) => c.textContent);
  assert.ok(labels.every((t) => /^\d\d:\d\d$/.test(t)), labels.join());
  // Layout: labels centred on their gridline, inside the 262-high viewBox below the CH lane
  // (y 226..240), gridlines spanning the plot and lanes; no label within half a label of the edge.
  const vbH = +HTML.match(/id="gstrip"[^>]*viewBox="0 0 780 (\d+)"|viewBox="0 0 780 (\d+)"[^>]*id="gstrip"/).slice(1).find(Boolean);
  for (const tx of g.children.filter((c) => c.tag === 'text')) {
    assert.equal(tx.attrs['text-anchor'], 'middle');
    assert.ok(+tx.attrs.y > 240 && +tx.attrs.y <= vbH, 'label y ' + tx.attrs.y + ' (viewBox height ' + vbH + ')');
    assert.ok(+tx.attrs.x >= C_X0 + 16 && +tx.attrs.x <= C_X1 - 7, 'label x ' + tx.attrs.x + ' too close to the edge');
  }
  for (const l of g.children.filter((c) => c.tag === 'line')) {
    assert.ok(+l.attrs.y1 <= 10 && +l.attrs.y2 >= 240 && +l.attrs.y2 < vbH, 'gridline ' + l.attrs.y1 + '..' + l.attrs.y2);
  }
  render(now + 11 * MIN - win, win);                     // 11 min later: a tick left, one entered
  assert.ok(created > first, 'rebuilt when the tick set changed');
  assert.equal(g.children.length % 2, 0);
});

// ---- exportPng ------------------------------------------------------------------------------

function fakeText(cls) {
  return { cls, attrs: {}, setAttribute(k, v) { this.attrs[k] = String(v); } };
}
function fakeSvg(texts, viewBox) {
  const svg = { texts, attrs: {}, viewBox: { baseVal: viewBox }, card: { isCard: true },
    closest(sel) { assert.equal(sel, '.graphcard'); return this.card; },
    setAttribute(k, v) { this.attrs[k] = String(v); },
    querySelectorAll(sel) { assert.equal(sel, 'text'); return this.texts; },
    cloneNode(deep) {
      assert.equal(deep, true);
      const c = fakeSvg(this.texts.map((t) => { const n = fakeText(t.cls); Object.assign(n.attrs, t.attrs); return n; }), viewBox);
      Object.assign(c.attrs, this.attrs); c.isClone = true; return c;
    } };
  return svg;
}

// theme = the computed styles the page yields: card background, legend label colour, axis text fill.
const DARK = { cardBg: 'rgb(22, 27, 34)', legendCol: 'rgb(230, 237, 243)', axisFill: 'rgb(139, 148, 158)' };
const LIGHT = { cardBg: 'rgb(255, 255, 255)', legendCol: 'rgb(31, 35, 40)', axisFill: 'rgb(124, 90, 60)' };
function runExport(theme = DARK) {
  setTz('Europe/Amsterdam');
  const texts = ['gAxHi', 'gAxMid', 'gAxLo', 'flame', 'DHW', 'time1', 'time2'].map(() => fakeText('axis-txt'));
  const svg = fakeSvg(texts, { width: 780, height: 262 });
  const legend = { color: theme.legendCol, fontFamily: 'ui-monospace, monospace', children: [
    { style: { opacity: '1' }, firstChild: { style: { background: 'rgb(248, 81, 73)' } }, textContent: 'boiler' },
    { style: { opacity: '1' }, firstChild: { style: { background: 'rgb(88, 166, 255)' } }, textContent: 'return' },
    { style: { opacity: '0.4' }, firstChild: { style: { background: 'rgb(63, 185, 80)' } }, textContent: 'room' },
    { style: { opacity: '1' }, firstChild: { style: { background: 'rgb(0, 191, 254)' } }, textContent: 'modulation' } ] };
  const calls = [], rec = { canvases: [], downloads: [], imgs: [], serialized: [] };
  const ctx = new Proxy({ measureText: (t) => ({ width: t.length * 6 }) }, {
    get(t, k) { return k in t ? t[k] : (...a) => { calls.push([k, ...a]); }; },
    set(t, k, v) { calls.push(['set:' + k, v]); return true; } });
  const document = {
    getElementById(id) { return id === 'gstrip' ? svg : id === 'gLegend' ? legend : null; },
    createElement(tag) {
      assert.equal(tag, 'canvas');
      const cv = { width: 0, height: 0, getContext: () => ctx, toBlob: (cb) => cb({ fake: 'blob' }) };
      rec.canvases.push(cv); return cv;
    } };
  class Image { set src(v) { this._src = v; rec.imgs.push(this); setTimeout(() => this.onload(), 0); } get src() { return this._src; } }
  class XMLSerializer {
    serializeToString(n) {
      assert.ok(n.isClone, 'the live #gstrip must not be serialised: styles are inlined on a clone');
      const esc = (v) => v.replace(/&/g, '&amp;').replace(/"/g, '&quot;');   // as XMLSerializer does
      const s = '<svg ' + Object.entries(n.attrs).map(([k, v]) => k + '="' + esc(v) + '"').join(' ') + '>' +
        n.texts.map((t) => '<text ' + Object.entries(t.attrs).map(([k, v]) => k + '="' + esc(v) + '"').join(' ') + '/>').join('') + '</svg>';
      rec.serialized.push(s); return s;
    } }
  const getComputedStyle = (el) => {
    if (el.isCard) return { backgroundColor: theme.cardBg };
    if (el === legend || legend.children.includes(el)) return legend;   // label spans inherit the legend colour
    return { fill: theme.axisFill, fontSize: '10px', fontFamily: '"JetBrains Mono", monospace' };
  };
  const body = [extractFunction(SRC, 'paintOr', true), extractFunction(SRC, 'inlineSvgTextStyles', true), extractFunction(SRC, 'legendEntries', true),
    extractFunction(SRC, 'layoutLegend', true), extractFunction(SRC, 'exportPng')].join('\n') +
    '\nreturn exportPng;';
  const exportPng = new Function('document', 'Image', 'XMLSerializer', 'getComputedStyle', 'downloadBlob', 'ts', body)(
    document, Image, XMLSerializer, getComputedStyle, (blob, name) => rec.downloads.push([blob, name]), () => '20261022-120000');
  return new Promise((resolve) => {
    exportPng();
    const t = setInterval(() => { if (rec.downloads.length) { clearInterval(t); resolve({ rec, calls, svg, texts }); } }, 5);
    setTimeout(() => { clearInterval(t); resolve({ rec, calls, svg, texts }); }, 500);
  });
}

test('exportPng inlines fill/font on the cloned SVG text, draws the legend of the visible series, then the SVG', async () => {
  const { rec, calls, texts } = await runExport(DARK);
  assert.equal(rec.downloads.length, 1, 'a PNG must be downloaded');
  assert.match(rec.downloads[0][1], /^otgw-graph-.*\.png$/);

  // 1. The SVG handed to the <img> carries the paint and font on every <text>.
  assert.equal(rec.imgs.length, 1);
  const xml = Buffer.from(rec.imgs[0].src.replace('data:image/svg+xml;base64,', ''), 'base64').toString('utf8');
  const textTags = xml.match(/<text [^>]*>/g) || [];
  assert.equal(textTags.length, texts.length);
  for (const tag of textTags) {
    assert.match(tag, /fill="rgb\(139, 148, 158\)"/, 'text needs an inline fill: ' + tag);
    assert.match(tag, /font-size="10px"/);
    assert.match(tag, /font-family="[^"]*monospace/);
  }
  assert.match(xml, /width="780"/); assert.match(xml, /height="262"/);
  assert.ok(texts.every((t) => Object.keys(t.attrs).length === 0), 'the live SVG text elements must stay untouched');

  // 2. Legend: visible series only (room is hidden), colour swatch + label.
  const drawn = calls.filter((c) => c[0] === 'fillText').map((c) => c[1]);
  assert.deepEqual(drawn, ['boiler', 'return', 'modulation']);
  const rects = calls.filter((c) => c[0] === 'fillRect');
  assert.equal(rects.length, 1 + 3, 'background + one swatch per visible series');
  const fills = calls.filter((c) => c[0] === 'set:fillStyle').map((c) => c[1]);
  for (const col of ['rgb(248, 81, 73)', 'rgb(88, 166, 255)', 'rgb(0, 191, 254)']) assert.ok(fills.includes(col), col);
  assert.ok(!fills.includes('rgb(63, 185, 80)'), 'hidden series must not be drawn');

  // 3. Canvas: legend band on top, SVG below it, 2x scale, dark background.
  const cv = rec.canvases[0];
  assert.equal(cv.width, 1560);
  assert.equal(cv.height, (26 + 262) * 2);
  const di = calls.find((c) => c[0] === 'drawImage');
  assert.ok(di, 'the SVG image must be drawn');
  assert.deepEqual(di.slice(2), [0, 26, 780, 262]);
  assert.ok(calls.findIndex((c) => c[0] === 'fillText') < calls.indexOf(di), 'legend drawn above the graph');
  assert.equal(fills.filter((f) => f === 'rgb(22, 27, 34)').length, 1, 'canvas background = computed card background');
  assert.ok(fills.includes('rgb(230, 237, 243)'), 'legend labels use the computed legend colour');
});

test('exportPng follows the page theme: light card gives a light canvas with dark text', async () => {
  const { rec, calls } = await runExport(LIGHT);
  const xml = Buffer.from(rec.imgs[0].src.replace('data:image/svg+xml;base64,', ''), 'base64').toString('utf8');
  for (const tag of xml.match(/<text [^>]*>/g)) assert.match(tag, /fill="rgb\(124, 90, 60\)"/);
  const fills = calls.filter((c) => c[0] === 'set:fillStyle').map((c) => c[1]);
  assert.equal(fills[0], 'rgb(255, 255, 255)', 'first fill is the canvas background');
  assert.ok(fills.includes('rgb(31, 35, 40)'));
  assert.ok(!fills.includes('#0d1117'), 'no hard-coded dark background in a light theme');
});

test('exportPng falls back to #0d1117 / #c9d1d9 / #8b949e when the computed values are empty or transparent', async () => {
  const { rec, calls } = await runExport({ cardBg: 'rgba(0, 0, 0, 0)', legendCol: '', axisFill: 'none' });
  const xml = Buffer.from(rec.imgs[0].src.replace('data:image/svg+xml;base64,', ''), 'base64').toString('utf8');
  for (const tag of xml.match(/<text [^>]*>/g)) assert.match(tag, /fill="#8b949e"/);
  const fills = calls.filter((c) => c[0] === 'set:fillStyle').map((c) => c[1]);
  assert.equal(fills[0], '#0d1117');
  assert.ok(fills.includes('#c9d1d9'));
});

test('layoutLegend wraps onto a second row instead of clipping, and is empty without entries', () => {
  const layoutLegend = new Function(extractFunction(SRC, 'layoutLegend') + '\nreturn layoutLegend;')();
  const measure = (t) => t.length * 6;
  const few = layoutLegend([{ label: 'boiler', color: 'x' }, { label: 'return', color: 'x' }], 780, measure);
  assert.equal(few.height, 26); assert.ok(few.items.every((i) => i.row === 0));
  const many = layoutLegend(Array.from({ length: 14 }, (_, i) => ({ label: 'sensor-' + i + '-label', color: 'x' })), 780, measure);
  assert.ok(many.height > 26 && many.items.some((i) => i.row === 1), 'rows: ' + many.height);
  for (const it of many.items) assert.ok(it.x + 19 + measure(it.label) <= 780 - 6, 'entry ' + it.label + ' clipped at x=' + it.x);
  assert.equal(layoutLegend([], 780, measure).height, 0);
});

test('v2.html: #gstrip has room for the time axis and a #gTime group drawn before the series', () => {
  assert.match(HTML, /<svg viewBox="0 0 780 262" id="gstrip"/);
  const iTime = HTML.indexOf('id="gTime"'), iSer = HTML.indexOf('id="gSer"');
  assert.ok(iTime > 0 && iTime < iSer, 'gTime must exist and sit behind the series');
  assert.match(SRC, /renderGraphTime\(t0, win\)/, 'renderGraph must draw the axis');
});
