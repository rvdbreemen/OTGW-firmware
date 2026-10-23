// TASK-1297 guard: the Monitor > Graph pointer readout. Runs the REAL graphReadout() and gVal()
// cut out of data/v2.js by brace matching. Point OTGW_V2_JS at an older v2.js to see it red.
// Not covered: pointer/DOM handling and the tooltip placement (bench screenshots).
// Run: node --test tests/webui/graph-readout.test.mjs
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

const mb = SRC.match(/\bGBREAK_MS\s*=\s*(\d+)/);
const GBREAK_MS = mb ? Number(mb[1]) : undefined;
const readout = new Function('GBREAK_MS',
  extractFunction(SRC, 'gVal') + '\n' + extractFunction(SRC, 'graphReadout') + '\nreturn graphReadout;')(GBREAK_MS);

const DEG = '°C';
const DASH = '–';
const keys = ['flow', 'mod', 'flame', 'd:28AA'];
const metas = [['flow', 'boiler', '#f00', 't'], ['mod', 'modulation', '#0bf', 'm'],
  ['flame', 'flame', '#f70', 'l'], ['d:28AA', 'attic', '#5dd', 't']];
const rows = (r) => r.rows.map(x => x[0] + '=' + x[1]);

test('picks the last record at or before t', () => {
  const arr = [{ t: 1000, flow: 40, mod: 10, flame: 0 }, { t: 6000, flow: 50, mod: 20, flame: 1 }];
  assert.equal(readout(arr, keys, metas, {}, 5999).t, 1000);
  assert.equal(readout(arr, keys, metas, {}, 6000).t, 6000);
  assert.equal(readout(arr, keys, metas, {}, 9000).t, 6000);
});

test('t before the first record -> null; empty array -> null', () => {
  assert.equal(readout([{ t: 1000, flow: 1 }], keys, metas, {}, 999), null);
  assert.equal(readout([], keys, metas, {}, 5), null);
});

test('gap beyond GBREAK_MS -> null, inside it -> record', () => {
  const arr = [{ t: 1000, flow: 40 }];
  assert.ok(readout(arr, keys, metas, {}, 1000 + GBREAK_MS));
  assert.equal(readout(arr, keys, metas, {}, 1000 + GBREAK_MS + 1), null);
});

test('history record span is honoured', () => {
  const arr = [{ t: 1000, span: 120000, flow: 40 }];
  assert.ok(readout(arr, keys, metas, {}, 1000 + 200000));
  assert.equal(readout(arr, keys, metas, {}, 1000 + 120000 + GBREAK_MS + 1), null);
});

test('hidden series are omitted, order kept', () => {
  const arr = [{ t: 1000, flow: 40, mod: 10, flame: 1, d: { '28AA': 18 } }];
  assert.deepEqual(rows(readout(arr, keys, metas, {}, 1000)),
    ['boiler=40.0 ' + DEG, 'modulation=10 %', 'flame=on', 'attic=18.0 ' + DEG]);
  assert.deepEqual(rows(readout(arr, keys, metas, { mod: 1, 'd:28AA': 1 }, 1000)),
    ['boiler=40.0 ' + DEG, 'flame=on']);
});

test('formatting per kind and missing values', () => {
  const arr = [{ t: 1000, flow: 21.46, mod: 41.6, flame: 0 }];
  assert.deepEqual(rows(readout(arr, keys, metas, {}, 1000)),
    ['boiler=21.5 ' + DEG, 'modulation=42 %', 'flame=off', 'attic=' + DASH]);
  const bad = [{ t: 1000, flow: null, mod: NaN, flame: 1, d: { '28AA': undefined } }];
  assert.deepEqual(rows(readout(bad, keys, metas, {}, 1000)),
    ['boiler=' + DASH, 'modulation=' + DASH, 'flame=on', 'attic=' + DASH]);
  assert.equal(readout(arr, keys, metas, {}, 1000).rows[0][2], '#f00');
});
