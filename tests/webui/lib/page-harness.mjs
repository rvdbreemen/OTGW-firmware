// Shared harness for browser tests of the REAL v2.html/v2.js in headless system Chrome over
// CDP, against a fake device (same approach as v2-settings-sat-followups.test.mjs).
// OTGW_DATA_DIR points the fake device at an older copy of data/ to see a test red.
//
//   const h = await startPage({ api: (method, p, search, body) => jsonOrUndefined, width: 1280 });
//   h.evalJs(expr) / h.until(expr[, ms = 15000]) / h.shot(file) / h.log / h.exceptions / await h.close()
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawn, spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
export const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', '..', 'src', 'OTGW-firmware', 'data');
const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.svg': 'image/svg+xml' };
export const CHROME = [
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/usr/bin/google-chrome',
  '/usr/bin/chromium',
].find((p) => fs.existsSync(p));
export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
// Deadline for one CDP call (TASK-1715); OTGW_CDP_TIMEOUT_MS overrides it.
export const CDP_TIMEOUT_MS = Number(process.env.OTGW_CDP_TIMEOUT_MS) || 20000;

// TASK-1739: on Windows child.kill() ends only Chrome's browser process; its renderer, GPU and
// utility children stay behind (28 orphans seen on 2026-11-08), so stop the whole tree.
// guardChrome() also does that when the test exits early (a throw or process.exit).
export function stopChromeTree(child) {
  if (!child || child.exitCode !== null) return;
  if (process.platform === 'win32' && child.pid) {
    try { spawnSync('taskkill', ['/PID', String(child.pid), '/T', '/F'], { stdio: 'ignore', timeout: 10000 }); } catch { /* ignore */ }
  }
  try { child.kill(); } catch { /* ignore */ }
}
// TASK-1765: guardChrome() stops the tree on exit and on SIGINT/SIGTERM/SIGHUP, and frees the
// Chrome slot (below) when Chrome ends. Idempotent per child.
const guarded = new WeakSet();
export function guardChrome(child, slotFile) {
  if (!child || guarded.has(child)) return child;
  guarded.add(child);
  const release = () => { if (slotFile) { try { fs.rmSync(slotFile, { force: true }); } catch { /* ignore */ } } };
  process.on('exit', () => { stopChromeTree(child); release(); });
  for (const sig of ['SIGINT', 'SIGTERM', 'SIGHUP']) {
    process.once(sig, () => { stopChromeTree(child); release(); process.exit(130); });
  }
  child.once('exit', release);
  return child;
}

// TASK-1765: machine-wide cap on concurrent test Chromes. `node --test tests/webui/*.test.mjs`
// runs every file at once, each with its own Chrome tree (638 chrome.exe, 23 GB, 2026-11-09);
// with the cap the browser files run one at a time. Default 1; OTGW_WEBUI_MAX_CHROME=N raises it
// (scripts/run_tests.py sets it to its -j). A slot is a file <tmp>/otgw-cdp-slots/<testPid>-<n>.slot;
// a slot whose test process is gone is stale and removed. The wait (default 300 s) stays below
// run_tests.py's 600 s per-file timeout, so a starved file reports this error, not a TIMEOUT.
const SLOT_DIR = path.join(os.tmpdir(), 'otgw-cdp-slots');
export const MAX_CHROME = Math.max(1, Number(process.env.OTGW_WEBUI_MAX_CHROME) || 1);
const SLOT_WAIT_MS = Number(process.env.OTGW_WEBUI_SLOT_WAIT_MS) || 300000;
const pidAlive = (pid) => { try { process.kill(pid, 0); return true; } catch (e) { return e.code === 'EPERM'; } };
let slotSeq = 0;
// Counts the OTHER live test processes holding a slot: one test file may run several Chromes in
// turn (or overlap two) without blocking itself.
function liveSlots() {
  const pids = new Set();
  for (const f of fs.readdirSync(SLOT_DIR)) {
    if (!f.endsWith('.slot')) continue;
    const pid = Number(f.split('-')[0]);
    if (pid && pidAlive(pid)) { if (pid !== process.pid) pids.add(pid); }
    else { try { fs.rmSync(path.join(SLOT_DIR, f), { force: true }); } catch { /* ignore */ } }
  }
  return pids.size;
}
export async function acquireChromeSlot() {
  fs.mkdirSync(SLOT_DIR, { recursive: true });
  const t0 = Date.now(); let said = false;
  const lock = path.join(SLOT_DIR, '.lock');
  for (;;) {
    let fd = null;
    try { fd = fs.openSync(lock, 'wx'); } catch { /* another test is counting */ }
    if (fd !== null) {
      try {
        if (liveSlots() < MAX_CHROME) {
          const f = path.join(SLOT_DIR, `${process.pid}-${++slotSeq}.slot`);
          fs.writeFileSync(f, String(process.pid));
          return f;
        }
      } finally { fs.closeSync(fd); fs.rmSync(lock, { force: true }); }
    } else {
      try { if (Date.now() - fs.statSync(lock).mtimeMs > 10000) fs.rmSync(lock, { force: true }); } catch { /* gone */ }
    }
    if (!said) { said = true; console.error(`[webui] waiting for a Chrome slot (max ${MAX_CHROME}; set OTGW_WEBUI_MAX_CHROME to raise)`); }
    if (Date.now() - t0 > SLOT_WAIT_MS) {
      throw new Error(`no Chrome slot within ${SLOT_WAIT_MS} ms (max ${MAX_CHROME} concurrent test Chromes, OTGW_WEBUI_MAX_CHROME)`);
    }
    await sleep(250);
  }
}

