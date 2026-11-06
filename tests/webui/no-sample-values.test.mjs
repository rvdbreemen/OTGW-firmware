// TASK-1670 regression guard: the web UI never shows an invented value when data is
// missing (Robert 2026-11-06: "Verzin geen waarden in UI als de temperaturen er niet
// zijn. Laat dan — zien."). Two checks on the REAL shipped assets:
//   1. no live-value element in v2.html ships a sample number (the mockup texts such as
//      "ΔT 14.2°" stayed on screen until data arrived, or for good when it never did);
//   2. every ΔT render in v2.js falls back to "—" when flow or return is unknown, so a
//      value seen once is not left standing after it goes away.
// OTGW_DATA_DIR=<dir> reads another data dir (old-vs-fix).
//
// Run: node tests/webui/no-sample-values.test.mjs
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import test from 'node:test';
import assert from 'node:assert/strict';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const html = fs.readFileSync(path.join(DATA, 'v2.html'), 'utf8');
const js = fs.readFileSync(path.join(DATA, 'v2.js'), 'utf8');

// Elements that v2.js fills with live measurements (system view, basic view, connection map).
const LIVE_IDS = ['aDhw', 'aOut', 'aLcd', 'aModBig', 'aPress', 'aFlow', 'aRet', 'aDt', 'aRoom',
  'aRoomSet', 'bRoom', 'bSet', 'bFlow', 'bRet', 'bDt', 'bPress', 'bMod', 'bDhw', 'bOut', 'cnWifiSub'];

test('live-value elements ship no sample number in v2.html', () => {
  const bad = [];
  for (const id of LIVE_IDS) {
    const m = html.match(new RegExp('id="' + id + '"[^>]*>([^<]*)<'));
    assert.ok(m, 'element #' + id + ' not found in v2.html');
    if (/\d/.test(m[1])) bad.push('#' + id + ' "' + m[1] + '"');
  }
  assert.deepEqual(bad, [], 'sample values in markup: ' + bad.join(', '));
});

test('every ΔT render in v2.js falls back to "—"', () => {
  const lines = js.split('\n').filter(l => /txt\('(aDt|bDt)'/.test(l));
  assert.equal(lines.length, 2, 'expected one render each for #aDt and #bDt, found ' + lines.length);
  for (const l of lines) {
    assert.ok(!/^\s*if\s*\(/.test(l), 'ΔT render is guarded by an if without a "—" branch: ' + l.trim());
    assert.ok(l.includes("'—'"), 'ΔT render has no "—" fallback: ' + l.trim());
  }
});
