// TASK-1281 guard: every curated setting in the v2 Settings page has a help text, shown by an
// accessible info icon.
//
// Two parts:
//  1. Content: settings-help.json is valid JSON, at most 18 KB (LittleFS budget), has a text of
//     20..300 characters for EVERY key of SET_META and no key that SET_META does not know.
//     SET_META is cut out of the real data/v2.js, so a new setting without help fails here.
//  2. Behaviour: the REAL settingRow(), helpFor(), loadSettingsHelp(), showHelp(), hideHelp(),
//     helpPlace(), helpPopover() and addHelpIcon() are cut out of data/v2.js by brace matching and
//     run against a small DOM stub. The stub records what the real code does (attributes,
//     listeners, text, position) and refuses innerHTML, so help text can only arrive as text.
//
// Red runs (point at older copies, see the task notes):
//   OTGW_V2_JS=<git show HEAD:...v2.js>      -> the behaviour tests fail (no icon, no helpFor)
//   OTGW_SETTINGS_HELP=<missing file>        -> the content tests fail
//
// Not covered: real hover/tap/focus behaviour, layout and the 390 px fit in a real browser
// (needs the bench / Playwright), and the LittleFS image size (needs build.bat).
//
// Run: node --test tests/webui/settings-help.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const V2JS = process.env.OTGW_V2_JS || path.join(DATA, 'v2.js');
const V2CSS = process.env.OTGW_V2_CSS || path.join(DATA, 'v2.css');
const HELP_JSON = process.env.OTGW_SETTINGS_HELP || path.join(DATA, 'settings-help.json');
const SRC = fs.readFileSync(V2JS, 'utf8');
const MAX_BYTES = 18 * 1024;

// Cut "function <name>(...) { ... }" out of the source by brace matching, skipping string
// literals and comments. optional: '' when the function does not exist (older v2.js).
function extractFunction(name, optional = false) {
  const start = SRC.indexOf('function ' + name + '(');
  if (start < 0) { if (optional) return ''; throw new Error(name + ' not found in ' + V2JS); }
  return SRC.slice(start, closingBrace(SRC.indexOf('{', start)) + 1);
}
function closingBrace(i) {
  let depth = 0;
  for (; i < SRC.length; i++) {
    const c = SRC[i];
    if (c === "'" || c === '"' || c === '`') {
      for (i++; i < SRC.length && SRC[i] !== c; i++) if (SRC[i] === '\\') i++;
    } else if (c === '/' && SRC[i + 1] === '/') {
      i = SRC.indexOf('\n', i);
    } else if (c === '/' && SRC[i + 1] === '*') {
      i = SRC.indexOf('*/', i) + 1;
    } else if (c === '{') depth++;
    else if (c === '}' && --depth === 0) return i;
  }
  throw new Error('unbalanced braces');
}
const setMetaStart = SRC.indexOf('var SET_META = {');
if (setMetaStart < 0) throw new Error('SET_META not found in ' + V2JS);
const SET_META_SRC = SRC.slice(setMetaStart, closingBrace(SRC.indexOf('{', setMetaStart)) + 1) + ';';
const metaKeys = Object.keys(new Function(SET_META_SRC + '\nreturn SET_META;')());

// ---------------------------------------------------------------- content tests
function readHelp() {
  const raw = fs.readFileSync(HELP_JSON);
  return { raw, obj: JSON.parse(raw.toString('utf8')) };
}

test('settings-help.json is valid JSON, an object of strings, and at most 18 KB', () => {
  const { raw, obj } = readHelp();
  assert.ok(obj && typeof obj === 'object' && !Array.isArray(obj), 'top level must be an object');
  for (const [k, v] of Object.entries(obj)) assert.equal(typeof v, 'string', k + ' must be a string');
  assert.ok(raw.length <= MAX_BYTES, 'file is ' + raw.length + ' bytes, limit ' + MAX_BYTES);
  console.log('settings-help.json: ' + Object.keys(obj).length + ' keys, ' + raw.length + ' bytes (limit ' + MAX_BYTES + ')');
});

