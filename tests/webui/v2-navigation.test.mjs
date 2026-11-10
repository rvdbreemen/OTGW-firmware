// TASK-1312 / TASK-1326 browser check of the v2 navigation concept (docs/design/v2-navigation-concept.md).
//
// Drives the REAL shipped v2.html/v2.js in headless system Chrome over CDP against a
// fake device (same harness as v2-settings-sat-followups.test.mjs) and checks:
//   a  Settings keeps its rail (TASK-1326, maintainer decision 2026-10-23): one .rail-item per
//      non-empty category in SET_CATS order, the first one active, no collapsible groups
//   b  clicking a rail item shows that category's card panels only
//   c  search filters the categories in the rail and shows every matching one
//   d  the "View" link of the BLE discovery card selects Sensors and shows the roster, also
//      with a slow settings GET (800 ms) in a small viewport (roster card in view)
//   e  Monitor remembers the tab the user clicked (otgw-v2-montab) across a reload; a tab opened
//      from code (the connection-health button) is shown but not stored
//   f  the Advanced raw-debug dump is a details.grp without inline styling
//   g  edits in two categories share one save bar; a refused IP save selects the category of
//      the refused row and scrolls to it
//
// Run: node --test tests/webui/v2-navigation.test.mjs
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawn } from 'node:child_process';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { stopChromeTree, guardChrome, launchChrome, CDP_TIMEOUT_MS } from './lib/page-harness.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
let PORT = 0;   // TASK-1489: HTTP and CDP ports are picked by the OS, so parallel runs and
                // other sessions running the same test cannot collide on them.
// Chrome started with --remote-debugging-port=0 writes the port it bound to the first
// line of <profile>/DevToolsActivePort; it throws until Chrome is up.
function cdpPort() { return fs.readFileSync(path.join(profile, 'DevToolsActivePort'), 'utf8').split('\n')[0].trim(); }

const CHROME = [
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/usr/bin/google-chrome',
  '/usr/bin/chromium',
].find((p) => fs.existsSync(p));
if (!CHROME) { console.log('SKIP: no Chrome/Edge binary found; cannot run the browser harness'); process.exit(0); }

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.svg': 'image/svg+xml' };

// GET /api/v2/settings shapes (restAPI.ino addStr/addInt/addBool: value,type,maxlen|min,max).
const S = (value, maxlen = 15) => ({ value, type: 's', maxlen });
const SETTINGS = {
  boardmode: { value: 0, type: 'i', min: 0, max: 3 },
  wifistaticip: S(''), wifisubnet: S(''), ethstaticip: { value: false, type: 'b' },
  mqttbroker: S('broker.local', 64), mqttport: { value: 1883, type: 'i', min: 1, max: 65535 },
  ntpserver: S('pool.ntp.org', 40),
  webhookenable: { value: true, type: 'b' }, webhookurlon: S('http://x/on', 100),
  satbleenable: { value: true, type: 'b' }, satblenameprefix: S('', 23),
  sattargettemp: { value: 20, type: 'i', min: 5, max: 30 },
};
// One new, unnamed BLE sensor: the discovery poll turns it into the grouped "View" card.
const BLE = { max_slots: 8, populated_slots: 1, psram: 1, ble_enable: true, risk_ack: true,
  sensors: [{ mac: 'AA:BB:CC:DD:EE:01', valid: true, name: 'ATC_01', temp: 20.1, hum: 41, label: '' }] };

