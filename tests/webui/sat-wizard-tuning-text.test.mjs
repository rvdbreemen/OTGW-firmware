// TASK-1354 regression guard: the SAT setup wizard's tuning screen must not claim
// that SAT "learns your home's response over ~2 weeks and adjusts gains", nor offer
// satautogains as a tuning choice: SATpid.ino _pidCalculateGains never reads
// satautogains (settings.sat.fAutoGainsValue). The truthful text: Automatic = SAT
// computes the PID gains from the heating curve; Manual = the user sets the slope.
//
// Drives the REAL shipped wizard (OTGW_DATA_DIR=<dir> serves another data dir for
// old-vs-fix). Zero dependencies: system Chrome/Edge over the DevTools protocol.
// SHOT_PNG=<file> saves a screenshot of the tuning screen (Manual selected).
//
// Run: node tests/webui/sat-wizard-tuning-text.test.mjs
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
const wizText = () => evalJs(`(function(){var e=document.getElementById('satOnbBack');return e?e.textContent:null;})()`).catch(() => null);
async function waitFor(expr, ms = 15000) {
  for (let i = 0; i < ms / 100; i++) {
    const v = await evalJs(expr).catch(() => null);
    if (v) return v;
    await sleep(100);
  }
  return null;
}
// Click a wizard button by its data-sa action and wait for the re-render.
async function sa(action) {
  const hit = await evalJs(`(function(){var b=document.querySelector('#satOnbBack [data-sa="${action}"]');if(!b)return false;b.click();return true;})()`);
  if (!hit) throw new Error('wizard button data-sa="' + action + '" not found');
  await sleep(150);
}

let ok = true;
let failures = 0, total = 0;
function check(pass, msg) {
  total++;
  console.log((pass ? 'PASS' : 'FAIL') + ': ' + msg);
  if (!pass) { ok = false; failures++; }
}

await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/v2.html` });
// The "Run SAT setup" button (data-act="sat-onboarding") is wired at init.
const wired = await waitFor(`(function(){var b=document.querySelector('[data-act="sat-onboarding"]');return !!(b && document.readyState==='complete');})()`);
if (!wired) throw new Error('page never offered the Run SAT setup button');
await sleep(800);
await evalJs(`document.querySelector('[data-act="sat-onboarding"]').click()`);
if (!(await waitFor(`!!document.getElementById('satOnbBack')`))) throw new Error('SAT wizard did not open');
await sa('begin');
await sa('to-mfr');
await sa('to-tuning');

// Automatic (default) selected.
let t = await wizText();
check(typeof t === 'string' && t.indexOf('How should SAT tune itself?') !== -1, 'wizard reached the tuning screen');
check(!/learns your home/i.test(t || '') && !/2 weeks/i.test(t || ''), 'Automatic: no "learns your home ... ~2 weeks" claim');
check(!/automatic learning/i.test(t || ''), 'tuning intro: no "automatic learning"');
check((t || '').indexOf('satautogains') === -1, 'Automatic: satautogains is not presented');
check((t || '').indexOf('SAT computes the PID gains from the heating curve') !== -1, 'Automatic: says SAT computes the PID gains from the heating curve');

// Manual selected.
await sa('tune-manual');
t = await wizText();
check(!/auto-gains/i.test(t || ''), 'Manual: no "auto-gains" wording (no "auto-gains off", no "Auto-gains multiplier")');
check(!/learned corrections/i.test(t || ''), 'Manual: no "Scales learned corrections"');
check((t || '').indexOf('satautogains') === -1, 'Manual: satautogains is not presented');
check(!(await evalJs(`!!document.getElementById('satOnbGains')`)), 'Manual: no satautogains input field');
check((t || '').indexOf('Set the heating-curve slope yourself') !== -1, 'Manual: says the user sets the heating-curve slope');
check(!!(await evalJs(`!!document.getElementById('satOnbSlope')`)), 'Manual: slope slider is still there');
if (process.env.SHOT_PNG) {
  const shot = await send('Page.captureScreenshot', { format: 'png' });
  const b64 = shot.result && shot.result.data;
  if (b64) {
    fs.mkdirSync(path.dirname(process.env.SHOT_PNG), { recursive: true });
    fs.writeFileSync(process.env.SHOT_PNG, Buffer.from(b64, 'base64'));
    console.log(`screenshot: ${process.env.SHOT_PNG}`);
  }
}

try { ws.close(); } catch { /* ignore */ }
chrome.kill();
srv.close();
try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* ignore */ }

console.log('\n' + failures + ' failures of ' + total + ' checks');
process.exit(ok ? 0 : 1);
