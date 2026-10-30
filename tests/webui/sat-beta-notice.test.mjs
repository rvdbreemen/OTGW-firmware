// TASK-1507 guard: the first 2.0.0 beta is opt-in for SAT.
//   - Enabling SAT (the SAT-page toggle) opens the setup wizard on a beta-notice step first:
//     experimental, controls the boiler, field data wanted, capture tool + where to share.
//     Continue stays disabled until the box is ticked; Cancel closes the wizard, SAT stays off
//     and nothing is POSTed.
//   - The SAT page shows the same notice on a prerelease firmware version (-alpha.N / -beta.N)
//     and not on a release version; Dismiss hides it for the browser session only.
//
// Drives the REAL shipped v2.html/v2.js in headless system Chrome/Edge over the DevTools
// protocol, against a local fake device. No npm install; prints SKIP when no browser is found.
// OTGW_DATA_DIR=<dir> serves another data dir (old-vs-fix runs). SHOT_DIR=<dir> saves screenshots.
//
// Run: node tests/webui/sat-beta-notice.test.mjs
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
// TASK-1489: both ports are picked by the OS (listen(0), --remote-debugging-port=0), so a
// parallel run, or another session running the same test, cannot collide on them.
let PORT = 0;

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
  enabled: false, active: true, safety_tripped: false, window_open: false, summer_active: false,
  control_mode: 1, room_temp: 20.9, target_temp: 21.0, final_setpoint: 32.0, heating_curve: 30.0,
  pid_output: 2.0, error: 0.1, flame: false, boiler_status: 'idle',
  dhw_enabled: true, dhw_enable: true, dhw_setpoint: 50, cycle_mode: 'continuous',
};
let statusHits = 0;
let FW = '2.0.0-beta.1+abc1234';
const posts = [];
const allPosts = [];   // never cleared, for the whole-run assertion

const srv = http.createServer((q, s) => {
  const url = new URL(q.url, `http://127.0.0.1:${PORT}`);
  let p = decodeURIComponent(url.pathname);
  if (p.indexOf('/api/') === 0) {
    if (q.method === 'POST') {
      let body = '';
      q.on('data', (c) => { body += c; });
      q.on('end', () => {
        posts.push(p + ' ' + body);
        allPosts.push(p + ' ' + body);
        s.writeHead(200, { 'content-type': 'application/json' });
        s.end('{}');
      });
      return;
    }
    s.writeHead(200, { 'content-type': 'application/json' });
    if (p.endsWith('/v2/sat/status')) { statusHits++; s.end(JSON.stringify(STATUS)); }
    else if (p.endsWith('/v2/device/info')) s.end(JSON.stringify({ device: { fwversion: FW, hostname: 'otgw-test' } }));
    else if (p.endsWith('/v2/settings')) s.end(JSON.stringify({ settings: { satenabled: { value: false, type: 'b' }, sat_onboarded: { value: false, type: 'b' }, satsystem: { value: '0' }, satmanufacturer: { value: '0' } } }));
    else s.end('{}');
    return;
  }
  if (p === '/') p = '/v2.html';
  const f = path.join(DATA, p);
  if (fs.existsSync(f) && fs.statSync(f).isFile()) {
    s.writeHead(200, { 'content-type': MIME[path.extname(f)] || 'application/octet-stream' });
    fs.createReadStream(f).pipe(s);
  } else { s.writeHead(404); s.end('nf'); }
});
await new Promise((r) => srv.listen(0, '127.0.0.1', r));
PORT = srv.address().port;

const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'otgw-cdp-'));
const chrome = spawn(CHROME, [
  '--headless=new', '--remote-debugging-port=0', `--user-data-dir=${profile}`,
  '--no-first-run', '--no-default-browser-check', '--disable-gpu', 'about:blank',
], { stdio: 'ignore' });

