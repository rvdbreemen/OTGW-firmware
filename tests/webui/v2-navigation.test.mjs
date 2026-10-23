// TASK-1312 browser check of the v2 navigation concept (docs/design/v2-navigation-concept.md).
//
// Drives the REAL shipped v2.html/v2.js in headless system Chrome over CDP against a
// fake device (same harness as v2-settings-sat-followups.test.mjs) and checks:
//   a  Settings has no rail: one details.grp per non-empty category in SET_CATS order, all closed
//   b  clicking a summary opens its group; the open set survives a re-render (search typed,
//      then cleared) and a page reload (localStorage otgw-v2-setopen)
//   c  searching shows only matching groups, all open; clearing restores the stored open set
//   d  the "View" link of the BLE discovery card opens the Sensors group without storing it
//   e  Monitor remembers the tab the user clicked (otgw-v2-montab) across a reload; a tab opened
//      from code (the connection-health button) is shown but not stored
//   f  the Advanced raw-debug dump is a details.grp without inline styling
//
// Run: node --test tests/webui/v2-navigation.test.mjs
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const PORT = 8148;
const CDP_PORT = 9340;

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
await new Promise((r) => srv.listen(PORT, '127.0.0.1', r));

const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'otgw-cdp-'));
const chrome = spawn(CHROME, ['--headless=new', `--remote-debugging-port=${CDP_PORT}`, `--user-data-dir=${profile}`,
  '--no-first-run', '--no-default-browser-check', '--disable-gpu', '--window-size=1400,1000', 'about:blank'], { stdio: 'ignore' });
let target = null;
for (let i = 0; i < 100 && !target; i++) {
  try { const r = await fetch(`http://127.0.0.1:${CDP_PORT}/json/new?about:blank`, { method: 'PUT' }); if (r.ok) target = await r.json(); } catch { /* not up yet */ }
  if (!target) await new Promise((r) => setTimeout(r, 200));
}
if (!target) throw new Error('Chrome DevTools endpoint never came up');
const ws = new WebSocket(target.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
let msgId = 0; const pending = new Map(); const exceptions = [];
ws.addEventListener('message', (ev) => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
  if (m.method === 'Runtime.exceptionThrown') exceptions.push(JSON.stringify(m.params.exceptionDetails).slice(0, 300));
});
const send = (method, params = {}) => new Promise((res) => { const id = ++msgId; pending.set(id, res); ws.send(JSON.stringify({ id, method, params })); });
const evalJs = async (expression) => {
  const r = await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
  return r.result && r.result.result ? r.result.result.value : undefined;
};
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function until(expr, ms = 5000) {
  for (let t = 0; t < ms; t += 100) { const v = await evalJs(expr).catch(() => null); if (v) return v; await sleep(100); }
  return evalJs(expr).catch(() => null);
}

let ok = true; let n = 0;
function check(pass, msg) { n++; console.log((pass ? 'PASS' : 'FAIL') + ': ' + msg); if (!pass) ok = false; }

await send('Page.enable'); await send('Runtime.enable');
const PAGE_URL = `http://127.0.0.1:${PORT}/v2.html`;
async function load(page) {
  await send('Page.navigate', { url: PAGE_URL });
  await until(`document.readyState === 'complete' && !!document.querySelector('.seg[data-page="settings"]')`);
  if (page) await evalJs(`document.querySelector('.seg[data-page="${page}"]').click()`);
}
const groups = () => evalJs(`Array.prototype.map.call(document.querySelectorAll('#setCols details.grp'), function (d) { return d.dataset.grp + (d.open ? '+' : '-'); }).join(',')`);
const stored = (k) => evalJs(`localStorage.getItem('${k}')`);
const storedSet = async () => JSON.parse((await stored('otgw-v2-setopen')) || '[]').sort().join();
const setSearch = (v) => evalJs(`(function(){ var i = document.getElementById('setSearch'); i.value = ${JSON.stringify(v)}; i.dispatchEvent(new Event('input')); })()`);
const click = (sel) => evalJs(`document.querySelector(${JSON.stringify(sel)}).click()`);
const settle = async () => { await until(`document.querySelectorAll('#setCols details.grp').length >= 5`); await sleep(400); };

