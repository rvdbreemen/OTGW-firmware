// TASK-1314 regression guard: on a PIC gateway with a wired return-water sensor the
// firmware refreshes the sensor every minute with PR=D / PR=E, and the PIC answers
// 'PR: D=R' (sensor function) and 'PR: E=29.38' ('PR: E=-' without a reading). The v2
// UI must take the E value as the return temperature only while the function is R.
// OT frames for MsgID 28 (Read-Ack) keep setting it as before.
//
// Drives the REAL shipped v2.html/v2.js in headless system Chrome over the DevTools
// protocol. The page's WebSocket is replaced (before any page script runs) by a stub
// that records its instances, so the test feeds WS log lines through the page's own
// ws.onmessage handler (onWsMessage). OTGW_DATA_DIR serves another data dir (old-vs-fix).
//
// Run: node tests/webui/tret-pr-events.test.mjs
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const PORT = 8151;
const CDP_PORT = 9351;

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

const MIME = {
  '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css',
  '.json': 'application/json', '.svg': 'image/svg+xml', '.ico': 'image/x-icon', '.png': 'image/png',
};

const srv = http.createServer((q, s) => {
  const url = new URL(q.url, `http://127.0.0.1:${PORT}`);
  let p = decodeURIComponent(url.pathname);
  if (p.indexOf('/api/') === 0) {
    s.writeHead(200, { 'content-type': 'application/json' });
    s.end('{}');
    return;
  }
  if (p === '/') p = '/v2.html';
  const f = path.join(DATA, p);
  if (fs.existsSync(f) && fs.statSync(f).isFile()) {
    s.writeHead(200, { 'content-type': MIME[path.extname(f)] || 'application/octet-stream' });
    fs.createReadStream(f).pipe(s);
  } else {
    s.writeHead(404);
    s.end('nf');
  }
});
await new Promise((r) => srv.listen(PORT, '127.0.0.1', r));

// --- headless Chrome over CDP, no dependencies -----------------------------
const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'otgw-cdp-'));
const chrome = spawn(CHROME, [
  '--headless=new',
  `--remote-debugging-port=${CDP_PORT}`,
  `--user-data-dir=${profile}`,
  '--no-first-run', '--no-default-browser-check', '--disable-gpu',
  'about:blank',
], { stdio: 'ignore' });

async function cdpTarget() {
  for (let i = 0; i < 100; i++) {
    try {
      const r = await fetch(`http://127.0.0.1:${CDP_PORT}/json/new?about:blank`, { method: 'PUT' });
      if (r.ok) return await r.json();
    } catch { /* not up yet */ }
    await new Promise((r) => setTimeout(r, 200));
  }
  throw new Error('Chrome DevTools endpoint never came up');
}

const target = await cdpTarget();
const ws = new WebSocket(target.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });

let msgId = 0;
const pending = new Map();
ws.addEventListener('message', (ev) => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
});
function send(method, params = {}) {
  const id = ++msgId;
  return new Promise((res) => { pending.set(id, res); ws.send(JSON.stringify({ id, method, params })); });
}
async function evalJs(expression) {
  const r = await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
  if (r.result && r.result.exceptionDetails) throw new Error(JSON.stringify(r.result.exceptionDetails));
  return r.result && r.result.result ? r.result.result.value : undefined;
}

await send('Page.enable');
await send('Runtime.enable');

// A WebSocket stub that never connects and records every instance.
await send('Page.addScriptToEvaluateOnNewDocument', {
  source: `window.__fakeWs = [];
    window.WebSocket = function (url) { this.url = url; this.readyState = 0; window.__fakeWs.push(this); };
    window.WebSocket.prototype.close = function () {};
    window.WebSocket.prototype.send = function () {};`,
});

let ok = true;
function check(pass, msg) {
  console.log((pass ? 'PASS' : 'FAIL') + ': ' + msg);
  if (!pass) ok = false;
}
let nChecks = 0, nFail = 0;
const _check = check;
function chk(pass, msg) { nChecks++; if (!pass) nFail++; _check(pass, msg); }

await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/v2.html` });
for (let i = 0; i < 100; i++) {
  await new Promise((r) => setTimeout(r, 100));
  const n = await evalJs('(window.__fakeWs||[]).length').catch(() => 0);
  if (n > 0) break;
}

async function feed(line) {
  await evalJs(`(function(){var w=window.__fakeWs[window.__fakeWs.length-1];w.onmessage({data:${JSON.stringify(line)}});return true;})()`);
  await new Promise((r) => setTimeout(r, 250));   // let the requestAnimationFrame render run
}
async function readRet() {
  return await evalJs(`(function(){var e=document.getElementById('aRet');return e?e.textContent:null;})()`);
}

const TS = '12:00:00.123456 ';
const pr = (body) => TS + '< ' + body;
// OT frame: 18-char source field + space + raw frame; MsgID 28 (0x1C) Read-Ack (0x40), 35.5 deg (0x2380)
const otFrame = (raw) => TS + 'Boiler'.padEnd(18) + ' ' + raw;

chk((await readRet()) !== null, 'Home return readout #aRet exists');

await feed(pr('PR: D=R'));
await feed(pr('PR: E=41.20'));
let v = await readRet();
chk(/41\.2/.test(v || ''), `D=R then E=41.20 -> return readout shows 41.2 (got "${v}")`);

await feed(pr('PR: E=-'));
v = await readRet();
chk(/41\.2/.test(v || ''), `E=- (no reading) -> still 41.2 (got "${v}")`);

await feed(pr('PR: E=42.30'));
v = await readRet();
chk(/42\.3/.test(v || ''), `E=42.30 -> 42.3 (got "${v}")`);

await feed(pr('PR: E=300.00'));
v = await readRet();
chk(/42\.3/.test(v || ''), `E=300.00 (out of range) -> still 42.3 (got "${v}")`);

await feed(pr('PR: D=O'));
await feed(pr('PR: E=5.00'));
v = await readRet();
chk(/42\.3/.test(v || ''), `D=O (outside sensor) then E=5.00 -> still 42.3 (got "${v}")`);

await feed(otFrame('B401C2380'));
v = await readRet();
chk(/35\.5/.test(v || ''), `OT frame MsgID 28 Read-Ack 35.5 -> 35.5 as before (got "${v}")`);

// --------------------------------------------------------------------- done
try { ws.close(); } catch { /* ignore */ }
chrome.kill();
srv.close();
try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* ignore */ }

console.log('\n' + nFail + ' failures of ' + nChecks + ' checks');
process.exit(ok ? 0 : 1);