test('every SET_META key has a help text of 20..300 characters', () => {
  const { obj } = readHelp();
  const missing = metaKeys.filter((k) => !(k in obj));
  assert.deepEqual(missing, [], 'settings without help: ' + missing.join(', '));
  const badLen = metaKeys.filter((k) => obj[k].length < 20 || obj[k].length > 300).map((k) => k + ' (' + obj[k].length + ')');
  assert.deepEqual(badLen, [], 'help text outside 20..300 characters: ' + badLen.join(', '));
  console.log('SET_META keys: ' + metaKeys.length + ', all with help');
});

test('settings-help.json has no key that SET_META does not know, and no em or en dash', () => {
  const { obj } = readHelp();
  const orphans = Object.keys(obj).filter((k) => !metaKeys.includes(k));
  assert.deepEqual(orphans, [], 'orphan help keys: ' + orphans.join(', '));
  const dashes = Object.keys(obj).filter((k) => /[–—]/.test(obj[k]));
  assert.deepEqual(dashes, [], 'dashes in: ' + dashes.join(', '));
});

test('the SAT Indoor temperature explanation moved into the popup text (AC3)', () => {
  const { obj } = readHelp();
  assert.match(obj.satexternaltemp, /sat\/indoor_temp/);
  assert.match(obj.satexternaltemp, /\/api\/v2\/sat\/externaltemp/);
  assert.match(obj.satexternaltemp, /Sensor max age/);
  assert.ok(obj.satsensormaxage && obj.satsensormaxage.length >= 20);
  const meta = new Function(SET_META_SRC + '\nreturn SET_META;')();
  assert.equal(meta.satexternaltemp.help, undefined, 'the inline help property must be gone from SET_META.satexternaltemp');
  assert.ok(!/className\s*=\s*'shelp'/.test(SRC), 'the inline .shelp block must be gone from v2.js');
});

test('v2.css styles the icon (>= 24 px hit area, visible focus ring) and the popover (max 320 px, fits 390 px)', () => {
  const css = fs.readFileSync(V2CSS, 'utf8');
  const sinfo = css.match(/\.sinfo\{([^}]*)\}/);
  assert.ok(sinfo, '.sinfo rule missing');
  const w = sinfo[1].match(/width:(\d+)px/), h = sinfo[1].match(/height:(\d+)px/);
  assert.ok(w && +w[1] >= 24 && h && +h[1] >= 24, '.sinfo must be at least 24x24 px');
  assert.match(css, /\.sinfo:focus-visible\{[^}]*outline:/, 'visible focus ring missing');
  const pop = css.match(/#shelp\{([^}]*)\}/);
  assert.ok(pop, '#shelp rule missing');
  assert.match(pop[1], /max-width:min\(320px,calc\(100vw - 16px\)\)/, 'popover must be max 320 px and never wider than the viewport minus 16 px');
  assert.match(pop[1], /position:fixed/);
});

