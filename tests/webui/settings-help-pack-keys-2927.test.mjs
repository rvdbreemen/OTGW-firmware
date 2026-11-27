// TASK-2927 guard: the language packs carry a translated help text (settings.help.<key>) for every key of the English help
// (src/OTGW-firmware/settingsHelp.h), with the same numbers, no orphan keys, no dashes and a length that fits the 320 px popover.
// A key a pack lacks falls back to English per key at run time (helpFor), so this test is the completeness gate: it fails when a new English
// help key lands without a translation in nl, de and el.
//
// Run: node --test tests/webui/settings-help-pack-keys-2927.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const hdr = fs.readFileSync(path.join(ROOT, 'src', 'OTGW-firmware', 'settingsHelp.h'), 'utf8');
const a = hdr.indexOf('R"json(') + 7;
const EN = JSON.parse(hdr.slice(a, hdr.indexOf(')json"', a)));
const OLD = new Set(['satboilerratedkw', 'satmanufacturer', 'satsource']);   // translated before TASK-2927 (SET_META help texts, longer than the popover limit)
const pack = (l) => JSON.parse(fs.readFileSync(path.join(ROOT, 'i18n', l + '.json'), 'utf8'));

for (const l of ['nl', 'de', 'el']) {
  test(`${l}: every English help key has a translation, no orphans, numbers kept, no dashes, at most 300 characters`, () => {
    const help = pack(l).settings.help;
    const missing = Object.keys(EN).filter((k) => typeof help[k] !== 'string' || !help[k].trim());
    assert.deepEqual(missing, [], `${l}: help keys without a translation`);
    assert.deepEqual(Object.keys(help).filter((k) => !(k in EN)), [], `${l}: help keys English does not have`);
    for (const k of Object.keys(EN)) {
      const t = help[k];
      if (!OLD.has(k)) assert.ok(t.length <= 300, `${l}.${k}: ${t.length} characters`);
      assert.ok(!/[–—]/.test(t), `${l}.${k}: a dash`);
      for (const d of new Set(EN[k].match(/\d+/g) || [])) assert.ok(t.includes(d), `${l}.${k}: the number ${d} of the English is missing`);
    }
  });
  test(`${l}: pack size is measured (gzip, the LittleFS image holds it gzipped)`, () => {
    const gz = zlib.gzipSync(fs.readFileSync(path.join(ROOT, 'i18n', l + '.json'))).length;
    console.log(`# ${l}.json.gz ${gz} bytes`);
    assert.ok(gz < 64 * 1024, `${l}.json.gz is ${gz} bytes`);   // measured and recorded (AC3); the hard fit check is the LittleFS free space at build time
  });
}
