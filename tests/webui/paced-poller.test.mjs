// TASK-1037 regression guard for the paced pollers in data/index.js (ADR-173).
//
//   D2  A 429 Retry-After is trusted only up to 4 poll periods. Before the fix a
//       Retry-After of 1702968 s (what the firmware sent after a 30-day idle,
//       see test/host/test_rate_limit_gcra.cpp) parked the poller for 19.7 days.
//   D3  stop() is sticky. Before the fix an in-flight device/time request re-armed
//       the poller after a hidden-tab stop, and a stop() + start() inside one
//       in-flight window put two device/time requests in flight at once.
//   D4  The stale marker restores the element's own title. Before the fix
//       #heap-info lost its index.html tooltip after one stale episode.
//
// This drives the REAL shipped index.js in headless system Chrome over the
// DevTools protocol. A local server serves the data dir plus a scripted /api/v2:
// the test switches the otmonitor and device/time answers (status, Retry-After,
// delay) and logs every request with its start time and concurrency. The page's
// own pollers, visibility handler and stale marker do the rest.
//
// Zero dependencies: system Chrome, Node's built-in fetch and WebSocket. Ports
// are ephemeral (HTTP listen(0), CDP --remote-debugging-port=0), so it can run
// beside other harnesses.
//
// Run: node tests/webui/paced-poller.test.mjs
//   OTGW_DATA_DIR  data dir to serve (default src/OTGW-firmware/data)
//   OTGW_INDEX_JS  serve this file as /index.js instead, e.g. an older revision
//                  from `git show <rev>:src/OTGW-firmware/data/index.js`, for an
//                  old-vs-fix run. Matched on the path, so ?v=<hash> still hits it.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const INDEX_JS = process.env.OTGW_INDEX_JS || '';

const CHROME_CANDIDATES = [
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/usr/bin/google-chrome',
  '/usr/bin/chromium',
];
const CHROME = CHROME_CANDIDATES.find((p) => fs.existsSync(p));
if (!CHROME) {
  console.log('SKIP: no Chrome/Edge binary found; cannot run the browser harness');
  process.exit(0);
}
if (INDEX_JS && !fs.existsSync(INDEX_JS)) {
  console.error('FATAL: OTGW_INDEX_JS does not exist: ' + INDEX_JS);
  process.exit(2);
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const MIME = {
  '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css',
  '.json': 'application/json', '.svg': 'image/svg+xml', '.ico': 'image/x-icon', '.png': 'image/png',
};

// ---------------------------------------------------------------- fake device
// What the fake device answers. The test mutates `mode` between phases.
const DEVTIME_BODY = JSON.stringify({ devtime: {
  dateTime: '2026-09-30 12:00:00', epoch: 1790769600, message: '', freeheap: 123456, maxfreeblock: 65536,
} });
const OTMON_BODY = JSON.stringify({ otmonitor: {} });

let mode, log, active, maxActive;
function resetFakeDevice() {
  mode = {
    devtime: { status: 200, delayMs: 0, retryAfter: 0 },
    otmon:   { status: 200, delayMs: 0, retryAfter: 0 },
  };
  log = [];                                // { t, ep, status }
  active = { devtime: 0, otmon: 0 };       // requests received and not yet answered
  maxActive = { devtime: 0, otmon: 0 };
}
resetFakeDevice();

function answerPolled(ep, okBody, res) {
  const m = Object.assign({}, mode[ep]);   // the mode at arrival decides the answer
  log.push({ t: Date.now(), ep, status: m.status });
  active[ep]++;
  maxActive[ep] = Math.max(maxActive[ep], active[ep]);
  const answer = () => {
    active[ep]--;
    const headers = { 'cache-control': 'no-store' };
    let body = okBody;
    if (m.status === 429) {
      headers['content-type'] = 'application/problem+json';
      headers['retry-after'] = String(m.retryAfter);
      body = JSON.stringify({ status: 429, title: 'Rate limit exceeded', retry_after: m.retryAfter });
    } else if (m.status !== 200) {
      headers['content-type'] = 'application/json';
      body = JSON.stringify({ error: { status: m.status, message: 'fake device' } });
    } else {
      headers['content-type'] = 'application/json';
    }
    try { res.writeHead(m.status, headers); res.end(body); } catch { /* client went away */ }
  };
  if (m.delayMs > 0) setTimeout(answer, m.delayMs); else answer();
}

const srv = http.createServer((q, s) => {
  const url = new URL(q.url, 'http://127.0.0.1');
  let p = decodeURIComponent(url.pathname);
  if (p === '/api/v2/device/time')    return answerPolled('devtime', DEVTIME_BODY, s);
  if (p === '/api/v2/otgw/otmonitor') return answerPolled('otmon', OTMON_BODY, s);
  if (p.indexOf('/api/') === 0) {
    s.writeHead(200, { 'content-type': 'application/json', 'cache-control': 'no-store' });
    s.end('{}');
    return;
  }
  if (p === '/') p = '/index.html';
  const f = (INDEX_JS && p === '/index.js') ? INDEX_JS : path.join(DATA, p);
  if (fs.existsSync(f) && fs.statSync(f).isFile()) {
    s.writeHead(200, { 'content-type': MIME[path.extname(f)] || 'application/octet-stream', 'cache-control': 'no-store' });
    fs.createReadStream(f).pipe(s);
  } else {
    s.writeHead(404);
    s.end('nf');
  }
});
await new Promise((r) => srv.listen(0, '127.0.0.1', r));
const PORT = srv.address().port;

// ---------------------------------------------------- headless Chrome over CDP
const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'otgw-cdp-'));
const chrome = spawn(CHROME, [
  '--headless=new',
  '--remote-debugging-port=0',
  `--user-data-dir=${profile}`,
  '--no-first-run', '--no-default-browser-check', '--disable-gpu',
  'about:blank',
], { stdio: 'ignore' });

