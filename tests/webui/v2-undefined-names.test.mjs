// TASK-1769: parser-based gate for undefined names in v2.js.
// beta.166 shipped esc() calls outside the functions that define esc: a ReferenceError
// on the SAT page that no static regex test caught. This test parses v2.js with acorn,
// runs eslint-scope's scope analysis and fails on every name that resolves to no
// declaration and is not a known browser global (the allowlist below).
//
// Test-only dependencies (tests/webui/package.json, pinned): install once with
//   npm ci --prefix tests/webui
// OTGW_V2_JS points at another copy of v2.js (e.g. beta.166) to see the gate go red.
//
// Run: node --test tests/webui/v2-undefined-names.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const V2_JS = process.env.OTGW_V2_JS || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data', 'v2.js');

// Browser and language globals v2.js may use without declaring them. Add a name here
// only when it really is provided by the browser; a typo or a helper out of scope
// must stay red.
const ALLOW = new Set([
  // ECMAScript built-ins
  'Array', 'Boolean', 'Date', 'Error', 'Infinity', 'Intl', 'JSON', 'Math', 'Number', 'Object',
  'Promise', 'Set', 'String', 'undefined', 'isFinite', 'isNaN', 'parseFloat', 'parseInt',
  'encodeURIComponent', 'unescape',
  // Window / DOM
  'window', 'document', 'location', 'navigator', 'localStorage', 'sessionStorage',
  'getComputedStyle', 'requestAnimationFrame', 'confirm', 'btoa',
  'setTimeout', 'clearTimeout', 'setInterval', 'clearInterval',
  // Web APIs
  'fetch', 'AbortController', 'Blob', 'FormData', 'Image', 'URL', 'WebSocket', 'XMLSerializer',
]);

function loadDeps() {
  const req = createRequire(path.join(HERE, 'package.json'));
  try {
    return { acorn: req('acorn'), scope: req('eslint-scope') };
  } catch (e) {
    throw new Error('test dependencies missing: run "npm ci --prefix tests/webui" (' + e.message + ')');
  }
}

function unresolvedNames(src) {
  const { acorn, scope } = loadDeps();
  const ast = acorn.parse(src, { ecmaVersion: 'latest', sourceType: 'script', ranges: true, locations: true });
  const sm = scope.analyze(ast, { ecmaVersion: 2022, sourceType: 'script' });
  const found = new Map();
  for (const ref of sm.globalScope.through) {
    const name = ref.identifier.name;
    if (!found.has(name)) found.set(name, ref.identifier.loc.start.line);
  }
  return found;
}

test('v2.js uses no undeclared names beyond the browser-global allowlist', () => {
  const found = unresolvedNames(fs.readFileSync(V2_JS, 'utf8'));
  const bad = [...found.entries()].filter(([n]) => !ALLOW.has(n)).map(([n, l]) => `${n} (line ${l})`);
  assert.deepEqual(bad, [], 'undeclared name(s) in v2.js, a ReferenceError at runtime: ' + bad.join(', '));
});

test('the allowlist holds no stale entries (each one is still used)', () => {
  const found = unresolvedNames(fs.readFileSync(V2_JS, 'utf8'));
  if (process.env.OTGW_V2_JS) return;  // an older copy may use fewer globals
  const stale = [...ALLOW].filter((n) => !found.has(n));
  assert.deepEqual(stale, [], 'allowlist names no longer used by v2.js, remove them: ' + stale.join(', '));
});
