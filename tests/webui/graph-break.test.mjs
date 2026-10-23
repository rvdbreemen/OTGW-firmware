// TASK-1296 guard: the Monitor > Graph must not draw a line across a hole in the data.
// Two consecutive points of a series more than GBREAK_MS (150 s) apart start a new
// subpath; a lane must not hold its value across such a gap. History slots are 120 s
// apart, which must stay ONE segment, and a missing history slot (240 s) must break.
//
// Runs the REAL shipped graphPath() cut out of data/v2.js, with GBREAK_MS read from the
// shipped source. Older v2.js has no graphPath, so extraction fails (red).
//
// Not covered: the browser painting of the <path> elements and the PNG export of them
// (bench screenshot; graph-time-axis-and-png-export.test.mjs covers the export plumbing).
//
// Run: node --test tests/webui/graph-break.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const V2JS = process.env.OTGW_V2_JS || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data', 'v2.js');
const SRC = fs.readFileSync(V2JS, 'utf8');

function extractFunction(src, name) {
  const start = src.indexOf('function ' + name + '(');
  if (start < 0) throw new Error(name + ' not found in ' + V2JS);
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

const GBREAK_MS = Number(SRC.match(/GBREAK_MS = (\d+)/)[1]);
const graphPath = new Function('GBREAK_MS', extractFunction(SRC, 'graphPath') + '\nreturn graphPath;')(GBREAK_MS);
const y = v => 170 - v;                 // any monotone mapping will do
const T0 = 1_800_000_000_000;
const segs = d => (d.match(/M/g) || []).length;

// points: [x, value, t]
const pts = (...ts) => ts.map((t, i) => [i * 10, 50 + i, T0 + t]);

test('shipped break threshold is 150 s', () => assert.equal(GBREAK_MS, 150000));

test('temperature line: 120 s spacing is one segment, 240 s makes two', () => {
  assert.equal(segs(graphPath(pts(0, 120000, 240000), 't', y)), 1);
  assert.equal(segs(graphPath(pts(0, 120000, 360000, 480000), 't', y)), 2);
  assert.equal(segs(graphPath(pts(0, 5000, 10000), 't', y)), 1);
});

test('exactly 150 s apart still joins, 150.005 s breaks', () => {
  assert.equal(segs(graphPath(pts(0, 150000), 't', y)), 1);
  assert.equal(segs(graphPath(pts(0, 155000), 't', y)), 2);
});

test('the break is between the right points', () => {
  const d = graphPath(pts(0, 5000, 400000, 405000), 't', y);
  assert.equal(d, 'M0,120.0 L10,119.0 M20,118.0 L30,117.0');
});

test('modulation area: each segment is closed on its own baseline', () => {
  const d = graphPath(pts(0, 5000, 400000, 405000).map(p => [p[0], 50, p[2]]), 'm', y);
  assert.equal(segs(d), 2);
  assert.equal((d.match(/Z/g) || []).length, 2);
  assert.match(d, /^M0,170 L0,150\.0 L10,150\.0 L10,170 Z M20,170 /);
});

test('lane does not hold its value across a gap', () => {
  const c = [[0, 1, T0], [10, 0, T0 + 5000], [20, 1, T0 + 400000], [30, 1, T0 + 405000]];
  const d = graphPath(c, 'l', y, 218);
  assert.equal(segs(d), 2);
  assert.equal(d, 'M0,204 L10,204 L10,218 M20,204 L30,204 L30,204');   // the stair step repeats a point, as before
  // no horizontal run from x=10 (before the gap) to x=20 (after it)
  assert.ok(!/10,218 L20/.test(d));
});

test('a single point is a segment of its own and nothing is invented', () => {
  assert.equal(graphPath(pts(0), 't', y), 'M0,120.0');
  assert.equal(graphPath([], 't', y), '');
});

// TASK-1296 review round 2: a device history point stands for data up to t + span (its slot
// interval), so the gap is measured from the END of its data. Without this, a 120 s history
// slot followed by live data 230 s after the slot START (115 s after its end) broke the line.
test('a history point (span = its interval) is joined to a point up to GBREAK_MS after its end', () => {
  const h = (x, v, t) => [x, v, T0 + t, 120000];          // device slot of 120 s
  assert.equal(segs(graphPath([h(0, 50, 0), [10, 51, T0 + 230000]], 't', y)), 1, '115 s after the slot end: joined');
  assert.equal(segs(graphPath([h(0, 50, 0), [10, 51, T0 + 275000]], 't', y)), 2, '155 s after the slot end: break');
  assert.equal(segs(graphPath([h(0, 50, 0), h(10, 51, 120000), h(20, 52, 240000)], 't', y)), 1, 'consecutive slots');
  assert.equal(segs(graphPath([[0, 50, T0], [10, 51, T0 + 230000]], 't', y)), 2, 'live points have no span');
});
