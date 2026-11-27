// TASK-3124 evidence: loads the REAL data/v2.js in headless Chrome against the fake device of tests/webui/lib/page-harness.mjs and
// records every WebSocket construction (time since page load). Scenarios:
//   refuse  the harness server never upgrades /ws, so every connect fails (a refusing endpoint)
//   flap    a stub WebSocket that opens and closes 300 ms later (an endpoint that accepts and drops)
// Usage: node capture.mjs <refuse|flap> <seconds> <lang> <outdir> [shotname]
// OTGW_DATA_DIR=<older data dir> runs the same scenario against the old code (the "before" timeline).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startPage, sleep } from '../../../tests/webui/lib/page-harness.mjs';

const [mode, secs, lang, outdir, shot, width] = process.argv.slice(2);
const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..', '..');
const asset = (p, q, s) => {
  const m = /^\/i18n\/([a-z]+)\.json$/.exec(p);
  if (!m) return false;
  const f = path.join(ROOT, 'i18n', m[1] + '.json');
  if (!fs.existsSync(f)) { s.writeHead(404); s.end('nf'); return true; }
  s.writeHead(200, { 'content-type': 'application/json' }); s.end(fs.readFileSync(f)); return true;
};
const api = (method, p) => {
  if (p.endsWith('/v2/health')) return { health: { otcommandinterface: 'PIC', otgwconnected: true, wifirssi: -60 } };
  if (p.endsWith('/v2/device/info')) return { device: { hostname: 'otgw', otdirectavailable: false, otcommandinterface: 'PIC' } };
  if (p.endsWith('/v2/settings')) return method === 'POST' ? { status: 'ok' } : { hostname: { value: 'otgw', type: 's', maxlen: 30 } };
  return undefined;
};
const before = `try { localStorage.setItem('otgw.lang', '${lang}'); } catch (e) {}
window.__ws = [];
(function () {
  var Native = window.WebSocket, t0 = performance.now();
  var flap = ${mode === 'flap'};
  window.WebSocket = function (u) {
    window.__ws.push(Math.round((performance.now() - t0)) / 1000);
    if (!flap) return new Native(u);
    var s = { readyState: 0, close: function () { if (s.readyState !== 3) { s.readyState = 3; setTimeout(function () { s.onclose && s.onclose(); }, 0); } } };
    setTimeout(function () { s.readyState = 1; s.onopen && s.onopen(); }, 50);
    setTimeout(function () { s.readyState = 3; s.onclose && s.onclose(); }, 350);
    return s;
  };
  window.WebSocket.CONNECTING = 0; window.WebSocket.OPEN = 1; window.WebSocket.CLOSING = 2; window.WebSocket.CLOSED = 3;
})();`;
const h = await startPage({ api, asset, beforeScript: before, width: Number(width) || 1400 });
try {
  await h.until(`!!document.querySelector('.connsum')`, 40000);
  await h.evalJs(`document.querySelector('button.seg[data-page="monitor"]').click()`);
  await sleep(Number(secs) * 1000);
  const log = await h.evalJs(`window.__ws`);
  const gaps = log.slice(1).map((v, i) => +(v - log[i]).toFixed(1));
  console.log(JSON.stringify({ mode, lang, data: process.env.OTGW_DATA_DIR || 'worktree', attempts: log.length, at_s: log, gaps_s: gaps }));
  if (shot) {
    await sleep(12000);   // the health poll refreshes the detail row text
    await h.evalJs(`[].forEach.call(document.querySelectorAll('button'), function (b) { if (/^(Not now|Niet nu)$/.test(b.textContent.trim())) b.click(); })`);
    await h.evalJs(`document.querySelector('button.subtab[data-tab="mconn"]').click()`);
    await sleep(800);
    await h.evalJs(`(document.getElementById('wsResumeBtn') || document.getElementById('connDetail')).scrollIntoView({ block: 'center' })`);
    await sleep(500);
    console.log('btn:', await h.evalJs(`(document.getElementById('wsResumeBtn') || {}).textContent`));
    await h.shot(path.join(outdir, shot));
  }
} finally { await h.close(); }
