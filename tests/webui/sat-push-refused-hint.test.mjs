// TASK-1343 guard: the SAT page shows 'The thermostat refuses the pushed setpoint' when
// /api/v2/sat/status reports push_refused, and hides it otherwise. Drives the REAL v2 page in
// headless Chrome/Edge over CDP against a local fake device. OTGW_DATA_DIR=<dir> for old-vs-fix;
// SHOT_PNG=<file> saves a screenshot. Prints SKIP without a browser.
//
// Run: node tests/webui/sat-push-refused-hint.test.mjs
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
let PORT = 0;   // TASK-1489: HTTP and CDP ports are picked by the OS, so parallel runs and
                // other sessions running the same test cannot collide on them.
// Chrome started with --remote-debugging-port=0 writes the port it bound to the first
// line of <profile>/DevToolsActivePort; it throws until Chrome is up.
function cdpPort() { return fs.readFileSync(path.join(profile, 'DevToolsActivePort'), 'utf8').split('\n')[0].trim(); }

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
await new Promise((r) => srv.listen(0, '127.0.0.1', r));
PORT = srv.address().port;

const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'otgw-cdp-'));
const chrome = spawn(CHROME, [
  '--headless=new',
  '--remote-debugging-port=0',
  `--user-data-dir=${profile}`,
  '--no-first-run', '--no-default-browser-check', '--disable-gpu',
  '--window-size=1400,900',
  'about:blank',
], { stdio: 'ignore' });

async function cdpTarget() {
  for (let i = 0; i < 100; i++) {
    try {
      const r = await fetch(`http://127.0.0.1:${cdpPort()}/json/new?about:blank`, { method: 'PUT' });
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

async function hintVisible(refused) {
  satBody = { enabled: true, active: true, safety_tripped: false, control_mode: 1, target_temp: 19.5, room_temp: 18.8, push_refused: refused };
  await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/v2.html` });
  await sleep(3000);
  await evalJs(`(function(){var b=document.querySelector('[data-page="sat"]'); if(b) b.click(); return !!b;})()`);
  await sleep(4000);
  return await evalJs(`(function(){var e=document.getElementById('satPushRefused'); if(!e) return 'missing'; return (!e.hidden && e.offsetParent!==null) ? 'visible' : 'hidden';})()`);
}
let failures = 0, total = 0;
function check(pass, msg) { total++; console.log((pass ? 'PASS' : 'FAIL') + ': ' + msg); if (!pass) failures++; }
let v = await hintVisible(true);
check(v === 'visible', `push_refused true: hint is visible (got ${v})`);
if (process.env.SHOT_PNG) {
  const shot = await send('Page.captureScreenshot', { format: 'png' });
  if (shot.result && shot.result.data) { fs.writeFileSync(process.env.SHOT_PNG, Buffer.from(shot.result.data, 'base64')); console.log('screenshot: ' + process.env.SHOT_PNG); }
}
v = await hintVisible(false);
check(v === 'hidden', `push_refused false: hint is hidden (got ${v})`);
try { ws.close(); } catch { /* ignore */ }
chrome.kill(); srv.close();
try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* ignore */ }
console.log('\n' + failures + ' failures of ' + total + ' checks');
process.exit(failures ? 1 : 0);
