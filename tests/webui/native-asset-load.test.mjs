// Drives the shipped boot code, not a copied loader. Run after generating v2-bundle.css:
// node --test tests/webui/native-asset-load.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';
const paths = ['/v2-bundle.css', '/v2.js', '/fonts/inter-400.woff2',
  '/fonts/inter-700.woff2', '/fonts/jetbrains-mono-400.woff2'];
const capture = `window.__assetEvents = []; window.__fontConstructions = 0;
for (const level of ['info', 'warn', 'error']) {
  const original = console[level];
  console[level] = function(name, detail) {
    if (/^asset_/.test(name)) window.__assetEvents.push({name, detail});
    return original.apply(console, arguments);
  };
}
if (window.FontFace) window.FontFace = new Proxy(window.FontFace, {
  construct(target, args) { window.__fontConstructions++; return Reflect.construct(target, args); }
});`;
const record = (h) => h.evalJs('window.otgwAssetLoad');
const events = (h) => h.evalJs('window.__assetEvents');
const requests = (h, p) => h.events.filter((e) => e.method === 'Network.requestWillBeSent'
  && new URL(e.params.request.url).pathname === p);
async function completed(h) {
  assert.ok(await h.until('window.otgwAssetLoad && window.otgwAssetLoad.complete', 15000),
    'all five assets must reach a terminal state');
  return record(h);
}
function failureServer(failPath, count) {
  let hits = 0;
  return (p, q, s) => {
    if (p !== failPath || ++hits > count) return false;
    s.writeHead(503, { 'content-type': 'text/plain', 'cache-control': 'no-store' });
    s.end('temporarily unavailable');
    return true;
  };
}

test('native first attempts complete once, reuse preloaded fonts and reset on reload', { skip }, async () => {
  const h = await startPage({ beforeScript: capture });
  try {
    const r = await completed(h);
    assert.equal(r.mode, 'native');
    assert.equal(r.cleanFirstAttempt, true);
    assert.equal(r.retryCount, 0);
    assert.equal(r.failureCount, 0);
    assert.deepEqual(Object.keys(r.assets).sort(), paths.map((p) => p.slice(1)).sort());
    for (const p of paths) {
      assert.equal(r.assets[p.slice(1)].attempts, 1, p);
      assert.equal(r.assets[p.slice(1)].retries, 0, p);
      assert.equal(r.assets[p.slice(1)].state, 'loaded', p);
      assert.equal(requests(h, p).length, 1, p + ' must download only once');
      assert.equal(requests(h, p)[0].params.initiator.type, 'parser', p + ' must be parser-discovered');
    }
    assert.equal(await h.evalJs('window.__fontConstructions'), 0, 'clean startup must not construct FontFace objects');
    const faces = await h.evalJs(`Promise.all([document.fonts.load('400 12px Inter'), document.fonts.load('700 12px Inter'), document.fonts.load('400 12px "JetBrains Mono"')]).then(groups => groups.map(g => g.length))`);
    assert.deepEqual(faces, [1, 1, 1], 'the bundle must retain all three usable font faces');
    for (const p of paths.slice(2)) assert.equal(requests(h, p).length, 1, 'CSS must reuse the preload: ' + p);
    assert.deepEqual((await events(h)).map((e) => e.name), ['asset_load_summary']);
    await h.evalJs(`for (const id of ['v2Stylesheet', 'v2Script', 'fontInter400', 'fontInter700', 'fontMono400']) {
      const node = document.getElementById(id);
      node.dispatchEvent(new Event('error'));
      node.dispatchEvent(new Event('load'));
    }`);
    assert.deepEqual(await record(h), r, 'late events cannot change completed first attempts');
    assert.deepEqual((await events(h)).map((e) => e.name), ['asset_load_summary']);
    const firstId = r.pageId;
    await h.send('Page.reload', { ignoreCache: true });
    assert.ok(await h.until(`window.otgwAssetLoad && window.otgwAssetLoad.pageId !== ${JSON.stringify(firstId)} && window.otgwAssetLoad.complete`));
    const next = await record(h);
    assert.equal(next.cleanFirstAttempt, true);
    assert.equal(next.retryCount, 0);
    assert.deepEqual((await events(h)).map((e) => e.name), ['asset_load_summary']);
  } finally { await h.close(); }
});

