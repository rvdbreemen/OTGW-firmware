// TASK-1703 guard: after a successful one-action update the /update page opens the
// web UI by itself; after a failed one it stays and shows the error.
//
// Runs the REAL second <script> of src/OTGW-firmware/webui/update.html (the DOM wiring:
// go(), redirectToHome(), the beforeunload guard) in a vm context with stub DOM
// elements, a manual timer queue and a fake OTGWUpd. The phase engine itself (U.run:
// the health poll, hash check and version read that decide "ok") is faked here; its
// real behaviour is covered by update-orchestrator.test.mjs.
// Point OTGW_UPDATE_HTML at an older copy of the page to show the gap.
//
// What this cannot prove: a real browser (navigation, the beforeunload dialog), a real
// unit restarting through safeboot, or the timing on a device.
//
// Run: node --test tests/webui/update-auto-return.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const PAGE = process.env.OTGW_UPDATE_HTML || path.join(ROOT, 'src', 'OTGW-firmware', 'webui', 'update.html');
const html = fs.readFileSync(PAGE, 'utf8');

// The DOM script is the first plain <script> after the updLogic script.
function domScript(src) {
  const a = src.indexOf("<script id='updLogic'>");
  if (a < 0) throw new Error('updLogic script not found');
  const s = src.indexOf('<script>', src.indexOf('</script>', a));
  return src.slice(s + '<script>'.length, src.indexOf('</script>', s));
}
const DOM_JS = domScript(html);

function stubEl(id) {
  const handlers = {};
  return {
    id, style: {}, textContent: '', className: '', href: '', disabled: false, files: [],
    handlers,
    addEventListener(t, f) { (handlers[t] = handlers[t] || []).push(f); },
    getElementsByTagName() { return []; },
    appendChild() {}, querySelector() { return stubEl(id + '>q'); },
    parentNode: { replaceChild() {} },
  };
}

async function flush() { for (let i = 0; i < 20; i++) await Promise.resolve(); }

// Build the page in a context; runResult is what the fake U.run resolves to.
async function page(runResult, pathname = '/update') {
  const els = {};
  const timers = [];
  const winHandlers = {};
  const loc = { pathname, protocol: 'http:', hostname: '192.168.1.5', href: 'http://192.168.1.5' + pathname };
  const U = {
    isIPv4: () => true,
    snapshot: () => Promise.resolve({ ok: true, sb: true, appOta: false, host: 'OTGW', ip: '192.168.1.5', ver: '2.0.0', fsMatch: true, net: 'WiFi' }),
    intake: () => Promise.resolve({ app: { u8: new Uint8Array(4), name: 'a.ino.bin' }, fs: { u8: new Uint8Array(4), name: 'a.littlefs.bin' }, E: '2.0.1', G: 'abcdef1' }),
    planFor: () => ({ from: 'handover', info: 'ok', warns: [] }),
    run: () => Promise.resolve(runResult),
  };
  const ctx = {
    OTGWUpd: U,
    location: loc,
    document: {
      getElementById(id) { return (els[id] = els[id] || stubEl(id)); },
      createElement(t) { return stubEl('new-' + t); },
      createTextNode() { return {}; },
    },
    localStorage: { getItem() { return null; }, setItem() {}, removeItem() {} },
    fetch: () => Promise.resolve({ ok: false, status: 404, json: () => Promise.resolve(null) }),
    confirm: () => false,
    console,
    Blob, Uint8Array, Promise, Date, Math, JSON,
    setTimeout(fn, ms) { timers.push({ fn, ms }); return timers.length; },
    clearTimeout() {}, setInterval() { return 0; }, clearInterval() {},
    addEventListener(t, f) { (winHandlers[t] = winHandlers[t] || []).push(f); },
    removeEventListener(t, f) { winHandlers[t] = (winHandlers[t] || []).filter((g) => g !== f); },
  };
  ctx.window = ctx;
  vm.createContext(ctx);
  vm.runInContext(DOM_JS, ctx);
  await flush();                                             // preflight -> snapshot
  els.oneFiles.files = [{ name: 'release.zip' }];
  els.oneFiles.handlers.change[0]();                         // the user picks the zip
  await flush();
  assert.equal(els.oneStart.disabled, false, 'Update button stays disabled');
  els.oneStart.handlers.click[0]();                          // the user presses Update
  await flush();                                             // U.run resolves
  const guardAtEnd = (winHandlers.beforeunload || []).length;
  const fired = [];
  while (timers.length) { const t = timers.shift(); fired.push(t.ms); t.fn(); }
  return { els, loc, guardAtEnd, fired };
}

test('the DOM script parses', () => {
  assert.doesNotThrow(() => new vm.Script(DOM_JS));
});

test('success: the page opens the web UI by itself after a short pause', async () => {
  const p = await page({ ok: true });
  assert.match(p.loc.href, /^\/\?_ota=\d+$/, 'the page did not navigate to the web UI (href ' + p.loc.href + ')');
  assert.equal(p.guardAtEnd, 0, 'the beforeunload guard is still attached, the browser would ask to leave');
  assert.ok(p.fired.some((ms) => ms >= 1000 && ms <= 5000), 'no short pause before the redirect: ' + p.fired.join(','));
  assert.notEqual(p.els.oneOpen.style.display, 'none', 'the open-now link is hidden');
});

test('success behind a path prefix: the redirect keeps the prefix', async () => {
  const p = await page({ ok: true }, '/api/hassio_ingress/tok/update');
  assert.match(p.loc.href, /^\/api\/hassio_ingress\/tok\/\?_ota=\d+$/, 'href ' + p.loc.href);
});

test('failure: the page stays and shows the error', async () => {
  const p = await page({ ok: false, stage: 'fs', code: 'fshttp', msg: 'The unit answered HTTP 500 to the web files.', rec: 'Try again.' });
  assert.equal(p.loc.href, 'http://192.168.1.5/update', 'the page navigated away after a failure');
  assert.equal(p.els.oneErr.textContent, 'The unit answered HTTP 500 to the web files.');
  assert.equal(p.els.oneRec.textContent, 'Try again.');
  assert.notEqual(p.els.oneRetry.style.display, 'none');
  assert.notEqual(p.els.oneBack.style.display, 'none');
});