let settingsDelay = 0;      // ms added to GET /api/v2/settings (a slow device)
const reqLog = [];          // "METHOD path body" of every /api/ request
const srv = http.createServer((q, s) => {
  const u = new URL(q.url, `http://127.0.0.1:${PORT}`);
  const p = decodeURIComponent(u.pathname);
  const api = p.indexOf('/api/') === 0;
  let body = '';
  q.on('data', (c) => { body += c; });
  q.on('end', () => {
    const json = (o, delay = 0) => setTimeout(() => { s.writeHead(200, { 'content-type': 'application/json' }); s.end(JSON.stringify(o)); }, delay);
    if (api) reqLog.push(q.method + ' ' + p + ' ' + body);
    if (p.endsWith('/v2/settings')) return q.method === 'POST' ? json({ status: 'ok' }) : json(SETTINGS, settingsDelay);
    if (p.endsWith('/v2/sat/ble/discovery')) return json(BLE);
    if (api) return json({});
    const f = path.join(DATA, p === '/' ? '/v2.html' : p);
    if (fs.existsSync(f) && fs.statSync(f).isFile()) {
      s.writeHead(200, { 'content-type': MIME[path.extname(f)] || 'application/octet-stream' });
      fs.createReadStream(f).pipe(s);
    } else { s.writeHead(404); s.end('nf'); }
  });
});
// TASK-1766: the fake device accepts the page's WebSocket and keeps it open (it sends no frames),
// as a real device does. Refused, every connect closed at once: ws.onclose then fetched otgw/otmonitor
// (v2.js connectWs: onclose -> fetchSeed()) and retried 5 s later, and the 20 s closed-socket
// fallback fetched it too, so otmonitor GETs landed at random moments inside the checks below.
const wsSocks = new Set();
srv.on('upgrade', (q, sock) => {
  const key = q.headers['sec-websocket-key'];
  if (!key) { sock.destroy(); return; }
  const accept = crypto.createHash('sha1').update(key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
  sock.write(['HTTP/1.1 101 Switching Protocols', 'Upgrade: websocket', 'Connection: Upgrade', 'Sec-WebSocket-Accept: ' + accept, '', ''].join(String.fromCharCode(13, 10)));
  wsSocks.add(sock);
  sock.on('data', (b) => { if ((b[0] & 0x0f) === 8) sock.end(Buffer.from([0x88, 0x00])); });   // the page left: answer its close frame, then end
  sock.on('error', () => {});
  sock.on('close', () => wsSocks.delete(sock));
});
await new Promise((r) => srv.listen(0, '127.0.0.1', r));
PORT = srv.address().port;

// TASK-1766: start-up (Chrome, its CDP socket, the first Settings render) gets its own deadlines and one
// retry with a fresh Chrome. In 5 of 17 runs on 2026-11-10 a busy machine's headless Chrome failed here,
// before any check: no reply to Page.enable or Runtime.evaluate, the CDP socket closed or refused, a
// first page load of 14 s. Without deadlines the file then hung with no output until an outer timeout.
let profile = null; let chrome = null; let ws = null;
let msgId = 0; const pending = new Map(); let exceptions = [];
// Every CDP call has a deadline (as in lib/page-harness.mjs, TASK-1715).
const send = (method, params = {}) => new Promise((res, rej) => {
  const id = ++msgId;
  const timer = setTimeout(() => { if (pending.delete(id)) rej(new Error(`CDP ${method} got no reply within ${CDP_TIMEOUT_MS} ms (Chrome stalled?)`)); }, CDP_TIMEOUT_MS);
  pending.set(id, (m) => { clearTimeout(timer); if (m.closed) rej(new Error(`CDP socket closed during ${method}`)); else res(m); });
  try { ws.send(JSON.stringify({ id, method, params })); } catch (e) { pending.delete(id); clearTimeout(timer); rej(e); }
});
async function startChrome() {
  profile = fs.mkdtempSync(path.join(os.tmpdir(), 'otgw-cdp-'));
  chrome = await launchChrome(CHROME, ['--headless=new', '--remote-debugging-port=0', `--user-data-dir=${profile}`,
    '--no-first-run', '--no-default-browser-check', '--disable-gpu', '--window-size=1400,1000', 'about:blank'], { stdio: 'ignore' });
  guardChrome(chrome);   // TASK-1739
  let target = null;
  for (const t0 = Date.now(); !target && Date.now() - t0 < 20000;) {   // 20 s wall time, also when each try waits its 3 s
    try { const r = await fetch(`http://127.0.0.1:${cdpPort()}/json/new?about:blank`, { method: 'PUT', signal: AbortSignal.timeout(3000) }); if (r.ok) target = await r.json(); } catch { /* not up yet */ }
    if (!target) await new Promise((r) => setTimeout(r, 200));
  }
  if (!target) throw new Error('Chrome DevTools endpoint never came up');
  ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((res, rej) => { ws.onopen = res; ws.onerror = () => rej(new Error('CDP WebSocket refused')); setTimeout(() => rej(new Error('CDP WebSocket did not open within 10 s (Chrome stalled?)')), 10000); });
  exceptions = [];
  ws.addEventListener('message', (ev) => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
    if (m.method === 'Runtime.exceptionThrown') exceptions.push(JSON.stringify(m.params.exceptionDetails).slice(0, 300));
  });
  const sock = ws;   // a late close of a failed attempt's socket must not fail the next attempt's calls
  ws.addEventListener('close', () => { if (sock !== ws) return; for (const [id, fn] of pending) { pending.delete(id); fn({ closed: true }); } });
  await send('Page.enable'); await send('Runtime.enable');
  // v2.js gives each tab a random poll phase, TAB_PHASE_MS = Math.floor(Math.random() * 15000) (TASK-1741,
  // its only Math.random), and the first BLE discovery poll comes 13.75 s + that phase after load. A phase
  // above about 11 s put the card past the 25 s waits below. Phase 0 makes the timing the same in every run.
  await send('Page.addScriptToEvaluateOnNewDocument', { source: 'Math.random = function () { return 0; };' });
}
async function stopChrome() {
  try { ws.close(); } catch { /* ignore */ }
  for (const sk of wsSocks) sk.destroy();
  if (!chrome) return;
  stopChromeTree(chrome);
  // TASK-1492: Chrome holds the profile until it has exited; wait for that, then retry the removal.
  await new Promise((r) => { if (chrome.exitCode !== null || chrome.signalCode !== null) return r(); chrome.once('exit', r); setTimeout(r, 5000); });
  try { fs.rmSync(profile, { recursive: true, force: true, maxRetries: 10, retryDelay: 200 }); } catch (e) { console.log('profile cleanup failed: ' + e.message); }
}
const evalJs = async (expression) => {
  const r = await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
  return r.result && r.result.result ? r.result.result.value : undefined;
};
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
// Bounded by wall time: counted in loop turns, a stalled Chrome (each evalJs waiting CDP_TIMEOUT_MS) stretched
// a 20 s wait to over an hour.
async function until(expr, ms = 5000) {
  for (const t0 = Date.now(); Date.now() - t0 < ms;) { const v = await evalJs(expr).catch(() => null); if (v) return v; await sleep(100); }
  return evalJs(expr).catch(() => null);
}