// ---------------------------------------------------------------- DOM stub
function makeDom() {
  const docListeners = {}, winListeners = {};
  class Node { constructor() { this.parent = null; } }
  class TextNode extends Node {
    constructor(t) { super(); this.nodeType = 3; this.textContent = String(t); }
  }
  class El extends Node {
    constructor(tag) {
      super();
      this.nodeType = 1; this.tagName = tag.toUpperCase(); this.children = []; this.attrs = {};
      this.listeners = {}; this.style = {}; this.dataset = {}; this.className = ''; this.id = '';
      this._text = ''; this.rect = { left: 0, top: 0, right: 24, bottom: 24 }; this.size = { w: 320, h: 60 };
      const self = this;
      this.classList = {
        add(c) { if (!self.className.split(' ').includes(c)) self.className = (self.className + ' ' + c).trim(); },
        remove(c) { self.className = self.className.split(' ').filter((x) => x && x !== c).join(' '); },
        contains(c) { return self.className.split(' ').includes(c); },
      };
    }
    get textContent() { return this._text + this.children.map((c) => c.textContent).join(''); }
    set textContent(v) { this._text = String(v); this.children = []; }
    set innerHTML(v) { throw new Error('innerHTML must not be used for help text'); }
    appendChild(c) { c.parent = this; this.children.push(c); return c; }
    setAttribute(k, v) { this.attrs[k] = String(v); }
    getAttribute(k) { return k in this.attrs ? this.attrs[k] : null; }
    removeAttribute(k) { delete this.attrs[k]; }
    addEventListener(t, f) { (this.listeners[t] = this.listeners[t] || []).push(f); }
    fire(t, ev) { (this.listeners[t] || []).forEach((f) => f(Object.assign({ target: this, preventDefault() {} }, ev))); }
    get isConnected() { let n = this; while (n) { if (n === dom.document.body) return true; n = n.parent; } return false; }
    contains(n) { while (n) { if (n === this) return true; n = n.parent; } return false; }
    getBoundingClientRect() { return this.rect; }
    // A fixed box with only `left` set shrinks to fit the space right of it (CSS 2.1 10.3.7):
    // the stub reproduces that so a measurement at a stale left is caught (review finding).
    get offsetWidth() {
      const left = parseInt(this.style.left, 10) || 0;
      return Math.min(this.size.w, dom.document.documentElement.clientWidth - left);
    }
    get offsetHeight() { return this.size.h; }
    // focus({ kb: true }) is keyboard focus (:focus-visible matches); a mouse click leaves
    // focus behind without :focus-visible, which is what the popover must tell apart.
    focus(opts) {
      this._kb = !!(opts && opts.kb);
      if (dom.document.activeElement === this) return;
      dom.document.activeElement = this; this.fire('focus', {});
    }
    matches(sel) {
      if (sel === ':focus-visible') return dom.document.activeElement === this && !!this._kb;
      throw new Error('stub matches(): unsupported selector ' + sel);
    }
    blur() {
      if (dom.document.activeElement !== this) return;
      dom.document.activeElement = dom.document.body; this.fire('blur', {});
    }
    find(pred) {
      if (pred(this)) return this;
      for (const c of this.children) { if (c.find) { const r = c.find(pred); if (r) return r; } }
      return null;
    }
    findAll(pred, out = []) {
      if (pred(this)) out.push(this);
      for (const c of this.children) if (c.findAll) c.findAll(pred, out);
      return out;
    }
  }
  const dom = {
    document: {
      body: new El('body'),
      documentElement: { clientWidth: 390, clientHeight: 700 },
      activeElement: null,
      createElement: (t) => new El(t),
      createTextNode: (t) => new TextNode(t),
      addEventListener(t, f) { (docListeners[t] = docListeners[t] || []).push(f); },
      fire(t, ev) { (docListeners[t] || []).forEach((f) => f(ev)); },
    },
    window: {
      addEventListener(t, f) { (winListeners[t] = winListeners[t] || []).push(f); },
      fire(t, ev) { (winListeners[t] || []).forEach((f) => f(ev || {})); },
    },
    El,
  };
  dom.document.activeElement = dom.document.body;
  return dom;
}

const flush = () => new Promise((r) => setImmediate(r));

// Build the real functions around the DOM stub. fetchImpl(url, init) -> Promise<response>.
function load(fetchImpl) {
  const dom = makeDom();
  const fetchCalls = [];
  const fetchStub = (url, init) => { fetchCalls.push([url, init]); return fetchImpl(url, init); };
  // The real state declarations, never a stand-in: a renamed or missing variable must fail here.
  const stateDecls = [/^[ \t]*var SETTINGS_HELP[^;]*;/m, /^[ \t]*var helpPop[^;]*;/m].map((re) => {
    const m = SRC.match(re);
    if (!m) throw new Error('state declaration ' + re + ' not found in ' + V2JS);
    return m[0];
  });
  const names = ['humanizeKey', 'labelFor', 'hintFor', 'subFor', 'helpFor', 'loadSettingsHelp', 'helpPlace', 'hideHelp',
    'helpKbFocus', 'helpPopover', 'showHelp', 'addHelpIcon', 'settingRow'];
  const body = [
    SET_META_SRC,
    stateDecls.join('\n'),
    'var setData = {}, setDirty = {}, ENUM_OPTS = {}, IP_KEY_RE = /^$/;',
    'function curVal(k) { return setData[k].value; }',
    'function fieldDirty() { return false; }',
    'function markDirty() {}',
    'function ipPrefill() {}',
    'function ipv4Err() { return ""; }',
    ...names.map((n) => extractFunction(n, true)),
    'return {',
    '  SET_META: SET_META, setData: setData, settingRow: typeof settingRow === "function" ? settingRow : null,',
    '  helpFor: typeof helpFor === "function" ? helpFor : null,',
    '  loadSettingsHelp: typeof loadSettingsHelp === "function" ? loadSettingsHelp : null,',
    '  hideHelp: typeof hideHelp === "function" ? hideHelp : null,',
    '  get SETTINGS_HELP() { return SETTINGS_HELP; }, set SETTINGS_HELP(v) { SETTINGS_HELP = v; },',
    '};',
  ].join('\n');
  const api = new Function('document', 'window', 'fetch', 'confirm', body)(dom.document, dom.window, fetchStub, () => true);
  return { api, dom, fetchCalls };
}

