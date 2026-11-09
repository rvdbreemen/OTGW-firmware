// TASK-1546: while the room temperature is not valid, the Monitor > Graph draws the room series as an interruption
// line in a different colour (not a straight line across the gap, not the last value held), and the CSV export writes an
// empty room value for those samples. SAT on: SAT's own verdict (sat/status room_temp_lost); SAT off: the thermostat has
// not reported MsgID 24 for 270 s (the otmonitor epoch of roomtemperature stops changing).
//
// Drives the REAL shipped v2.html/v2.js in headless Chrome against a fake device. The page clock is skewed with an
// injected Date.now offset so a 20 minute dropout takes seconds. OTGW_DATA_DIR=<dir> serves another data dir (old-vs-fix),
// OTGW_SHOT_DIR=<dir> saves a screenshot of the graph per scenario (synthetic data).
//
// Run: node tests/webui/graph-room-dropout.test.mjs
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { stopChromeTree, guardChrome, launchChrome } from './lib/page-harness.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
let PORT = 0;   // TASK-1489: ports are picked by the OS
function cdpPort() { return fs.readFileSync(path.join(profile, 'DevToolsActivePort'), 'utf8').split('\n')[0].trim(); }

const CHROME = [
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/usr/bin/google-chrome',
  '/usr/bin/chromium',
].find((p) => fs.existsSync(p));
if (!CHROME) { console.log('SKIP: no Chrome/Edge binary found; cannot run the browser harness'); process.exit(0); }

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.svg': 'image/svg+xml', '.ico': 'image/x-icon', '.png': 'image/png' };