let ok = true; let n = 0;
function check(pass, msg) { n++; console.log((pass ? 'PASS' : 'FAIL') + ': ' + msg); if (!pass) ok = false; }

const PAGE_URL = `http://127.0.0.1:${PORT}/v2.html`;
// window.__otgwOld marks the document a load() leaves, so READY can only hold on the new one.
const READY = `!window.__otgwOld && document.readyState === 'complete' && !!document.querySelector('.seg[data-page="settings"]')`;
let loadMark = 0;   // reqLog index at the last load()
async function load(page) {
  loadMark = reqLog.length;
  await evalJs('window.__otgwOld = 1').catch(() => {});
  await send('Page.navigate', { url: PAGE_URL });
  // 20 s: on a busy machine Chrome sometimes holds a page's requests for several seconds before they reach
  // the fake device; a page that is not up after 20 s fails here by name instead of in the next check.
  if (!(await until(READY, 20000))) check(false, `load(${page || ''}): the page did not finish loading within 20 s`);
  if (page) await click(`.seg[data-page="${page}"]`);
}
const railItems = () => evalJs(`Array.prototype.map.call(document.querySelectorAll('#setRail .rail-item'), function (r) { return r.dataset.cat + (r.classList.contains('active') ? '*' : ''); }).join(',')`);
const rowKeys = () => evalJs(`Array.prototype.map.call(document.querySelectorAll('#setCols .srow'), function (r) { return r.dataset.key; }).sort().join(',')`);
const stored = (k) => evalJs(`localStorage.getItem('${k}')`);
const setSearch = (v) => evalJs(`(function(){ var i = document.getElementById('setSearch'); i.value = ${JSON.stringify(v)}; i.dispatchEvent(new Event('input')); })()`);
// A click whose target is missing is a failure of its own, not a silent no-op.
const click = async (sel) => { if (!(await evalJs(`(function(){ var e = document.querySelector(${JSON.stringify(sel)}); if (!e) return false; e.click(); return true; })()`))) check(false, `click: ${sel} not found`); };
const settle = async () => { await until(`document.querySelectorAll('#setRail .rail-item').length >= 5`); await sleep(400); };

for (let attempt = 1; ; attempt++) {
  try {
    await startChrome();
    const nav = async () => { await send('Page.navigate', { url: PAGE_URL }); if (!(await until(READY, 15000))) throw new Error('the first page load did not finish within 15 s'); };
    await nav();
    await evalJs(`localStorage.clear()`);
    await nav();
    if (!(await evalJs(`(function(){ var e = document.querySelector('.seg[data-page="settings"]'); if (!e) return false; e.click(); return true; })()`))) throw new Error('no Settings button');
    if (!(await until(`document.querySelectorAll('#setRail .rail-item').length >= 5`, 15000))) throw new Error('the first Settings render did not come within 15 s');
    await sleep(400);
    break;
  } catch (e) {
    if (attempt >= 2) throw e;
    console.log(`NOTE: Chrome start-up attempt ${attempt} failed (${e.message}); starting a fresh Chrome once more`);
    await stopChrome();
  }
}

