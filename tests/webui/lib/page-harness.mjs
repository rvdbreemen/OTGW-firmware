// Shared harness for browser tests of the REAL v2.html/v2.js in headless system Chrome over
// CDP, against a fake device (same approach as v2-settings-sat-followups.test.mjs).
// OTGW_DATA_DIR points the fake device at an older copy of data/ to see a test red.
//
//   const h = await startPage({ api: (method, p, search, body) => jsonOrUndefined, width: 1280 });
//   h.evalJs(expr) / h.until(expr) / h.shot(file) / h.log / h.exceptions / await h.close()
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
export const DATA = process.env.OTGW_DATA_DIR || path.join(HERE, '..', '..', '..', 'src', 'OTGW-firmware', 'data');
const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.svg': 'image/svg+xml' };
export const CHROME = [
  'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/usr/bin/google-chrome',
  '/usr/bin/chromium',
].find((p) => fs.existsSync(p));
export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

export async function startPage({ api, asset, beforeScript, width = 1400, height = 1000, page = '/v2.html' } = {}) {
  const log = [];
  let PORT = 0;
  const srv = http.createServer((q, s) => {
    const u = new URL(q.url, 'http://127.0.0.1');
    const p = decodeURIComponent(u.pathname);
    let body = '';
    q.on('data', (c) => { body += c; });
    q.on('end', () => {
      if (p.indexOf('/api/') === 0) {
        log.push(q.method + ' ' + p + u.search + ' ' + body);
        const r = api ? api(q.method, p, u.search, body) : undefined;
        s.writeHead(200, { 'content-type': 'application/json' });
        s.end(JSON.stringify(r === undefined ? {} : r));
        return;
      }
      if (asset && asset(p, q, s)) return;
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
  const cdpPort = () => fs.readFileSync(path.join(profile, 'DevToolsActivePort'), 'utf8').split('\n')[0].trim();
  const chrome = spawn(CHROME, ['--headless=new', '--remote-debugging-port=0', `--user-data-dir=${profile}`,
    '--no-first-run', '--no-default-browser-check', '--disable-gpu', `--window-size=${width},${height}`, 'about:blank'], { stdio: 'ignore' });
  let target = null;
  for (let i = 0; i < 100 && !target; i++) {
    try { const r = await fetch(`http://127.0.0.1:${cdpPort()}/json/new?about:blank`, { method: 'PUT' }); if (r.ok) target = await r.json(); } catch { /* not up yet */ }
    if (!target) await sleep(200);
  }
  if (!target) throw new Error('Chrome DevTools endpoint never came up');
  const ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
  let msgId = 0; const pending = new Map(); const exceptions = []; const events = [];
  ws.addEventListener('message', (ev) => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
    if (m.method && (m.method.startsWith('Network.') || m.method === 'Runtime.consoleAPICalled')) events.push(m);
    if (m.method === 'Runtime.exceptionThrown') exceptions.push(JSON.stringify(m.params.exceptionDetails).slice(0, 300));
  });
  const send = (method, params = {}) => new Promise((res) => { const id = ++msgId; pending.set(id, res); ws.send(JSON.stringify({ id, method, params })); });
  const evalJs = async (expression) => {
    const r = await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
    return r.result && r.result.result ? r.result.result.value : undefined;
  };
  const until = async (expr, ms = 5000) => {
    for (let t = 0; t < ms; t += 100) { const v = await evalJs(expr).catch(() => null); if (v) return v; await sleep(100); }
    return evalJs(expr).catch(() => null);
  };
  await send('Page.enable'); await send('Runtime.enable');
  await send('Network.enable');
  await send('Network.setCacheDisabled', { cacheDisabled: true });
  if (beforeScript) await send('Page.addScriptToEvaluateOnNewDocument', { source: beforeScript });
  const resize = (w, h = height) => send('Emulation.setDeviceMetricsOverride', { width: w, height: h, deviceScaleFactor: 1, mobile: false });
  await resize(width);
  await send('Page.navigate', { url: `http://127.0.0.1:${PORT}${page}` });
  await until(`document.readyState === 'complete'`);
  const shot = async (file) => {
    fs.mkdirSync(path.dirname(file), { recursive: true });
    const img = await send('Page.captureScreenshot', { format: 'png' });
    fs.writeFileSync(file, Buffer.from(img.result.data, 'base64'));
  };
  const close = async () => {
    try { ws.close(); } catch { /* ignore */ }
    chrome.kill();
    srv.close();
    await sleep(300);
    try { fs.rmSync(profile, { recursive: true, force: true }); } catch { /* chrome may still hold files */ }
  };
  return { PORT, log, exceptions, events, send, evalJs, until, sleep, shot, resize, close };
}