let ws = null;
let ok = true;
const results = [];
function check(pass, msg) {
  const line = (pass ? 'PASS' : 'FAIL') + ': ' + msg;
  results.push(line);
  console.log(line);
  if (!pass) ok = false;
}

function cleanup() {
  try { if (ws) ws.close(); } catch { /* ignore */ }
  try { chrome.kill(); } catch { /* ignore */ }
  try { srv.close(); } catch { /* ignore */ }
  try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* ignore */ }
}

try {
  let cdpPort = 0;
  for (let i = 0; i < 150 && !cdpPort; i++) {
    try { cdpPort = parseInt(fs.readFileSync(path.join(profile, 'DevToolsActivePort'), 'utf8').split('\n')[0], 10) || 0; }
    catch { /* not written yet */ }
    if (!cdpPort) await sleep(100);
  }
  if (!cdpPort) throw new Error('Chrome never wrote DevToolsActivePort');

  let target = null;
  for (let i = 0; i < 50 && !target; i++) {
    try {
      const r = await fetch(`http://127.0.0.1:${cdpPort}/json/new?about:blank`, { method: 'PUT' });
      if (r.ok) target = await r.json();
    } catch { /* not up yet */ }
    if (!target) await sleep(200);
  }
  if (!target) throw new Error('Chrome DevTools endpoint never came up');

  ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });

  let msgId = 0;
  const pending = new Map();
  const pageErrors = [];
  ws.addEventListener('message', (ev) => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); return; }
    if (m.method === 'Runtime.exceptionThrown') {
      const d = m.params.exceptionDetails || {};
      pageErrors.push((d.exception && d.exception.description) || d.text || 'exception');
    }
  });
  const send = (method, params = {}) => {
    const id = ++msgId;
    return new Promise((res) => { pending.set(id, res); ws.send(JSON.stringify({ id, method, params })); });
  };
  const evalJs = async (expression) => {
    const r = await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
    if (r.result && r.result.exceptionDetails) throw new Error(JSON.stringify(r.result.exceptionDetails));
    return r.result && r.result.result ? r.result.result.value : undefined;
  };
  const waitFor = async (pred, timeoutMs, stepMs = 50) => {
    const end = Date.now() + timeoutMs;
    while (Date.now() < end) {
      if (await pred()) return true;
      await sleep(stepMs);
    }
    return false;
  };

  await send('Page.enable');
  await send('Runtime.enable');
  // Chrome's HTTP cache holds a second GET for a URL while the first one is still
  // in flight (cache lock), which hides an overlapping fetch() from the fake
  // device. With the cache disabled, as DevTools' "Disable cache" does, every
  // fetch() the page issues reaches the device when it is issued.
  await send('Network.enable');
  await send('Network.setCacheDisabled', { cacheDisabled: true });

  // Fresh page per case: a case may leave the page hidden or paced. Let the
  // direct init fetches (initMainPage/showMainPage call refreshDevTime and
  // refreshOTmonitor outside the pollers) finish, and let both pollers take over,
  // before a case flips the fake device. Counting only starts after the flip.
  async function freshPage(label) {
    await waitFor(() => active.devtime === 0 && active.otmon === 0, 10000);
    resetFakeDevice();
    await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/index.html` });
    const loaded = await waitFor(async () => {
      try { return (await evalJs('document.readyState')) === 'complete' && (await evalJs('typeof devTimePoller')) === 'object'; }
      catch { return false; }
    }, 20000, 100);
    if (!loaded) throw new Error(label + ': index.html never finished loading');
    await sleep(7000);   // > one device/time period (5 s) and three otmonitor periods (2 s)
    const vis = await evalJs('document.visibilityState');
    const since = Date.now() - 6500;
    const polled = log.filter((e) => e.t >= since);
    if (vis !== 'visible' || !polled.some((e) => e.ep === 'devtime') || !polled.some((e) => e.ep === 'otmon')) {
      throw new Error(`${label}: pollers not running before the case (visibility ${vis}, ` +
        `requests in the last 6.5 s: ${polled.map((e) => e.ep).join(',') || 'none'})`);
    }
  }

  const fmtOffsets = (entries, t0) => entries.map((e) => '+' + ((e.t - t0) / 1000).toFixed(1) + 's').join(' ');

  // ------------------------------------------------------------------ D2
  {
    await freshPage('D2');
    const RETRY_AFTER = 1702968;           // s: 19.7 days
    const tFlip = Date.now();
    mode.otmon = { status: 429, delayMs: 0, retryAfter: RETRY_AFTER };
    await waitFor(() => log.some((e) => e.ep === 'otmon' && e.t >= tFlip), 6000);
    const first = log.find((e) => e.ep === 'otmon' && e.t >= tFlip);
    if (!first) throw new Error('D2: no otmonitor poll within 6 s of the flip');
    await sleep(Math.max(0, first.t + 24000 - Date.now()));
    const later = log.filter((e) => e.ep === 'otmon' && e.t > first.t);
    const stale = await evalJs(`(function(){var e=document.querySelector('.otmontable');return e?e.hasAttribute('data-stale'):'no .otmontable';})()`);
    const gaps = [];
    let prev = first.t;
    for (const e of later) { gaps.push(e.t - prev); prev = e.t; }
    check(later.length >= 2,
      `D2: after a 429 with Retry-After ${RETRY_AFTER} s (19.7 d), otmonitor is polled again within 24 s: ` +
      `${later.length} request(s) ${fmtOffsets(later, first.t)} (want >= 2)`);
    check(gaps.length > 0 && gaps.every((g) => g >= 7500 && g <= 10500),
      `D2: each retry waits the capped 4 x 2 s plus up to one period of jitter (8-10 s): gaps ` +
      `${gaps.map((g) => (g / 1000).toFixed(1) + 's').join(' ') || 'none'}`);
    check(stale === true, `D2: .otmontable carries data-stale after three refusals (got ${stale})`);
  }

  // ------------------------------------------------------------------ D3a
  {
    await freshPage('D3a');
    mode.devtime = { status: 200, delayMs: 3000, retryAfter: 0 };
    const inflight = await waitFor(() => active.devtime > 0, 8000);
    const tStop = Date.now();
    // Shadow the visibility getters on the document instance and fire the event:
    // the page's own visibilitychange handler (index.js) then stops the pollers.
    const vis = await evalJs(`(function(){
      Object.defineProperty(document, 'hidden', { configurable: true, get: function () { return true; } });
      Object.defineProperty(document, 'visibilityState', { configurable: true, get: function () { return 'hidden'; } });
      document.dispatchEvent(new Event('visibilitychange'));
      return document.visibilityState;
    })()`);
    await sleep(20000);
    const after = log.filter((e) => e.ep === 'devtime' && e.t > tStop);
    check(inflight && vis === 'hidden', `D3a: a device/time request was in flight when the tab went hidden (in flight ${inflight}, visibility ${vis})`);
    check(after.length === 0,
      `D3a: no device/time request starts in the 20 s after the hidden-tab stop: ${after.length} ${fmtOffsets(after, tStop)} (want 0)`);
  }

  // ------------------------------------------------------------------ D3b
  {
    await freshPage('D3b');
    mode.devtime = { status: 200, delayMs: 7000, retryAfter: 0 };   // longer than the 5 s period
    const inflight = await waitFor(() => active.devtime > 0, 8000);
    maxActive.devtime = active.devtime;
    const tCycle = Date.now();
    await evalJs(`(function(){ stopTimeUpdates(); startTimeUpdates(); return true; })()`);
    await sleep(12000);
    const after = log.filter((e) => e.ep === 'devtime' && e.t > tCycle);
    check(inflight, 'D3b: a device/time request was in flight at stop() + start()');
    check(maxActive.devtime === 1,
      `D3b: stop() + start() inside one in-flight window never has two device/time requests in flight: ` +
      `max ${maxActive.devtime} (want 1); new requests ${fmtOffsets(after, tCycle) || 'none'}`);
  }

  // ------------------------------------------------------------------ D4
  {
    await freshPage('D4');
    const q = (sel, attr) => evalJs(`(function(){var e=document.querySelector(${JSON.stringify(sel)});return e?e.getAttribute(${JSON.stringify(attr)}):'(no element)';})()`);
    const origHeap = await q('#heap-info', 'title');
    const origTime = await q('#theTime', 'title');
    mode.devtime = { status: 503, delayMs: 0, retryAfter: 0 };
    const staleOn = await waitFor(async () => (await q('#heap-info', 'data-stale')) !== null, 30000, 250);
    const titleStale = await q('#heap-info', 'title');
    mode.devtime = { status: 200, delayMs: 0, retryAfter: 0 };
    const staleOff = await waitFor(async () => (await q('#heap-info', 'data-stale')) === null, 15000, 250);
    const titleHeap = await q('#heap-info', 'title');
    const titleTime = await q('#theTime', 'title');
    check(typeof origHeap === 'string' && origHeap.length > 0 && origTime === null,
      `D4: before the episode #heap-info has its own title and #theTime has none (${JSON.stringify(origHeap)}, ${JSON.stringify(origTime)})`);
    check(staleOn && titleStale !== origHeap,
      `D4: after three device/time 503s #heap-info is stale with the pacing title (stale ${staleOn}, title ${JSON.stringify(titleStale)})`);
    check(staleOff, 'D4: data-stale is cleared on the first success');
    check(titleHeap === origHeap,
      `D4: #heap-info gets its own title back after the episode (got ${JSON.stringify(titleHeap)})`);
    check(titleTime === null,
      `D4: #theTime ends the episode with no title, as it started (got ${JSON.stringify(titleTime)})`);
  }

  if (pageErrors.length) {
    console.log(`INFO: ${pageErrors.length} uncaught page exception(s), first: ${pageErrors[0]}`);
  }
} catch (e) {
  check(false, 'harness error: ' + (e && e.message ? e.message : e));
} finally {
  cleanup();
}

console.log(`\nindex.js served from: ${INDEX_JS || path.join(DATA, 'index.js')}`);
console.log(results.filter((l) => l.startsWith('FAIL')).length + ' failures of ' + results.length + ' checks');
process.exit(ok ? 0 : 1);