for (const failPath of ['/v2-bundle.css', '/v2.js', '/fonts/inter-400.woff2']) {
  test('native failure recovers without calling it clean: ' + failPath, { skip }, async () => {
    const h = await startPage({ beforeScript: capture, asset: failureServer(failPath, 1) });
    try {
      const r = await completed(h);
      assert.equal(r.cleanFirstAttempt, false);
      assert.ok(r.retryCount > 0);
      assert.ok(r.failureCount > 0);
      const a = r.assets[failPath.slice(1)];
      assert.equal(a.state, 'loaded');
      assert.equal(a.attempts, 2);
      assert.equal(a.retries, 1);
      const rows = await events(h);
      assert.equal(rows.filter((e) => e.name === 'asset_recovered' && e.detail.asset === failPath.slice(1)).length, 1);
      assert.equal(rows.filter((e) => e.name === 'asset_load_summary').length, 1);
      for (const e of rows) {
        assert.equal(e.detail.pageId, r.pageId);
        assert.equal(e.detail.status, undefined, 'load/error cannot reveal an HTTP status');
      }
      assert.equal(await h.evalJs(`document.querySelectorAll('p[role="alert"]').length`), 0);
    } finally { await h.close(); }
  });
}

for (const failPath of ['/v2-bundle.css', '/v2.js', '/fonts/inter-400.woff2']) {
  test('exhaustion is terminal and visible: ' + failPath, { skip }, async () => {
    const h = await startPage({ beforeScript: capture, asset: failureServer(failPath, Infinity) });
    try {
      const r = await completed(h);
      const a = r.assets[failPath.slice(1)];
      assert.equal(a.state, 'failed');
      assert.equal(a.attempts, 9);
      assert.equal(a.retries, 8);
      assert.equal(r.cleanFirstAttempt, false);
      const rows = await events(h);
      assert.equal(rows.filter((e) => e.name === 'asset_failed').length, 1);
      assert.equal(rows.filter((e) => e.name === 'asset_load_summary').length, 1);
      assert.ok(await h.evalJs(`Array.from(document.querySelectorAll('p[role="alert"]')).some(el => el.textContent.includes(${JSON.stringify(failPath.slice(1))}) && el.getBoundingClientRect().height > 0)`));
      // Late/duplicate native events cannot erase a failure, retry again or initialize twice.
      await h.evalJs(`document.getElementById(${JSON.stringify(failPath === '/v2.js' ? 'v2Script' : failPath === '/v2-bundle.css' ? 'v2Stylesheet' : 'fontInter400')})?.dispatchEvent(new Event('load'))`);
      assert.deepEqual(await record(h), r);
      assert.deepEqual(await events(h), rows);
    } finally { await h.close(); }
  });
}

test('an unfinished native request can never be clean', { skip }, async () => {
  let held;
  const h = await startPage({ beforeScript: capture, page: '/v2.html', asset(p, q, s) {
    if (p !== '/fonts/inter-400.woff2') return false;
    held = s;
    return true;
  } });
  try {
    const r = await record(h);
    assert.ok(r, 'debug record exists before completion');
    assert.equal(r.complete, false);
    assert.equal(r.cleanFirstAttempt, false);
    assert.equal(r.assets['fonts/inter-400.woff2'].state, 'loading');
    assert.equal((await events(h)).filter((e) => e.name === 'asset_load_summary').length, 0);
  } finally { if (held) held.destroy(); await h.close(); }
});

test('missing FontFace support is skipped, not falsely reported as clean fonts', { skip }, async () => {
  const h = await startPage({ beforeScript: capture + '\nwindow.FontFace = undefined;' });
  try {
    const r = await completed(h);
    assert.equal(r.cleanFirstAttempt, false);
    for (const p of paths.slice(2)) assert.equal(r.assets[p.slice(1)].state, 'skipped');
    assert.equal((await events(h)).filter((e) => e.name === 'asset_load_summary').length, 1);
  } finally { await h.close(); }
});
