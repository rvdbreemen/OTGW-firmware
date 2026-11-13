// TASK-1884 guard: the SAT page explains its state from the existing /api/v2/sat/status fields:
// U1 fallback_reason as text, U2 a why-line, U3 a messages button + panel (opens by itself on a
// warning), U4 a room trend arrow from indoor_rise_rate, U5 curve_recommendation as text, U6 a 24 h
// cycles card. It logs every /api path the page requests; with API_ALLOWED=<comma list> (the set
// measured on the code before TASK-1884) it fails on any other path (AC3: no new fetch).
// Drives the REAL v2 page in headless Chrome/Edge over CDP against a local fake device.
// OTGW_DATA_DIR=<dir> for old-vs-fix; SHOT_PNG=<file> saves a screenshot. Prints SKIP without a browser.
//
// Run: node tests/webui/sat-explain-state.test.mjs
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { stopChromeTree, guardChrome, launchChrome } from './lib/page-harness.mjs';

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
const apiPaths = new Set();
// Thermostat setpoint (OT MsgID 16 / otmonitor "roomsetpoint").
const OTMON = { otmonitor: { roomsetpoint: { value: '15.0', epoch: 1700000000 } } };

const srv = http.createServer((q, s) => {
  const url = new URL(q.url, `http://127.0.0.1:${PORT}`);
  let p = decodeURIComponent(url.pathname);
  if (p.indexOf('/api/') === 0) apiPaths.add(p);
  if (p.endsWith('/v2/sat/status')) {
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
const chrome = await launchChrome(CHROME, [
  '--headless=new',
  '--remote-debugging-port=0',
  `--user-data-dir=${profile}`,
  '--no-first-run', '--no-default-browser-check', '--disable-gpu',
  '--window-size=1400,900',
  'about:blank',
], { stdio: 'ignore' });
guardChrome(chrome);   // TASK-1739

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
const BASE = { enabled: true, active: true, safety_tripped: false, control_mode: 1, target_temp: 19.5, room_temp: 18.8,
  boiler_status: 'idle', push_refused: false, fallback_active: false, fallback_reason: 0, valves_open: true,
  dhw_active: false, mod_suppressed: false, pwm_veto_reason: 0, slave_cfg_valid: true, slave_low_off_denied: false,
  indoor_rise_rate: 0, curve_recommendation: 'insufficient' };
let loaded = false;
async function show(extra) {
  satBody = Object.assign({}, BASE, extra);
  if (!loaded) {
    await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/v2.html` });
    await sleep(3000);
    await evalJs(`(function(){var b=document.querySelector('[data-page="sat"]'); if(b) b.click(); return !!b;})()`);
    loaded = true;
  }
  await sleep(6500);   // the page polls sat/status every 5 s: wait for one poll with the new body
  return await evalJs(`(function(){
    function g(id){return document.getElementById(id);}
    function vis(id){var e=g(id);return !!(e && !e.hidden && e.offsetParent!==null);}
    function tx(id){var e=g(id);return e?e.textContent:null;}
    var b=g('satMsgBtn');
    return { why: tx('satWhy'), whyVis: vis('satWhy'), btnVis: vis('satMsgBtn'), count: tx('satMsgCount'),
      panelVis: vis('satMsgs'), msgs: tx('satMsgInfo'), push: vis('satPushRefused'), trend: tx('satRoomTrend'),
      trendTitle: (g('satRoomTrend')||{}).title, curveRec: tx('satCurveRec'), c24: tx('satCycles24Kv'), sim: tx('satSimKv'),
      btnWarn: !!(b && / warn/.test(' ' + b.className)) };
  })()`);
}
async function panelOpen() {
  return evalJs(`(function(){var e=document.getElementById('satMsgs');return !!(e&&!e.hidden&&e.offsetParent!==null);})()`);
}
async function clickMsgBtn() {
  await evalJs(`(function(){var b=document.getElementById('satMsgBtn'); if(b) b.click(); return !!b;})()`);
  await sleep(300);
}
let failures = 0, total = 0;
function check(pass, msg) { total++; console.log((pass ? 'PASS' : 'FAIL') + ': ' + msg); if (!pass) failures++; }

// 1. PWM refused by the boiler + valves closed: the why-line shows the higher-priority reason, the panel stays shut.
let r = await show({ pwm_veto_reason: 2, slave_low_off_denied: true, valves_open: false, indoor_rise_rate: 0.6,
  curve_recommendation: 'increase', '24h_cycles': 14, '24h_duty_ratio': 0.42, '24h_overshoot_fraction': 0.07,
  '24h_underheat_fraction': 0.14, '24h_long_cycle_fraction': 0.21 });
check(!!r.whyVis && /bit 4/.test(r.why || ''), `U2 why-line names the PWM refusal first (got ${JSON.stringify(r.why)})`);
check(!!r.btnVis && r.count === '2' && !r.btnWarn, `U3 messages button shows 2, info level (got vis=${r.btnVis} count=${r.count} warn=${r.btnWarn})`);
check(!r.panelVis, `U3 panel stays closed without a warning (got ${r.panelVis})`);
check(/valves are closed/.test(r.msgs || ''), `U3 panel lists the valves message (got ${JSON.stringify(r.msgs)})`);
check(r.trend === '↑' && /\+0\.6/.test(r.trendTitle || ''), `U4 rising arrow with rate (got ${r.trend} / ${r.trendTitle})`);
check(/below target/.test(r.curveRec || ''), `U5 curve advice 'increase' as text (got ${JSON.stringify(r.curveRec)})`);
check(/14/.test(r.c24 || '') && /42 %/.test(r.c24 || '') && /21 %/.test(r.c24 || ''), `U6 24 h card shows 14 cycles, 42 % duty, 21 % long (got ${JSON.stringify(r.c24)})`);

// 2. The button opens the panel (and the screenshot shows it open).
await clickMsgBtn();
check(await panelOpen(), 'U3 the button opens the panel');
if (process.env.SHOT_PNG) {
  const shot = await send('Page.captureScreenshot', { format: 'png' });
  if (shot.result && shot.result.data) { fs.writeFileSync(process.env.SHOT_PNG, Buffer.from(shot.result.data, 'base64')); console.log('screenshot: ' + process.env.SHOT_PNG); }
}
await clickMsgBtn();
check(!(await panelOpen()), 'U3 a second click closes it again');

// 3. MQTT-loss takeover: a warning, the panel opens by itself, the reason as text (U1).
r = await show({ enabled: false, fallback_active: true, fallback_reason: 2, indoor_rise_rate: -0.5 });
check(/MQTT lost/.test(r.why || ''), `U1+U2 why-line gives the takeover reason as text (got ${JSON.stringify(r.why)})`);
check(!!r.panelVis && r.btnWarn, `U3 a warning opens the panel and marks the button (got panel=${r.panelVis} warn=${r.btnWarn})`);
check(/MQTT lost/.test(r.sim || ''), `U1 Simulation card shows the reason as text (got ${JSON.stringify(r.sim)})`);
check(r.trend === '↓', `U4 falling arrow (got ${r.trend})`);

// 4. Push refused only: the old hint stays visible, inside the panel that opened by itself.
r = await show({ push_refused: true });
check(!!r.push && !!r.panelVis && r.count === '1', `push-refused hint visible inside the open panel (got push=${r.push} panel=${r.panelVis} count=${r.count})`);
check(!r.whyVis && r.trend === '', `no why-line and no arrow when nothing applies (got why=${r.whyVis} trend=${JSON.stringify(r.trend)})`);

// 5. A sticky veto reason without the live flag: no PWM message.
r = await show({ pwm_veto_reason: 1, slave_cfg_valid: true });
check(!r.whyVis && !r.btnVis, `an old PWM refusal is not shown once MsgID 3 answered (got why=${r.whyVis} btn=${r.btnVis})`);

// AC3: every /api path the page requested. API_ALLOWED = the set measured on the code before TASK-1884.
// Paths are compared from 'api/' on: Git Bash rewrites a leading '/api/...' in an env var into a Windows path.
const norm = (p) => p.replace(/^.*?(api\/)/, '$1');
const ALLOWED = new Set((process.env.API_ALLOWED || '').split(',').filter(Boolean).map(norm));
console.log('api paths: ' + [...apiPaths].sort().join(','));
if (ALLOWED.size) {
  const extra = [...apiPaths].map(norm).filter((p) => !ALLOWED.has(p));
  check(extra.length === 0, `AC3 no new API path (extra: ${extra.join(',') || 'none'})`);
}
try { ws.close(); } catch { /* ignore */ }
stopChromeTree(chrome); srv.close();
// TASK-1492: Chrome holds the profile until it has exited; wait for that, then retry the removal.
await new Promise((r) => { if (chrome.exitCode !== null || chrome.signalCode !== null) return r(); chrome.once('exit', r); setTimeout(r, 5000); });
try { fs.rmSync(profile, { recursive: true, force: true, maxRetries: 10, retryDelay: 200 }); } catch (e) { console.log('profile cleanup failed: ' + e.message); }
console.log('\n' + failures + ' failures of ' + total + ' checks');
process.exit(failures ? 1 : 0);
