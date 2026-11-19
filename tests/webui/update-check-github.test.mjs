// TASK-2023 guard: the web UI shows 'New version available' from the GitHub releases API, checked in the BROWSER.
// The gateway makes no outbound request (it stays HTTP-only, ADR-003).
//
// Runs the REAL update-check code cut out of data/v2.js by brace matching:
//   - the UPD_* constants and updParse, updCmp, updPickNewer, updFlag, updShowNotice, updPost, updAsk, maybeUpdateCheck;
//   - the page's ADR-184 /api/ queue (the first IIFE of v2.js), wrapped around a native-fetch stub, so the test also
//     shows that the GitHub request never takes one of the device's 2 REST slots.
// Built-in English of I18N_EN, against a small DOM stub and a localStorage stub.
// Point OTGW_V2_JS at an older copy (git show origin/dev:src/OTGW-firmware/data/v2.js > file) to see it red.
//
// Run: node --test tests/webui/update-check-github.test.mjs
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
function updConstants(src) {
  const a = src.indexOf('  var UPD_API = ');
  const b = src.indexOf('UPD_MAX_AGE_MS = ', a);
  if (a < 0 || b < 0) throw new Error('UPD_* constants not found in ' + V2JS);
  return src.slice(a, src.indexOf('\n', b));
}
function apiQueue(src) {
  const a = src.indexOf('(function () {');
  const b = src.indexOf('})();', a);
  const q = src.slice(a, b + 5);
  if (q.indexOf('__otgwApiQueue') < 0) throw new Error('ADR-184 queue IIFE not found in ' + V2JS);
  return q;
}

const { t } = i18nEn(SRC);

// ---- tiny DOM ----
class El {
  constructor(tag) {
    this.tagName = tag; this.children = []; this.attrs = {}; this.listeners = {}; this.parentNode = null;
    this.textContent = ''; this.id = ''; this.className = ''; this.href = ''; this.title = '';
  }
  appendChild(c) { c.parentNode = this; this.children.push(c); return c; }
  removeChild(c) { this.children = this.children.filter((x) => x !== c); c.parentNode = null; return c; }
  insertBefore(c, ref) {
    c.parentNode = this;
    const i = ref ? this.children.indexOf(ref) : -1;
    if (i < 0) this.children.push(c); else this.children.splice(i, 0, c);
    return c;
  }
  get nextSibling() {
    if (!this.parentNode) return null;
    const s = this.parentNode.children; const i = s.indexOf(this);
    return i >= 0 && i + 1 < s.length ? s[i + 1] : null;
  }
  setAttribute(k, v) { this.attrs[k] = v; }
  addEventListener(e, f) { this.listeners[e] = f; }
  click() { this.listeners.click && this.listeners.click({ target: this }); }
}
function makeDoc() {
  const body = new El('body');
  const net = new El('span'); body.appendChild(net);
  const host = new El('span'); host.id = 'hdrHost'; net.appendChild(host);
  const hnet = new El('span'); hnet.id = 'hdrNet'; net.appendChild(hnet);
  function walk(n, id) {
    if (n.id === id) return n;
    for (const c of n.children) { const r = walk(c, id); if (r) return r; }
    return null;
  }
  return { body, createElement: (tag) => new El(tag), getElementById: (id) => walk(body, id) };
}
function makeStorage(init) {
  const m = new Map(Object.entries(init || {}));
  return { getItem: (k) => (m.has(k) ? m.get(k) : null), setItem: (k, v) => m.set(k, '' + v), _m: m };
}
const flush = async () => { for (let i = 0; i < 20; i++) await new Promise((r) => setImmediate(r)); };

const REL = (tag, extra) => Object.assign({ tag_name: tag, draft: false, prerelease: /-beta\./.test(tag) }, extra || {});
const GITHUB_LIST = [
  REL('v2.0.0-beta.390'), REL('v1.7.9'), REL('v2.0.0-beta.374'), REL('v1.8.0'), REL('v2.0.0-beta.395', { draft: true }),
];

