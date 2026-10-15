// TASK-1233 regression guard: the v2 1-Wire discovery toast must fire for a newly
// seen Dallas sensor, and stay silent for a known one.
//
// The bug: pollDallasDiscovery() read sensors.dallas[] while GET /api/v2/sensors
// returns sensors.devices{} keyed by address (restAPI.ino sendSensorStatus), so no
// card ever appeared.
//
// This test runs the REAL shipped function: it cuts the source text of
// pollDallasDiscovery out of data/v2.js and evaluates it with stubbed fetch,
// discoverSeenSet and showDiscoverCard. Nothing is re-implemented here. Point
// OTGW_V2_JS at an older copy of v2.js to reproduce the old behaviour (the test
// then fails on "fires for a new sensor").
//
// Run: node --test tests/webui/dallas-discovery-toast.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const V2JS = process.env.OTGW_V2_JS || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data', 'v2.js');

// Body shape built from restAPI.ino sendSensorStatus(): root {sensors:{dallas_*,
// simulated, devices:{<16-hex addr>:{temp, epoch}}, s0:{...}}}.
const SENSORS_BODY = {
  sensors: {
    dallas_enabled: true, dallas_detected: true, dallas_count: 2,
    dallas_gpio: 10, dallas_poll_sec: 20, simulated: false,
    devices: {
      '28FF64D1841703F1': { temp: 21.4375, epoch: 1760536800 },
      '28AA11223344556F': { temp: 48.5, epoch: 1760536800 },
    },
    s0: { enabled: false, gpio: 12, poll_sec: 60, pulses: 0, total: 0, power_kw: 0, epoch: 0 },
  },
};

// Cut "function <name>(...) { ... }" out of the source by brace matching,
// skipping string literals and comments.
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

async function runPoll(body, seenAddrs) {
  const src = extractFunction(fs.readFileSync(V2JS, 'utf8'), 'pollDallasDiscovery');
  const cards = [];
  const seen = {};
  seenAddrs.forEach((a) => { seen[a] = 1; });
  const fetchStub = async () => ({ ok: true, status: 200, json: async () => JSON.parse(JSON.stringify(body)) });
  // pushGraphSlot / gDallas: the same poll also feeds the Monitor graph (TASK-1226).
  const make = new Function('fetch', 'APIGW', 'discoverSeenSet', 'showDiscoverCard', 'setTimeout', 'pushGraphSlot',
    'var gDallas = null;\n' + src + '\nreturn pollDallasDiscovery;');
  const poll = make(fetchStub, '/api/', () => seen, (o) => cards.push(o), () => 0, () => {});
  poll(0);
  for (let k = 0; k < 10; k++) await new Promise((r) => setImmediate(r));   // drain the promise chain
  return cards;
}

test('fires a discovery card for each newly seen 1-Wire sensor', async () => {
  const cards = await runPoll(SENSORS_BODY, []);
  assert.equal(cards.length, 2, 'expected one card per new address, got ' + JSON.stringify(cards));
  assert.deepEqual(cards.map((c) => c.id).sort(), ['28AA11223344556F', '28FF64D1841703F1']);
  const c = cards.find((x) => x.id === '28FF64D1841703F1');
  assert.equal(c.bus, 'dallas');
  assert.match(c.sub, /21\.4°C/);
});

test('stays silent for a sensor already seen', async () => {
  const cards = await runPoll(SENSORS_BODY, ['28FF64D1841703F1']);
  assert.deepEqual(cards.map((c) => c.id), ['28AA11223344556F']);
});

test('no devices object (no sensor detected) means no card and no throw', async () => {
  const body = JSON.parse(JSON.stringify(SENSORS_BODY));
  delete body.sensors.devices;
  assert.deepEqual(await runPoll(body, []), []);
});
