// TASK-1764: heuristic guard against user-visible English that bypasses t() / data-i18n.
// PURE STATIC: reads data/v2.js and data/v2.html as text. It never imports the page harness
// and never starts a browser, so it is cheap to run anywhere:
//   node --test tests/webui/i18n-no-hardcoded.test.mjs
// Point it at another copy with OTGW_V2_DIR=<dir holding v2.js and v2.html> (used to show
// that the test fails on the code from before the translation work).
//
// JS scan: a string literal that is assigned to textContent / innerText / title / placeholder /
// alt, set through setAttribute('title'|'placeholder'|'aria-label'|'alt', ...), passed to
// bleToast / alert / confirm / prompt, or sits as text between tags in an innerHTML literal,
// must go through t(). HTML scan: a visible text node needs data-i18n on itself or an
// ancestor (or data-i18n-skip), and title / placeholder / aria-label / alt need the matching
// data-i18n-* attribute. Text from the device at runtime is not a literal, so it never trips this.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const DIR = process.env.OTGW_V2_DIR || path.join(ROOT, 'src', 'OTGW-firmware', 'data');
const JS = fs.readFileSync(path.join(DIR, 'v2.js'), 'utf8').replace(/\r\n/g, '\n');
const HTML = fs.readFileSync(path.join(DIR, 'v2.html'), 'utf8').replace(/\r\n/g, '\n');

// ---- what counts as "English text" -----------------------------------------------------
// Words that stay English everywhere: product and protocol names (see TASK-1764 scope).
const KEEP = /\b(OTGW|SAT|OpenTherm|OT-Direct|Home Assistant|MQTT|PIC|BLE|Dallas|1-Wire|DS18B20|DS18x20|OT|CH|DHW|NTP|DHCP|DNS|HTTP|REST|JSON|API|MAC|IP|UDP|TCP|GPIO|RSSI|PWM|PID|LittleFS|GW|TT|CS|PR|SR|CR|RM|CM|MsgID|ID|URL|ESP32|ESP32-S3|WiFi|Wi-Fi|Telnet|WebSocket|WS|HA)\b/g;
const UNIT = /\b\d+([.,]\d+)?\s*(°C|°F|bar|%|kW|W|V|A|l\/min|ms|s|min|h|kB|MB|B|Hz|dBm)\b/g;

