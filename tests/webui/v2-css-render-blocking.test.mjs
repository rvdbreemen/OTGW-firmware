// TASK-1643: v2.html must load v2-bundle.css through a render-blocking <link> in the head
// (a script-inserted stylesheet does not block rendering: the page painted unstyled first),
// keep the retry-on-503 with linear backoff (200 ms * attempt, 8 retries) and start v2.js
// only after the stylesheet has loaded (one file at a time, ADR-165/188/147).
//
// The inline scripts of the real v2.html run in node:vm against a minimal fake DOM; the
// test plays the browser by settling each <link> with load or error.
// Run: node --test tests/webui/v2-css-render-blocking.test.mjs
// OTGW_V2_HTML=<file> runs it against another copy (e.g. the old file from git show).
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const FILE = process.env.OTGW_V2_HTML ||
  path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data', 'v2.html');
const HTML = fs.readFileSync(FILE, 'utf8');
const HEAD = HTML.slice(HTML.indexOf('<head>'), HTML.indexOf('</head>'));

function inlineScripts(head) {
  return [...head.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);
}
function staticLinkTag(head) {
  const m = head.match(/<link\s+rel="stylesheet"[^>]*>/);
  return m ? m[0] : null;
}
function attr(tag, name) {
  const m = tag.match(new RegExp('\\s' + name + '="([^"]*)"'));
  return m ? m[1] : null;
}

// Fake browser. Returns { events, timers, settle(link, ok), run() }.
function makeBrowser() {
  const appended = [];            // everything appended to <head> by script, in order
  const timers = [];
  const events = [];              // 'link:<href>' / 'script:<src>' as they hit the head
  const mk = (tag) => {
    const el = { tag, attrs: {}, parentNode: null,
      setAttribute(k, v) { this.attrs[k] = String(v); },
      getAttribute(k) { return k in this.attrs ? this.attrs[k] : null; } };
    return el;
  };
  const head = { appendChild(el) {
    el.parentNode = head; appended.push(el);
    events.push(el.tag + ':' + (el.href || el.src)); return el; },
    removeChild(el) { el.parentNode = null; } };
  let staticEl = null;
  const tag = staticLinkTag(HEAD);
  if (tag) {
    staticEl = mk('link');
    staticEl.href = attr(tag, 'href'); staticEl.rel = 'stylesheet';
    staticEl.id = attr(tag, 'id');
    staticEl._onload = attr(tag, 'onload'); staticEl._onerror = attr(tag, 'onerror');
  }
  const document = {
    head, documentElement: mk('html'),
    createElement: mk,
    getElementById: (id) => (staticEl && staticEl.id === id ? staticEl : null),
  };
  const window = { document, matchMedia: () => ({ matches: false }), FontFace: undefined };
  window.window = window;
  const ctx = vm.createContext({
    window, document, localStorage: { getItem: () => null },
    console: { error() {}, log() {} },
    setTimeout: (fn, ms) => { timers.push({ fn, ms }); return timers.length; },
  });
  // Browser behaviour: fire load/error on a link (attribute handler, then property handler).
  function settle(link, ok) {
    const key = ok ? '_onload' : '_onerror';
    if (link[key]) vm.runInContext('(function(){' + link[key] + '}).call(__el)',
      Object.assign(ctx, { __el: link }));
    const prop = ok ? link.onload : link.onerror;
    if (typeof prop === 'function') prop.call(link);
  }
  function run() { for (const s of inlineScripts(HEAD)) vm.runInContext(s, ctx); }
  return { events, timers, appended, staticEl, settle, run,
           links: () => appended.filter(e => e.tag === 'link'),
           scripts: () => appended.filter(e => e.tag === 'script') };
}

test('head has a render-blocking stylesheet link to v2-bundle.css before any script', () => {
  const tag = staticLinkTag(HEAD);
  assert.ok(tag, 'no static <link rel="stylesheet"> in <head>');
  assert.equal(attr(tag, 'href'), 'v2-bundle.css');
  assert.equal(attr(tag, 'media'), null, 'a media attribute would make it non-blocking');
  assert.equal(attr(tag, 'rel'), 'stylesheet');
  assert.ok(!/\sasync\b|\sdisabled\b|\bpreload\b/.test(tag));
  assert.ok(HEAD.indexOf(tag) < HEAD.indexOf('<script'), 'link must precede the first script');
  assert.ok(!/\?v=/.test(HTML.slice(0, HTML.indexOf('</head>'))), 'no ?v= query (ADR-139)');
});

test('a failing stylesheet is re-added with the 200 ms * attempt backoff, up to 8 retries', () => {
  const b = makeBrowser();
  b.run();
  // first load: the static link (or the loader link on the old file) errors
  let link = b.staticEl || b.links()[0];
  assert.ok(link, 'no stylesheet link');
  b.settle(link, false);
  const delays = [];
  for (let i = 0; i < 8; i++) {
    assert.equal(b.timers.length, i + 1, 'one retry timer per failure');
    delays.push(b.timers[i].ms);
    b.timers[i].fn();
    const re = b.links().at(-1);
    assert.equal(re.href, 'v2-bundle.css', 're-added link points at v2-bundle.css');
    assert.ok(re.parentNode, "re-added link is in the head");
    assert.equal(b.scripts().length, 0, 'v2.js must not start while the stylesheet is failing');
    b.settle(re, false);
  }
  assert.deepEqual(delays, [200, 400, 600, 800, 1000, 1200, 1400, 1600]);
  assert.equal(b.timers.length, 8, 'gives up after 8 retries (no 9th timer)');
});

test('v2.js starts only after the stylesheet has loaded, also after a retry', () => {
  const b = makeBrowser();
  b.run();
  const first = b.staticEl || b.links()[0];
  assert.equal(b.scripts().length, 0, 'v2.js must not start before the stylesheet settled');
  b.settle(first, false);
  assert.equal(b.scripts().length, 0);
  b.timers[0].fn();
  assert.equal(b.scripts().length, 0, 'still waiting for the retried stylesheet');
  b.settle(b.links().at(-1), true);
  assert.equal(b.scripts().length, 1);
  assert.equal(b.scripts()[0].src, './v2.js');
});

test('a stylesheet that finished before the loader script ran still starts v2.js', () => {
  const b = makeBrowser();
  if (!b.staticEl) return;   // old layout has no static link to settle early
  b.settle(b.staticEl, true);   // load fired before the loader script was parsed
  b.run();
  assert.equal(b.scripts().length, 1);
});
