// ADR-178 / TASK-1153 guard, rebuilt for the v2 UI (TASK-1234).
//
// The SAT area-sensor reference is either a 16-char Dallas ROM address or a BLE MAC
// (17 chars, colon-separated). The v2 Settings flow (populateAreaSensorChoices in
// v2.js) must offer BOTH sources in the four satsensorarea* pickers, label each by
// source, and save a chosen BLE MAC intact through POST /api/v2/settings.
//
// This drives the REAL shipped v2.html/v2.js in headless system Chrome over CDP: a
// local server fakes the device endpoints, the page's own fetchSettings() and
// populateAreaSensorChoices() run, and we read the rendered <select> and the POST
// body the page's own saveSettings() sends. v2.js is an IIFE, so nothing is
// reimplemented here.
//
// What v2 does NOT cover (firmware side of ADR-178, not reachable from the UI test):
// the sSensorArea width, the restAPI validation, and the Dallas/BLE fan-out.
//
// Zero dependencies: system Chrome/Edge over the DevTools protocol, Node's built-in
// WebSocket. OTGW_DATA_DIR points the server at another data dir (old-vs-fix runs).
//
// Run: node --test tests/webui/sat-area-sensor-mapping.test.mjs
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

const DALLAS = '28FF641E0C1A0B33';
const BLE_MAC = 'A4:C1:38:9F:12:7E';

const CHROME = [
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/usr/bin/google-chrome',
  '/usr/bin/chromium',
].find((p) => fs.existsSync(p));
if (!CHROME) { console.log('SKIP: no Chrome/Edge binary found; cannot run the browser harness'); process.exit(0); }

const MIME = {
  '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css',
  '.json': 'application/json', '.svg': 'image/svg+xml', '.ico': 'image/x-icon', '.png': 'image/png',
};

// The four area settings exactly as GET /api/v2/settings shapes them.
const SETTINGS = {};
for (let i = 0; i < 4; i++) SETTINGS['satsensorarea' + i] = { value: '', type: 's', maxlen: 17 };

const posts = [];   // JSON bodies POSTed to /api/v2/settings

const srv = http.createServer((q, s) => {
  const p = decodeURIComponent(new URL(q.url, `http://127.0.0.1:${PORT}`).pathname);
  const json = (o, code = 200) => { s.writeHead(code, { 'content-type': 'application/json' }); s.end(JSON.stringify(o)); };

  if (p.endsWith('/v2/settings')) {
    if (q.method === 'POST') {
      let body = '';
      q.on('data', (c) => { body += c; });
      q.on('end', () => { posts.push(body); json({ status: 'ok' }); });
      return;
    }
    return json(SETTINGS);
  }
  if (p.endsWith('/v2/sensors/labels')) return json({ [DALLAS]: 'Living room floor' });
  if (p.endsWith('/v2/sensors')) {
    const devices = {}; devices[DALLAS] = { tempC: 20.5 };
    return json({ sensors: { devices } });
  }
  if (p.endsWith('/v2/sat/ble/discovery')) {
    return json({ max_slots: 8, sensors: [{ slot: 0, mac: BLE_MAC.toLowerCase(), label: 'Bedroom', name: 'ATC_9F127E', temp: 19.4, valid: true }] });
  }
  if (p.indexOf('/api/') === 0) return json({});

  const f = path.join(DATA, p === '/' ? '/v2.html' : p);
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

let target = null;
for (let i = 0; i < 100 && !target; i++) {
  try {
    const r = await fetch(`http://127.0.0.1:${cdpPort()}/json/new?about:blank`, { method: 'PUT' });
    if (r.ok) target = await r.json();
  } catch { /* not up yet */ }
  if (!target) await new Promise((r) => setTimeout(r, 200));
}
if (!target) throw new Error('Chrome DevTools endpoint never came up');
const ws = new WebSocket(target.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });

let msgId = 0; const pending = new Map();
ws.addEventListener('message', (ev) => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
});
const send = (method, params = {}) => new Promise((res) => {
  const id = ++msgId; pending.set(id, res); ws.send(JSON.stringify({ id, method, params }));
});
async function evalJs(expression) {
  const r = await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
  return r.result && r.result.result ? r.result.result.value : undefined;
}