function looksEnglish(text) {
  let s = String(text);
  s = s.replace(/<[^>]*>/g, ' ').replace(/&[a-z#0-9]+;/gi, ' ');
  s = s.replace(/\{\{[^}]*\}\}|\$\{[^}]*\}/g, ' ');
  s = s.replace(UNIT, ' ').replace(KEEP, ' ');
  // a CSS selector, path, url, identifier or format token is not prose
  if (/^[\w.#:\[\]=>~*, -]*$/.test(s.trim()) && /[#.\[\]=>:_]|^[a-z0-9-]+$/.test(s.trim()) && !/\s[A-Za-z]{3,}\s/.test(' ' + s.trim() + ' ')) return false;
  const words = s.match(/[A-Za-z]{3,}/g) || [];
  if (!words.length) return false;
  // all-uppercase tokens with no lower-case word are codes (LED, ERR ...), not prose
  if (words.every(w => w === w.toUpperCase())) return false;
  return true;
}

// ---- allowlist: strings deliberately left English --------------------------------------
// Each entry is a regex tested against the literal; the comment says why it may stay English.
const ALLOW_JS = [
  [/^LittleFS$/, 'file system product name'],
  [/^\.\. \(parent\)$/, 'directory listing token, same in FSexplorer'],
  [/^\.local$/, 'mDNS suffix'],
  [/^<DIR>$/, 'directory listing token, same in FSexplorer'],
];
const ALLOW_HTML = [
  [/^OTGW firmware$/, 'product name (document title)'],
  // language names are shown in their own language on purpose, so a user can find theirs
  [/^(English|Nederlands|Deutsch|Ελληνικά)$/, 'language endonym in the language menu'],
  [/^Language: English$/, 'aria-label rewritten by syncLangMenu() with t(lang.label)'],
  // text that JS overwrites with t() at load or on every render; the HTML holds the English default
  [/^Switch to dark theme$/, 'themeToggle: applyTheme() sets text, aria-label and title with t()'],
  [/^🌙 Dark$/, 'themeToggle: applyTheme() sets text with t()'],
  [/^System view$/, 'viewPickLabel: showDesign() sets it with t()'],
  [/^target —$/, 'bSet: render sets it with t(home.target)'],
  [/^standby$/, 'bDhwState: render sets it with t(home.dhw_*)'],
  [/^Outside — · OTGW firmware$/, 'bOut: render sets it with t(home.outside_firmware)'],
  [/^Enabled$/, 'satEnableLbl: render sets it with t(tools.enabled/disabled)'],
  [/^Loading…$/, 'valStatus: renderValues() sets it with t(values.loading)'],
  [/^(master|slave|gateway)$/, 'cnThermSub/cnBoilerSub/cnPicSub: renderConnMap sets them with t()'],
  [/^⛓ 0 injected$/, 'cnOvrTxt: renderConnMap sets it with t(settings.ovr_injected)'],
  [/^Current directory: \/$/, 'fsCrumb: renderFs sets it with t(adv.fs_crumb)'],
  [/^Stream every frame straight to a local file you pick$/, 'chipStream: wiring sets data-i18n-title (sat.stream_title) and applyI18n re-runs'],
  // names that stay English
  [/^Open-Meteo$/, 'weather service name'],
  [/^SAT · Smart Autotune Thermostat$/, 'product name with its expansion'],
  [/^Hex$/, 'placeholder naming the number base of the input; same word in NL, DE and EL'],
];
// KNOWN GAPS: English text in v2.html that nothing translates yet. They are listed here so the
// test is green today and still catches every NEW untranslated string; each one is a defect
// to fix by giving it data-i18n (then delete the entry). Reported in TASK-1764.
const KNOWN_GAPS_HTML = [
  [/^1 hour$/, 'Monitor > Graph window chip: the graph code reads the chip text back by regex (/min/), so it stays English'],
  [/^· ws ● live$/, 'Mission control view heading "OTGW // MISSION CONTROL · ws ● live"'],
];
const allowedJs = s => ALLOW_JS.some(([re]) => re.test(s.trim()));
const allowedHtml = s => ALLOW_HTML.concat(KNOWN_GAPS_HTML).some(([re]) => re.test(s.trim()));

// ---- JS scan ----------------------------------------------------------------------------
// Whole-line comments are dropped; the literal patterns below are anchored on an assignment or
// call, so a trailing comment cannot match them.
function jsCode(src) {
  return src.split('\n').map(l => /^\s*(\/\/|\*|\/\*)/.test(l) ? '' : l);
}
const LIT = String.raw`(?:'((?:[^'\\\n]|\\.)*)'|"((?:[^"\\\n]|\\.)*)"|` + '`((?:[^`\\\\]|\\\\.)*)`)';
const JS_PATTERNS = [
  ['textContent/innerText', new RegExp(String.raw`\.(?:textContent|innerText)\s*\+?=\s*` + LIT, 'g')],
  ['title/placeholder/alt property', new RegExp(String.raw`\.(?:title|placeholder|alt)\s*=\s*` + LIT, 'g')],
  ['setAttribute text', new RegExp(String.raw`setAttribute\(\s*['"](?:title|placeholder|aria-label|alt)['"]\s*,\s*` + LIT, 'g')],
  ['toast/dialog', new RegExp(String.raw`\b(?:bleToast|alert|confirm|prompt)\(\s*` + LIT, 'g')],
  ['toast/dialog ternary', new RegExp(String.raw`\b(?:bleToast|alert|confirm|prompt)\([^;\n]*?\?\s*` + LIT + String.raw`\s*:\s*` + LIT, 'g')],
];
const lit = m => { for (let i = m.length - 1; i > 0; i--) if (m[i] !== undefined) return m[i]; return ''; };

export function scanJs(src) {
  const hits = [];
  jsCode(src).forEach((line, i) => {
    if (!line) return;
    for (const [what, re] of JS_PATTERNS) {
      re.lastIndex = 0;
      let m;
      while ((m = re.exec(line))) {
        const all = m.slice(1).filter(x => x !== undefined);
        for (const s of all) if (looksEnglish(s) && !allowedJs(s)) hits.push({ line: i + 1, what, text: s });
      }
    }
    // innerHTML = '...<tag>Text</tag>...': text between tags in a literal
    const ih = /innerHTML\s*\+?=\s*(.*)$/.exec(line);
    if (ih) {
      const lits = [...ih[1].matchAll(new RegExp(LIT, 'g'))].map(lit);
      for (const s of lits) {
        for (const tn of s.matchAll(/>([^<>]*)</g)) {
          if (looksEnglish(tn[1]) && !allowedJs(tn[1])) hits.push({ line: i + 1, what: 'innerHTML text', text: tn[1] });
        }
      }
    }
  });
  return hits;
}

// ---- HTML scan --------------------------------------------------------------------------
const VOID = new Set(['area', 'base', 'br', 'col', 'embed', 'hr', 'img', 'input', 'link', 'meta', 'source', 'track', 'wbr']);
const NOTEXT = new Set(['script', 'style']);
export function scanHtml(src) {
  const hits = [];
  const body = src.replace(/<!--[\s\S]*?-->/g, m => m.replace(/[^\n]/g, ' ')).replace(/<!doctype[^>]*>/i, m => ' '.repeat(m.length));
  const re = /<(\/?)([a-zA-Z][\w-]*)((?:"[^"]*"|'[^']*'|[^'">])*)>|([^<]+)/g;
  const stack = [];
  const lineAt = idx => body.slice(0, idx).split('\n').length;
  let m;
  while ((m = re.exec(body))) {
    if (m[4] !== undefined) {
      const text = m[4];
      if (stack.some(e => NOTEXT.has(e.tag))) continue;
      if (stack.some(e => e.i18n || e.skip)) continue;
      const t = text.replace(/\s+/g, ' ').trim();
      if (t && looksEnglish(t) && !allowedHtml(t)) hits.push({ line: lineAt(m.index), what: 'text node in <' + (stack.length ? stack[stack.length - 1].tag : '?') + '>', text: t });
      continue;
    }
    const closing = m[1] === '/', tag = m[2].toLowerCase(), attrs = m[3] || '';
    if (closing) {
      for (let k = stack.length - 1; k >= 0; k--) if (stack[k].tag === tag) { stack.length = k; break; }
      continue;
    }
    const has = n => new RegExp('(^|\\s)' + n + '(\\s|=|$)').test(attrs);
    const attr = n => { const a = new RegExp('(?:^|\\s)' + n + '="([^"]*)"').exec(attrs); return a ? a[1] : null; };
    const skip = has('data-i18n-skip');
    for (const [a, d] of [['title', 'data-i18n-title'], ['placeholder', 'data-i18n-placeholder'], ['aria-label', 'data-i18n-aria-label'], ['alt', 'data-i18n-alt']]) {
      const v = attr(a);
      if (v !== null && !has(d) && !skip && looksEnglish(v) && !allowedHtml(v)) hits.push({ line: lineAt(m.index), what: a + ' without ' + d + ' on <' + tag + '>', text: v });
    }
    if (tag === 'option' || tag === 'input' || tag === 'button') { /* text handled by the text-node rule */ }
    if (!VOID.has(tag) && !/\/\s*$/.test(attrs)) stack.push({ tag, i18n: has('data-i18n'), skip });
  }
  return hits;
}

