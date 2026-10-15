// TASK-1230 / TASK-1231 / TASK-1232 browser check of the v2 follow-ups.
//
// Drives the REAL shipped v2.html/v2.js in headless system Chrome over CDP against a
// fake device (same approach as sat-area-sensor-mapping.test.mjs), and checks:
//   1230a  typing a Wi-Fi static IP prefills mask/gateway/DNS from device/info; a bad
//          octet blocks the whole save (no POST); a fixed one saves
//   1230a  ethstaticip on a Wi-Fi-connected unit does NOT copy the Wi-Fi IP into Ethernet
//   1230b  Board mode change asks confirm(); cancel reverts, OK marks it dirty
//   1230c  the Webhook card has an OFF test that POSTs webhook/test?state=off
//   1231   BLE name prefix + ingest toggle render in Sensors > BLE, save, then rescan
//   1232a  a real mouse click on the heating curve POSTs a marker; the list delete DELETEs it
//   1232b  the DHW slider takes 35-60 from MsgID 48, and 40-65 when MsgID 48 is 0
// It also fails on any uncaught page exception and records the peak number of /api/
// requests in flight at the fake device.
//
// Run: node --test tests/webui/v2-settings-sat-followups.test.mjs
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data');
const PORT = 8147;
const CDP_PORT = 9339;

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
  wifistaticip: S(''), wifisubnet: S(''), wifigateway: S(''), wifidns1: S(''), wifidns2: S(''),
  ethstaticip: { value: false, type: 'b' }, ethipaddress: S(''), ethsubnet: S(''), ethgateway: S(''), ethdns: S(''),
  webhookenable: { value: true, type: 'b' }, webhookurlon: S('http://x/on', 100), webhookurloff: S('http://x/off', 100),
  satbleenable: { value: true, type: 'b' },
  satblenameprefix: S('', 23), satblenamefilteringest: { value: false, type: 'b' },
};
// device/info (restAPI.ino:3227-3240): ipaddress = active transport; WiFi active here.
const DEVINFO = { device: {
  ipaddress: '192.168.88.61', networkmode: 'WiFi',
  wifi_current_subnet: '255.255.255.0', wifi_current_gateway: '192.168.88.1',
  wifi_current_dns1: '192.168.88.1', wifi_current_dns2: '0.0.0.0',
  eth_current_subnet: '255.255.0.0', eth_current_gateway: '10.1.0.1', eth_current_dns: '10.1.0.1',
} };
const SAT_STATUS = { enabled: true, coefficient: 1.5, target_temp: 20, heating_system: 1, outside_temp: 8, final_setpoint: 45, dhw_setpoint: 50, control_mode: 1 };
let msg48 = (60 << 8) | 35;          // UB 60, LB 35
let markers = [];

