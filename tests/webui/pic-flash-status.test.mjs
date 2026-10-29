// TASK-1255 guard: a successful PIC flash must render as success (not "Error: PIC
// upgrade was successful"), refresh the header, the update banner and the PR= table
// one after another with a fresh device/info, and a real failure must still show red.
//
// Runs the REAL shipped pollPicFlash() (plus picFlashOutcome() and picAfterFlash()
// when the file has them): their source is cut out of data/v2.js by brace matching
// and evaluated against stub DOM elements, a scripted fetch and a manual timer queue.
// fetchPic / picUpdatePoll / fetchPicSettings are stubs that record the call order.
// The firmware check reads fwupgradedone() in OTGW-Core.ino.
// Point OTGW_V2_JS / OTGW_CORE_INO at older copies to show the gap.
//
// What this cannot prove: a real PIC flash, when the PIC banner (new version) lands
// relative to the 2.5 s refresh delay, or how long the PR= cycle takes on hardware.
//
// Run: node --test tests/webui/pic-flash-status.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const SRC = path.join(HERE, '..', '..', 'src', 'OTGW-firmware');
const V2JS = process.env.OTGW_V2_JS || path.join(SRC, 'data', 'v2.js');
const CORE = process.env.OTGW_CORE_INO || path.join(SRC, 'OTGW-Core.ino');

// TASK-1491: wait for the chain's end signal instead of a fixed 60/80 ms, so timer latency
// under machine load cannot cut the chain short. The cap only stops a broken chain.
async function waitFor(cond, capMs = 5000) {
  const t0 = Date.now();
  while (!cond() && Date.now() - t0 < capMs) await new Promise((r) => setTimeout(r, 5));
}

// Same brace-matching cutter as settings-error-toast.test.mjs (returns null if absent).
function extractFunction(src, name) {
  const start = src.indexOf('function ' + name + '(');
  if (start < 0) return null;
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

function stubEl() {
  return { textContent: '', style: { width: '', background: '', display: '' } };
}

// statuses: the flash-status answers in order; the last one repeats. The value
// NOT_OK stands for a non-OK reply (for example a 503 from the REST gate).
const NOT_OK = Symbol('not-ok');
function load(statuses) {
  const src = fs.readFileSync(V2JS, 'utf8');
  const poll = extractFunction(src, 'pollPicFlash');
  if (!poll) throw new Error('pollPicFlash not found in ' + V2JS);
  const outcome = extractFunction(src, 'picFlashOutcome');
  const after = extractFunction(src, 'picAfterFlash');
  const legacyLine = (src.match(/var PIC_FLASH_LEGACY_OK = [^;]+;/) || [''])[0];
  // The poll-state declarations as shipped (falls back to the pre-fix line).
  const pollVars = (src.match(/var picPendingPolls = [^;]+;/) || ['var picPendingPolls = 0, PIC_PENDING_MAX = 10;'])[0];
  const els = { picProgBar: stubEl(), picProgTxt: stubEl() };
  const calls = [];
  const timers = [];
  let n = 0;
  const env = {
    document: { getElementById: (id) => els[id] || null },
    fetch: (url) => {
      calls.push('GET ' + url);
      const s = statuses[Math.min(n++, statuses.length - 1)];
      if (s === NOT_OK) return Promise.resolve({ ok: false, status: 503, json: () => Promise.resolve({}) });
      return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve({ flashstatus: s }) });
    },
    setTimeout: (fn, ms) => { timers.push({ fn, ms }); return timers.length; },
    clearTimeout: () => {},
    log: (c) => calls.push(c),
  };
  const body =
    'var APIGW = "/api/", picPollTimer = null, picBusy = true, devInfoTs = 12345;\n' +
    pollVars + '\n' + legacyLine + '\n' +
    // Refresh stubs: record order (and devInfoTs when fetchPic runs), then complete.
    'function fetchPic(done) { log("fetchPic devInfoTs=" + devInfoTs); if (typeof done === "function") Promise.resolve().then(done); }\n' +
    'function picUpdatePoll(done) { log("picUpdatePoll"); if (typeof done === "function") Promise.resolve().then(done); }\n' +
    'function fetchPicSettings(full) { log("fetchPicSettings full=" + (full === true)); }\n' +
    (outcome || '') + '\n' + (after || '') + '\n' + poll + '\n' +
    'return { poll: pollPicFlash, outcome: ' + (outcome ? 'picFlashOutcome' : 'null') + ', busy: function () { return picBusy; } };';
  const api = new Function('document', 'fetch', 'setTimeout', 'clearTimeout', 'log', body)(
    env.document, env.fetch, env.setTimeout, env.clearTimeout, env.log);
  // Drive the poll loop: settle the promise chain, fire due timers, repeat.
  async function run(rounds = 120) {
    api.poll();
    for (let r = 0; r < rounds; r++) {
      await new Promise((res) => setImmediate(res));
      await new Promise((res) => setImmediate(res));
      const due = timers.splice(0);
      if (!due.length) {
        await new Promise((res) => setTimeout(res, 5));
        if (!timers.length) break;
        continue;
      }
      due.forEach((t) => t.fn());
    }
  }
  return { api, els, calls, run };
}

