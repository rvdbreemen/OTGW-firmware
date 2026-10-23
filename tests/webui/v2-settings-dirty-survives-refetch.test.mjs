// TASK-1318: a jump into Settings (BLE "Assign", the sensor-roster link, any showPage('settings'))
// refetches /api/v2/settings. The refetch must not throw away unsaved edits.
//
// Drives the REAL v2.html/v2.js in headless Chrome over CDP against a fake device (same
// harness as v2-settings-sat-followups.test.mjs). Set OTGW_DATA_DIR to a directory holding an
// older v2.html/v2.js to run it against that copy (red on HEAD, green on the fix).
//
// Run: node tests/webui/v2-settings-dirty-survives-refetch.test.mjs
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
const S = (value, maxlen = 15) => ({ value, type: 's', maxlen });
const SETTINGS = {
  satblenameprefix: S('', 23), satblenamefilteringest: { value: false, type: 'b' },
  satbleenable: { value: true, type: 'b' },
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
    if (p.endsWith('/v2/sat/ble/discovery')) return json({ max_slots: 8, populated_slots: 0, sensors: [], psram: 1, ble_enable: true, risk_ack: true });
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
const settingsGets = () => log.filter((l) => l.startsWith('GET /api/v2/settings')).length;

let ok = true; let n = 0;
function check(pass, msg) { n++; console.log((pass ? 'PASS' : 'FAIL') + ': ' + msg); if (!pass) ok = false; }

await send('Page.enable'); await send('Runtime.enable');
await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/v2.html` });
await until(`document.readyState === 'complete' && !!document.querySelector('.seg[data-page="settings"]')`);
await evalJs(`document.querySelector('.seg[data-page="settings"]').click()`);
for (let i = 0; i < 50 && settingsGets() < 2; i++) await sleep(100);
await sleep(300);
const rail = (cat) => until(`(function(){ var r = document.querySelector('.rail-item[data-cat="${cat}"]'); if (!r) return false; r.click(); return true; })()`);
const state = () => evalJs(`(function(){
  var p = document.querySelector('.srow[data-key="satblenameprefix"] input'), t = document.querySelector('.srow[data-key="satblenamefilteringest"] input');
  var bar = document.getElementById('saveBar');
  return p && t ? { prefix: p.value, prefixDirty: p.closest('.srow').classList.contains('dirty'), ingest: t.checked,
    ingestDirty: t.closest('.srow').classList.contains('dirty'), count: document.getElementById('dirtyCount').textContent,
    bar: bar.classList.contains('show') } : null; })()`);

// ---- edit two fields on Settings > Sensors ----
await rail('sensors');
await until(`!!document.querySelector('.srow[data-key="satblenameprefix"] input')`);
await evalJs(`(function(){ var p = document.querySelector('.srow[data-key="satblenameprefix"] input'); p.value = 'ATC_'; p.dispatchEvent(new Event('input'));
  var t = document.querySelector('.srow[data-key="satblenamefilteringest"] input'); t.checked = true; t.dispatchEvent(new Event('change')); })()`);
const before = await state();
check(before && before.prefix === 'ATC_' && before.prefixDirty && before.count === '2' && before.bar, `two edits are dirty before the jump (${JSON.stringify(before)})`);

// ---- the same path as BLE Assign / the sensor-roster link: showPage('settings') refetches ----
await evalJs(`document.querySelector('.seg[data-page="home"]').click()`);
const g0 = settingsGets();
await evalJs(`document.querySelector('.seg[data-page="settings"]').click()`);
for (let i = 0; i < 50 && settingsGets() <= g0; i++) await sleep(100);
await sleep(400);
await rail('sensors');
await until(`!!document.querySelector('.srow[data-key="satblenameprefix"] input')`);
const after = await state();
check(settingsGets() > g0, 'the jump into Settings refetched /api/v2/settings');
check(after && after.prefix === 'ATC_' && after.prefixDirty, `edited text field keeps its value and dirty mark (${JSON.stringify(after)})`);
check(after && after.ingest === true && after.ingestDirty, 'edited toggle keeps its value and dirty mark');
check(after && after.count === '2' && after.bar, 'the save bar still shows 2 unsaved changes');

// ---- a real save still clears the dirty state, also across another refetch ----
const nPost = log.length;
await evalJs(`document.getElementById('btnSave').click()`);
await until(`(document.getElementById('toast') || {}).textContent === 'Settings saved'`, 5000);
await sleep(300);
const posts = log.slice(nPost).filter((l) => l.startsWith('POST /api/v2/settings'));
check(posts.length === 2, `save POSTs both keys, one at a time (${posts.length})`);
const saved = await state();
check(saved && !saved.prefixDirty && !saved.ingestDirty && saved.count === '0' && !saved.bar, `after save nothing is dirty (${JSON.stringify(saved)})`);
await evalJs(`document.querySelector('.seg[data-page="home"]').click()`);
const g1 = settingsGets();
await evalJs(`document.querySelector('.seg[data-page="settings"]').click()`);
for (let i = 0; i < 50 && settingsGets() <= g1; i++) await sleep(100);
await sleep(400);
await rail('sensors');
await until(`!!document.querySelector('.srow[data-key="satblenameprefix"] input')`);
const again = await state();
check(again && !again.prefixDirty && !again.ingestDirty && again.count === '0' && !again.bar, `a later refetch does not resurrect saved edits (${JSON.stringify(again)})`);

// ---- discard clears too ----
await evalJs(`(function(){ var p = document.querySelector('.srow[data-key="satblenameprefix"] input'); p.value = 'X'; p.dispatchEvent(new Event('input')); })()`);
await evalJs(`document.getElementById('btnDiscard').click()`);
const disc = await state();
check(disc && !disc.prefixDirty && disc.count === '0' && !disc.bar, `Discard still clears the dirty state (${JSON.stringify(disc)})`);

check(exceptions.length === 0, `no uncaught page exceptions (${exceptions.join(' ; ')})`);
try { ws.close(); } catch { /* ignore */ }
chrome.kill(); srv.close();
try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* ignore */ }
console.log('\n' + (ok ? 0 : 'some') + ' failures of ' + n + ' checks');
process.exit(ok ? 0 : 1);