await load();
await evalJs(`localStorage.clear()`);
await load('settings');
await settle();

// ---- a: no rail, one closed group per non-empty category, SET_CATS order ----
const ORDER = ['system', 'network', 'mqtt', 'ntp', 'otgw', 'otd', 'sensors', 'outputs', 's0', 'webhook', 'behavior', 'sat'];
const g0 = (await groups()).split(',');
const ids0 = g0.map((x) => x.slice(0, -1));
check(await evalJs(`!document.getElementById('setRail') && !document.querySelector('.rail-item,.set-rail')`), 'no #setRail / .rail-item in the Settings page');
check(['system', 'network', 'mqtt', 'ntp', 'sensors', 'webhook', 'sat'].every((c) => ids0.includes(c)) && ids0.length === 7, `one group per non-empty category (${ids0.join(',')})`);
check(JSON.stringify(ids0) === JSON.stringify(ORDER.filter((c) => ids0.includes(c))), 'groups follow the SET_CATS order');
check(g0.every((x) => x.endsWith('-')), `all groups are closed by default (${g0.join(',')})`);
check(await evalJs(`Array.prototype.every.call(document.querySelectorAll('#setCols details.grp > summary'), function (s) { return !!s.querySelector('.cnt') || s.parentNode.dataset.grp === 'sat'; })`), 'each summary carries a count (the SAT pointer group has none)');

// ---- b: click opens; survives re-render and reload ----
await click('#setCols details.grp[data-grp="network"] > summary');
await click('#setCols details.grp[data-grp="mqtt"] > summary');
let g = await groups();
check(g.includes('network+') && g.includes('mqtt+'), `clicking a summary opens its group (${g})`);
check((await storedSet()) === 'mqtt,network', `open set stored (${await stored('otgw-v2-setopen')})`);
await setSearch('zzz'); await sleep(100); await setSearch(''); await sleep(100);
g = await groups();
check(g.includes('network+') && g.includes('mqtt+') && g.includes('ntp-'), `open state survives a re-render (${g})`);
await click('#setCols details.grp[data-grp="mqtt"] > summary');   // close again
check((await storedSet()) === 'network', `closing removes it from the stored set (${await stored('otgw-v2-setopen')})`);
await click('#setCols details.grp[data-grp="mqtt"] > summary');   // reopen
await load('settings'); await settle();
g = await groups();
check(g.includes('network+') && g.includes('mqtt+') && g.includes('system-'), `open state survives a reload (${g})`);
await evalJs(`localStorage.setItem('otgw-v2-setopen', '{garbage')`);
await load('settings'); await settle();
check((await groups()).split(',').every((x) => x.endsWith('-')) && exceptions.length === 0, 'a garbage stored value is tolerated (all closed, no exception)');
await evalJs(`localStorage.setItem('otgw-v2-setopen', JSON.stringify(['network','mqtt']))`);
await load('settings'); await settle();

// ---- c: search shows only matching groups, open; clearing restores ----
const before = await groups();
await setSearch('mqttbroker'); await sleep(150);
check((await groups()) === 'mqtt+', `search for a key shows only its group, open (${await groups()})`);
check((await storedSet()) === 'mqtt,network', 'searching does not touch the stored open set');
await setSearch('webhook'); await sleep(150);
check((await groups()) === 'webhook+', `a second search shows only the webhook group (${await groups()})`);
await setSearch(''); await sleep(150);
check((await groups()) === before, `clearing the search restores the previous open set (${before} vs ${await groups()})`);