const okFetch = (obj) => () => Promise.resolve({ ok: true, status: 200, text: () => Promise.resolve(typeof obj === 'string' ? obj : JSON.stringify(obj)) });
const need = (fn, name) => { assert.ok(fn, name + '() does not exist in ' + V2JS); return fn; };

function rowFor(ctx, key, type = 's', expectIcon = true) {
  ctx.api.setData[key] = { type, value: type === 'b' ? 'false' : 'x' };
  const row = need(ctx.api.settingRow, 'settingRow')(key);
  ctx.dom.document.body.appendChild(row);
  const btn = row.find((e) => e.tagName === 'BUTTON' && e.className === 'sinfo');
  if (expectIcon) assert.ok(btn, 'no <button class="sinfo"> in the ' + key + ' row');
  return { row, btn };
}
const popover = (ctx) => ctx.dom.document.body.find((e) => e.id === 'shelp');
const visible = (pop) => !!pop && pop.classList.contains('show');

// ---------------------------------------------------------------- behaviour tests
test('settingRow renders a real info button after the label with the aria wiring (AC1)', () => {
  const ctx = load(okFetch({}));
  const { row, btn } = rowFor(ctx, 'hostname');
  assert.ok(btn, 'no <button class="sinfo"> in the hostname row');
  assert.equal(btn.type, 'button');
  assert.equal(btn.getAttribute('aria-label'), 'Help: Hostname');
  assert.equal(btn.getAttribute('aria-expanded'), 'false');
  assert.equal(btn.getAttribute('aria-controls'), 'shelp');
  const lbl = row.find((e) => e.className === 'slbl');
  const kinds = lbl.children.map((c) => (c.nodeType === 3 ? 'text' : c.className));
  assert.deepEqual(kinds, ['text', 'sinfo', 'shint'], 'order in the label: text, icon, hint');
  assert.equal(row.find((e) => e.className === 'shelp'), null, 'no inline .shelp block any more');
  assert.equal(popover(ctx), null, 'no popover is built before the first interaction');
});

test('a key outside SET_META gets no icon', () => {
  const ctx = load(okFetch({}));
  const { btn } = rowFor(ctx, 'zzuncurated', 's', false);
  assert.equal(btn, null);
});

test('helpFor: SET_META.help wins over the JSON, the JSON over nothing', () => {
  const ctx = load(okFetch({}));
  const helpFor = need(ctx.api.helpFor, 'helpFor');
  assert.equal(helpFor('hostname'), '', 'unknown until the JSON is loaded');
  ctx.api.SETTINGS_HELP = { hostname: 'from json', ledblink: 'led from json' };
  assert.equal(helpFor('hostname'), 'from json');
  ctx.api.SET_META.hostname.help = 'from meta';
  assert.equal(helpFor('hostname'), 'from meta');
  assert.equal(helpFor('ledblink'), 'led from json');
  assert.equal(helpFor('zzuncurated'), '');
});

test('focus shows "Loading", then the text after the one fetch; blur hides an unpinned popover', async () => {
  const ctx = load(okFetch({ hostname: 'Network name <b>of</b> this gateway.' }));
  const { btn } = rowFor(ctx, 'hostname');
  btn.focus();
  const pop = popover(ctx);
  assert.ok(pop, 'popover #shelp not created');
  assert.equal(pop.getAttribute('role'), 'tooltip');
  assert.equal(pop.id, 'shelp');
  assert.ok(visible(pop));
  assert.equal(pop.textContent, 'Loading…');
  assert.equal(pop.getAttribute('aria-live'), 'polite', 'the text that replaces Loading must be announced');
  assert.equal(btn.getAttribute('aria-expanded'), 'false', 'hover/focus is not "expanded"; only a pin is');
  assert.equal(btn.getAttribute('aria-describedby'), 'shelp');
  await flush();
  assert.equal(pop.textContent, 'Network name <b>of</b> this gateway.', 'markup in help text must stay text');
  assert.deepEqual(ctx.fetchCalls.map((c) => c[0]), ['settings-help.json']);
  btn.blur();
  assert.ok(!visible(pop));
  assert.equal(btn.getAttribute('aria-expanded'), 'false');
  assert.equal(btn.getAttribute('aria-describedby'), null);
});

