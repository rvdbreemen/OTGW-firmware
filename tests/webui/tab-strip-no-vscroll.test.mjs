// TASK-1350 guard: the v2 tab strips (.sat-views = SAT Thermostat/Control/Technical, .subtabs =
// Monitor/Advanced sub-tabs) scroll sideways on a narrow screen but must not show a vertical
// scrollbar. overflow-x:auto makes overflow-y compute to auto, and the tabs sit 1 px below the
// row (margin-bottom:-1px), so without overflow-y:hidden the browser draws a vertical scrollbar
// (Sergeant D, 2026-10-24).
//
// Drives the REAL v2.html/v2.css in headless Chrome/Edge over CDP against a local fake device.
// OTGW_DATA_DIR=<dir> serves another data dir (old-vs-fix). Prints SKIP without a browser.
//
// Run: node tests/webui/tab-strip-no-vscroll.test.mjs
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const PORT = 8161;
const CDP_PORT = 9361;

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

let satBody = {};
let statusHits = 0;
// Thermostat setpoint (OT MsgID 16 / otmonitor "roomsetpoint").
const OTMON = { otmonitor: { roomsetpoint: { value: '15.0', epoch: 1700000000 } } };

const srv = http.createServer((q, s) => {
  const url = new URL(q.url, `http://127.0.0.1:${PORT}`);
  let p = decodeURIComponent(url.pathname);
  if (p.endsWith('/v2/sat/status')) {
    statusHits++;
    s.writeHead(200, { 'content-type': 'application/json' });
    s.end(JSON.stringify(satBody));
    return;
  }
  if (p.endsWith('/v2/otgw/otmonitor')) {
    s.writeHead(200, { 'content-type': 'application/json' });
    s.end(JSON.stringify(OTMON));
    return;
  }
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

const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'otgw-cdp-'));
const chrome = spawn(CHROME, [
  '--headless=new',
  `--remote-debugging-port=${CDP_PORT}`,
  `--user-data-dir=${profile}`,
  '--no-first-run', '--no-default-browser-check', '--disable-gpu',
  '--window-size=1400,900',
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

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const tileText = () => evalJs(`(function(){var e=document.getElementById('aRoomSet');return e?e.textContent:null;})()`).catch(() => null);

await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/v2.html` });
await sleep(4000);
const res = await evalJs(`(function(){
  var out = [];
  document.querySelectorAll('.sat-views, .subtabs').forEach(function (el) {
    var cs = getComputedStyle(el);
    out.push({ cls: el.className, oy: cs.overflowY, ox: cs.overflowX });
  });
  return out;
})()`);
let failures = 0, total = 0;
function check(pass, msg) { total++; console.log((pass ? 'PASS' : 'FAIL') + ': ' + msg); if (!pass) failures++; }
check(Array.isArray(res) && res.length > 0, `found tab strips (${res ? res.length : 0})`);
(res || []).forEach(function (r) {
  check(r.oy === 'hidden', `.${r.cls}: overflow-y is hidden (got ${r.oy})`);
  check(r.ox === 'auto', `.${r.cls}: overflow-x stays auto for narrow screens (got ${r.ox})`);
});
try { ws.close(); } catch { /* ignore */ }
chrome.kill();
srv.close();
try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* ignore */ }
console.log('\n' + failures + ' failures of ' + total + ' checks');
process.exit(failures ? 1 : 0);
