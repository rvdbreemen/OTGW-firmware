// TASK-1702: after the boot auto-detect switched an OT-Direct unit to Master (RAM only), the
// settings page still showed the stored Gateway and choosing Gateway POSTed nothing, so the
// user could not switch back without a reboot. The OT-Direct mode row now says what runs and
// offers a button that marks the stored mode for saving; Save POSTs otdmode, which the
// firmware applies live.
//
// Drives the REAL v2.html/v2.js in headless Chrome over CDP against a fake device (harness of
// v2-settings-dirty-survives-refetch.test.mjs). OTGW_DATA_DIR points at an older copy for the
// old-vs-fix run. OTD_RUNNING=gateway runs the no-mismatch control.
//
// Run: node tests/webui/otd-mode-running-note.test.mjs
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

const CHROME = [
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/usr/bin/google-chrome',
  '/usr/bin/chromium',
].find((p) => fs.existsSync(p));
if (!CHROME) { console.log('SKIP: no Chrome/Edge binary found; cannot run the browser harness'); process.exit(0); }

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.svg': 'image/svg+xml' };
const S = (value, maxlen = 15) => ({ value, type: 's', maxlen });
// Stored Gateway (1, the default) with auto-detect on; the bus runs Master because the boot
// auto-detect saw no thermostat (RAM only, TASK-1694). device/info reports the running mode.
const RUNNING = process.env.OTD_RUNNING || 'master';
const SETTINGS = {
  otdmode: { value: 1, type: 'i', min: 0, max: 4 },
  otdautodetect: { value: true, type: 'b' },
  mqttbroker: S('broker.local', 30),
};
const log = [];
const srv = http.createServer((q, s) => {
  const u = new URL(q.url, `http://127.0.0.1:${PORT}`);
  const p = decodeURIComponent(u.pathname);
  const api = p.indexOf('/api/') === 0;
  let body = '';
  q.on('data', (c) => { body += c; });
  q.on('end', () => {
    const json = (o, code = 200) => setTimeout(() => { s.writeHead(code, { 'content-type': 'application/json' }); s.end(JSON.stringify(o)); }, 15);
    if (api) log.push(q.method + ' ' + p + u.search + ' ' + body);
    if (p.endsWith('/v2/settings')) return q.method === 'POST' ? json({ status: 'ok' }) : json(SETTINGS);
    if (p.endsWith('/v2/device/info')) return json({ device: { otdmode: RUNNING, hardwaremode: 'OT-Direct', otdirectavailable: true } });
    if (p.endsWith('/v2/sat/ble/discovery')) return json({ max_slots: 8, populated_slots: 0, sensors: [], psram: 1, ble_enable: true, risk_ack: true });
    if (api) return json({});
    const f = path.join(DATA, p === '/' ? '/v2.html' : p);
    if (fs.existsSync(f) && fs.statSync(f).isFile()) {
      s.writeHead(200, { 'content-type': MIME[path.extname(f)] || 'application/octet-stream' });
      fs.createReadStream(f).pipe(s);
    } else { s.writeHead(404); s.end('nf'); }
  });
});
await new Promise((r) => srv.listen(0, '127.0.0.1', r));
PORT = srv.address().port;

const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'otgw-cdp-'));
const chrome = spawn(CHROME, ['--headless=new', '--remote-debugging-port=0', `--user-data-dir=${profile}`,
  '--no-first-run', '--no-default-browser-check', '--disable-gpu', '--window-size=1400,1000', 'about:blank'], { stdio: 'ignore' });
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
await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/v2.html` });
await until(`document.readyState === 'complete' && !!document.querySelector('.seg[data-page="settings"]')`);
for (let i = 0; i < 50 && !log.some((l) => l.startsWith('GET /api/v2/device/info')); i++) await sleep(100);
await sleep(400);
await evalJs(`document.querySelector('.seg[data-page="settings"]').click()`);
await until(`(function(){ var r = document.querySelector('.rail-item[data-cat="otd"]'); if (!r) return false; r.click(); return true; })()`);
await until(`!!document.querySelector('.srow[data-key="otdmode"] select')`);
const row = () => evalJs(`(function(){ var r = document.querySelector('.srow[data-key="otdmode"]'); if (!r) return null;
  var b = r.querySelector('.shint button'); return { text: r.querySelector('.slbl').textContent, btn: b ? b.textContent : '',
  sel: r.querySelector('select').value, dirty: r.classList.contains('dirty'), count: document.getElementById('dirtyCount').textContent }; })()`);
const r0 = await row();
if (RUNNING === 'master') {
  check(r0 && /Running now: Master/.test(r0.text), `the row says the bus runs Master while Gateway is stored (${JSON.stringify(r0)})`);
  check(r0 && r0.btn === 'Switch to gateway', 'the row offers to switch to the stored mode');
  await evalJs(`(function(){ var b = document.querySelector('.srow[data-key="otdmode"] .shint button'); if (b) b.click(); })()`);
  const r1 = await row();
  check(r1 && r1.dirty && r1.count === '1', `the button marks otdmode for saving (${JSON.stringify(r1)})`);
  const nPost = log.length;
  await evalJs(`document.getElementById('btnSave').click()`);
  for (let i = 0; i < 50 && !log.slice(nPost).some((l) => l.startsWith('POST /api/v2/settings')); i++) await sleep(100);
  const posts = log.slice(nPost).filter((l) => l.startsWith('POST /api/v2/settings'));
  check(posts.length === 1 && /otdmode/.test(posts[0]) && /"?1"?/.test(posts[0]), `Save POSTs otdmode=1 (${posts.join(' | ')})`);
} else {
  check(r0 && !/Running now/.test(r0.text) && r0.btn === '', `no note when the running mode equals the stored one (${JSON.stringify(r0)})`);
  await evalJs(`(function(){ var s = document.querySelector('.srow[data-key="otdmode"] select'); s.value = '1'; s.dispatchEvent(new Event('change')); })()`);
  const r1 = await row();
  check(r1 && !r1.dirty && r1.count === '0', `re-choosing the stored mode stays clean (${JSON.stringify(r1)})`);
}

check(exceptions.length === 0, `no uncaught page exceptions (${exceptions.join(' ; ')})`);
try { ws.close(); } catch { /* ignore */ }
chrome.kill(); srv.close();
// TASK-1492: Chrome holds the profile until it has exited; wait for that, then retry the removal.
await new Promise((r) => { if (chrome.exitCode !== null || chrome.signalCode !== null) return r(); chrome.once('exit', r); setTimeout(r, 5000); });
try { fs.rmSync(profile, { recursive: true, force: true, maxRetries: 10, retryDelay: 200 }); } catch (e) { console.log('profile cleanup failed: ' + e.message); }
console.log('\n' + (ok ? 0 : 'some') + ' failures of ' + n + ' checks');
process.exit(ok ? 0 : 1);