test('hovering several icons fetches the file once', async () => {
  const ctx = load(okFetch({ hostname: 'a'.repeat(30), ledblink: 'b'.repeat(30) }));
  const a = rowFor(ctx, 'hostname').btn, b = rowFor(ctx, 'ledblink').btn;
  a.fire('mouseenter', {}); await flush();
  a.fire('mouseleave', {}); b.fire('mouseenter', {}); await flush();
  b.fire('mouseleave', {}); a.fire('mouseenter', {}); await flush();
  assert.equal(ctx.fetchCalls.length, 1);
  assert.equal(popover(ctx).textContent, 'a'.repeat(30));
});

test('click pins the popover (survives mouseleave and blur), a second click closes it', async () => {
  const ctx = load(okFetch({ hostname: 'x'.repeat(40) }));
  const { btn } = rowFor(ctx, 'hostname');
  btn.fire('mouseenter', {}); btn.focus(); btn.fire('click', {});
  await flush();
  const pop = popover(ctx);
  assert.equal(btn.getAttribute('aria-expanded'), 'true');
  btn.fire('mouseleave', {}); btn.blur();
  assert.ok(visible(pop), 'a pinned popover must stay after mouseleave and blur');
  btn.fire('click', {});
  assert.ok(!visible(pop), 'second click closes');
  assert.equal(btn.getAttribute('aria-expanded'), 'false');
});

test('a tap without focus (Safari) still pins: mouseenter then click', async () => {
  const ctx = load(okFetch({ hostname: 'y'.repeat(40) }));
  const { btn } = rowFor(ctx, 'hostname');
  btn.fire('mouseenter', {}); btn.fire('click', {});
  await flush();
  btn.fire('mouseleave', {});
  assert.ok(visible(popover(ctx)));
  assert.equal(popover(ctx).textContent, 'y'.repeat(40));
});

test('Escape closes a pinned popover and focus stays on / returns to the button', async () => {
  const ctx = load(okFetch({ hostname: 'z'.repeat(40) }));
  const { btn } = rowFor(ctx, 'hostname');
  const other = ctx.dom.document.createElement('input'); ctx.dom.document.body.appendChild(other);
  btn.focus(); btn.fire('click', {}); await flush();
  other.focus();                                   // focus wandered off while pinned
  assert.ok(visible(popover(ctx)));
  ctx.dom.document.fire('keydown', { key: 'Escape' });
  assert.ok(!visible(popover(ctx)));
  assert.equal(btn.getAttribute('aria-expanded'), 'false');
  assert.equal(ctx.dom.document.activeElement, btn, 'focus must return to the button');
  assert.ok(!visible(popover(ctx)), 'returning the focus must not re-open the popover');
  btn.fire('click', {}); await flush();            // pin again with the focus already on the button
  assert.ok(visible(popover(ctx)));
  ctx.dom.document.fire('keydown', { key: 'Escape' });
  assert.ok(!visible(popover(ctx)));
  assert.equal(ctx.dom.document.activeElement, btn, 'focus stays on the button');
});

test('a click outside the button and popover unpins; a click inside the popover does not', async () => {
  const ctx = load(okFetch({ hostname: 'q'.repeat(40) }));
  const { btn } = rowFor(ctx, 'hostname');
  btn.fire('click', {}); await flush();
  const pop = popover(ctx);
  ctx.dom.document.fire('click', { target: pop });
  assert.ok(visible(pop), 'click inside the popover keeps it');
  ctx.dom.document.fire('click', { target: btn });
  assert.ok(visible(pop), 'the button own click is handled by the button');
  ctx.dom.document.fire('click', { target: ctx.dom.document.body });
  assert.ok(!visible(pop), 'click elsewhere closes it');
});

