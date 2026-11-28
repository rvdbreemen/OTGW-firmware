// TASK-2927 guard: the settings help popover shows the language pack's text ('settings.help.<key>') when the active pack has it,
// and falls back to the English text of settingsHelp.h per key when it has not. Before, helpFor() returned the English text of
// settingsHelp.h whatever the language, so 59 popups stayed English under German, Dutch and Greek labels.
//
// Runs the REAL helpFor() cut out of data/v2.js; setT() is the pack lookup stub (the real one is i18nString-based, see settings-help.test.mjs
// for the full DOM path). OTGW_V2_JS=<older v2.js> runs the old code (red).
// Run: node --test tests/webui/settings-help-translated-2927.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const V2JS = process.env.OTGW_V2_JS || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data', 'v2.js');
const SRC = fs.readFileSync(V2JS, 'utf8');

function fn(name) {
  const start = SRC.indexOf('function ' + name + '(');
  assert.ok(start >= 0, name + ' not found in ' + V2JS);
  let i = SRC.indexOf('{', start), depth = 0;
  for (; i < SRC.length; i++) {
    const c = SRC[i];
    if (c === "'" || c === '"') { for (i++; i < SRC.length && SRC[i] !== c; i++) if (SRC[i] === '\\') i++; }
    else if (c === '{') depth++; else if (c === '}' && --depth === 0) return SRC.slice(start, i + 1);
  }
  throw new Error('unbalanced');
}

function helpWith(pack, english) {
  const body = 'var SET_META = {}, SETTINGS_HELP = ENGLISH;\nfunction satPwmHelp() { return "pwm"; }\n' + fn('helpFor') + '\nreturn helpFor;';
  const setT = (key, fb) => (key in pack ? pack[key] : fb);
  return new Function('ENGLISH', 'setT', body)(english, setT);
}
const EN = { hostname: 'Network name of this gateway.', satsource: 'Appliance type.' };

test('no pack text: the English help text of settingsHelp.h', () => {
  const h = helpWith({}, EN);
  assert.equal(h('hostname'), EN.hostname);
});

test('a pack text for the key wins over the English text', () => {
  const h = helpWith({ 'settings.help.hostname': 'Netzwerkname dieses Gateways.' }, EN);
  assert.equal(h('hostname'), 'Netzwerkname dieses Gateways.');
});

test('per key fallback: a key the pack lacks stays English, a key it has is translated', () => {
  const h = helpWith({ 'settings.help.hostname': 'Netwerknaam van deze gateway.' }, EN);
  assert.equal(h('hostname'), 'Netwerknaam van deze gateway.');
  assert.equal(h('satsource'), EN.satsource);
});

test('a key without any English or pack text gives the empty string (not known)', () => {
  assert.equal(helpWith({}, EN)('nosuchkey'), '');
});
