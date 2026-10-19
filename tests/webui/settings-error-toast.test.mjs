// TASK-1254 guard: a failed settings save must show the red error toast, a good
// save the green one.
//
// Runs the REAL shipped bleToast() and saveSettings(): their source is cut out of
// data/v2.js by brace matching and evaluated against a stub #toast element and a
// stub fetch. The helpers saveSettings() calls that are unrelated to the toast
// (IP validation, SAT onboarding, re-render) are stubbed. The CSS check reads the
// shipped v2-bundle.css. Point OTGW_V2_JS / OTGW_V2_CSS at older copies to show
// the gap.
//
// Run: node --test tests/webui/settings-error-toast.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const V2JS = process.env.OTGW_V2_JS || path.join(DATA, 'v2.js');
const V2CSS = process.env.OTGW_V2_CSS || path.join(DATA, 'v2-bundle.css');

// Same brace-matching cutter as static-ip-validation.test.mjs.
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

function stubToast() {
  const cls = new Set();
  return {
    textContent: '',
    classList: {
      add: (c) => cls.add(c),
      remove: (c) => cls.delete(c),
      contains: (c) => cls.has(c),
      toggle: (c, on) => { const v = on === undefined ? !cls.has(c) : !!on; if (v) cls.add(c); else cls.delete(c); return v; },
    },
  };
}

// Build saveSettings() + bleToast() with the given POST status per key.
function load(statusFor) {
  const src = fs.readFileSync(V2JS, 'utf8');
  const toast = stubToast();
  const posted = [];
  // Fake timer queue: tests fire pending hide callbacks by hand.
  const timers = new Map();
  let nextId = 1;
  const env = {
    document: { getElementById: (id) => (id === 'toast' ? toast : null) },
    fetch: (url, opts) => {
      const name = JSON.parse(opts.body).name;
      posted.push(name);
      const status = statusFor(name);
      return Promise.resolve({ ok: status >= 200 && status < 300, status });
    },
    setTimeout: (fn) => { const id = nextId++; timers.set(id, fn); return id; },
    clearTimeout: (id) => { timers.delete(id); },
  };
  const body =
    'var setDirty = {}, setData = {}, APIGW = "/api/", IP_KEY_RE = /^$/;\n' +
    'function ipv4Err() { return ""; } function ipGroupsErr() { return ""; }\n' +
    'function subFor() { return ""; } function labelFor() { return ""; } function curVal() { return ""; }\n' +
    'function satOnbNeeded() { return false; } function renderSettings() {}\n' +
    'function bleAction() {} function startSatOnboarding() {}\n' +
    extractFunction(src, 'bleToast') + '\n' + extractFunction(src, 'saveSettings') + '\n' +
    'return { save: function (d) { setDirty = d; saveSettings(); }, bleToast: bleToast };';
  const api = new Function('document', 'fetch', 'setTimeout', 'clearTimeout', body)(env.document, env.fetch, env.setTimeout, env.clearTimeout);
  const fireTimers = () => { const fns = [...timers.values()]; timers.clear(); fns.forEach((f) => f()); };
  return { api, toast, posted, timers, fireTimers };
}

const settle = () => new Promise((r) => setTimeout(r, 20));

test('a rejected save shows "Save failed" in the red error variant', async () => {
  const { api, toast } = load((k) => (k === 'mqttuselegacyottopics' ? 400 : 200));
  api.save({ mqttuselegacyottopics: 'true' });
  await settle();
  assert.equal(toast.textContent, 'Save failed');
  assert.ok(toast.classList.contains('show'));
  assert.ok(toast.classList.contains('err'), 'failure toast must carry the .err class');
});

test('a good save after a failed one shows the green toast (err cleared)', async () => {
  const { api, toast } = load(() => 200);
  api.bleToast('Save failed', true);
  assert.ok(toast.classList.contains('err'));
  api.save({ hostname: 'otgw' });
  await settle();
  assert.equal(toast.textContent, 'Settings saved');
  assert.ok(!toast.classList.contains('err'), 'success toast must not carry .err');
});

test('a new toast cancels the previous toast\'s pending hide timer', () => {
  const { api, toast, timers, fireTimers } = load(() => 200);
  api.bleToast('Settings saved');
  api.bleToast('Save failed', true, 2500);
  assert.equal(timers.size, 1, 'only the newest toast may own a pending hide timer');
  assert.ok(toast.classList.contains('show'));
  fireTimers();
  assert.ok(!toast.classList.contains('show'));
});

test('v2-bundle.css defines a red .toast.err variant from a design token', () => {
  const css = fs.readFileSync(V2CSS, 'utf8');
  assert.match(css, /\.toast\.err\s*\{[^}]*background:\s*var\(--status-critical\)/);
});
