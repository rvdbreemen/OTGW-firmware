// TASK-1257 regression guard: SAT "Detect location" on plain HTTP.
//
// Browsers only allow navigator.geolocation in a secure context (HTTPS or
// localhost). The device serves plain HTTP, so the button always ended in a bare
// "Location error". The fix hides the button on a non-secure context and shows a
// hint to the manual Latitude/Longitude fields, keeps it working on a secure
// context (HTTPS reverse proxy), and names the reason when geolocation fails.
//
// This test runs the REAL shipped functions: it cuts satGeoUsable,
// satDetectLocation, satWireDetectLocation and the SAT_GEO_ERR table out of
// data/v2.js and evaluates them with stubbed window/navigator/document. Point
// OTGW_V2_JS at an older copy of v2.js to reproduce the old behaviour: code
// without SAT_GEO_ERR / satGeoUsable / satWireDetectLocation gets stand-ins that
// mirror the old wiring (always attach the click handler), so the old
// satDetectLocation runs and the tests fail on their assertions.
//
// Run: node --test tests/webui/sat-detect-location.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const V2JS = process.env.OTGW_V2_JS || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data', 'v2.js');

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

function extractVarLine(src, name) {
  const m = src.match(new RegExp('^\\s*var ' + name + ' = .*;$', 'm'));
  if (!m) throw new Error('var ' + name + ' not found in ' + V2JS);
  return m[0];
}

// Old v2.js (before TASK-1257) has none of these; stand in with its behaviour.
const OLD_FALLBACK = {
  SAT_GEO_ERR: 'var SAT_GEO_ERR = null;',
  satGeoUsable: 'function satGeoUsable() { return !!navigator.geolocation; }',
  satWireDetectLocation: "function satWireDetectLocation() { var dl = document.getElementById('satDetectLoc'); if (dl) dl.addEventListener('click', satDetectLocation); }",
};
function optional(fn, src, name) {
  try { return fn(src, name); } catch (e) { if (/not found/.test(e.message)) return OLD_FALLBACK[name]; throw e; }
}

function el() {
  return {
    hidden: false, textContent: '', listeners: {},
    addEventListener(ev, fn) { this.listeners[ev] = fn; },
  };
}

// Build the functions against a stubbed browser. secure: window.isSecureContext;
// geo: navigator.geolocation stub (or undefined).
function load({ secure, geo }) {
  const src = fs.readFileSync(V2JS, 'utf8');
  const body = [
    optional(extractVarLine, src, 'SAT_GEO_ERR'),
    optional(extractFunction, src, 'satGeoUsable'),
    extractFunction(src, 'satDetectLocation'),
    optional(extractFunction, src, 'satWireDetectLocation'),
    'return { satGeoUsable, satDetectLocation, satWireDetectLocation };',
  ].join('\n');
  const dom = { satDetectLoc: el(), satLocHint: Object.assign(el(), { hidden: true }), satCoords: el() };
  const posts = [];
  const fetchStub = async (url, opts) => { posts.push(JSON.parse(opts.body)); return { ok: true, status: 200 }; };
  const make = new Function('window', 'navigator', 'document', 'txt', 'fetch', 'APIGW', 'setTimeout', 'fetchSatPageWeather', body);
  const fns = make(
    { isSecureContext: secure },
    { geolocation: geo },
    { getElementById: (id) => dom[id] || null },
    (id, t) => { if (dom[id]) dom[id].textContent = t; },
    fetchStub, '/api/', () => 0, () => {});
  return { fns, dom, posts };
}

const drain = async () => { for (let k = 0; k < 10; k++) await new Promise((r) => setImmediate(r)); };

test('insecure context (plain HTTP): button hidden, hint names the manual fields', () => {
  const geo = { getCurrentPosition() { throw new Error('must not be called'); } };
  const { fns, dom } = load({ secure: false, geo });
  fns.satWireDetectLocation();
  assert.equal(dom.satDetectLoc.hidden, true, 'button must be hidden on plain HTTP');
  assert.equal(dom.satDetectLoc.listeners.click, undefined, 'no click handler on a button that cannot work');
  assert.equal(dom.satLocHint.hidden, false, 'hint must be shown');
  assert.match(dom.satLocHint.textContent, /HTTPS/);
  assert.match(dom.satLocHint.textContent, /Latitude and Longitude under SAT settings > Weather/);
});

test('geolocation API missing: same hint, button hidden', () => {
  const { fns, dom } = load({ secure: true, geo: undefined });
  fns.satWireDetectLocation();
  assert.equal(dom.satDetectLoc.hidden, true);
  assert.equal(dom.satLocHint.hidden, false);
});

test('secure context (HTTPS proxy): button stays, click detects and saves lat then lon', async () => {
  const geo = { getCurrentPosition(ok) { ok({ coords: { latitude: 52.0907, longitude: 5.1214 } }); } };
  const { fns, dom, posts } = load({ secure: true, geo });
  fns.satWireDetectLocation();
  assert.equal(dom.satDetectLoc.hidden, false, 'button must stay visible on HTTPS');
  assert.equal(dom.satLocHint.hidden, true, 'no hint on HTTPS');
  assert.equal(typeof dom.satDetectLoc.listeners.click, 'function');
  dom.satDetectLoc.listeners.click();
  await drain();
  assert.deepEqual(posts, [{ name: 'SATweatherlat', value: '52.0907' }, { name: 'SATweatherlon', value: '5.1214' }]);
  assert.equal(dom.satCoords.textContent, '52.0907, 5.1214');
});

for (const [code, re] of [[1, /denied/i], [2, /unavailable/i], [3, /timed out/i]]) {
  test('geolocation error code ' + code + ' explains why', () => {
    const geo = { getCurrentPosition(ok, fail) { fail({ code }); } };
    const { fns, dom } = load({ secure: true, geo });
    fns.satDetectLocation();
    assert.match(dom.satCoords.textContent, re);
    assert.notEqual(dom.satCoords.textContent, 'Location error');
  });
}

test('insecure context: a direct call reports HTTPS, never calls geolocation', () => {
  let called = false;
  const geo = { getCurrentPosition() { called = true; } };
  const { fns, dom } = load({ secure: false, geo });
  fns.satDetectLocation();
  assert.equal(called, false);
  assert.match(dom.satCoords.textContent, /HTTPS/);
});