// ---- d: the BLE discovery "View" link opens Sensors transiently ----
const view = await until(`(function(){ var b = document.querySelector('[data-ble-group] button'); return b ? b.textContent : null; })()`, 25000);
check(!!view, `the grouped BLE discovery card appears (button "${view}")`);
await evalJs(`(function(){ var bs = document.querySelectorAll('[data-ble-group] button'); for (var i = 0; i < bs.length; i++) if (/view/i.test(bs[i].textContent)) { bs[i].click(); return; } bs[0].click(); })()`);
await until(`(document.querySelector('#setCols details.grp[data-grp="sensors"]') || {}).open === true`);
g = await groups();
check(g.includes('sensors+') && g.includes('network+') && g.includes('system-'), `the link opens Sensors next to the stored groups (${g})`);
check(await evalJs(`document.getElementById('page-settings').classList.contains('active')`), 'the link switched to the Settings page');
check((await storedSet()) === 'mqtt,network', `the link does not store the Sensors group (${await stored('otgw-v2-setopen')})`);
await sleep(600);   // the 350 ms follow-up and any settings re-render must keep it open
check((await groups()).includes('sensors+'), 'Sensors stays open across the re-render after the link');
check(await evalJs(`!!document.querySelector('#setCols details.grp[data-grp="sensors"] #setcard-ble')`), 'the BLE roster card sits inside the Sensors group');
await click('#setCols details.grp[data-grp="sensors"] > summary');   // the user closes it
check(!(await groups()).includes('sensors+') && (await storedSet()) === 'mqtt,network', 'closing a link-opened group stores nothing');

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

// ---- g: search interplay, multi-match, script-set open ----
const ALL_BUT_SENSORS = ['system', 'network', 'mqtt', 'ntp', 'webhook', 'sat'];
const preset = async (arr) => { await evalJs(`localStorage.setItem('otgw-v2-setopen', ${JSON.stringify(JSON.stringify(arr))})`); await load('settings'); await settle(); };
await preset(['mqtt']);
await setSearch('ntpserver'); await sleep(150);
await click('#setCols details.grp[data-grp="ntp"] > summary');   // click while searching: closes it visibly
check((await groups()) === 'ntp-' && (await storedSet()) === 'mqtt', `a click during search toggles the group but stores nothing (${await groups()}, stored ${await stored('otgw-v2-setopen')})`);
await setSearch(''); await sleep(150);
check((await groups()) === 'system-,network-,mqtt+,ntp-,sensors-,webhook-,sat-', `clearing restores the pre-search set (mqtt only) (${await groups()})`);
await setSearch('enable'); await sleep(150);
check((await groups()) === 'sensors+,webhook+', `a term matching two groups opens both (${await groups()})`);
await setSearch(''); await sleep(100);
await evalJs(`document.querySelector('#setCols details.grp[data-grp="sensors"]').open = true`);   // set by script, not by the user
await sleep(300);
check((await storedSet()) === 'mqtt', `a script-set details.open is not stored (${await stored('otgw-v2-setopen')})`);
await setSearch('zzz'); await sleep(100); await setSearch(''); await sleep(100);
check((await groups()).includes('sensors-'), 'a script-opened group is closed again by the next render (setOpen is the truth)');

// ---- rule 6: edits in two groups, one save bar, one save posts both ----
await evalJs(`(function(){ var a = document.querySelector('.srow[data-key="ntpserver"] input'); a.value = 'ntp.example'; a.dispatchEvent(new Event('input'));
  var b = document.querySelector('.srow[data-key="mqttbroker"] input'); b.value = 'mq.example'; b.dispatchEvent(new Event('input')); })()`);
check(await evalJs(`document.getElementById('dirtyCount').textContent === '2' && document.getElementById('saveBar').classList.contains('show') && document.querySelectorAll('#page-settings .savebar').length === 1`), 'two edits in two groups show one save bar with count 2');
const nReq = reqLog.length;
await click('#btnSave');
await sleep(800);
const posts = reqLog.slice(nReq).filter((l) => l.startsWith('POST /api/v2/settings')).join('|');
check(/"name":"ntpserver","value":"ntp.example"/.test(posts) && /"name":"mqttbroker","value":"mq.example"/.test(posts), `one save POSTs the keys of both groups (${posts})`);

// ---- keyboard: Enter on a focused summary toggles and stores ----
await preset(['mqtt']);
await evalJs(`document.querySelector('#setCols details.grp[data-grp="ntp"] > summary').focus()`);
for (const type of ['keyDown', 'keyUp']) {
  await send('Input.dispatchKeyEvent', Object.assign({ type, key: 'Enter', code: 'Enter', windowsVirtualKeyCode: 13 }, type === 'keyDown' ? { text: '\r' } : {}));
}
await sleep(200);
check((await groups()).includes('ntp+') && (await storedSet()) === 'mqtt,ntp', `Enter on a focused summary opens the group and stores it (${await groups()}, stored ${await stored('otgw-v2-setopen')})`);

