// TASK-1152 regression guard: the SAT status pill must report BURNER ACTIVITY,
// never the CONTROL MODE.
//
// Reported on Discord 2026-06-21 by sergeantd: "since SAT is not heating at the
// moment, why it says continuous instead of idle?". 'Continuous' answers "how does
// SAT modulate the burner", which is not the question a status pill is asked.
//
// This drives the REAL shipped assets: a local server hands the page a synthetic
// /api/v2/sat/status body, the page's own poll parses it and calls the production
// renderSatPage(), and we read the resulting DOM. v2.js is an IIFE, so there is no
// way to shortcut into its internals - which is exactly why this covers the whole chain from HTTP body to rendered pill.
//
// Zero dependencies: system Chrome over the DevTools protocol, Node's built-in
// WebSocket. No playwright install needed.
//
// Run: node tests/webui/sat-status-pill.test.mjs
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

// The scenario the fake device reports. Mutated between cases.
let scenario = {};
let statusHits = 0;

const srv = http.createServer((q, s) => {
  const url = new URL(q.url, `http://127.0.0.1:${PORT}`);
  let p = decodeURIComponent(url.pathname);

  if (p.endsWith('/v2/sat/status')) {
    statusHits++;
    s.writeHead(200, { 'content-type': 'application/json' });
    s.end(JSON.stringify(scenario));
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

// --- headless Chrome over CDP, no dependencies -----------------------------
const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'otgw-cdp-'));
const chrome = spawn(CHROME, [
  '--headless=new',
  '--remote-debugging-port=0',
  `--user-data-dir=${profile}`,
  '--no-first-run', '--no-default-browser-check', '--disable-gpu',
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

// Load a page and wait until the SAT status poll has been served at least once
// and the pill has rendered something.
// An optional starter expression runs until it returns true (for example a click
// that opens the SAT page).
async function loadAndSettle(url, readySelector, starter) {
  const before = statusHits;
  await send('Page.navigate', { url });
  let started = false;
  for (let i = 0; i < 200; i++) {
    await new Promise((r) => setTimeout(r, 100));
    if (!started && starter) {
      started = await evalJs(starter).catch(() => false);
      if (!started) continue;
    }
    if (statusHits <= before) continue;
    const txt = await evalJs(
      `(function(){var e=document.querySelector(${JSON.stringify(readySelector)});return e?e.textContent:null;})()`
    ).catch(() => null);
    // 'Error' is a fetch-failure text. It can appear transiently if the first
    // poll races page init; the 5 s poll retries, so keep waiting rather than
    // asserting on a value that is about to be replaced.
    if (txt && txt !== 'Error') { await new Promise((r) => setTimeout(r, 250)); return txt; }
  }
  throw new Error(`page never settled: ${url} (status hits ${before} -> ${statusHits}, started=${started})`);
}

let ok = true;
const results = [];
function check(pass, msg) {
  results.push((pass ? 'PASS' : 'FAIL') + ': ' + msg);
  console.log((pass ? 'PASS' : 'FAIL') + ': ' + msg);
  if (!pass) ok = false;
}

const BASE = {
  enabled: true, active: true, safety_tripped: false, window_open: false, summer_active: false,
  control_mode: 1, room_temp: 20.9, target_temp: 21.0, final_setpoint: 32.0,
  heating_curve: 30.0, pid_output: 2.0, error: 0.1, flame: false,
};

// --------------------------------------------------------------------- v2 UI
async function v2Pill(extra) {
  scenario = Object.assign({}, BASE, extra);
  await loadAndSettle(`http://127.0.0.1:${PORT}/v2.html`, 'body');
  await evalJs(`(function(){
    var b=document.querySelector('[data-page="sat"]')||document.querySelector('#navsat')||
          Array.prototype.find.call(document.querySelectorAll('button,a,div'),function(e){return /\\bSAT\\b/.test(e.textContent||'')&&e.offsetParent!==null;});
    if(b) b.click();
    return !!b;
  })()`);
  const before = statusHits;
  for (let i = 0; i < 100; i++) {
    await new Promise((r) => setTimeout(r, 100));
    if (statusHits > before) break;
  }
  await new Promise((r) => setTimeout(r, 400));
  return await evalJs(`(function(){
    return JSON.stringify({
      pill: (document.getElementById('satPill')||{}).textContent,
      big:  (document.getElementById('satStatusBig')||{}).textContent,
      en:   (document.getElementById('satEnableLbl')||{}).textContent
    });
  })()`).then(JSON.parse);
}

let r = await v2Pill({ boiler_status: 'idle' });
check(r.pill === 'Idle', `v2: engaged + boiler idle -> pill reads "Idle" (got "${r.pill}")`);
check(/idle/i.test(r.big || ''), `v2: headline reports idle, not "Heating to ..." (got "${r.big}")`);

r = await v2Pill({ boiler_status: 'heating' });
check(r.pill === 'Heating', `v2: burner firing -> pill reads "Heating" (got "${r.pill}")`);
check(/Heating to/.test(r.big || ''), `v2: headline reports the target while firing (got "${r.big}")`);

// TASK-1234: the precedence cases that used to run against classic only. In v2 the
// pill reports burner activity alone (Off / Heating / Idle); safety, window and summer
// are reported by the headline (#satStatusBig), in that order of precedence.
r = await v2Pill({ boiler_status: 'heating', safety_tripped: true, window_open: true, summer_active: true });
check(r.big === 'Safety tripped!', `v2: safety trip outranks window, summer and activity in the headline (got "${r.big}")`);
check(r.pill === 'Heating', `v2: pill keeps reporting burner activity during a safety trip (got "${r.pill}")`);

r = await v2Pill({ boiler_status: 'heating', window_open: true, summer_active: true });
check(r.big === 'Window open', `v2: window open outranks summer and activity in the headline (got "${r.big}")`);

r = await v2Pill({ boiler_status: 'heating', summer_active: true });
check(r.big === 'Summer mode', `v2: summer mode outranks activity in the headline (got "${r.big}")`);

r = await v2Pill({ boiler_status: 'heating', enabled: false, safety_tripped: true });
check(r.pill === 'Off', `v2: disabled -> pill reads "Off" even with the burner firing (got "${r.pill}")`);
check(r.big === 'SAT is off', `v2: disabled outranks everything in the headline, safety included (got "${r.big}")`);
check(r.en === 'Disabled', `v2: enable switch label reads "Disabled" (got "${r.en}")`);

// Firmware that omits boiler_status: satBurnerIsFiring() falls back to `active`.
r = await v2Pill({ active: true });
check(r.pill === 'Heating', `v2: no boiler_status + active -> falls back to control-loop state "Heating" (got "${r.pill}")`);
r = await v2Pill({ active: false });
check(r.pill === 'Idle', `v2: no boiler_status + inactive -> "Idle" (got "${r.pill}")`);
check(r.big === 'At target · idle', `v2: no boiler_status + inactive -> idle headline (got "${r.big}")`);

// ------------------------------------------------- visual receipt (opt-in)
// Exactly the state sergeantd reported: SAT engaged, control mode Continuous,
// burner not firing. Done last and in isolation so the page navigation cannot
// perturb the assertions above.
if (process.env.OTGW_SHOT_DIR) {
  await v2Pill({ boiler_status: 'idle' });
  const shot = await send('Page.captureScreenshot', { format: 'png' });
  const b64 = shot.result && shot.result.data;
  if (b64) {
    const out = path.join(process.env.OTGW_SHOT_DIR, 'task-1152-sat-pill-idle.png');
    fs.writeFileSync(out, Buffer.from(b64, 'base64'));
    const shown = await evalJs(`(document.getElementById('satPill')||{}).textContent`).catch(() => '?');
    console.log(`screenshot: ${out} (pill shows "${shown}")`);
  }
}

// --------------------------------------------------------------------- done
try { ws.close(); } catch { /* ignore */ }
chrome.kill();
srv.close();
try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* ignore */ }

console.log('\n' + results.filter((l) => l.startsWith('FAIL')).length + ' failures of ' + results.length + ' checks');
process.exit(ok ? 0 : 1);
