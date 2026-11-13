// TASK-1880 (U10): the SAT Technical view shows the last PWM ON phases from /api/v2/sat/status: asked ON time,
// how long until the burner lit (or "not lit"), CH flame seconds and how the phase ended, plus the counters.
//
// Runs the REAL satRenderPwmPhases() and satKv() cut out of data/v2.js by brace matching, against a small DOM stub
// and a fixture payload shaped like the firmware's JSON (SATcontrol.ino satSendStatusJSON, pwm_phases[]).
// Point OTGW_V2_JS / OTGW_V2_HTML at an older copy to see it red.
//
// Run: node --test tests/webui/sat-pwm-phases.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { i18nEn } from './lib/i18n-en.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const V2JS = process.env.OTGW_V2_JS || path.join(DATA, 'v2.js');
const SRC = fs.readFileSync(V2JS, 'utf8');
const HTML = fs.readFileSync(process.env.OTGW_V2_HTML || path.join(DATA, 'v2.html'), 'utf8');

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

// Minimal DOM: elements with tag, className, textContent and children; textContent = '' clears the children.
function makeEl(tag) {
  const el = { tag, className: '', children: [], _text: '' };
  Object.defineProperty(el, 'textContent', {
    get() { return el._text + el.children.map((c) => c.textContent).join(''); },
    set(v) { el._text = String(v); el.children = []; },
  });
  el.appendChild = (c) => { el.children.push(c); return c; };
  return el;
}
function load() {
  const el = { satPwmPhaseKv: makeEl('div'), satPwmPhaseList: makeEl('div') };
  const { t } = i18nEn(SRC);
  const body = 'var document = { getElementById: function (id) { return el[id] || null; }, createElement: mk };\n' +
    extractFunction(SRC, 'satKv') + '\n' + extractFunction(SRC, 'satRenderPwmPhases') + '\n' +
    'return { satRenderPwmPhases: satRenderPwmPhases };';
  return { el, f: new Function('el', 'mk', 't', body)(el, makeEl, t) };
}
const kvRows = (box) => box.children.map((r) => [r.children[0].textContent, r.children[1].textContent]);
const tableRows = (box) => box.children[0].children.map((tr) => tr.children.map((c) => c.textContent));

// Fixture: gruijter-like phases, newest first; the second never lit (the Q-P2 case), the first lit after 22 s.
const FIXTURE = {
  pwm_phases_total: 3, pwm_phases_ignited: 2, pwm_phases_not_ignited: 1,
  pwm_ignition_delay_max_s: 22, pwm_ignition_delay_avg_s: 11.0,
  pwm_phases: [
    { age_s: 600, cmd_on_s: 446, final_on_s: 350, len_s: 390, ignited: true, ignition_delay_s: 22, flame_on_s: 368, end: 'off' },
    { age_s: 1800, cmd_on_s: 2400, final_on_s: 2400, len_s: 300, ignited: false, ignition_delay_s: null, flame_on_s: 0, end: 'cut' },
    { age_s: 3000, cmd_on_s: 300, final_on_s: 290, len_s: 300, ignited: true, ignition_delay_s: 0, flame_on_s: 300, end: 'off' },
  ],
};

test('the card sits in the Technical layer with its own title', () => {
  const l3 = HTML.slice(HTML.indexOf('<div class="sat-l3">'));
  assert.match(l3, /<h4 data-i18n="h_mon\.sat_pwm_phases">PWM ON phases: asked vs done<\/h4><div class="kv" id="satPwmPhaseKv"><\/div><div id="satPwmPhaseList"><\/div>/);
  assert.equal(i18nEn(SRC).i18nString('h_mon.sat_pwm_phases'), 'PWM ON phases: asked vs done');
  assert.match(SRC, /satKv\('satPwmKv', \[[\s\S]*?\]\);\s*satRenderPwmPhases\(d\);/, 'renderSatPage calls it with the status');
});

test('counters: phases, never lit, ignition delay avg / max', () => {
  const { el, f } = load();
  f.satRenderPwmPhases(FIXTURE);
  assert.deepEqual(kvRows(el.satPwmPhaseKv), [
    ['PWM ON phases since boot', '3'], ['Never lit', '1'], ['Ignition delay (avg / max)', '11 s / 22 s'],
  ]);
});

test('one row per phase, newest first; a phase that never lit reads "not lit" and "cut short"', () => {
  const { el, f } = load();
  f.satRenderPwmPhases(FIXTURE);
  assert.deepEqual(tableRows(el.satPwmPhaseList), [
    ['Started', 'Asked ON', 'Lit after', 'Flame on', 'End'],
    ['10 min ago', '446 s', '22 s', '368 s', 'ON time done'],
    ['30 min ago', '2400 s', 'not lit', '0 s', 'cut short'],
    ['50 min ago', '300 s', '0 s', '300 s', 'ON time done'],
  ]);
  assert.equal(el.satPwmPhaseList.children[0].className, 'stats', 'the design-system table');
});

test('no phase yet, or a firmware without the fields: a note and dashes, no table', () => {
  for (const d of [{ pwm_phases_total: 0, pwm_phases_ignited: 0, pwm_phases_not_ignited: 0, pwm_ignition_delay_max_s: 0, pwm_ignition_delay_avg_s: null, pwm_phases: [] }, {}]) {
    const { el, f } = load();
    f.satRenderPwmPhases(d);
    assert.equal(el.satPwmPhaseList.textContent, 'No PWM ON phase since boot');
    assert.equal(kvRows(el.satPwmPhaseKv)[2][1], '— / —');
  }
});

test('a second render replaces the rows instead of adding to them', () => {
  const { el, f } = load();
  f.satRenderPwmPhases(FIXTURE);
  f.satRenderPwmPhases({ ...FIXTURE, pwm_phases: FIXTURE.pwm_phases.slice(0, 1) });
  assert.equal(tableRows(el.satPwmPhaseList).length, 2);
  assert.equal(el.satPwmPhaseList.children.length, 1);
});