// ---- a: the rail, one item per non-empty category in SET_CATS order, first category shown ----
const ORDER = ['system', 'network', 'mqtt', 'ntp', 'otgw', 'otd', 'sensors', 'outputs', 's0', 'webhook', 'behavior', 'sat'];
const r0 = (await railItems()).split(',');
const ids0 = r0.map((x) => x.replace('*', ''));
check(await evalJs(`!!document.querySelector('.set-layout > nav#setRail.set-rail')`), 'Settings has the #setRail nav inside .set-layout');
check(['system', 'network', 'mqtt', 'ntp', 'sensors', 'webhook', 'sat'].every((c) => ids0.includes(c)) && ids0.length === 7, `one rail item per non-empty category (${ids0.join(',')})`);
check(JSON.stringify(ids0) === JSON.stringify(ORDER.filter((c) => ids0.includes(c))), 'rail items follow the SET_CATS order');
check(r0[0] === 'system*' && r0.filter((x) => x.endsWith('*')).length === 1, `the first category is active (${r0.join(',')})`);
check(await evalJs(`!document.querySelector('#setCols details')`), 'the content has no collapsible groups');
check(await evalJs(`Array.prototype.every.call(document.querySelectorAll('#setRail .rail-item'), function (r) { return !!r.querySelector('.cnt'); })`), 'each rail item carries a count');
check((await rowKeys()) === 'boardmode' && (await evalJs(`document.querySelectorAll('#setCols .set-cat-title').length`)) === 1, `only the first category's cards are shown (${await rowKeys()})`);

// ---- b: clicking a rail item shows that category only ----
await click('#setRail .rail-item[data-cat="mqtt"]');
check((await rowKeys()) === 'mqttbroker,mqttport' && (await railItems()).includes('mqtt*') && (await evalJs(`document.querySelectorAll('#setCols .set-cat-title').length`)) === 1, `clicking MQTT shows the MQTT cards only (${await rowKeys()}, ${await railItems()})`);
await click('#setRail .rail-item[data-cat="network"]');
check((await rowKeys()) === 'ethstaticip,wifistaticip,wifisubnet' && (await railItems()).includes('network*') && !(await railItems()).includes('mqtt*'), `clicking Network switches to the Network cards (${await rowKeys()})`);
await setSearch('zzz'); await sleep(100);
check((await railItems()) === '' && (await rowKeys()) === '', 'a search without a match empties the rail and the cards');
await setSearch(''); await sleep(100);
check((await railItems()).includes('network*') && (await rowKeys()).includes('wifistaticip'), `clearing the search returns to the selected category (${await railItems()})`);

// ---- c: search filters the categories ----
await setSearch('mqttbroker'); await sleep(150);
check((await railItems()) === 'mqtt' && (await rowKeys()) === 'mqttbroker', `search for a key leaves its category in the rail and its row (${await railItems()}, ${await rowKeys()})`);
await setSearch('enable'); await sleep(150);
check((await railItems()) === 'sensors,webhook' && (await evalJs(`document.querySelectorAll('#setCols .set-cat-title').length`)) === 2, `a term matching two categories shows both (${await railItems()})`);
await setSearch(''); await sleep(150);

// ---- d: the BLE discovery "View" link selects Sensors and scrolls to the roster ----
await click('#setRail .rail-item[data-cat="ntp"]');
const view = await until(`(function(){ var b = document.querySelector('[data-ble-group] button'); return b ? b.textContent : null; })()`, 25000);
check(!!view, `the grouped BLE discovery card appears (button "${view}")`);
await evalJs(`(function(){ var bs = document.querySelectorAll('[data-ble-group] button'); for (var i = 0; i < bs.length; i++) if (/view/i.test(bs[i].textContent)) { bs[i].click(); return; } bs[0].click(); })()`);
await until(`!!document.getElementById('setcard-ble')`);
check((await railItems()).includes('sensors*') && await evalJs(`document.getElementById('page-settings').classList.contains('active')`), `the link switched to Settings with Sensors selected (${await railItems()})`);
await sleep(600);   // the settings re-render after the link must keep the selection
check((await railItems()).includes('sensors*') && await evalJs(`!!document.getElementById('setcard-ble')`), 'Sensors stays selected and the BLE roster card is shown');