const RUNNING = { flashing: true, progress: 40, filename: 'gateway.hex', error: '' };
const refreshes = (calls) => calls.filter((c) => !c.startsWith('GET '));

test('success as sent by firmware before TASK-1255 (legacy text in error) renders as success', async () => {
  const h = load([RUNNING, { flashing: false, progress: 100, filename: 'gateway.hex', error: 'PIC upgrade was successful' }]);
  await h.run();
  assert.doesNotMatch(h.els.picProgTxt.textContent, /^Error/, 'success must not render as "Error: ..."');
  assert.match(h.els.picProgTxt.textContent, /^Flash complete/);
  assert.notEqual(h.els.picProgBar.style.background, 'var(--hot,#c33)', 'success bar must not be red');
});

test('success from fixed firmware (progress 100, empty error) refreshes header, banner and PR= in order with a fresh device/info', async () => {
  const h = load([RUNNING, { flashing: false, progress: 100, filename: 'gateway.hex', error: '' }]);
  await h.run();
  assert.match(h.els.picProgTxt.textContent, /^Flash complete/);
  assert.equal(h.els.picProgBar.style.width, '100%');
  assert.deepEqual(refreshes(h.calls), ['fetchPic devInfoTs=0', 'picUpdatePoll', 'fetchPicSettings full=true']);
  assert.equal(h.api.busy(), false);
});

test('a real failure still shows the red error and does not run the success refresh', async () => {
  const h = load([RUNNING, { flashing: false, progress: -1, filename: 'gateway.hex', error: 'Bad checksum in hex file' }]);
  await h.run();
  assert.equal(h.els.picProgTxt.textContent, 'Error: Bad checksum in hex file');
  assert.equal(h.els.picProgBar.style.background, 'var(--hot,#c33)');
  const r = refreshes(h.calls);
  assert.ok(!r.includes('picUpdatePoll') && !r.some((c) => c.startsWith('fetchPicSettings')), 'no success refresh on failure: ' + r);
});

test('a poll that lands before the deferred flash starts keeps polling instead of reporting a result', async () => {
  const stale = { flashing: false, progress: 0, filename: '', error: '' };
  const h = load([stale, stale, RUNNING, { flashing: false, progress: 100, filename: 'gateway.hex', error: '' }]);
  await h.run();
  assert.equal(h.calls.filter((c) => c.startsWith('GET ')).length, 4, 'must poll through the not-started window');
  assert.match(h.els.picProgTxt.textContent, /^Flash complete/);
});

test('a flash that never starts ends as an error after the bounded wait', async () => {
  const h = load([{ flashing: false, progress: 0, filename: '', error: '' }]);
  await h.run();
  assert.match(h.els.picProgTxt.textContent, /^Error: /);
  assert.equal(h.api.busy(), false);
});

test('non-OK replies (503 gate) during a real flash do not end as "did not start"', async () => {
  const gated = Array(15).fill(NOT_OK);
  const h = load([RUNNING, ...gated, RUNNING, { flashing: false, progress: 100, filename: 'gateway.hex', error: '' }]);
  await h.run();
  assert.match(h.els.picProgTxt.textContent, /^Flash complete/, 'got: ' + h.els.picProgTxt.textContent);
  assert.equal(h.calls.filter((c) => c.startsWith('GET ')).length, 18);
});

test('a persistent non-OK reply ends with a neutral message, not a red error or a success', async () => {
  const h = load([NOT_OK]);
  await h.run();
  assert.doesNotMatch(h.els.picProgTxt.textContent, /^(Error|Flash complete)/);
  assert.equal(h.els.picProgBar.style.background, '');
  assert.equal(h.api.busy(), false);
});

