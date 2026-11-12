// TASK-1828 (09's bench findings on beta.187): the Monitor > Graph window chips are translated and
// the window comes from a data attribute, not from the English chip text; the temperature axis
// reads °C, as the 1.x OTGraph ('{value}°C').
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const DATA = process.env.OTGW_DATA_DIR || path.join(ROOT, 'src', 'OTGW-firmware', 'data');
const SRC = fs.readFileSync(path.join(DATA, 'v2.js'), 'utf8');
const HTML = fs.readFileSync(path.join(DATA, 'v2.html'), 'utf8');
const PACK = (l) => JSON.parse(fs.readFileSync(path.join(ROOT, 'i18n', l + '.json'), 'utf8'));

function fn(name) {
  const start = SRC.indexOf('function ' + name + '(');
  assert.ok(start >= 0, name + ' not found in v2.js');
  let i = SRC.indexOf('{', start), depth = 0;
  for (; i < SRC.length; i++) { if (SRC[i] === '{') depth++; else if (SRC[i] === '}' && --depth === 0) return SRC.slice(start, i + 1); }
  throw new Error('unbalanced');
}
const toolbar = (() => { const m = /<div id="mgraph"[\s\S]*?<div class="graphcard">/.exec(HTML); assert.ok(m); return m[0]; })();

test('window chips carry their window in ms and a translation key', () => {
  const chips = [...toolbar.matchAll(/<span class="tchip[^"]*"([^>]*)>/g)].map((m) => m[1]).filter((a) => !/\bid="/.test(a));
  const got = chips.map((a) => [(/data-win-ms="(\d+)"/.exec(a) || [])[1], (/data-i18n="([^"]+)"/.exec(a) || [])[1]]);
  assert.deepEqual(got, [['600000', 'h_mon.graph_win_10m'], ['3600000', 'h_mon.graph_win_1h'],
    ['14400000', 'h_mon.graph_win_4h'], ['86400000', 'h_mon.graph_win_24h']]);
  for (const l of ['nl', 'de', 'el']) for (const k of ['graph_win_10m', 'graph_win_1h', 'graph_win_4h', 'graph_win_24h']) {
    assert.ok(PACK(l).h_mon[k], l + ' has ' + k);
  }
});

test('the window comes from the data attribute, whatever the chip text says', () => {
  const win = new Function(fn('graphChipWindowMs') + '\nreturn graphChipWindowMs;')();
  assert.equal(win({ getAttribute: (k) => (k === 'data-win-ms' ? '86400000' : null), textContent: '24 ώρες' }), 86400000);
  assert.equal(win({ getAttribute: (k) => (k === 'data-win-ms' ? '600000' : null), textContent: '10 λεπ.' }), 600000);
  assert.equal(win({ getAttribute: () => null, textContent: '' }), 3600000, 'a chip without the attribute falls back to 1 h');
});

test('temperature axis labels read °C', () => {
  const f = new Function(fn('graphAxisTemp') + '\nreturn graphAxisTemp;')();
  assert.equal(f(60), '60°C'); assert.equal(f(29.6), '30°C'); assert.equal(f(-5.2), '-5°C');
  assert.match(SRC, /textContent = graphAxisTemp\(a\[1\]\)/, 'renderGraph uses it for gAxHi/gAxMid/gAxLo');
});