let satBody = { enabled: false };
// TASK-1578: the device's own history (GET /api/v2/graph/history). null = not served (404, as on an old firmware).
// 25 slots of 120 s ending 60 s ago; the room column is null for the newest 10 slots (a dropout the device stored as no value).
let deviceHistory = null;
function makeHistory(roomNullFrom) {
  const n = 25, col = (f) => Array.from({ length: n }, (_, i) => f(i));
  return { interval_s: 120, newest_age_ms: 60000, count: n, scale: 10,
    flow: col(() => 450), ret: col(() => 300), chSet: col(() => 500), roomSet: col(() => 200),
    room: col((i) => (i >= roomNullFrom ? null : 193)), outside: col(() => 50), mod: col(() => 30),
    flame: col(() => 1), dhw_on: col(() => 0), ch: col(() => 1) };
}
let epoch = 1700000000, rolling = true;
const srv = http.createServer((q, s) => {
  const url = new URL(q.url, `http://127.0.0.1:${PORT}`);
  let p = decodeURIComponent(url.pathname);
  const js = (o) => { s.writeHead(200, { 'content-type': 'application/json' }); s.end(JSON.stringify(o)); };
  if (p.endsWith('/v2/sat/status')) return js(satBody);
  if (p.endsWith('/v2/graph/history')) { if (!deviceHistory) { s.writeHead(404); return s.end('nf'); } return js(deviceHistory); }
  if (p.endsWith('/v2/otgw/otmonitor')) {
    if (rolling) epoch++;
    return js({ otmonitor: { roomtemperature: { value: '19.34', epoch }, boilertemperature: { value: '45.0', epoch: epoch + 1 }, roomsetpoint: { value: '20.0', epoch: 1700000000 } } });
  }
  if (p.indexOf('/api/') === 0) return js({});
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
const chrome = await launchChrome(CHROME, ['--headless=new', '--remote-debugging-port=0', `--user-data-dir=${profile}`,
  '--no-first-run', '--no-default-browser-check', '--disable-gpu', '--window-size=1400,1000', 'about:blank'], { stdio: 'ignore' });
guardChrome(chrome);   // TASK-1739
let target = null;
for (let i = 0; i < 100 && !target; i++) {
  try { const r = await fetch(`http://127.0.0.1:${cdpPort()}/json/new?about:blank`, { method: 'PUT' }); if (r.ok) target = await r.json(); } catch { /* not up yet */ }
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
  if (r.result && r.result.exceptionDetails) throw new Error(JSON.stringify(r.result.exceptionDetails).slice(0, 300));
  return r.result && r.result.result ? r.result.result.value : undefined;
};
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
await send('Page.enable'); await send('Runtime.enable');
await send('Page.addScriptToEvaluateOnNewDocument', { source: `(function(){ var o = Date.now; window.__real = function(){ return o.call(Date); }; window.__r0 = o.call(Date); window.__t0 = window.__r0; window.__rate = 1;
  Date.now = function(){ return window.__t0 + (o.call(Date) - window.__r0) * window.__rate; };
  var oc = URL.createObjectURL; URL.createObjectURL = function(b){ window.__blob = b; return oc.call(URL, b); }; })();` });

let ok = true;
const check = (c, m) => { console.log((c ? 'PASS: ' : 'FAIL: ') + m); if (!c) ok = false; };

const GRAPH = `(function(){
  function xs(d) { var r = []; (d || '').replace(/(-?[\\d.]+),(-?[\\d.]+)/g, function (m, x) { r.push(+x); }); return r; }
  var g = document.getElementById('gSer'); if (!g) return null;
  var room = g.querySelector('[data-k="room"]'), rx = g.querySelector('[data-k="roomX"]');
  var rd = room ? room.getAttribute('d') : '', xd = rx ? rx.getAttribute('d') : '';
  var rxs = xs(rd), xxs = xs(xd);
  return { roomSub: (rd.match(/M/g) || []).length, roomMax: rxs.length ? Math.max.apply(null, rxs) : null, roomN: rxs.length,
    xN: xxs.length, xMin: xxs.length ? Math.min.apply(null, xxs) : null,
    roomColour: room ? room.getAttribute('stroke') : null, xColour: rx ? rx.getAttribute('stroke') : null };
})()`;

// phase 1 = 8 s of polling at the real time, then the page clock runs 60 times faster for `fastSec` real seconds
// (a poll every 2 s is then a sample every 2 minutes, which the graph still draws as one line)
async function scenario(sat, rollingEpoch, fastSec, shot, hist) {
  satBody = sat; deviceHistory = hist || null; rolling = rollingEpoch; epoch += 1000;
  await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/v2.html` });
  await sleep(3800);   // startup: otmonitor seed at 2 s, sat/status at 2.8 s
  await evalJs(`document.querySelector('.seg[data-page="monitor"]').click()`);
  await evalJs(`document.querySelector('.subtab[data-tab="mvalues"]').click()`);
  await sleep(8000);
  if (fastSec) { await evalJs(`window.__t0 = Date.now(); window.__r0 = window.__real(); window.__rate = 60`); await sleep(fastSec * 1000); }
  await evalJs(`document.querySelector('.subtab[data-tab="mgraph"]').click()`);
  await sleep(1200);
  const g = await evalJs(GRAPH);
  if (shot && process.env.OTGW_SHOT_DIR) {
    await evalJs(`document.getElementById('gstrip').scrollIntoView({ block: 'center' })`); await sleep(300);
    const img = await send('Page.captureScreenshot', { format: 'png' });
    fs.writeFileSync(path.join(process.env.OTGW_SHOT_DIR, shot), Buffer.from((img.result || img).data, 'base64'));
  }
  const csv = await evalJs(`(async function(){ window.__blob = null;
    var b = Array.prototype.find.call(document.querySelectorAll('#mgraph button'), function (x) { return /csv/i.test(x.textContent) && !/auto/i.test(x.id || ''); });
    if (!b) return null; b.click(); await new Promise(function (r) { setTimeout(r, 400); }); return window.__blob ? await window.__blob.text() : null; })()`);
  let rows = null;
  if (csv) {
    const ls = csv.split('\n'), ri = ls[0].split(',').indexOf('room');
    rows = ls.slice(1).map((l) => l.split(',')[ri]);
  }
  return { g, rows };
}

// 1. thermostat reports all the time (SAT off): nothing changes, no interruption line
const a = await scenario({ enabled: false }, true, 0, 'dropout-1-valid.png');
check(!!a.g && a.g.roomN > 0 && a.g.xN === 0, `valid room temperature: room line drawn, no interruption line (${JSON.stringify(a.g && { roomN: a.g.roomN, xN: a.g.xN })})`);
check(!!a.rows && a.rows.length > 0 && a.rows.every((v) => v === '19.34'), `valid room temperature: CSV room column all filled (${JSON.stringify(a.rows && a.rows.slice(0, 3))})`);

// 2. SAT off, thermostat unplugged: epoch frozen, after 20 page minutes the room has been silent for more than 270 s
const b = await scenario({ enabled: false }, false, 20, 'dropout-2-sat-off.png');
check(!!b.g && b.g.xN > 0 && !!b.g.xColour && !!b.g.roomColour && b.g.xColour !== b.g.roomColour,
  `SAT off, dropout: an interruption line in another colour than the room line (${JSON.stringify(b.g && { xN: b.g.xN, x: b.g.xColour, room: b.g.roomColour })})`);
check(!!b.g && b.g.roomN > 0 && b.g.xMin !== null && b.g.roomMax < b.g.xMin,
  `SAT off, dropout: the room line stops where the interruption line starts, no line across the gap (room max x ${b.g && b.g.roomMax}, interruption from ${b.g && b.g.xMin})`);
check(!!b.g && b.g.roomSub === 1, `SAT off, dropout: the valid stretch is one unbroken subpath (${b.g && b.g.roomSub})`);
check(!!b.rows && b.rows.some((v) => v === '19.34') && b.rows.some((v) => v === ''),
  `SAT off, dropout: CSV has the value before and an empty room value in the dropout (${JSON.stringify(b.rows && [b.rows[0], b.rows[b.rows.length - 1]])})`);
check(!!b.rows && b.rows[b.rows.length - 1] === '', 'SAT off, dropout: the newest CSV row has an empty room value, not the last reading');

// 3. SAT on: SAT says the room temperature is lost (sat/status room_temp_lost); the thermostat value is not held
const c = await scenario({ enabled: true, active: true, room_temp_lost: true }, true, 0, 'dropout-3-sat-on.png');
// (the first sample is taken at 2 s, before the sat/status poll at 2.8 s has told the page that the room temperature is lost)
check(!!c.g && c.g.xN > 0 && c.g.roomN <= 1, `SAT on, room_temp_lost: interruption line drawn, at most the start-up sample on the room line (${JSON.stringify(c.g && { xN: c.g.xN, roomN: c.g.roomN })})`);
check(!!c.rows && c.rows.length > 1 && c.rows.slice(1).every((v) => v === ''), `SAT on, room_temp_lost: CSV room column empty once SAT reports it (${JSON.stringify(c.rows && c.rows.slice(0, 4))})`);

// 4. SAT on, room valid: no interruption line
const d = await scenario({ enabled: true, active: true, room_temp_lost: false }, true, 0, null);
check(!!d.g && d.g.roomN > 0 && d.g.xN === 0, `SAT on, room valid: room line, no interruption line (${JSON.stringify(d.g && { roomN: d.g.roomN, xN: d.g.xN })})`);

// 5. TASK-1578: after a reload the part of the graph that comes from the device history shows the same interruption.
// The live thermostat reports fine (rolling epoch, SAT off); only the history has a null room stretch.
const e = await scenario({ enabled: false }, true, 0, 'dropout-5-history.png', makeHistory(15));
check(!!e.g && e.g.xN > 0 && !!e.g.xColour && e.g.xColour !== e.g.roomColour,
  `device history with a room dropout: interruption line drawn from history (${JSON.stringify(e.g && { xN: e.g.xN, x: e.g.xColour, room: e.g.roomColour })})`);
check(!!e.g && e.g.roomN > 0 && e.g.xMin !== null, `device history with a room dropout: the room line is drawn too (before the dropout and live) (${JSON.stringify(e.g && { roomN: e.g.roomN, xMin: e.g.xMin })})`);
const f = await scenario({ enabled: false }, true, 0, null, makeHistory(0));
check(!!f.g && f.g.xN === 0, `device history where the room never reported: no interruption line (${JSON.stringify(f.g && { xN: f.g.xN })})`);

check(exceptions.length === 0, `no uncaught page exceptions (${exceptions.join(' ; ')})`);
console.log(ok ? '\n0 failures' : '\nsome failures');
try { ws.close(); } catch { /* ignore */ }
stopChromeTree(chrome); srv.close();
setTimeout(() => process.exit(ok ? 0 : 1), 200);
