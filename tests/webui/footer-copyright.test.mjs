// TASK-1838 (Robert 2026-11-12): the page footer "OTGW firmware · v2 Web UI · live from the OpenTherm bus"
// is replaced by the 1.x footer line "© 2021-2026 Robert van den Breemen" (v1.7.9 index.html), the same in
// every language; the old text and its translations are gone.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const DATA = process.env.OTGW_DATA_DIR || path.join(ROOT, 'src', 'OTGW-firmware', 'data');
const HTML = fs.readFileSync(path.join(DATA, 'v2.html'), 'utf8');
const SRC = fs.readFileSync(path.join(DATA, 'v2.js'), 'utf8');

test('the page footer is the 1.x copyright line', () => {
  const m = /<div class="foot"(?! id=)[^>]*>([^<]*)<\/div>/.exec(HTML);
  assert.ok(m, 'page footer present');
  assert.equal(m[1], '© 2021-2026 Robert van den Breemen');
  assert.doesNotMatch(m[0], /data-i18n/, 'a name and a year: not translated');
});

test('the old footer text and its translation key are gone', () => {
  assert.doesNotMatch(HTML + SRC, /live from the OpenTherm bus/);
  for (const l of ['nl', 'de', 'el']) {
    const p = JSON.parse(fs.readFileSync(path.join(ROOT, 'i18n', l + '.json'), 'utf8'));
    assert.equal(p.h_set && p.h_set.foot, undefined, l + ' still has h_set.foot');
  }
});