// ---- refused IP save: the group opens for this visit, scrolls into view, is not stored ----
await preset(['mqtt']);
await send('Emulation.setDeviceMetricsOverride', { width: 1000, height: 500, deviceScaleFactor: 1, mobile: false });
await evalJs(`(function(){ var i = document.querySelector('.srow[data-key="wifistaticip"] input'); i.value = '1.2.3'; i.dispatchEvent(new Event('input')); window.scrollTo(0, 0); })()`);
await click('#btnSave');
await sleep(500);
check((await groups()).includes('network+') && (await storedSet()) === 'mqtt', `a refused IP save opens its group, not stored (${await groups()})`);
check(await evalJs(`(function(){ var r = document.querySelector('#setCols details.grp[data-grp="network"]').getBoundingClientRect(); return r.top >= -1 && r.top < innerHeight; })()`), 'the refused IP group is scrolled into view');
await send('Emulation.clearDeviceMetricsOverride');
await setSearch('zzz'); await sleep(100); await setSearch(''); await sleep(100);
check((await groups()).includes('network+'), 'a refusal-opened group stays open for the visit (search + clear keeps it)');
await evalJs(`document.querySelector('.seg[data-page="home"]').click()`);
await evalJs(`document.querySelector('.seg[data-page="settings"]').click()`);
await sleep(500);
check((await groups()) === 'system-,network-,mqtt+,ntp-,sensors-,webhook-,sat-' && (await storedSet()) === 'mqtt', `leaving Settings ends the visit: back to the stored set (${await groups()})`);

// ---- d2: the link with a slow settings GET, long page, small viewport ----
settingsDelay = 800;
await evalJs(`localStorage.removeItem('otgw-ble-seen'); localStorage.setItem('otgw-v2-setopen', ${JSON.stringify(JSON.stringify(ALL_BUT_SENSORS))})`);
await send('Emulation.setDeviceMetricsOverride', { width: 1000, height: 500, deviceScaleFactor: 1, mobile: false });
await load('home');
const view2 = await until(`(function(){ var b = document.querySelector('[data-ble-group] button'); return b ? b.textContent : null; })()`, 25000);
check(!!view2, 'the BLE discovery card appears again for the slow-device run');
await evalJs(`(function(){ var bs = document.querySelectorAll('[data-ble-group] button'); for (var i = 0; i < bs.length; i++) if (/view/i.test(bs[i].textContent)) { bs[i].click(); return; } bs[0].click(); })()`);
const inView = await until(`(function(){ var c = document.getElementById('setcard-ble'); if (!c) return null; var r = c.getBoundingClientRect(); return r.top >= -1 && r.top < innerHeight && window.scrollY > 0 ? 'y' + Math.round(window.scrollY) : null; })()`, 8000);
check(!!inView && (await groups()).includes('sensors+'), `with a slow GET settings (800 ms) the link still ends with Sensors open and the roster card in view (${inView})`);
check((await storedSet()) === ALL_BUT_SENSORS.slice().sort().join(), 'the link left the stored set unchanged');
settingsDelay = 0;
await send('Emulation.clearDeviceMetricsOverride');
await evalJs(`document.querySelector('.seg[data-page="home"]').click()`);
await evalJs(`document.querySelector('.seg[data-page="settings"]').click()`);
await until(`(document.querySelector('#setCols details.grp[data-grp="sensors"]') || {}).open === false`, 6000);
check((await groups()).includes('sensors-') && (await storedSet()) === ALL_BUT_SENSORS.slice().sort().join(), `after Home and back Sensors is closed again, stored set unchanged (${await groups()})`);

// ---- Monitor: a caller-chosen tab does not restore or fetch the stored tab ----
await evalJs(`localStorage.setItem('otgw-v2-montab', 'mvalues')`);
await load('home');
const chip = await until(`!!document.querySelector('.connsum')`, 20000);
check(!!chip, 'the connection-health chip is there');
await sleep(500);
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

try { ws.close(); } catch { /* ignore */ }
chrome.kill(); srv.close();
try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* ignore */ }
console.log('\n' + (ok ? 0 : 'some') + ' failures of ' + n + ' checks');
process.exit(ok ? 0 : 1);