const fmt = h => h.map(x => `  line ${x.line} [${x.what}] ${JSON.stringify(x.text)}`).join('\n');

test('v2.js: no user-visible English literal bypasses t()', () => {
  const hits = scanJs(JS);
  assert.equal(hits.length, 0, `${hits.length} hard-coded English literal(s) in v2.js (wrap in t(), or add to ALLOW_JS with a reason):\n${fmt(hits)}`);
});

test('v2.html: every visible text node and text attribute is translatable', () => {
  const hits = scanHtml(HTML);
  assert.equal(hits.length, 0, `${hits.length} untranslated English text(s) in v2.html (add data-i18n / data-i18n-skip, or add to ALLOW_HTML with a reason):\n${fmt(hits)}`);
});

test('scanner self-check: it does flag untranslated text and accepts translated text', () => {
  assert.equal(scanJs("el.textContent = 'Save settings';").length, 1);
  assert.equal(scanJs("bleToast('Saved the thing', true);").length, 1);
  assert.equal(scanJs("x.title = 'Click to reboot';").length, 1);
  assert.equal(scanJs("d.innerHTML = '<b>Hello world</b>';").length, 1);
  assert.equal(scanJs("el.textContent = t('nav.settings');").length, 0);
  assert.equal(scanJs("el.textContent = 'OTGW';").length, 0);
  assert.equal(scanJs("el.textContent = '12 bar';").length, 0);
  assert.equal(scanJs("// el.textContent = 'Save settings';").length, 0);
  assert.equal(scanHtml('<div><p>Save settings</p></div>').length, 1);
  assert.equal(scanHtml('<div><p data-i18n="a.b">Save settings</p></div>').length, 0);
  assert.equal(scanHtml('<p data-i18n="a.b">Save <b>all</b> settings</p>').length, 0);
  assert.equal(scanHtml('<button title="Go home">12</button>').length, 1);
  assert.equal(scanHtml('<button title="Go home" data-i18n-title="a.b">12</button>').length, 0);
  assert.equal(scanHtml('<p data-i18n-skip>Save settings</p>').length, 0);
  assert.equal(scanHtml('<span>MQTT</span><span>42 %</span><span>°C</span>').length, 0);
});
