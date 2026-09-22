// TASK-1153 — the SAT area-sensor mapping must offer BLE sensors, not DS18B20 only.
//
// Reported on Discord 2026-06-21 by sergeantd, seconded by number3nl: hardwiring a
// DS18B20 into another room is impractical, adding a BLE sensor is trivial, so the
// area dropdown should list every discovered sensor and the panel should drop
// "DS18B20" from its name.
//
// Drives the shipped classic UI in headless system Chrome over CDP (no dependency),
// serving both sensor sources plus the current mappings, and asserts:
//   - the panel is no longer DS18B20-branded,
//   - the dropdown offers the Dallas address AND the BLE MAC, each marked by source,
//   - saving a BLE MAC PATCHes that MAC to /api/v2/sat/sensor-areas.
//
// Run: node tests/webui/sat-area-sensor-mapping.test.mjs
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const PORT = 8145;
const CDP_PORT = 9337;

const DALLAS = '28FF641E0C1A0B33';
const BLE_MAC = 'A4:C1:38:9F:12:7E';

const CHROME = [
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/usr/bin/google-chrome',
].find((p) => fs.existsSync(p));
if (!CHROME) { console.log('SKIP: no Chrome/Edge found'); process.exit(0); }

const MIME = {
  '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css',
  '.json': 'application/json', '.svg': 'image/svg+xml', '.ico': 'image/x-icon', '.png': 'image/png',
};

const patches = [];   // PATCH bodies seen on /api/v2/sat/sensor-areas
let areasHits = 0;

const srv = http.createServer((q, s) => {
  const p = decodeURIComponent(new URL(q.url, `http://127.0.0.1:${PORT}`).pathname);
  const json = (o, code = 200) => { s.writeHead(code, { 'content-type': 'application/json' }); s.end(JSON.stringify(o)); };

  if (p.endsWith('/v2/sat/sensor-areas')) {
    if (q.method === 'PATCH' || q.method === 'POST' || q.method === 'PUT') {
      let body = '';
      q.on('data', (c) => { body += c; });
      q.on('end', () => { patches.push(body); json({ status: 'ok' }); });
      return;
    }
    areasHits++;
    return json({ areas: [{ index: 0, sensor: '' }, { index: 1, sensor: '' }, { index: 2, sensor: '' }, { index: 3, sensor: '' }] });
  }
  if (p.endsWith('/v2/sensors')) {
    const devices = {}; devices[DALLAS] = { tempC: 20.5 };
    return json({ sensors: { devices } });
  }
  if (p.endsWith('/v2/sensors/labels')) return json({ [DALLAS]: 'Living room floor' });
  if (p.endsWith('/v2/sat/ble/discovery')) {
    return json({ max_slots: 8, sensors: [{ slot: 0, mac: BLE_MAC, label: 'Bedroom', name: 'ATC_9F127E', temp: 19.4, valid: true }] });
  }
  if (p.indexOf('/api/') === 0) return json({});

  const f = path.join(DATA, p === '/' ? '/index.html' : p);
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

let target = null;
for (let i = 0; i < 100 && !target; i++) {
  try {
    const r = await fetch(`http://127.0.0.1:${CDP_PORT}/json/new?about:blank`, { method: 'PUT' });
    if (r.ok) target = await r.json();
  } catch { /* not up */ }
  if (!target) await new Promise((r) => setTimeout(r, 200));
}
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
await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/index.html` });

// Build the SAT settings panel and populate the dropdowns using the page's own
// functions, then wait for the mapping fetch chain to land.
let built = false;
for (let i = 0; i < 200 && !built; i++) {
  await new Promise((r) => setTimeout(r, 100));
  built = await evalJs(`(function(){
    if (document.readyState !== 'complete') return false;
    if (typeof buildDallasSensorAreasPanel !== 'function' || typeof refreshDallasSensorAreas !== 'function') return false;
    if (!document.getElementById('dallas-areas-grid')) {
      var host = document.getElementById('displaySATSettingsPage') || document.body;
      buildDallasSensorAreasPanel(host);
    }
    refreshDallasSensorAreas();
    return true;
  })()`).catch(() => false);
}

let options = [];
for (let i = 0; i < 100; i++) {
  await new Promise((r) => setTimeout(r, 100));
  options = await evalJs(`(function(){
    var sel = document.getElementById('dallas-area-sel-0');
    if (!sel) return [];
    return Array.prototype.map.call(sel.options, function(o){ return o.value + '|' + o.textContent; });
  })()`).catch(() => []);
  if (options && options.length > 1) break;
}

let ok = true;
function check(pass, msg) { console.log((pass ? 'PASS' : 'FAIL') + ': ' + msg); if (!pass) ok = false; }

check(built, 'the area-mapping panel builds and refreshes');
check(areasHits > 0, 'the page fetched the current area mappings');

const title = await evalJs(`(function(){
  var h = document.getElementById('sat-grp-dallas-areas-header');
  return h ? h.textContent : '';
})()`).catch(() => '');
check(!/DS18B20/i.test(title || ''), `panel title is no longer DS18B20-branded (got "${(title || '').trim()}")`);
check(/Area Sensor Mapping/i.test(title || ''), 'panel is titled "Area Sensor Mapping"');

const dallasOpt = options.find((o) => o.indexOf(DALLAS) === 0);
const bleOpt = options.find((o) => o.indexOf(BLE_MAC) === 0);
check(!!dallasOpt, `the Dallas sensor is offered (options: ${JSON.stringify(options)})`);
check(!!bleOpt, `the BLE sensor is offered (options: ${JSON.stringify(options)})`);
check(!!dallasOpt && /\[DS18B20\]/.test(dallasOpt), `the Dallas entry names its source (got "${dallasOpt}")`);
check(!!bleOpt && /\[BLE\]/.test(bleOpt), `the BLE entry names its source (got "${bleOpt}")`);
check(!!bleOpt && /Bedroom/.test(bleOpt), `the BLE entry shows its user label (got "${bleOpt}")`);

// Selecting the BLE MAC and saving must PATCH that MAC, not a truncated value.
await evalJs(`(function(){
  var sel = document.getElementById('dallas-area-sel-0');
  sel.value = ${JSON.stringify(BLE_MAC)};
  saveDallasAreaSensor(0, sel.value);
  return true;
})()`);
for (let i = 0; i < 60 && patches.length === 0; i++) await new Promise((r) => setTimeout(r, 100));

let body = null;
try { body = JSON.parse(patches[0] || 'null'); } catch { /* leave null */ }
check(!!body, `saving issued a PATCH (bodies: ${JSON.stringify(patches)})`);
check(body && body.sensor === BLE_MAC, `the PATCH carries the full BLE MAC (got "${body && body.sensor}")`);
check(body && body.area === 0, 'the PATCH targets area 0');

try { ws.close(); } catch { /* ignore */ }
chrome.kill(); srv.close();
try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* ignore */ }

console.log('\n' + (ok ? '0' : 'some') + ' failures');
process.exit(ok ? 0 : 1);