// Sets up the real code with stubs. net: (url, init) -> Promise<Response-like> for the NATIVE fetch.
function page({ settings, fw = '2.0.0-beta.382+e562f82', storage = {}, net }) {
  const doc = makeDoc();
  const ls = makeStorage(storage);
  const calls = [];
  const nativeFetch = (url, init) => { calls.push({ url: '' + url, init: init || {} }); return net(url, init); };
  const window = { fetch: nativeFetch, location: { href: 'http://otgw.local/', pathname: '/', origin: 'http://otgw.local' } };
  window.fetch.bind = () => nativeFetch;
  new Function('window', 'URL', apiQueue(SRC))(window, URL);
  const setData = {};
  Object.entries(settings).forEach(([k, v]) => { setData[k] = { value: v, type: 'b' }; });
  const body = updConstants(SRC) + '\n' +
    ['updParse', 'updCmp', 'updPickNewer', 'updFlag', 'updShowNotice', 'updPost', 'updAsk', 'maybeUpdateCheck']
      .map((n) => extractFunction(SRC, n)).join('\n') +
    '\nreturn { maybeUpdateCheck: maybeUpdateCheck, updPickNewer: updPickNewer };';
  const api = new Function('document', 'localStorage', 'fetch', 'setData', 'withDeviceInfo', 't', 'APIGW', body)(
    doc, ls, window.fetch, setData, (cb) => cb({ fwversion: fw }), t, 'http://otgw.local/api/');
  return { api, doc, ls, calls, setData, queue: window.__otgwApiQueue };
}
const ok = (json) => Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve(json), clone() { return { arrayBuffer: () => Promise.resolve(new ArrayBuffer(0)) }; } });
const status = (code) => Promise.resolve({ ok: false, status: code, json: () => Promise.resolve({ message: 'rate limit' }), clone() { return { arrayBuffer: () => Promise.resolve(new ArrayBuffer(0)) }; } });
const gh = (calls) => calls.filter((c) => c.url.indexOf('https://api.github.com/') === 0);
const notice = (doc) => doc.getElementById('hdrUpd');

test('updPickNewer: semver, major filter, beta/stable rule, drafts', () => {
  const { api } = page({ settings: { updatecheck: false, updatecheckasked: true }, net: () => ok([]) });
  const p = api.updPickNewer;
  assert.equal(p(GITHUB_LIST, '2.0.0-beta.382+e562f82', false), 'v2.0.0-beta.390', 'beta user: newest 2.x beta, draft 395 ignored');
  assert.equal(p(GITHUB_LIST, '2.0.0-beta.390', false), null, 'equal: no notice');
  assert.equal(p(GITHUB_LIST, '2.0.0-beta.391', false), null, 'alpha newer than the latest release: no notice');
  assert.equal(p([REL('v2.0.0')].concat(GITHUB_LIST), '2.0.0-beta.382', false), 'v2.0.0', 'stable 2.0.0 ranks above every beta');
  assert.equal(p(GITHUB_LIST, '2.0.0', false), null, 'stable user: betas ignored');
  assert.equal(p(GITHUB_LIST.concat([REL('v2.0.1-beta.1')]), '2.0.0', true), 'v2.0.1-beta.1', 'stable user who opted in sees betas');
  assert.equal(p([REL('v2.0.1')].concat(GITHUB_LIST), '2.0.0', false), 'v2.0.1', 'stable user sees a newer stable');
  assert.equal(p([REL('v1.9.0'), REL('v3.0.0-beta.1')], '2.0.0-beta.5', false), null, 'other majors (1.x, 3.x) ignored');
  assert.equal(p([REL('v2.0.0-beta.10')], '2.0.0-beta.9', false), 'v2.0.0-beta.10', 'beta.N compares numerically');
  assert.equal(p([REL('v2.0.0-beta.400<img>'), { tag_name: 7 }, null], '2.0.0-beta.1', false), null, 'odd tags ignored');
  assert.equal(p(GITHUB_LIST, 'garbage', false), null, 'unparsable running version: nothing');
});

test('setting off and question answered: no request to GitHub, no dialog', async () => {
  const s = page({ settings: { updatecheck: false, updatecheckasked: true, ui_onboarded: true }, net: () => ok(GITHUB_LIST) });
  s.api.maybeUpdateCheck(); await flush();
  assert.equal(gh(s.calls).length, 0);
  assert.equal(s.calls.length, 0, 'no request at all');
  assert.equal(s.doc.getElementById('updBackdrop'), null);
  assert.equal(notice(s.doc), null);
});

test('setting off, not asked yet: the one-time question, still no request to GitHub', async () => {
  const s = page({ settings: { updatecheck: false, updatecheckasked: false, ui_onboarded: true }, net: () => ok({}) });
  s.api.maybeUpdateCheck(); await flush();
  const bd = s.doc.getElementById('updBackdrop');
  assert.ok(bd, 'the question is shown');
  assert.equal(gh(s.calls).length, 0, 'no GitHub request before consent');
  // "No": the setting stays off and the asked flag is written, one POST at a time, in this order.
  const buttons = bd.children[0].children[2].children;
  buttons[0].click(); await flush();
  const posts = s.calls.map((c) => JSON.parse(c.init.body));
  assert.deepEqual(posts, [{ name: 'updatecheck', value: false }, { name: 'updatecheckasked', value: true }]);
  assert.equal(gh(s.calls).length, 0);
});

test('the question waits for the first-time wizard', async () => {
  const s = page({ settings: { updatecheck: false, updatecheckasked: false, ui_onboarded: false }, net: () => ok({}) });
  s.api.maybeUpdateCheck(); await flush();
  assert.equal(s.doc.getElementById('updBackdrop'), null);
});

