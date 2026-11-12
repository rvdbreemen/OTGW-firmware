// TASK-1820: Monitor > Values row names follow a live language switch.
// Runs the shipped renderValues / buildValRow / fillValName / valDisplayName and the
// i18n lookup (cut out of data/v2.js by brace matching) against a minimal DOM.
// Bug: fillValName ran only when a row was built, so a row built in Greek kept its
// Greek name after a switch to English (i18nRerender calls renderValues), and a row
// built before the NL pack arrived stayed English.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const V2JS = process.env.OTGW_V2_JS || path.join(ROOT, 'src', 'OTGW-firmware', 'data', 'v2.js');
const SRC = fs.readFileSync(V2JS, 'utf8');
const PACK = (l) => JSON.parse(fs.readFileSync(path.join(ROOT, 'i18n', l + '.json'), 'utf8'));

function balanced(src, start, open, close, what) {
  let i = src.indexOf(open, start), depth = 0;
  for (; i < src.length; i++) {
    const c = src[i];
    if (c === "'" || c === '"' || c === '`') {
      for (i++; i < src.length && src[i] !== c; i++) if (src[i] === '\\') i++;
    } else if (c === '/' && src[i + 1] === '/') {
      i = src.indexOf('\n', i);
    } else if (c === '/' && src[i + 1] === '*') {
      i = src.indexOf('*/', i) + 1;
    } else if (c === open) depth++;
    else if (c === close && --depth === 0) return src.slice(start, i + 1);
  }
  throw new Error('unbalanced in ' + what);
}
const fn = (n) => {
  const s = SRC.indexOf('function ' + n + '(');
  if (s < 0) throw new Error(n + ' not found');
  return balanced(SRC, s, '{', '}', n);
};
const obj = (n) => {
  const s = SRC.indexOf('var ' + n + ' = {');
  if (s < 0) throw new Error(n + ' not found');
  return balanced(SRC, s, '{', '}', n) + ';';
};

// Minimal DOM: enough for the Values table code.
function el(tag) {
  const e = {
    tagName: tag, children: [], dataset: {}, style: {}, attrs: {}, className: '', parentNode: null,
    _text: '',
    get textContent() { return this._text + this.children.map((c) => c.textContent).join(''); },
    set textContent(v) { this._text = String(v); this.children = []; },
    appendChild(c) { c.parentNode = this; this.children.push(c); return c; },
    remove() { if (this.parentNode) this.parentNode.children = this.parentNode.children.filter((x) => x !== this); },
    setAttribute(k, v) { this.attrs[k] = v; },
    addEventListener() {},
    classList: { toggle() {} },
  };
  return e;
}

function load() {
  const tbody = el('tbody');
  const status = el('span');
  const document = {
    createElement: el,
    querySelector: (q) => (q === '#valTable tbody' ? tbody : null),
    getElementById: (id) => (id === 'valStatus' ? status : null),
  };
  const body =
    obj('I18N_EN') + '\n' + obj('OTMON_LABELS') + '\n' +
    "var i18nLang = 'en', i18nPack = null;\n" +
    'var VAL_STALE_MS = 120000, valRows = {}, valSearch = "", otmonData = null, otmonLastOkMs = Date.now(), valView = "table";\n' +
    'function dallasLabelFor() { return ""; }\nfunction openValLabelEditor() {}\n' +
    ['i18nLookup', 'i18nString', 't', 'isDallasEntry', 'fmtOtmonValue', 'fmtAge', 'valDisplayName',
      'buildValRow', 'fillValName', 'renderValues', 'renderValGrid'].map(fn).join('\n') + '\n' +
    'return { render: renderValues, setPack: function (l, p) { i18nLang = l; i18nPack = p; },' +
    ' setData: function (d) { otmonData = d; } };';
  const api = new Function('document', body)(document);
  api.names = () => tbody.children.map((tr) => tr.children[0].textContent);
  return api;
}

const DATA = {
  flamestatus: { value: 'On', unit: '', epoch: 100 },
  chmodus: { value: 'Off', unit: '', epoch: 100 },
};

test('rows built in Greek switch to English, then Dutch, without a reload', () => {
  const ui = load();
  ui.setData(DATA);
  ui.setPack('el', PACK('el'));
  ui.render();
  assert.equal(ui.names()[0], PACK('el').values.otmon.flamestatus);
  ui.setPack('en', null);          // what loadI18nPack('en') does, then i18nRerender -> renderValues
  ui.render();
  assert.equal(ui.names()[0], 'Flame Status');
  ui.setPack('nl', PACK('nl'));
  ui.render();
  assert.equal(ui.names()[0], PACK('nl').values.otmon.flamestatus);
  assert.equal(ui.names()[1], PACK('nl').values.otmon.chmodus);
});

test('rows built before the pack arrived get the pack names on the next render', () => {
  const ui = load();
  ui.setData(DATA);
  ui.render();                     // English first, as on page load
  assert.equal(ui.names()[0], 'Flame Status');
  ui.setPack('de', PACK('de'));
  ui.render();
  assert.equal(ui.names()[0], PACK('de').values.otmon.flamestatus);
});
