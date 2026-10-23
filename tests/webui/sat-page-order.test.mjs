// TASK-1310 guard: the SAT Thermostat view puts the end-user things first (dial, presets,
// status, temperature history) and keeps the edge-case controls out of the way:
//   - Hot water (standalone DHW + DHW setpoint) sits in a collapsed <details class="grp"> below
//     the history; its open state is stored only on a click on the summary (concept rule 4);
//   - Burner cycle mode shows at Control level and up, never on the Thermostat view;
//   - every moved control still makes the same REST call (endpoint + body).
//
// Drives the REAL shipped v2.html/v2.js in headless system Chrome/Edge over the DevTools
// protocol, against a local fake device that records every POST. No npm install; prints SKIP
// when no browser is found. OTGW_DATA_DIR=<dir> serves another data dir (old-vs-fix runs).
//
// Run: node tests/webui/sat-page-order.test.mjs
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const PORT = 8152;
const CDP_PORT = 9352;

const CHROME = [
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/usr/bin/google-chrome',
  '/usr/bin/chromium',
].find((p) => fs.existsSync(p));
if (!CHROME) {
  console.log('SKIP: no Chrome/Edge binary found; cannot run the browser harness');
  process.exit(0);
}

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.svg': 'image/svg+xml', '.ico': 'image/x-icon', '.png': 'image/png' };

const STATUS = {
  enabled: true, active: true, safety_tripped: false, window_open: false, summer_active: false,
  control_mode: 1, room_temp: 20.9, target_temp: 21.0, final_setpoint: 32.0, heating_curve: 30.0,
  pid_output: 2.0, error: 0.1, flame: false, boiler_status: 'idle',
  dhw_enabled: true, dhw_enable: true, dhw_setpoint: 50, cycle_mode: 'continuous',
};
let statusHits = 0;
const posts = [];

const srv = http.createServer((q, s) => {
  const url = new URL(q.url, `http://127.0.0.1:${PORT}`);
  let p = decodeURIComponent(url.pathname);
  if (p.indexOf('/api/') === 0) {
    if (q.method === 'POST') {
      let body = '';
      q.on('data', (c) => { body += c; });
      q.on('end', () => {
        posts.push(p + ' ' + body);
        s.writeHead(200, { 'content-type': 'application/json' });
        s.end('{}');
      });
      return;
    }
    s.writeHead(200, { 'content-type': 'application/json' });
    if (p.endsWith('/v2/sat/status')) { statusHits++; s.end(JSON.stringify(STATUS)); } else s.end('{}');
    return;
  }
  if (p === '/') p = '/v2.html';
  const f = path.join(DATA, p);
  if (fs.existsSync(f) && fs.statSync(f).isFile()) {
    s.writeHead(200, { 'content-type': MIME[path.extname(f)] || 'application/octet-stream' });
    fs.createReadStream(f).pipe(s);
  } else { s.writeHead(404); s.end('nf'); }
});
await new Promise((r) => srv.listen(PORT, '127.0.0.1', r));