test('"Yes" switches it on and checks at once', async () => {
  const s = page({ settings: { updatecheck: false, updatecheckasked: false, ui_onboarded: true },
    net: (url) => ok(('' + url).indexOf('api.github.com') >= 0 ? GITHUB_LIST : {}) });
  s.api.maybeUpdateCheck(); await flush();
  s.doc.getElementById('updBackdrop').children[0].children[2].children[1].click(); await flush();
  assert.equal(gh(s.calls).length, 1, 'one GitHub request after consent');
  assert.equal(notice(s.doc).children[0].textContent, 'New version available: v2.0.0-beta.390');
});

test('setting on: one GitHub request, outside the device queue, privacy-safe; the notice links the release', async () => {
  let seenInFlight = -1, s;
  s = page({ settings: { updatecheck: true, updatecheckasked: true },
    net: (url) => { if (('' + url).indexOf('api.github.com') >= 0) seenInFlight = s.queue.inFlight(); return ok(GITHUB_LIST); } });
  s.api.maybeUpdateCheck(); await flush();
  const g = gh(s.calls);
  assert.equal(g.length, 1);
  assert.equal(g[0].url, 'https://api.github.com/repos/rvdbreemen/OTGW-firmware/releases?per_page=30', 'not /releases/latest (that is the 1.x line)');
  assert.equal(g[0].init.credentials, 'omit');
  assert.equal(g[0].init.referrerPolicy, 'no-referrer', 'the gateway address is not sent to GitHub');
  assert.equal(seenInFlight, 0, 'the GitHub request takes no device REST slot (ADR-184 queue untouched)');
  const n = notice(s.doc);
  assert.ok(n, 'notice shown');
  assert.equal(n.children[0].textContent, 'New version available: v2.0.0-beta.390');
  assert.equal(n.children[0].href, 'https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v2.0.0-beta.390');
  assert.equal(n.children[0].rel, 'noopener noreferrer');
  // Cached: a second page start within 6 h makes no request.
  const s2 = page({ settings: { updatecheck: true, updatecheckasked: true }, storage: Object.fromEntries(s.ls._m), net: () => ok(GITHUB_LIST) });
  s2.api.maybeUpdateCheck(); await flush();
  assert.equal(gh(s2.calls).length, 0, 'cached for 6 h');
  assert.ok(notice(s2.doc), 'notice from the cache');
});

test('dismiss hides this version only', async () => {
  const s = page({ settings: { updatecheck: true, updatecheckasked: true }, net: () => ok(GITHUB_LIST) });
  s.api.maybeUpdateCheck(); await flush();
  notice(s.doc).children[1].click();
  assert.equal(notice(s.doc), null);
  const s2 = page({ settings: { updatecheck: true, updatecheckasked: true }, storage: Object.fromEntries(s.ls._m), net: () => ok(GITHUB_LIST) });
  s2.api.maybeUpdateCheck(); await flush();
  assert.equal(notice(s2.doc), null, 'the dismissed tag stays hidden');
  const newer = [REL('v2.0.0-beta.391')];
  const s3 = page({ settings: { updatecheck: true, updatecheckasked: true }, storage: { 'otgw-upd-dismissed': 'v2.0.0-beta.390' }, net: () => ok(newer) });
  s3.api.maybeUpdateCheck(); await flush();
  assert.ok(notice(s3.doc), 'a newer one shows again');
});

for (const [name, net] of [['rate-limited (403)', () => status(403)], ['rate-limited (429)', () => status(429)],
  ['unreachable', () => Promise.reject(new TypeError('Failed to fetch'))], ['not a list', () => ok({ message: 'x' })]]) {
  test('GitHub ' + name + ': quiet, no notice, no retry before 6 h', async () => {
    const s = page({ settings: { updatecheck: true, updatecheckasked: true }, net });
    s.api.maybeUpdateCheck(); await flush();
    assert.equal(notice(s.doc), null);
    assert.equal(gh(s.calls).length, 1);
    const s2 = page({ settings: { updatecheck: true, updatecheckasked: true }, storage: Object.fromEntries(s.ls._m), net });
    s2.api.maybeUpdateCheck(); await flush();
    assert.equal(gh(s2.calls).length, 0, 'the failure is remembered for 6 h');
  });
}

test('switched on in Settings: the question is marked answered once', async () => {
  const s = page({ settings: { updatecheck: true, updatecheckasked: false }, net: (url) => ok(('' + url).indexOf('github') >= 0 ? [] : {}) });
  s.api.maybeUpdateCheck(); await flush();
  const posts = s.calls.filter((c) => c.url.indexOf('/api/v2/settings') >= 0).map((c) => JSON.parse(c.init.body));
  assert.deepEqual(posts, [{ name: 'updatecheckasked', value: true }]);
});

test('a firmware without the setting: nothing happens', async () => {
  const s = page({ settings: {}, net: () => ok(GITHUB_LIST) });
  s.api.maybeUpdateCheck(); await flush();
  assert.equal(s.calls.length, 0);
});