test('old firmware: a previous flash result "PIC upgrade was successful" before this flash runs is not a success', async () => {
  const prev = { flashing: false, progress: 100, filename: 'gateway.hex', error: 'PIC upgrade was successful' };
  const h = load([prev, prev, RUNNING, { flashing: false, progress: -1, filename: 'gateway.hex', error: 'Too many retries' }]);
  await h.run();
  assert.equal(h.els.picProgTxt.textContent, 'Error: Too many retries');
  assert.ok(!refreshes(h.calls).includes('picUpdatePoll'), 'no success refresh for a stale result');
});

test('picFlashOutcome() decision table', () => {
  const { api } = load([{}]);
  assert.ok(api.outcome, 'picFlashOutcome() missing from ' + V2JS);
  assert.equal(api.outcome({ flashing: true, progress: 5, error: '' }), 'running');
  assert.equal(api.outcome({ flashing: false, progress: 100, error: '' }), 'success');
  assert.equal(api.outcome({ flashing: false, progress: 100, error: 'PIC upgrade was successful' }, true), 'success');
  assert.equal(api.outcome({ flashing: false, progress: 100, error: 'PIC upgrade was successful' }, false), 'pending',
    'legacy text before this flash was seen running is the previous flash result');
  assert.equal(api.outcome({ flashing: false, progress: -1, error: 'Too many retries' }), 'error');
  assert.equal(api.outcome({ flashing: false, progress: -1, error: '' }), 'error');
  assert.equal(api.outcome({ flashing: false, progress: 0, error: '' }), 'pending');
  assert.equal(api.outcome(null), 'pending');
});

test('firmware: fwupgradedone() leaves sError empty on success and restarts the PR= readout', () => {
  const src = fs.readFileSync(CORE, 'utf8');
  const start = src.indexOf('void fwupgradedone(');
  assert.ok(start >= 0, 'fwupgradedone not found');
  const body = src.slice(start, src.indexOf('\nvoid fwupgradestep(', start));
  const ok = body.slice(body.indexOf('*** UPGRADE SUCCESSFUL ***'), body.indexOf('*** UPGRADE FAILED ***'));
  assert.match(ok, /state\.flash\.sError\[0\]\s*=\s*'\\0'/, 'success branch must clear state.flash.sError');
  assert.match(ok, /triggerPICsettingsReadout\(\)/, 'success branch must restart the PR= readout');
});

// The real fetchPic() / fetchPicFiles() / picUpdatePoll() / picAfterFlash() chain:
// each request starts only after the previous one finished (max-2 rule, ADR-165),
// and fetchPicSettings gets the post-flash full=true flag.
test('picAfterFlash() runs the real refresh functions one request at a time', async () => {
  const src = fs.readFileSync(V2JS, 'utf8');
  const names = ['picAfterFlash', 'fetchPic', 'fetchPicFiles', 'picUpdatePoll'];
  const parts = names.map((nm) => extractFunction(src, nm));
  assert.ok(parts.every(Boolean), 'missing one of ' + names.join(', ') + ' in ' + V2JS);
  const log = [];
  let inflight = 0, maxInflight = 0;
  const answers = {
    '/api/v2/firmware/files': [],
    '/api/v2/pic/update-check?recheck=1': { pic_update: { status: 'ready', current: '6.8', latest: '6.8', update_available: false } },
  };
  const fetchStub = (url) => {
    inflight++; maxInflight = Math.max(maxInflight, inflight); log.push('GET ' + url);
    return new Promise((res) => setTimeout(() => { inflight--; res({ ok: true, json: () => Promise.resolve(answers[url]) }); }, 2));
  };
  const body =
    'var APIGW = "/api/", devInfoTs = 999, picGateRetries = 0, picDevVer = "", simActive = false, PIC_NONE_MSG = "";\n' +
    'function withDeviceInfo(cb) { log.push("device/info devInfoTs=" + devInfoTs); cb({ otcommandinterface: "PIC", picfwversion: "6.8" }); }\n' +
    'function txt() {} function renderPicFiles() { log.push("renderPicFiles"); } function renderPicBanner(u) { log.push("banner " + u.current); }\n' +
    'function fetchPicSettings(full) { log.push("fetchPicSettings full=" + (full === true)); }\n' +
    parts.join('\n') + '\nreturn picAfterFlash;';
  const doc = { getElementById: () => null };
  const after = new Function('document', 'fetch', 'log', 'setTimeout', body)(doc, fetchStub, log, setTimeout);
  after();
  await waitFor(() => log.includes('fetchPicSettings full=true'));   // last step of the chain
  assert.deepEqual(log, [
    'device/info devInfoTs=0', 'GET /api/v2/firmware/files', 'renderPicFiles',
    'GET /api/v2/pic/update-check?recheck=1', 'banner 6.8', 'fetchPicSettings full=true',
  ]);
  assert.equal(maxInflight, 1, 'refresh requests must not overlap');
});