const log = [];                       // "METHOD path body"
let inflight = 0, peak = 0;
const srv = http.createServer((q, s) => {
  const u = new URL(q.url, `http://127.0.0.1:${PORT}`);
  const p = decodeURIComponent(u.pathname);
  const api = p.indexOf('/api/') === 0;
  if (api) { inflight++; peak = Math.max(peak, inflight); }
  let body = '';
  q.on('data', (c) => { body += c; });
  q.on('end', () => {
    const json = (o, code = 200) => setTimeout(() => {   // small latency so overlaps would show
      inflight--; s.writeHead(code, { 'content-type': 'application/json' }); s.end(JSON.stringify(o));
    }, 15);
    if (api) log.push(q.method + ' ' + p + u.search + ' ' + body);
    if (p.endsWith('/v2/settings')) return q.method === 'POST' ? json({ status: 'ok' }) : json(SETTINGS);
    if (p.endsWith('/v2/device/info')) return json(DEVINFO);
    if (p.endsWith('/v2/webhook/test')) return json({ status: 'ok' });
    if (p.endsWith('/v2/sat/ble/discovery')) return json({ max_slots: 8, populated_slots: 0, sensors: [], psram: 1, ble_enable: true, risk_ack: true });
    if (p.endsWith('/v2/sat/status')) return json(SAT_STATUS);
    if (p.endsWith('/v2/otgw/messages/48')) return json({ label: 'TdhwSetUBTdhwSetLB', value: msg48, unit: '°C' });
    if (p.endsWith('/v2/sat/markers') && q.method === 'POST') {
      let b = {}; try { b = JSON.parse(body); } catch { /* keep */ }
      markers.push({ id: markers.length + 1, outside_temp: b.outside_temp, flow_temp: b.flow_temp, added: 0, label: b.label });
      return json({ status: 'ok', id: markers.length }, 201);
    }
    if (p.endsWith('/v2/sat/markers')) return json(markers);
    const del = p.match(/\/v2\/sat\/markers\/(\d+)$/);
    if (del && q.method === 'DELETE') { markers = markers.filter((m) => m.id !== +del[1]); return json({ status: 'ok' }); }
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
await send('Page.navigate', { url: `http://127.0.0.1:${PORT}/v2.html` });
await until(`document.readyState === 'complete' && !!document.querySelector('.seg[data-page="settings"]')`);
await evalJs(`document.querySelector('.seg[data-page="settings"]').click()`);
// v2 fetches settings at boot AND on opening Settings; a late answer resets unsaved
// edits (setDirty = {}), so wait for both before editing.
for (let i = 0; i < 50 && log.filter((l) => l.startsWith('GET /api/v2/settings')).length < 2; i++) await sleep(100);
await sleep(300);
const rail = (cat) => until(`(function(){ var r = document.querySelector('.rail-item[data-cat="${cat}"]'); if (!r) return false; r.click(); return true; })()`);

// ---- 1230b board-mode confirm ----
await rail('system');
await until(`!!document.querySelector('.srow[data-key="boardmode"] select')`);
const bm = await evalJs(`(function(){
  var sel = document.querySelector('.srow[data-key="boardmode"] select'), asked = [];
  window.confirm = function (m) { asked.push(m); return false; };
  sel.value = '1'; sel.dispatchEvent(new Event('change'));
  var afterCancel = sel.value, dirtyCancel = sel.closest('.srow').classList.contains('dirty');
  window.confirm = function (m) { asked.push(m); return true; };
  sel.value = '2'; sel.dispatchEvent(new Event('change'));
  return { asked: asked.length, msg: asked[0] || '', afterCancel: afterCancel, dirtyCancel: dirtyCancel,
           afterOk: sel.value, dirtyOk: sel.closest('.srow').classList.contains('dirty') };
})()`);
check(bm && bm.asked === 2 && /non-functional/.test(bm.msg), `board mode change asks confirm() with the warning (${JSON.stringify(bm)})`);
check(bm && bm.afterCancel === '0' && !bm.dirtyCancel, 'cancel restores the previous board mode and leaves it clean');
check(bm && bm.afterOk === '2' && bm.dirtyOk, 'OK keeps the new board mode and marks it dirty');
// Leave boardmode dirty: the save below must post nothing while an IP is invalid.

// ---- 1230a Wi-Fi prefill + validation ----
await rail('network');
await until(`!!document.querySelector('.srow[data-key="wifistaticip"] input')`);
// Wi-Fi still on DHCP: an edited field with a bad octet must not save either.
const nDhcp = log.length;
await evalJs(`(function(){ var i = document.querySelector('.srow[data-key="wifigateway"] input'); i.value = '10.0.0.999'; i.dispatchEvent(new Event('input'));
  document.getElementById('btnSave').click(); })()`);
await sleep(500);
const toastDhcp = await evalJs(`(document.getElementById('toast') || {}).textContent`);
check(!log.slice(nDhcp).some((l) => l.startsWith('POST')) && /Wi-Fi Gateway: octet 4/.test(toastDhcp || ''),
  `on DHCP, a dirty bad gateway blocks the save too ("${toastDhcp}")`);
await evalJs(`(function(){ var i = document.querySelector('.srow[data-key="wifigateway"] input'); i.value = ''; i.dispatchEvent(new Event('input')); })()`);
await evalJs(`(function(){ var i = document.querySelector('.srow[data-key="wifistaticip"] input'); i.value = '192.168.88.77'; i.dispatchEvent(new Event('input')); })()`);
const pre = await until(`(function(){ var g = function (k) { var i = document.querySelector('.srow[data-key="' + k + '"] input'); return i ? i.value : null; };
  return g('wifigateway') ? { sub: g('wifisubnet'), gw: g('wifigateway'), d1: g('wifidns1'), d2: g('wifidns2') } : null; })()`);
check(pre && pre.sub === '255.255.255.0' && pre.gw === '192.168.88.1' && pre.d1 === '192.168.88.1',
  `typing a Wi-Fi static IP prefills mask/gateway/DNS1 from the lease (${JSON.stringify(pre)})`);
check(pre && pre.d2 === '', 'the 0.0.0.0 DNS2 from the lease is not copied');
const badBorder = await evalJs(`(function(){ var i = document.querySelector('.srow[data-key="wifigateway"] input');
  i.value = '192.168.88.300'; i.dispatchEvent(new Event('input')); return i.style.borderColor + '|' + i.title; })()`);
check(/octet 4/.test(badBorder || ''), `a bad octet is flagged while typing (${badBorder})`);
const postsBefore = log.filter((l) => l.startsWith('POST /api/v2/settings')).length;
await evalJs(`document.getElementById('btnSave') && document.getElementById('btnSave').click()`);
await sleep(600);
const toast = await evalJs(`(document.getElementById('toast') || {}).textContent`);
check(log.filter((l) => l.startsWith('POST /api/v2/settings')).length === postsBefore, 'a bad gateway octet blocks the whole save (no POST at all)');
check(/Wi-Fi (gateway|Gateway): octet 4/.test(toast || ''), `the toast names the bad field and octet ("${toast}")`);
await evalJs(`(function(){ var i = document.querySelector('.srow[data-key="wifigateway"] input'); i.value = '192.168.88.1'; i.dispatchEvent(new Event('input'));
  document.getElementById('btnSave').click(); })()`);
await until(`(document.getElementById('toast') || {}).textContent === 'Settings saved'`, 5000);
const saved = log.filter((l) => l.startsWith('POST /api/v2/settings')).map((l) => { try { return JSON.parse(l.slice(l.indexOf('{'))); } catch { return {}; } });
const sv = Object.fromEntries(saved.map((b) => [b.name, b.value]));
check(sv.wifistaticip === '192.168.88.77' && sv.wifisubnet === '255.255.255.0' && sv.wifigateway === '192.168.88.1' && sv.boardmode === '2',
  `a valid static group saves (${JSON.stringify(sv)})`);

// ---- 1230a Ethernet on a Wi-Fi-connected unit ----
await rail('network');
await until(`!!document.querySelector('.srow[data-key="ethstaticip"] input')`);
await evalJs(`(function(){ var c = document.querySelector('.srow[data-key="ethstaticip"] input'); c.checked = true; c.dispatchEvent(new Event('change')); })()`);
const eth = await until(`(function(){ var g = function (k) { return document.querySelector('.srow[data-key="' + k + '"] input').value; };
  return g('ethsubnet') ? { ip: g('ethipaddress'), sub: g('ethsubnet'), gw: g('ethgateway') } : null; })()`);
check(eth && eth.sub === '255.255.0.0' && eth.gw === '10.1.0.1', `ethstaticip prefills mask/gateway from eth_current_* (${JSON.stringify(eth)})`);
check(eth && eth.ip === '', 'the Wi-Fi active IP is not copied into the Ethernet IP');
await evalJs(`document.getElementById('btnDiscard') && document.getElementById('btnDiscard').click()`);

// ---- 1230c webhook OFF test ----
await rail('webhook');
const whBtns = await until(`(function(){ var b = document.querySelectorAll('.wh-actions button'); return b.length ? Array.prototype.map.call(b, function (x) { return x.textContent; }) : null; })()`);
check(Array.isArray(whBtns) && whBtns.join('|') === 'Send ON test|Send OFF test', `the webhook card has ON and OFF tests (${JSON.stringify(whBtns)})`);
await evalJs(`document.querySelectorAll('.wh-actions button')[1].click()`);
const whStatus = await until(`(function(){ var s = document.querySelector('.wh-status'); return s && /OFF ok/.test(s.textContent) ? s.textContent : null; })()`);
check(log.some((l) => l.startsWith('POST /api/v2/webhook/test?state=off')), 'the OFF test POSTs webhook/test?state=off');
check(!!whStatus, `the OFF test reports the ack (${whStatus})`);

// ---- 1231 BLE name filter ----
await rail('sensors');
const ble = await until(`(function(){ var p = document.querySelector('.srow[data-key="satblenameprefix"] input'), t = document.querySelector('.srow[data-key="satblenamefilteringest"] input');
  if (!p || !t) return null; var card = p.closest('.set-group'); return { max: p.maxLength, cb: t.type, card: card && card.querySelector('h3') ? card.querySelector('h3').textContent : '' }; })()`);
check(ble && ble.max === 23 && ble.cb === 'checkbox' && ble.card === 'BLE sensors', `prefix field (maxlen 23) and ingest toggle render in the BLE sensors card (${JSON.stringify(ble)})`);
const nBefore = log.length;
await evalJs(`(function(){ var p = document.querySelector('.srow[data-key="satblenameprefix"] input'); p.value = 'ATC_'; p.dispatchEvent(new Event('input'));
  var t = document.querySelector('.srow[data-key="satblenamefilteringest"] input'); t.checked = true; t.dispatchEvent(new Event('change'));
  document.getElementById('btnSave').click(); })()`);
await until(`(document.getElementById('toast') || {}).textContent === 'Settings saved'`, 5000);
await sleep(400);
const after = log.slice(nBefore).filter((l) => l.startsWith('POST'));
check(after.some((l) => /"name":"satblenameprefix","value":"ATC_"/.test(l)) && after.some((l) => /"name":"satblenamefilteringest","value":"true"/.test(l)),
  `both BLE filter keys are POSTed (${JSON.stringify(after)})`);
const iRescan = after.findIndex((l) => l.startsWith('POST /api/v2/sat/ble/rescan'));
check(iRescan > 0 && after.slice(0, iRescan).every((l) => l.startsWith('POST /api/v2/settings')), 'a rescan follows after the settings save');

// ---- 1232 SAT: DHW bounds + markers ----
await evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
await evalJs(`(function(){ var b = document.querySelector('.satview-btn[data-sv="technical"]'); if (b) b.click(); })()`);
const dhw = await until(`(function(){ var s = document.getElementById('satDhwSlider'); return s && s.max === '60' ? s.min + '-' + s.max : null; })()`);
check(dhw === '35-60', `DHW slider takes MsgID 48 bounds 35-60 (got ${dhw})`);
const pt = await until(`(function(){ var svg = document.getElementById('satCurveBig'); if (!svg || !svg.querySelector('polyline')) return null;
  svg.scrollIntoView({ block: 'center' }); var p = svg.createSVGPoint(); p.x = 315; p.y = 270 - (50 - 20) / 65 * 240;
  p = p.matrixTransform(svg.getScreenCTM()); return { x: p.x, y: p.y }; })()`);
check(!!pt, 'the heating curve rendered');
if (pt) {
  await send('Input.dispatchMouseEvent', { type: 'mousePressed', x: pt.x, y: pt.y, button: 'left', clickCount: 1 });
  await send('Input.dispatchMouseEvent', { type: 'mouseReleased', x: pt.x, y: pt.y, button: 'left', clickCount: 1 });
}
const li = await until(`(function(){ var b = document.querySelector('#satMarkerList li button'); return b ? document.querySelector('#satMarkerList li').textContent : null; })()`);
const mp = log.find((l) => l.startsWith('POST /api/v2/sat/markers'));
let mb = null; try { mb = JSON.parse(mp.slice(mp.indexOf('{'))); } catch { /* keep null */ }
check(!!mb && Math.abs(mb.outside_temp - 5) <= 0.1 && Math.abs(mb.flow_temp - 50) <= 0.5 && /^Marker /.test(mb.label),
  `a click at (5° out, 50° flow) POSTs that marker (${mp})`);
check(!!li, `the new marker is listed with a delete button (${li})`);
await evalJs(`document.querySelector('#satMarkerList li button').click()`);
const gone = await until(`/No calibration markers/.test(document.getElementById('satMarkerList').textContent)`);
check(log.some((l) => l.startsWith('DELETE /api/v2/sat/markers/1')) && !!gone, 'delete sends DELETE sat/markers/1 and the list empties');
// MsgID 48 never reported -> default bounds on the next SAT visit.
msg48 = 0;
await evalJs(`document.querySelector('.seg[data-page="settings"]').click()`);
await sleep(300);
await evalJs(`document.querySelector('.seg[data-page="sat"]').click()`);
const dhw2 = await until(`(function(){ var s = document.getElementById('satDhwSlider'); return s && s.max === '65' ? s.min + '-' + s.max : null; })()`);
check(dhw2 === '40-65', `with MsgID 48 = 0 the slider falls back to 40-65 (got ${dhw2})`);

check(exceptions.length === 0, `no uncaught page exceptions (${exceptions.join(' ; ')})`);
console.log('peak /api/ requests in flight at the fake device: ' + peak);
check(peak <= 2, 'never more than 2 API requests in flight');

try { ws.close(); } catch { /* ignore */ }
chrome.kill(); srv.close();
try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* ignore */ }
console.log('\n' + (ok ? 0 : 'some') + ' failures of ' + n + ' checks');
process.exit(ok ? 0 : 1);