test('a failed fetch shows "Help not available." and the next open retries; bad JSON counts as failed', async () => {
  let n = 0;
  const flaky = () => (++n === 1 ? Promise.resolve({ ok: false, status: 503, text: () => Promise.resolve('') })
    : n === 2 ? Promise.resolve({ ok: true, status: 200, text: () => Promise.resolve('{not json') })
      : Promise.resolve({ ok: true, status: 200, text: () => Promise.resolve(JSON.stringify({ hostname: 'finally here, text.' })) }));
  const ctx = load(flaky);
  const { btn } = rowFor(ctx, 'hostname');
  btn.fire('mouseenter', {}); await flush();
  assert.equal(popover(ctx).textContent, 'Help not available.');
  btn.fire('mouseleave', {}); btn.fire('mouseenter', {}); await flush();
  assert.equal(popover(ctx).textContent, 'Help not available.', 'invalid JSON');
  btn.fire('mouseleave', {}); btn.fire('mouseenter', {}); await flush();
  assert.equal(popover(ctx).textContent, 'finally here, text.');
  assert.equal(ctx.fetchCalls.length, 3);
});

test('SET_META.help is shown at once without any fetch', () => {
  const ctx = load(okFetch({}));
  ctx.api.SET_META.hostname.help = 'inline wins';
  const { btn } = rowFor(ctx, 'hostname');
  btn.focus();
  assert.equal(popover(ctx).textContent, 'inline wins');
  assert.equal(ctx.fetchCalls.length, 0);
});

test('the popover is clamped inside a 390 px viewport and flips above when there is no room below', async () => {
  const ctx = load(okFetch({ hostname: 'p'.repeat(40) }));
  const { btn } = rowFor(ctx, 'hostname');
  btn.rect = { left: 366, top: 100, right: 390, bottom: 124 };       // icon at the right edge
  btn.focus(); await flush();
  const pop = popover(ctx);
  assert.equal(pop.style.left, (390 - 320 - 8) + 'px', 'left clamped so a 320 px popover ends 8 px before the edge');
  assert.equal(pop.style.top, '130px', 'below the icon');
  btn.blur();
  btn.rect = { left: 10, top: 660, right: 34, bottom: 684 };         // no room below in a 700 px viewport
  btn.focus();
  assert.equal(pop.style.left, '10px');
  assert.equal(pop.style.top, (660 - 6 - 60) + 'px', 'above the icon');
});

test('re-rendering the list (the anchor leaves the DOM) closes the popover', async () => {
  const ctx = load(okFetch({ hostname: 'r'.repeat(40) }));
  const { row, btn } = rowFor(ctx, 'hostname');
  btn.fire('click', {}); await flush();
  const pop = popover(ctx);
  ctx.dom.document.body.children = ctx.dom.document.body.children.filter((c) => c !== row);
  row.parent = null;
  ctx.dom.window.fire('scroll', {});
  assert.ok(!visible(pop));
});

// ---------------------------------------------------------------- review findings (TASK-1281)
const wait = (ms) => new Promise((r) => setTimeout(r, ms));

test('leaving an unpinned, unfocused icon hides the popover after the grace period', async () => {
  const ctx = load(okFetch({ hostname: 'h'.repeat(30) }));
  const { btn } = rowFor(ctx, 'hostname');
  btn.fire('mouseenter', {}); await flush();
  assert.ok(visible(popover(ctx)));
  btn.fire('mouseleave', {});
  assert.ok(visible(popover(ctx)), 'still shown during the grace period (the pointer may be crossing to the popover)');
  await wait(150);
  assert.ok(!visible(popover(ctx)), 'hidden after mouseleave');
});

test('the focus a mouse click leaves behind must not keep the next hover open; keyboard focus does', async () => {
  const ctx = load(okFetch({ hostname: 'm'.repeat(30) }));
  const { btn } = rowFor(ctx, 'hostname');
  btn.fire('mouseenter', {}); btn.focus(); btn.fire('click', {}); await flush();   // mouse: pin
  btn.fire('click', {});                                                              // mouse: close, focus stays
  assert.equal(ctx.dom.document.activeElement, btn);
  btn.fire('mouseleave', {}); await wait(150);
  btn.fire('mouseenter', {}); btn.fire('mouseleave', {}); await wait(150);
  assert.ok(!visible(popover(ctx)), 'a hover after a mouse click must close on mouseleave');
  btn.blur(); btn.focus({ kb: true });                                                // keyboard: Tab away and back onto the icon
  assert.ok(visible(popover(ctx)));
  btn.fire('mouseenter', {}); btn.fire('mouseleave', {}); await wait(150);
  assert.ok(visible(popover(ctx)), 'keyboard focus keeps the popover open');
  btn.blur();
  assert.ok(!visible(popover(ctx)));
});