// Chrome writes the port it bound to the first line of <profile>/DevToolsActivePort.
async function cdpTarget() {
  for (let i = 0; i < 150; i++) {
    try {
      const cdpPort = fs.readFileSync(path.join(profile, 'DevToolsActivePort'), 'utf8').split('\n')[0].trim();
      const r = await fetch(`http://127.0.0.1:${cdpPort}/json/new?about:blank`, { method: 'PUT' });
      if (r.ok) return await r.json();
    } catch { /* not up yet */ }
    await new Promise((r) => setTimeout(r, 200));
  }
  throw new Error('Chrome DevTools endpoint never came up');
}
const cdpT0 = Date.now();
const target = await cdpTarget();
console.log(`INFO: Chrome DevTools endpoint up after ${Date.now() - cdpT0} ms`);
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
// TASK-1489: the .seg buttons are static HTML, but the loader injects v2.js only after the
// CSS, so a click before init() has bound the handlers does nothing. init() sets
// window.OTGWv2 as its last step: wait for that before clicking.
async function openSat(view) {
  const before = statusHits;
  await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/v2.html` });
  let ready = false;
  for (let i = 0; i < 300 && !ready; i++) {
    await sleep(100);
    ready = await evalJs(`!!window.OTGWv2`).catch(() => false);
  }
  if (!ready) throw new Error('v2.js init() did not finish within 30 s (window.OTGWv2 not set)');
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
  var dial = el('satDial'), hist = el('satHistChart'), cyc = el('satCycleMode');
  var sum = g ? g.querySelector('summary') : null;
  var f = Node.DOCUMENT_POSITION_FOLLOWING;
  return JSON.stringify({
    group: !!g, groupOpen: g ? g.open : null, summaryVisible: vis(sum),
    dialBeforeHist: !!(dial && hist && (dial.compareDocumentPosition(hist) & f)),
    histBeforeGroup: !!(hist && g && (hist.compareDocumentPosition(g) & f)),
    histVisible: vis(hist), dialVisible: vis(dial),
    cycleVisible: vis(cyc), cycleText: cyc ? cyc.textContent : null, cycleButtons: cyc ? !!cyc.closest('.card').querySelector('button') : null, cycleSub: el('satCycleSub') ? el('satCycleSub').textContent : null, sliderVisible: vis(el('satDhwSlider')),
    stored: (function(){try{return localStorage.getItem('otgw-v2-satopen');}catch(e){return 'ERR';}})()
  });
})()`;
const facts = async () => JSON.parse(await evalJs(FACTS));


const shotDir = process.env.SHOT_DIR;
async function shot(name) {
  if (!shotDir) return;
  const r = await send('Page.captureScreenshot', { format: 'png' });
  if (r.result && r.result.data) {
    fs.mkdirSync(shotDir, { recursive: true });
    fs.writeFileSync(path.join(shotDir, name), Buffer.from(r.result.data, 'base64'));
    console.log('screenshot: ' + path.join(shotDir, name));
  }
}
async function loadSat(fw) {
  FW = fw;
  await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/v2.html` });
  await sleep(300);
  await evalJs(`try{sessionStorage.clear()}catch(e){}`).catch(() => { });
  let ready = false;
  for (let i = 0; i < 300 && !ready; i++) { await sleep(100); ready = await evalJs(`!!window.OTGWv2`).catch(() => false); }
  if (!ready) throw new Error('v2.js init() did not finish (window.OTGWv2 not set)');
  const before = statusHits;
  await evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
  for (let i = 0; i < 100 && statusHits <= before; i++) await sleep(100);
  await sleep(800);
}
const noticeShown = () => evalJs(`(function(){var e=document.getElementById('satBetaNotice');return !!e && e.style.display!=='none' && e.offsetParent!==null;})()`);
const noticeText = () => evalJs(`(document.getElementById('satBetaNoticeText')||{}).textContent||''`);
const wizText = () => evalJs(`(function(){var e=document.getElementById('satOnbBack');return e?e.textContent:null;})()`);
const settingPosts = () => allPosts.filter((x) => x.indexOf('/v2/settings') !== -1 || x.indexOf('/v2/sat/') !== -1);
const tick = (id) => evalJs(`(function(){var c=document.getElementById('${id}');if(!c)return;c.checked=true;c.dispatchEvent(new Event('change',{bubbles:true}));})()`);

// (1) prerelease firmware: notice on the SAT page
await loadSat('2.0.0-beta.1+abc1234');
check(await noticeShown(), 'prerelease 2.0.0-beta.1: the SAT page shows the beta notice');
const nt = await noticeText();
check(/experimental/i.test(nt) && /boiler/i.test(nt) && /field data/i.test(nt) && /capture/i.test(nt) && /#alpha-testing/.test(nt) && /github\.com\/rvdbreemen\/OTGW-firmware\/issues/.test(nt),
  'notice names: experimental, boiler, field data, capture tool, Discord #alpha-testing, GitHub issues');
await shot('after-sat-page-notice-prerelease.png');

// (2) dismiss hides it for the session
await evalJs(`var b=document.getElementById('satBetaNoticeClose');if(b)b.click()`);
check(!(await noticeShown()), 'Dismiss hides the notice');
check((await evalJs(`sessionStorage.getItem('otgw-v2-satbeta-dismissed')`)) === '1', 'dismissal is kept in sessionStorage');
check((await evalJs(`localStorage.getItem('otgw-v2-satbeta-dismissed')`)) === null, 'dismissal is NOT kept in localStorage');

// (3) release firmware: no notice
await loadSat('2.0.0+abc1234');
check(!(await noticeShown()), 'release 2.0.0: the SAT page shows no beta notice');
await shot('after-sat-page-no-notice-release.png');

// (4) enabling SAT: the beta step comes first; Cancel leaves SAT off
await loadSat('2.0.0-beta.1+abc1234');
const postsBefore = settingPosts().length;
await tick('satEnable');
let w = null;
for (let i = 0; i < 50 && !w; i++) { await sleep(100); w = await wizText(); }
check(typeof w === 'string' && w.indexOf('Before you enable SAT') !== -1, 'enabling SAT opens the wizard on the beta-notice step first');
check(typeof w === 'string' && /experimental/i.test(w) && /field data/i.test(w) && /#alpha-testing/.test(w), 'beta step states experimental, field data and where to share');
check((await evalJs(`(document.getElementById('satOnbBetaGo')||{}).disabled`)) === true, 'Continue is disabled before the box is ticked');
await evalJs(`var b=document.querySelector('#satOnbBack [data-sa="beta-ok"]');if(b)b.click()`);
await sleep(200);
check(((await wizText()) || '').indexOf('Before you enable SAT') !== -1, 'a click on the disabled Continue does not advance');
await shot('after-wizard-beta-step.png');
await tick('satOnbBetaOk');
check((await evalJs(`(document.getElementById('satOnbBetaGo')||{}).disabled`)) === false, 'Continue is enabled after the box is ticked');
await evalJs(`var b=document.querySelector('#satOnbBack [data-sa="notnow"]');if(b)b.click()`);
await sleep(200);
check((await wizText()) === null, 'Cancel closes the wizard');
check((await evalJs(`document.getElementById('satEnable').checked`)) === false, 'Cancel leaves the SAT toggle off');
check(settingPosts().length === postsBefore, 'Cancel POSTs nothing (SAT stays off)');

// (5) confirming moves on to the existing wizard
await tick('satEnable');
for (let i = 0; i < 50 && !(await wizText()); i++) await sleep(100);
await tick('satOnbBetaOk');
await evalJs(`var b=document.querySelector('#satOnbBack [data-sa="beta-ok"]');if(b)b.click()`);
await sleep(200);
check(((await wizText()) || '').indexOf("Let's tune SAT for your system") !== -1, 'after confirming, the existing wizard continues at its welcome screen');
check(settingPosts().length === postsBefore, 'still nothing POSTed before the wizard Finish');
await shot('after-wizard-welcome.png');

try { ws.close(); } catch { /* ignore */ }
chrome.kill();
srv.close();
try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* ignore */ }
check(exceptions.length === 0, 'no uncaught page exceptions' + (exceptions.length ? ': ' + exceptions[0] : ''));
console.log(ok ? '\nALL PASS' : '\nFAILED');
process.exit(ok ? 0 : 1);