// Bench 2026-10-26 (Classic .63, alpha.508): after a real flash the recheck answered
// "checking" and the next GET timed out; picUpdatePoll's catch only finished, so the
// banner stayed on "Checking for updates…". Runs the REAL picUpdatePoll with fast timers.
function runPoll(failures) {
  const src = fs.readFileSync(V2JS, 'utf8');
  const poll = extractFunction(src, 'picUpdatePoll');
  assert.ok(poll, 'picUpdatePoll missing in ' + V2JS);
  const log = [];
  let failsLeft = failures;
  const banner = { className: '', textContent: 'Checking for updates…' };
  const fetchStub = (url) => {
    log.push('GET ' + url);
    if (url.endsWith('?recheck=1')) return Promise.resolve({ ok: true, json: () => Promise.resolve({ pic_update: { status: 'checking' } }) });
    if (failsLeft-- > 0) return Promise.reject(new TypeError('net::ERR_CONNECTION_TIMED_OUT'));
    return Promise.resolve({ ok: true, json: () => Promise.resolve({ pic_update: { status: 'ready', current: '6.8', latest: '6.8', update_available: false } }) });
  };
  const body = 'var APIGW = "/api/";\n' +
    'function renderPicBanner(u) { log.push("render " + (u.status || "")); if (u.status === "ready") banner.textContent = "up to date " + u.current; }\n' +
    poll + '\nreturn picUpdatePoll;';
  const fn = new Function('document', 'fetch', 'setTimeout', 'log', 'banner', body)(
    { getElementById: (id) => (id === 'picBanner' ? banner : null) }, fetchStub, (f) => setTimeout(f, 1), log, banner);
  let done = 0;
  fn(() => { done++; }, true);
  // Wait for the done callback, then a short settle so a second (wrong) call is still seen.
  return waitFor(() => done > 0).then(() => new Promise((r) => setTimeout(r, 30))).then(() => ({ log, banner, done }));
}

test('picUpdatePoll: one timed-out GET after "checking" is retried and the banner shows the result', async () => {
  const h = await runPoll(1);
  assert.equal(h.banner.textContent, 'up to date 6.8', 'banner stuck: ' + h.log.join(' | '));
  assert.equal(h.done, 1, 'done callback runs once');
});

test('picUpdatePoll: a persistent network error ends on "Update check failed", not "Checking…"', async () => {
  const h = await runPoll(99);
  assert.equal(h.banner.textContent, 'Update check failed', 'banner stuck: ' + h.log.join(' | '));
  assert.equal(h.done, 1, 'done callback runs once');
  assert.ok(h.log.filter((l) => l === 'GET /api/v2/pic/update-check').length <= 4, 'bounded retries: ' + h.log.join(' | '));
});

// Post-flash PR= window: with a complete (but old-firmware) cache, a normal tab-open
// read stops after one GET; the post-flash read keeps going so the restarted PR=
// cycle's new values reach the table.
test('readPicSettings(d, true) keeps re-reading a complete cache; the tab-open read stops', async () => {
  const src = fs.readFileSync(V2JS, 'utf8');
  const fn = extractFunction(src, 'readPicSettings');
  const constLine = (src.match(/var picSetTimer = [^;]+;/) || [''])[0];
  assert.ok(fn && constLine);
  async function count(full) {
    let gets = 0; const timers = [];
    const body = constLine + '\nvar APIGW = "/api/";\n' +
      'function isAdvVisible() { return true; } function renderPicSettings() { return 0; }\n' +
      fn + '\nreturn readPicSettings;';
    const fetchStub = () => { gets++; return Promise.resolve({ ok: true, json: () => Promise.resolve({ pic_settings: {} }) }); };
    const read = new Function('document', 'fetch', 'setTimeout', body)(
      { hidden: false, getElementById: () => null }, fetchStub, (f) => { timers.push(f); return timers.length; });
    read({}, full);
    for (let i = 0; i < 30; i++) {
      await new Promise((r) => setImmediate(r)); await new Promise((r) => setImmediate(r));
      const f = timers.shift(); if (!f) break; f();
    }
    return gets;
  }
  assert.equal(await count(false), 1, 'tab-open read stops once nothing is missing');
  assert.ok(await count(true) >= 9, 'post-flash read must cover at least the 45 s PR= cycle');
});