const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'otgw-cdp-'));
const chrome = spawn(CHROME, [
  '--headless=new', `--remote-debugging-port=${CDP_PORT}`, `--user-data-dir=${profile}`,
  '--no-first-run', '--no-default-browser-check', '--disable-gpu', 'about:blank',
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
const exceptions = [];
ws.addEventListener('message', (ev) => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
  if (m.method === 'Runtime.exceptionThrown') {
    const d = m.params.exceptionDetails;
    exceptions.push((d.exception && d.exception.description) || d.text);
  }
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
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
await send('Page.enable');
await send('Runtime.enable');

let ok = true;
function check(pass, msg) {
  console.log((pass ? 'PASS' : 'FAIL') + ': ' + msg);
  if (!pass) ok = false;
}

// Loads the page, opens the SAT page, waits for a status poll, then lets it settle.
async function openSat(view) {
  const before = statusHits;
  await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/v2.html` });
  for (let i = 0; i < 100; i++) {
    await sleep(100);
    if (await evalJs(`!!document.querySelector('.seg[data-page="sat"]')`).catch(() => false)) break;
  }
  await evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
  for (let i = 0; i < 100 && statusHits <= before; i++) await sleep(100);
  if (view) await evalJs(`document.querySelector('.satview-btn[data-sv="${view}"]').click()`);
  await sleep(500);
}

// One query for the facts the checks need. false/null where an element does not exist.
const FACTS = `(function(){
  function el(id){return document.getElementById(id);}
  // checkVisibility also sees the content-visibility:hidden of a closed <details>, which offsetParent does not
  function vis(e){return !!e && e.checkVisibility() && e.offsetParent !== null;}
  var g = document.querySelector('#satBody details.grp[data-grp="sat-dhw"]');
  var dial = el('satDial'), hist = el('satHistChart'), cyc = el('satCycleRow');
  var sum = g ? g.querySelector('summary') : null;
  var f = Node.DOCUMENT_POSITION_FOLLOWING;
  return JSON.stringify({
    group: !!g, groupOpen: g ? g.open : null, summaryVisible: vis(sum),
    dialBeforeHist: !!(dial && hist && (dial.compareDocumentPosition(hist) & f)),
    histBeforeGroup: !!(hist && g && (hist.compareDocumentPosition(g) & f)),
    histVisible: vis(hist), dialVisible: vis(dial),
    cycleVisible: vis(cyc), sliderVisible: vis(el('satDhwSlider')),
    stored: (function(){try{return localStorage.getItem('otgw-v2-satopen');}catch(e){return 'ERR';}})()
  });
})()`;
const facts = async () => JSON.parse(await evalJs(FACTS));

// ------------------------------------------------ (a) Thermostat view
await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/v2.html` });
await sleep(800);
await evalJs('localStorage.clear()');
await openSat('simple');
let f = await facts();
check(f.dialVisible && f.histVisible, `Thermostat view: dial and temperature history are visible (dial=${f.dialVisible}, hist=${f.histVisible})`);
check(f.dialBeforeHist && f.histBeforeGroup, `DOM order: dial < history < Hot water group (dialBeforeHist=${f.dialBeforeHist}, histBeforeGroup=${f.histBeforeGroup})`);
check(f.group && f.summaryVisible, `Thermostat view: the Hot water group header is visible (group=${f.group}, summaryVisible=${f.summaryVisible})`);
check(f.group && f.groupOpen === false && !f.sliderVisible, `Hot water group is closed by default (open=${f.groupOpen}, sliderVisible=${f.sliderVisible})`);
check(!f.cycleVisible, `Thermostat view: Burner cycle mode card is not visible (cycleVisible=${f.cycleVisible})`);
check(f.stored === null, `nothing stored for the group before any click (otgw-v2-satopen=${f.stored})`);

// A programmatic open must not be stored (rule 4: only a user click on the summary stores).
if (f.group) {
  await evalJs(`document.querySelector('#satBody details.grp[data-grp="sat-dhw"]').open = true`);
  await sleep(200);
  f = await facts();
  check(f.stored === null, `a programmatic open stores nothing (otgw-v2-satopen=${f.stored})`);
  await evalJs(`document.querySelector('#satBody details.grp[data-grp="sat-dhw"]').open = false`);
}

// ------------------------------------------------ (b) Control view
await evalJs(`document.querySelector('.satview-btn[data-sv="control"]').click()`);
await sleep(300);
f = await facts();
check(f.cycleVisible, `Control view: Burner cycle mode card is visible (cycleVisible=${f.cycleVisible})`);

// ------------------------------------------------ (c) same REST calls
// The Hot water group is opened with a real click on its summary where it exists.
posts.length = 0;
const hasSummary = await evalJs(`!!document.querySelector('#satBody details.grp[data-grp="sat-dhw"] summary')`);
if (hasSummary) {
  await evalJs(`document.querySelector('#satBody details.grp[data-grp="sat-dhw"] summary').click()`);
  await sleep(300);
}
await evalJs(`(function(){
  var s = document.getElementById('satDhwSlider'); s.disabled = false; s.value = '45';
  s.dispatchEvent(new Event('change', { bubbles: true }));
})()`);
await sleep(600);
await evalJs(`document.getElementById('satDhwEnabled').click()`);
await sleep(600);
await evalJs(`document.querySelector('#satCycleRow .ds-seg[data-mode="pwm"]').click()`);
await sleep(600);
const want = [
  '/api/v2/sat/settings/dhw_setpoint 45',
  '/api/v2/sat/settings/dhw_enabled false',
  '/api/v2/sat/mode pwm',
];
console.log('recorded POSTs: ' + JSON.stringify(posts));
check(JSON.stringify(posts) === JSON.stringify(want), 'DHW setpoint, standalone DHW and burner cycle POST the same endpoint + body as before');
if (hasSummary) {
  f = await facts();
  check(f.groupOpen === true && f.sliderVisible, `after the summary click the group is open and the DHW slider is usable (open=${f.groupOpen}, slider=${f.sliderVisible})`);
  check(f.stored === '["sat-dhw"]', `the click stored the open group (otgw-v2-satopen=${f.stored})`);

  // ---------------------------------------------- (d) persistence
  await openSat('simple');
  f = await facts();
  check(f.groupOpen === true && f.sliderVisible, `open state survives a reload (open=${f.groupOpen})`);
  await evalJs(`document.querySelector('#satBody details.grp[data-grp="sat-dhw"] summary').click()`);
  await sleep(200);
  f = await facts();
  check(f.groupOpen === false && f.stored === '[]', `a second click closes it and stores that (open=${f.groupOpen}, stored=${f.stored})`);
  await openSat('simple');
  f = await facts();
  check(f.groupOpen === false, `closed state survives a reload (open=${f.groupOpen})`);
} else {
  check(false, 'no Hot water group to open or persist');
}

// ------------------------------------------------ (e) no exceptions
check(exceptions.length === 0, `no page exception (${exceptions.length}${exceptions[0] ? ': ' + exceptions[0].split('\n')[0] : ''})`);

try { ws.close(); } catch { /* ignore */ }
chrome.kill();
srv.close();
try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* ignore */ }
process.exit(ok ? 0 : 1);