await send('Page.enable'); await send('Runtime.enable');
await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/v2.html` });

// Open Settings through the real nav button: showPage('settings') -> fetchSettings()
// -> renderSettings() + populateAreaSensorChoices(). The area rows render into the
// SAT page's settings cards (renderSatSettings), which exist in the DOM either way.
let opened = false;
for (let i = 0; i < 200 && !opened; i++) {
  await new Promise((r) => setTimeout(r, 100));
  opened = await evalJs(`(function(){
    if (document.readyState !== 'complete') return false;
    var b = document.querySelector('.seg[data-page="settings"]');
    if (!b) return false;
    b.click();
    return true;
  })()`).catch(() => false);
}

const readOptions = (i) => evalJs(`(function(){
  var sel = document.querySelector('[data-key="satsensorarea${i}"] select');
  if (!sel) return [];
  return Array.prototype.map.call(sel.options, function(o){ return o.value + '|' + o.textContent; });
})()`).catch(() => []);

let options = [];
for (let i = 0; i < 100; i++) {
  await new Promise((r) => setTimeout(r, 100));
  options = await readOptions(0);
  if (options && options.length > 2) break;
}
await new Promise((r) => setTimeout(r, 300));
options = await readOptions(0);

let ok = true; const results = [];
function check(pass, msg) {
  results.push(pass); console.log((pass ? 'PASS' : 'FAIL') + ': ' + msg);
  if (!pass) ok = false;
}

check(opened, 'the Settings page opened through its nav button');
check(options.length > 1, `satsensorarea0 renders as a picker (options: ${JSON.stringify(options)})`);

const dallasOpt = options.find((o) => o.indexOf(DALLAS + '|') === 0);
const bleOpt = options.find((o) => o.indexOf(BLE_MAC + '|') === 0);
check(!!dallasOpt, `the Dallas address is offered as a value (options: ${JSON.stringify(options)})`);
check(!!bleOpt, `the BLE MAC is offered as a value, upper-cased (options: ${JSON.stringify(options)})`);
check(!!dallasOpt && /\[DS18B20\]/.test(dallasOpt), `the Dallas entry names its source (got "${dallasOpt}")`);
check(!!dallasOpt && /Living room floor/.test(dallasOpt), `the Dallas entry shows its 1-Wire label (got "${dallasOpt}")`);
check(!!bleOpt && /\[BLE\]/.test(bleOpt), `the BLE entry names its source (got "${bleOpt}")`);
check(!!bleOpt && /Bedroom/.test(bleOpt), `the BLE entry shows its user label (got "${bleOpt}")`);

const opts3 = await readOptions(3);
check(JSON.stringify(opts3) === JSON.stringify(options), 'all four area pickers get the same choices (area 3 checked)');

// Choose the BLE MAC for area 0 and save through the page's own Save button.
const chosen = await evalJs(`(function(){
  var sel = document.querySelector('[data-key="satsensorarea0"] select');
  if (!sel) return false;
  sel.value = ${JSON.stringify(BLE_MAC)};
  sel.dispatchEvent(new Event('change'));
  var b = document.getElementById('satBtnSave') || document.getElementById('btnSave');
  if (!b) return false;
  b.click();
  return sel.value;
})()`).catch(() => false);
check(chosen === BLE_MAC, `the BLE MAC can be selected in the picker (got "${chosen}")`);
for (let i = 0; i < 60 && posts.length === 0; i++) await new Promise((r) => setTimeout(r, 100));

let body = null;
try { body = JSON.parse(posts[0] || 'null'); } catch { /* leave null */ }
check(!!body, `saving issued a POST /api/v2/settings (bodies: ${JSON.stringify(posts)})`);
check(!!body && body.name === 'satsensorarea0', `the POST targets satsensorarea0 (got "${body && body.name}")`);
check(!!body && body.value === BLE_MAC, `the POST carries the full 17-char BLE MAC (got "${body && body.value}")`);

try { ws.close(); } catch { /* ignore */ }
chrome.kill(); srv.close();
try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* ignore */ }

console.log('\n' + results.filter((p) => !p).length + ' failures of ' + results.length + ' checks');
process.exit(ok ? 0 : 1);
