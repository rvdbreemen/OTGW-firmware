// TASK-1689 (ADR-219/220/221): the web UI language mechanism in data/v2.js.
// Runs the shipped functions (cut out of v2.js by brace matching): language choice
// from navigator.languages with the localStorage override, English as the per-key
// fallback, {{name}} placeholders, CLDR plurals, and loading a pack served gzipped
// with Content-Encoding: gzip by a local HTTP server (node's fetch inflates it, as
// the browser does). Also checks every i18n/*.json pack against the English keys.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import http from 'node:http';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const V2JS = process.env.OTGW_V2_JS || path.join(ROOT, 'src', 'OTGW-firmware', 'data', 'v2.js');
const SRC = fs.readFileSync(V2JS, 'utf8');

function balanced(src, start, what) {
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
  throw new Error('unbalanced braces in ' + what);
}
function extractFunction(src, name) {
  const start = src.indexOf('function ' + name + '(');
  if (start < 0) throw new Error(name + ' not found in ' + V2JS);
  return balanced(src, start, name);
}
function extractVar(src, name) {
  const start = src.indexOf('var ' + name + ' = {');
  if (start < 0) throw new Error(name + ' not found in ' + V2JS);
  return balanced(src, start, name) + ';';
}

// A fresh copy of the shipped helpers; base is the pack server origin for the relative fetch.
function load(base) {
  const body =
    extractVar(SRC, 'I18N_EN') + '\n' +
    "var i18nLang = 'en', i18nPack = null;\n" +
    'function fetch(u, o) { return globalThis.fetch(new URL(u, BASE), o); }\n' +
    ['i18nPickLang', 'i18nLookup', 'i18nString', 't', 'loadI18nPack'].map(n => extractFunction(SRC, n)).join('\n') + '\n' +
    'return { I18N_EN: I18N_EN, pick: i18nPickLang, t: t,' +
    ' setPack: function (lang, p) { i18nLang = lang; i18nPack = p; },' +
    ' lang: function () { return i18nLang; },' +
    ' load: function (lang, tries) { return new Promise(function (res) { loadI18nPack(lang, res, tries); }); } };';
  return new Function('BASE', 'setTimeout', body)(base || 'http://127.0.0.1:1/', (f) => f());
}

test('language choice: override, then navigator.languages base tag, else English', () => {
  const h = load();
  const S = ['en', 'nl', 'de'];
  assert.equal(h.pick(['fr-FR', 'it'], S, null), 'en');          // unsupported -> English
  assert.equal(h.pick([], S, null), 'en');
  assert.equal(h.pick(undefined, S, null), 'en');
  assert.equal(h.pick(['nl-BE', 'en'], S, null), 'nl');           // region tag -> base tag
  assert.equal(h.pick(['fr', 'de-AT'], S, null), 'de');           // first SUPPORTED one
  assert.equal(h.pick(['de-DE'], S, 'nl'), 'nl');                  // override wins
  assert.equal(h.pick(['de-DE'], S, 'xx'), 'de');                  // unsupported override ignored
  assert.equal(h.pick(['NL_nl'], S, ''), 'nl');                    // case and underscore
});

test('t(): pack string, per-key English fallback, missing key shows the key', () => {
  const h = load();
  assert.equal(h.t('nav.settings'), 'Settings');
  h.setPack('nl', { nav: { settings: 'Instellingen' } });
  assert.equal(h.t('nav.settings'), 'Instellingen');
  assert.equal(h.t('nav.advanced'), 'Advanced');                   // absent in the pack -> English
  h.setPack('nl', { nav: { settings: '' } });
  assert.equal(h.t('nav.settings'), 'Settings');                   // blank (Weblate untranslated) -> English
  assert.equal(h.t('no.such.key'), 'no.such.key');
  assert.equal(h.t('nav'), 'nav');                                 // an object is not a string
});

test('t(): {{name}} placeholders and CLDR plurals', () => {
  const h = load();
  h.setPack('nl', {
    x: { hello: 'Hallo {{ name }}, {{name}}', left: '{{unknown}} blijft' },
    files_one: '{{count}} bestand', files_other: '{{count}} bestanden'
  });
  assert.equal(h.t('x.hello', { name: 'Marvin' }), 'Hallo Marvin, Marvin');
  assert.equal(h.t('x.left', {}), '{{unknown}} blijft');
  assert.equal(h.t('files', { count: 1 }), '1 bestand');
  assert.equal(h.t('files', { count: 42 }), '42 bestanden');
  assert.equal(h.t('files', { count: 0 }), '0 bestanden');
});

test('a pack served gzipped is loaded; a failing fetch keeps English', async () => {
  const pack = JSON.stringify({ nav: { settings: 'Einstellungen' } });
  const gz = zlib.gzipSync(Buffer.from(pack));
  const seen = [];
  const server = http.createServer((req, res) => {
    seen.push(req.url);
    if (req.url === '/i18n/de.json') {
      res.writeHead(200, { 'Content-Type': 'application/json', 'Content-Encoding': 'gzip' });
      res.end(gz);
    } else {
      res.writeHead(503); res.end();
    }
  });
  await new Promise(r => server.listen(0, '127.0.0.1', r));
  const base = 'http://127.0.0.1:' + server.address().port + '/';
  try {
    const h = load(base);
    assert.equal(await h.load('de'), true);
    assert.equal(h.lang(), 'de');
    assert.equal(h.t('nav.settings'), 'Einstellungen');
    assert.equal(h.t('nav.monitor'), 'Monitor');

    const g = load(base);
    assert.equal(await g.load('nl'), false);                        // 503 twice -> English stays
    assert.equal(g.lang(), 'en');
    assert.equal(g.t('nav.settings'), 'Settings');
    assert.equal(seen.filter(u => u === '/i18n/nl.json').length, 2); // one retry, no loop

    const e = load(base);
    assert.equal(await e.load('en'), true);                         // English needs no fetch
    assert.equal(seen.length, 3);
  } finally {
    server.close();
  }
});

// ADR-219/220: every pack has every English key, with the same {{placeholders}}, and no key
// that English lacks (a typo would otherwise never show). Plural keys follow _one/_other.
function flatten(obj, prefix, out) {
  for (const [k, v] of Object.entries(obj)) {
    const key = prefix ? prefix + '.' + k : k;
    if (v && typeof v === 'object') flatten(v, key, out);
    else out[key] = v;
  }
  return out;
}
const holders = s => [...String(s).matchAll(/\{\{\s*(\w+)\s*\}\}/g)].map(m => m[1]).sort().join(',');

test('i18n packs cover the English keys with the same placeholders', () => {
  const en = flatten(load().I18N_EN, '', {});
  const dir = path.join(ROOT, 'i18n');
  const packs = fs.readdirSync(dir).filter(f => f.endsWith('.json'));
  assert.deepEqual(packs.sort(), ['de.json', 'nl.json']);
  for (const file of packs) {
    const pack = flatten(JSON.parse(fs.readFileSync(path.join(dir, file), 'utf8')), '', {});
    for (const key of Object.keys(en)) {
      assert.ok(typeof pack[key] === 'string' && pack[key] !== '', file + ' misses ' + key);
      assert.equal(holders(pack[key]), holders(en[key]), file + ' placeholders of ' + key);
    }
    for (const key of Object.keys(pack)) assert.ok(key in en, file + ' has ' + key + ', English does not');
  }
});
