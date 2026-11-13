// TASK-1881 (U9) guard: the heating curve card shows the self-tune progress from the sat/status object curve_tune:
// counted days out of 3, clean hours today out of 6, the last daily record in words, and the hours each reason kept
// out of the window (non-zero only, largest first). Off, or a firmware without curve_tune, shows "off" or nothing.
//
// Runs the REAL satRenderTune(), satTuneReasonText() and esc() cut out of data/v2.js by brace matching, with the
// built-in English of I18N_EN, against a small DOM stub. The fixture is the curve_tune object as the firmware emits it
// (SATcontrol.ino satSendStatusJSON, keys pinned by tests/test_sat_curve_selftune.py ProgressWiring).
// Point OTGW_V2_JS / OTGW_V2_HTML at older copies (git show HEAD:... > file) to see it red.
//
// Run: node --test tests/webui/sat-tune-progress.test.mjs
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

const { t } = i18nEn(SRC);
// esc() is a one-liner with a regex literal holding a quote, which the brace matcher would read as a string: take
// the page-scope esc() (two-space indent, the one satRenderTune() sees) by its line.
function pageEsc(src) {
  const m = /\n  (function esc\(s\) \{[^\n]*\})\r?\n/.exec(src);
  if (!m) throw new Error('page-scope esc() not found in ' + V2JS);
  return m[1];
}
function render(status) {
  const box = { hidden: true, innerHTML: 'stale' };
  const body = 'var document = { getElementById: function (id) { return id === "satTuneProgress" ? box : null; } };\n' +
    pageEsc(SRC) + '\n' + extractFunction(SRC, 'satTuneReasonText') + '\n' +
    extractFunction(SRC, 'satRenderTune') + '\nsatRenderTune(status);';
  new Function('box', 't', 'status', body)(box, t, status);
  const lines = [...box.innerHTML.matchAll(/<div[^>]*>([^<]*)<\/div>/g)].map((m) => m[1]);
  return { hidden: box.hidden, html: box.innerHTML, lines };
}

const REASONS = ['not_sat', 'dhw', 'no_sun', 'sun', 'pwm', 'flame_off', 'min_mod', 'coeff_changed', 'no_ticks',
  'not_controlling', 'few_ticks', 'no_room'];
function fixture(over) {
  const excluded_hours = Object.fromEntries(REASONS.map((r) => [r, 0]));
  return Object.assign({
    curve_self_tune: true,
    curve_tune: {
      running: true, days: 2, days_needed: 3, clean_hours_today: 4, clean_hours_needed: 6, hours_today: 9,
      last_decision: 'hold', last_reason: 'few_days', excluded_hours,
    },
  }, over || {});
}

test('the curve card has the progress box, hidden until data arrives', () => {
  assert.match(HTML, /<div class="curve-legend" id="satCurveLegend"><\/div>\s*(<div class="sat-hint" id="satCurveRec"><\/div>\s*)?<div class="sat-tune-progress" id="satTuneProgress" hidden><\/div>/);
  assert.match(SRC, /satRenderMaxMod\(d\);\s*\n\s*satRenderTune\(d\);/);
});

test('progress: counted days, clean hours today, last day in words', () => {
  const r = render(fixture());
  assert.equal(r.hidden, false);
  assert.deepEqual(r.lines, [
    'Self-tune',
    '2/3 counted days, today 4/6 clean hours',
    'Last day: held (window not full yet)',
    'No hours left out',
  ]);
});

test('excluded hours: only the non-zero reasons, largest first, in words', () => {
  const f = fixture();
  Object.assign(f.curve_tune.excluded_hours, { dhw: 3, sun: 11, no_room: 1 });
  f.curve_tune.last_decision = 'raise'; f.curve_tune.last_reason = 'phases_agree';
  const r = render(f);
  assert.equal(r.lines[2], 'Last day: raised (warm-up and at-target agree)');
  assert.equal(r.lines[3], 'Hours not counted: sun too high 11 h, hot water 3 h, room temperature lost 1 h');
});

test('every reason the firmware sends has a text', () => {
  for (const code of REASONS.concat(['few_days', 'day_not_counted', 'no_phases', 'no_warmup', 'no_floating',
    'inside_band', 'phases_disagree', 'phases_agree', 'at_limit'])) {
    assert.notEqual(t('tools.sat_tune_r_' + code), 'tools.sat_tune_r_' + code, code + ' has no English text');
  }
});

test('an unknown reason shows its code, an unknown decision counts as no day yet', () => {
  const f = fixture();
  f.curve_tune.excluded_hours.new_reason = 2;
  f.curve_tune.last_decision = 'none'; f.curve_tune.last_reason = 'none';
  const r = render(f);
  assert.equal(r.lines[2], 'No day closed yet');
  assert.equal(r.lines[3], 'Hours not counted: new_reason 2 h');
});

test('self-tune off or not running: one "off" line', () => {
  assert.deepEqual(render(fixture({ curve_self_tune: false })).lines, ['Self-tune', 'Self-tune is off']);
  const f = fixture(); f.curve_tune.running = false;
  assert.deepEqual(render(f).lines, ['Self-tune', 'Self-tune is off']);
});

test('a firmware without curve_tune hides the box', () => {
  const r = render({ curve_self_tune: true });
  assert.equal(r.hidden, true);
  assert.equal(r.html, '');
});

test('device text is escaped', () => {
  const f = fixture(); f.curve_tune.last_reason = '<b>x';
  assert.match(render(f).html, /&lt;b>x/);
});