// ---- e: Monitor tab memory ----
await evalJs(`localStorage.removeItem('otgw-v2-montab')`);
await load('monitor');
check((await evalJs(`(document.querySelector('#page-monitor .subtab.active') || {}).dataset.tab`)) === 'mlog', 'Monitor starts on the Log tab when nothing is stored');
await click('#page-monitor .subtab[data-tab="mstats"]');
check((await stored('otgw-v2-montab')) === 'mstats', `a clicked tab is stored (${await stored('otgw-v2-montab')})`);
await load();   // restores the page (monitor) from otgw-v2-page
await until(`!!document.querySelector('#page-monitor.active')`);
const restored = await until(`(function(){ var a = document.querySelector('#page-monitor .subtab.active'); return a && a.dataset.tab === 'mstats' && document.getElementById('mstats').classList.contains('active') ? 'mstats' : null; })()`);
check(restored === 'mstats', 'after a reload the clicked Monitor tab is active again');
await evalJs(`localStorage.setItem('otgw-v2-montab', 'nonsense')`);
await load();
await until(`!!document.querySelector('#page-monitor.active')`);
check(await evalJs(`!!document.querySelector('#page-monitor .subtab.active')`) && exceptions.length === 0, 'an unknown stored tab is ignored without an exception');
await evalJs(`localStorage.setItem('otgw-v2-montab', 'mstats')`);
await load();
const hb = await until(`!!document.querySelector('.connsum')`, 20000);
if (hb) {
  await evalJs(`document.querySelector('.seg[data-page="home"]').click()`);
  await click('.connsum');
  const conn = await until(`(function(){ var a = document.querySelector('#page-monitor .subtab.active'); return a && a.dataset.tab === 'mconn' ? 'mconn' : null; })()`);
  check(conn === 'mconn' && (await stored('otgw-v2-montab')) === 'mstats', `a tab opened from code (Connection) is shown but not stored (stored: ${await stored('otgw-v2-montab')})`);
} else check(false, 'the connection-health button never appeared, programmatic showTab not exercised');

// ---- rule 6: edits in two categories, one save bar, one save posts both ----
await click('#setRail .rail-item[data-cat="ntp"]');
await evalJs(`(function(){ var a = document.querySelector('.srow[data-key="ntpserver"] input'); a.value = 'ntp.example'; a.dispatchEvent(new Event('input')); })()`);
await click('#setRail .rail-item[data-cat="mqtt"]');
await evalJs(`(function(){ var b = document.querySelector('.srow[data-key="mqttbroker"] input'); b.value = 'mq.example'; b.dispatchEvent(new Event('input')); })()`);
check(await evalJs(`document.getElementById('dirtyCount').textContent === '2' && document.getElementById('saveBar').classList.contains('show') && document.querySelectorAll('#page-settings .savebar').length === 1`), 'two edits in two categories show one save bar with count 2');
const nReq = reqLog.length;
await click('#btnSave');
await sleep(800);
const posts = reqLog.slice(nReq).filter((l) => l.startsWith('POST /api/v2/settings')).join('|');
check(/"name":"ntpserver","value":"ntp.example"/.test(posts) && /"name":"mqttbroker","value":"mq.example"/.test(posts), `one save POSTs the keys of both categories (${posts})`);

// ---- refused IP save: the category of the refused row is selected and the row scrolled into view ----
await load('settings'); await settle();
await click('#setRail .rail-item[data-cat="network"]');
await send('Emulation.setDeviceMetricsOverride', { width: 1000, height: 500, deviceScaleFactor: 1, mobile: false });
await evalJs(`(function(){ var i = document.querySelector('.srow[data-key="wifistaticip"] input'); i.value = '1.2.3'; i.dispatchEvent(new Event('input')); })()`);
await click('#setRail .rail-item[data-cat="mqtt"]');   // leave the network category: the refusal must bring it back
await evalJs(`window.scrollTo(0, 0)`);
await click('#btnSave');
await sleep(500);
check((await railItems()).includes('network*') && (await rowKeys()).includes('wifistaticip'), `a refused IP save selects the network category (${await railItems()})`);
check(await evalJs(`(function(){ var r = document.querySelector('.srow[data-key="wifistaticip"]').getBoundingClientRect(); return r.top >= -1 && r.bottom <= innerHeight + 1; })()`), 'the refused IP row is scrolled into view');
await send('Emulation.clearDeviceMetricsOverride');

