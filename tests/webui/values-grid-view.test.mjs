// TASK-1823: Monitor > Values gets a compact grid view next to the table, like the 1.x Home screen
// (OTGWELGA, v1.7.9): every otmonitor field in columns, on/off as a filled or empty box, numbers with
// their unit, fields without data left out. A Table/Grid chip switches, remembered per browser.
// Runs the shipped renderValues and helpers cut out of data/v2.js against a minimal DOM.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const DATA = process.env.OTGW_DATA_DIR || path.join(ROOT, 'src', 'OTGW-firmware', 'data');
const SRC = fs.readFileSync(path.join(DATA, 'v2.js'), 'utf8');
const HTML = fs.readFileSync(path.join(DATA, 'v2.html'), 'utf8');
const PACK = (l) => JSON.parse(fs.readFileSync(path.join(ROOT, 'i18n', l + '.json'), 'utf8'));

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
  throw new Error('unbalanced in ' + what);
}
const fn = (n) => { const s = SRC.indexOf('function ' + n + '('); assert.ok(s >= 0, n + ' not found in v2.js'); return balanced(SRC, s, n); };
const obj = (n) => { const s = SRC.indexOf('var ' + n + ' = {'); assert.ok(s >= 0, n + ' not found'); return balanced(SRC, s, n) + ';'; };

function el(tag) {
  const e = {
    tagName: tag, children: [], dataset: {}, style: {}, attrs: {}, className: '', parentNode: null, hidden: false, _text: '',
    get textContent() { return this._text + this.children.map((c) => c.textContent).join(''); },
    set textContent(v) { this._text = String(v); this.children = []; },
    appendChild(c) { c.parentNode = this; this.children.push(c); return c; },
    remove() { if (this.parentNode) this.parentNode.children = this.parentNode.children.filter((x) => x !== this); },
    setAttribute(k, v) { this.attrs[k] = String(v); }, addEventListener() {},
    classList: { _s: new Set(), toggle(c, on) { if (on === undefined ? !this._s.has(c) : on) this._s.add(c); else this._s.delete(c); }, contains(c) { return this._s.has(c); } },
  };
  e.classList = { _s: new Set(), toggle(c, on) { if (on === undefined ? !this._s.has(c) : on) this._s.add(c); else this._s.delete(c); }, contains(c) { return this._s.has(c); } };
  return e;
}

function load(store = {}) {
  const ids = { valGrid: el('div'), valTableWrap: el('div'), valViewTable: el('span'), valViewGrid: el('span'), valStatus: el('span'), valCount: el('b') };
  const tbody = el('tbody');
  const document = { createElement: el, querySelector: (q) => (q === '#valTable tbody' ? tbody : null), getElementById: (id) => ids[id] || null };
  const localStorage = { getItem: (k) => (k in store ? store[k] : null), setItem: (k, v) => { store[k] = String(v); } };
  const body =
    obj('I18N_EN') + '\n' + obj('OTMON_LABELS') + '\n' +
    "var i18nLang = 'en', i18nPack = null;\n" +
    'var VAL_STALE_MS = 120000, valRows = {}, valSearch = "", otmonData = null, otmonLastOkMs = Date.now(), valView = valViewPref();\n' +
    'function dallasLabelFor() { return ""; }\nfunction openValLabelEditor() {}\nfunction isMonitorVisible() { return true; }\n' +
    ['i18nLookup', 'i18nString', 't', 'isDallasEntry', 'fmtOtmonValue', 'fmtAge', 'valDisplayName',
      'buildValRow', 'fillValName', 'renderValues', 'renderValGrid', 'valViewPref', 'setValView'].map(fn).join('\n') + '\n' +
    'return { render: renderValues, set: setValView, view: function () { return valView; },' +
    ' setPack: function (l, p) { i18nLang = l; i18nPack = p; }, setData: function (d) { otmonData = d; }, search: function (q) { valSearch = q; } };';
  const api = new Function('document', 'localStorage', body)(document, localStorage);
  api.ids = ids;
  api.cells = () => ids.valGrid.children.map((c) => ({ name: c.children[0].textContent, val: c.children[1], unit: c.children[2].textContent }));
  return api;
}

const DATA_OT = {
  flamestatus: { value: 'On', unit: '', epoch: 100 },
  faultindicator: { value: 'On', unit: '', epoch: 100 },
  dhwmode: { value: 'Off', unit: '', epoch: 100 },
  boilertemperature: { value: 47.256, unit: '°C', epoch: 100 },
  outsidetemperature: { value: 0, unit: '°C', epoch: 0 },          // no data: left out, as in 1.x
  sensorsimulation: { value: 'Off', unit: '', epoch: 100 },          // debug flag: never shown
};

test('v2.html: Table/Grid chips and a grid container next to the table', () => {
  assert.match(HTML, /id="valViewTable"[^>]*data-i18n="h_mon\.val_view_table"/);
  assert.match(HTML, /id="valViewGrid"[^>]*data-i18n="h_mon\.val_view_grid"/);
  assert.match(HTML, /id="valTableWrap"/);
  assert.match(HTML, /<div class="valgrid" id="valGrid" hidden><\/div>/);
});

test('grid: every field with data, on/off as a box, numbers with unit, in firmware order', () => {
  const ui = load({ 'otgw-v2-valview': 'grid' });
  assert.equal(ui.view(), 'grid');
  ui.setData(DATA_OT); ui.render();
  assert.equal(ui.ids.valGrid.hidden, false); assert.equal(ui.ids.valTableWrap.hidden, true);
  const c = ui.cells();
  assert.deepEqual(c.map((x) => x.name), ['Flame Status', 'Fault Indicator', 'Domestic Hot Water Status', 'Boiler Temperature']);
  assert.equal(c[0].val.children[0].className, 'vsq on');
  assert.equal(c[1].val.children[0].className, 'vsq alarm', 'a fault flag that is on shows as an alert');
  assert.equal(c[2].val.children[0].className, 'vsq');
  assert.equal(c[3].val.textContent, '47.26'); assert.equal(c[3].unit, '°C');
});

test('grid follows the filter and a live language switch', () => {
  const ui = load({ 'otgw-v2-valview': 'grid' });
  ui.setData(DATA_OT); ui.render();
  ui.setPack('nl', PACK('nl')); ui.render();
  assert.equal(ui.cells()[0].name, PACK('nl').values.otmon.flamestatus);
  ui.search('boiler'); ui.render();
  assert.deepEqual(ui.cells().map((x) => x.name), [PACK('nl').values.otmon.boilertemperature]);
});

test('the choice is remembered per browser; table is the default', () => {
  const store = {};
  const ui = load(store);
  assert.equal(ui.view(), 'table');
  ui.setData(DATA_OT); ui.render();
  assert.equal(ui.ids.valGrid.hidden, true); assert.equal(ui.ids.valTableWrap.hidden, false);
  ui.set('grid');
  assert.equal(store['otgw-v2-valview'], 'grid');
  assert.equal(ui.ids.valViewGrid.classList.contains('on'), true); assert.equal(ui.ids.valViewTable.classList.contains('on'), false);
  assert.equal(ui.ids.valGrid.hidden, false);
  ui.set('table');
  assert.equal(store['otgw-v2-valview'], 'table'); assert.equal(ui.ids.valGrid.hidden, true);
});