// Start a guarded test Chrome inside a slot: use this instead of spawn(CHROME, ...).
export async function launchChrome(bin, args, opts = { stdio: 'ignore' }) {
  const slot = await acquireChromeSlot();
  let child;
  try { child = spawn(bin, args, { windowsHide: true, ...opts }); } catch (e) { fs.rmSync(slot, { force: true }); throw e; }
  return guardChrome(child, slot);
}

export async function startPage({ api, asset, beforeScript, width = 1400, height = 1000, page = '/v2.html' } = {}) {
  const log = [];
  let PORT = 0;
  const srv = http.createServer((q, s) => {
    const u = new URL(q.url, 'http://127.0.0.1');
    const p = decodeURIComponent(u.pathname);
    let body = '';
    q.on('data', (c) => { body += c; });
    q.on('end', () => {
      if (p.indexOf('/api/') === 0) {
        log.push(q.method + ' ' + p + u.search + ' ' + body);
        const r = api ? api(q.method, p, u.search, body) : undefined;
        // A handler may answer { __status: 500 } to make the request fail with that HTTP status.
        s.writeHead((r && r.__status) || 200, { 'content-type': 'application/json' });
        s.end(JSON.stringify(r === undefined ? {} : r));
        return;
      }
      if (asset && asset(p, q, s)) return;
      const f = path.join(DATA, p === '/' ? '/v2.html' : p);
      if (fs.existsSync(f) && fs.statSync(f).isFile()) {
        s.writeHead(200, { 'content-type': MIME[path.extname(f)] || 'application/octet-stream' });
        fs.createReadStream(f).pipe(s);
      } else { s.writeHead(404); s.end('nf'); }
    });
  });
  await new Promise((r) => srv.listen(0, '127.0.0.1', r));
  PORT = srv.address().port;

  const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'otgw-cdp-'));
  const cdpPort = () => fs.readFileSync(path.join(profile, 'DevToolsActivePort'), 'utf8').split('\n')[0].trim();
  const chrome = await launchChrome(CHROME, ['--headless=new', '--remote-debugging-port=0', `--user-data-dir=${profile}`,
    '--no-first-run', '--no-default-browser-check', '--disable-gpu', `--window-size=${width},${height}`, 'about:blank'], { stdio: 'ignore' });
  let target = null;
  for (let i = 0; i < 100 && !target; i++) {
    try { const r = await fetch(`http://127.0.0.1:${cdpPort()}/json/new?about:blank`, { method: 'PUT' }); if (r.ok) target = await r.json(); } catch { /* not up yet */ }
    if (!target) await sleep(200);
  }
  if (!target) throw new Error('Chrome DevTools endpoint never came up');
  const ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
  let msgId = 0; const pending = new Map(); const exceptions = []; const events = [];
  ws.addEventListener('message', (ev) => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
    if (m.method && (m.method.startsWith('Network.') || m.method === 'Runtime.consoleAPICalled')) events.push(m);
    if (m.method === 'Runtime.exceptionThrown') exceptions.push(JSON.stringify(m.params.exceptionDetails).slice(0, 300));
  });
  // TASK-1715: every CDP call has a deadline. A headless Chrome that stops answering under load
  // otherwise hangs the test until the node test timeout with no hint of the cause.
  const send = (method, params = {}, timeoutMs = CDP_TIMEOUT_MS) => new Promise((res, rej) => {
    const id = ++msgId;
    const timer = setTimeout(() => {
      if (!pending.has(id)) return;
      pending.delete(id);
      rej(new Error(`CDP ${method} got no reply within ${timeoutMs} ms (Chrome stalled?)`));
    }, timeoutMs);
    pending.set(id, (m) => { clearTimeout(timer); res(m); });
    ws.send(JSON.stringify({ id, method, params }));
  });
  const evalJs = async (expression) => {
    const r = await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
    return r.result && r.result.result ? r.result.result.value : undefined;
  };
  // Bounded by wall time (the old loop counted iterations, so slow evals stretched it without limit).
  // One last check at the deadline, unless the previous call already timed out. The default is 15 s:
  // callers that ignore the result relied on that stretching while a cold Chrome was slow.
  const until = async (expr, ms = 15000) => {
    const t0 = Date.now(); let stalled = false;
    while (Date.now() - t0 < ms) {
      const v = await evalJs(expr).then((x) => { stalled = false; return x; }, () => { stalled = true; return null; });
      if (v) return v;
      await sleep(100);
    }
    return stalled ? null : evalJs(expr).catch(() => null);
  };
  await send('Page.enable'); await send('Runtime.enable');
  await send('Network.enable');
  await send('Network.setCacheDisabled', { cacheDisabled: true });
  if (beforeScript) await send('Page.addScriptToEvaluateOnNewDocument', { source: beforeScript });
  const resize = (w, h = height) => send('Emulation.setDeviceMetricsOverride', { width: w, height: h, deviceScaleFactor: 1, mobile: false });
  await resize(width);
  await send('Page.navigate', { url: `http://127.0.0.1:${PORT}${page}` });
  await until(`document.readyState === 'complete'`);
  const shot = async (file) => {
    fs.mkdirSync(path.dirname(file), { recursive: true });
    const img = await send('Page.captureScreenshot', { format: 'png' });
    fs.writeFileSync(file, Buffer.from(img.result.data, 'base64'));
  };
  const close = async () => {
    try { ws.close(); } catch { /* ignore */ }
    stopChromeTree(chrome);
    srv.close();
    await sleep(300);
    try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* chrome may still hold files */ }
  };
  return { PORT, log, exceptions, events, send, evalJs, until, sleep, shot, resize, close };
}