// ---- d2: the link with a slow settings GET, small viewport ----
settingsDelay = 800;
await evalJs(`localStorage.removeItem('otgw-ble-seen')`);
await send('Emulation.setDeviceMetricsOverride', { width: 1000, height: 500, deviceScaleFactor: 1, mobile: false });
await load('home');
const view2 = await until(`(function(){ var b = document.querySelector('[data-ble-group] button'); return b ? b.textContent : null; })()`, 25000);
check(!!view2, 'the BLE discovery card appears again for the slow-device run');
await evalJs(`(function(){ var bs = document.querySelectorAll('[data-ble-group] button'); for (var i = 0; i < bs.length; i++) if (/view/i.test(bs[i].textContent)) { bs[i].click(); return; } bs[0].click(); })()`);
const inView = await until(`(function(){ var c = document.getElementById('setcard-ble'); if (!c) return null; var r = c.getBoundingClientRect(); return r.top >= -1 && r.top < innerHeight ? 'y' + Math.round(r.top) : null; })()`, 8000);
check(!!inView && (await railItems()).includes('sensors*'), `with a slow GET settings (800 ms) the link still ends on Sensors with the roster card in view (${inView})`);
settingsDelay = 0;
await send('Emulation.clearDeviceMetricsOverride');

// ---- Monitor: a caller-chosen tab does not restore or fetch the stored tab ----
await evalJs(`localStorage.setItem('otgw-v2-montab', 'mvalues')`);
await load('home');
const chip = await until(`!!document.querySelector('.connsum')`, 20000);
check(!!chip, 'the connection-health chip is there');
// With the socket open, the only otmonitor fetch on Home is the start-up seed 2 s after load
// (v2.js init: setTimeout(fetchSeed, 2000)). Wait for it, so the window below cannot catch it.
for (let i = 0; i < 100 && !reqLog.slice(loadMark).some((l) => /otgw\/otmonitor/.test(l)); i++) await sleep(50);
if (!reqLog.slice(loadMark).some((l) => /otgw\/otmonitor/.test(l))) console.log('NOTE: the start-up otmonitor seed has not come yet; a late one would land in the window below');
await sleep(200);
let mark = reqLog.length;
await click('.connsum');
await until(`(document.querySelector('#page-monitor .subtab.active') || {}).dataset && document.querySelector('#page-monitor .subtab.active').dataset.tab === 'mconn'`);
await sleep(800);
check(!reqLog.slice(mark).some((l) => /otgw\/otmonitor/.test(l)) && (await stored('otgw-v2-montab')) === 'mvalues', `the chip shows Connection without the stored Values tab's fetch, montab unchanged (${reqLog.slice(mark).map((l) => l.split(' ')[1]).join(',')})`);
await evalJs(`document.querySelector('.seg[data-page="home"]').click()`);
mark = reqLog.length;
await evalJs(`document.querySelector('.seg[data-page="monitor"]').click()`);
await sleep(800);
check(reqLog.slice(mark).some((l) => /otgw\/otmonitor/.test(l)) && (await evalJs(`document.querySelector('#page-monitor .subtab.active').dataset.tab`)) === 'mvalues', 'control: the plain Monitor button restores Values and does fetch it');

// ---- Advanced tab memory ----
await evalJs(`localStorage.setItem('otgw-v2-advtab', 'system')`);
await load('advanced');
await click('#advTabs .subtab[data-adv="debug"]');
check((await stored('otgw-v2-advtab')) === 'debug', 'a clicked Advanced tab is stored');
await load();
await until(`!!document.querySelector('#page-advanced.active')`);
check(await until(`(document.querySelector('#advTabs .subtab.active') || {}).dataset && document.querySelector('#advTabs .subtab.active').dataset.adv === 'debug'`), 'after a reload the clicked Advanced tab is active again');

// ---- f: Advanced raw dump uses the shared group style ----
check(await evalJs(`!!document.querySelector('#adv-debug details.grp > summary') && !document.querySelector('#adv-debug details[style]')`), 'the Advanced raw-debug dump is a details.grp without inline styling');

check(exceptions.length === 0, `no uncaught page exceptions (${exceptions.join(' ; ')})`);

await stopChrome(); srv.close();
console.log('\n' + (ok ? 0 : 'some') + ' failures of ' + n + ' checks');
process.exit(ok ? 0 : 1);