test('the tooltip is hoverable: the pointer may move onto the popover, leaving the popover hides it', async () => {
  const ctx = load(okFetch({ hostname: 'v'.repeat(30) }));
  const { btn } = rowFor(ctx, 'hostname');
  btn.fire('mouseenter', {}); await flush();
  const pop = popover(ctx);
  btn.fire('mouseleave', {}); pop.fire('mouseenter', {}); await wait(150);
  assert.ok(visible(pop), 'kept while the pointer is on the popover');
  pop.fire('mouseleave', {});
  assert.ok(!visible(pop), 'hidden when the pointer leaves the popover');
});

test('Escape on a hovered popover hides it without stealing the focus from the field being edited', async () => {
  const ctx = load(okFetch({ hostname: 'e'.repeat(30) }));
  const { btn } = rowFor(ctx, 'hostname');
  const field = ctx.dom.document.createElement('input'); ctx.dom.document.body.appendChild(field);
  field.focus({ kb: true });
  btn.fire('mouseenter', {}); await flush();
  ctx.dom.document.fire('keydown', { key: 'Escape' });
  assert.ok(!visible(popover(ctx)));
  assert.equal(ctx.dom.document.activeElement, field, 'focus must stay in the field');
});

test('aria-expanded is true only while the popover is pinned', async () => {
  const ctx = load(okFetch({ hostname: 'a'.repeat(30) }));
  const { btn } = rowFor(ctx, 'hostname');
  btn.fire('mouseenter', {}); await flush();
  assert.equal(btn.getAttribute('aria-expanded'), 'false');
  btn.fire('click', {});
  assert.equal(btn.getAttribute('aria-expanded'), 'true');
  btn.fire('click', {});
  assert.equal(btn.getAttribute('aria-expanded'), 'false');
});

test('two icons opened before the JSON arrives share one in-flight fetch', async () => {
  let resolve;
  const ctx = load(() => new Promise((r) => { resolve = r; }));
  const a = rowFor(ctx, 'hostname').btn, b = rowFor(ctx, 'ledblink').btn;
  a.fire('mouseenter', {}); a.fire('mouseleave', {}); b.fire('mouseenter', {});
  assert.equal(ctx.fetchCalls.length, 1, 'the second open must reuse the pending request');
  resolve({ ok: true, status: 200, text: () => Promise.resolve(JSON.stringify({ hostname: 'A text here.', ledblink: 'B text here.' })) });
  await flush(); await flush();
  assert.equal(popover(ctx).textContent, 'B text here.');
});

test('the popover is measured at left 0, so an icon in the middle of a phone screen gets the full width', async () => {
  const ctx = load(okFetch({ hostname: 'w'.repeat(60) }));
  const { btn } = rowFor(ctx, 'hostname');
  btn.rect = { left: 180, top: 100, right: 204, bottom: 124 };          // icon mid-screen on a 390 px phone
  btn.focus({ kb: true }); await flush();
  const pop = popover(ctx);
  pop.style.left = '180px';                                              // a stale left from an earlier placement
  btn.blur(); btn.focus({ kb: true });
  assert.equal(pop.style.left, (390 - 320 - 8) + 'px', 'a 320 px popover must end 8 px before the edge, not shrink to fit');
});

test('a pinned popover hides when its icon scrolls out of view', async () => {
  const ctx = load(okFetch({ hostname: 's'.repeat(30) }));
  const { btn } = rowFor(ctx, 'hostname');
  btn.fire('click', {}); await flush();
  assert.ok(visible(popover(ctx)));
  btn.rect = { left: 10, top: -50, right: 34, bottom: -26 };
  ctx.dom.window.fire('scroll');
  assert.ok(!visible(popover(ctx)), 'no anchor on screen, no popover');
});

test('renderSettings and renderSatSettings close the popover before they rebuild the rows', () => {
  for (const name of ['renderSettings', 'renderSatSettings']) {
    const body = extractFunction(name);
    assert.ok(body, name + ' not found');
    assert.match(body, /hideHelp\(\)/, name + ' must call hideHelp()');
  }
});
