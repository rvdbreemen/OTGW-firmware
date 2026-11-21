// TASK-2982 UI checks on the v2 web UI (aria-current at load, .seg focus ring, settings rail by keyboard).
// Runs in otgw-bench-chromium on the NAS: scripts/nas-bench/uicheck.sh <run> <ip> ui/task2982.mjs. GET + keyboard only;
// --no-zygote --in-process-gpu: Chromium 136 hangs at start on the NAS kernel (4.4) with the zygote; start takes ~35 s there.
// IP, hostname and MAC are scrubbed from the DOM before each screenshot. Usage: node task2982.mjs <ip> <outdir>
import { spawn } from 'node:child_process';
import { mkdtempSync, openSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
const [IP, OUT] = process.argv.slice(2);
const CHROME = process.env.CHROME || 'chromium-browser';
const PORT = 9354, sleep = ms => new Promise(r => setTimeout(r, ms));
const prof = mkdtempSync(join(tmpdir(), 'otgw-cdp-ui-'));
const ch = spawn(CHROME, ['--headless=new', '--no-sandbox', '--disable-gpu', '--disable-dev-shm-usage', '--no-zygote', '--in-process-gpu', '--disable-software-rasterizer', '--remote-allow-origins=*', '--remote-debugging-address=127.0.0.1', `--remote-debugging-port=${PORT}`, `--user-data-dir=${prof}`, '--no-first-run', '--window-size=1400,900', 'about:blank'], { stdio: ['ignore', 'ignore', openSync(join(OUT, 'chromium.log'), 'w')] });
const SCRUB = `(function(){var re=/192\\.168\\.88\\.\\d+|zaphod-classic-s3|marvin-otgw32|otgw-[0-9A-Fa-f]{12}|([0-9A-F]{2}:){5}[0-9A-F]{2}/g;
 var w=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT);var n;while(n=w.nextNode()){if(re.test(n.nodeValue)){n.nodeValue=n.nodeValue.replace(re,'xxx');}}return 'ok'})()`;
const F = {};
try {
  let tg; for (let k = 0; k < 360; k++) { try { tg = await (await fetch(`http://127.0.0.1:${PORT}/json`)).json(); break; } catch { await sleep(250); } }
  const ws = new WebSocket(tg.find(t => t.type === 'page').webSocketDebuggerUrl); await new Promise(r => ws.addEventListener('open', r));
  let id = 0; const pend = new Map();
  ws.addEventListener('message', m => { const d = JSON.parse(m.data); if (d.id && pend.has(d.id)) { pend.get(d.id)(d.result || d); pend.delete(d.id); } });
  const send = (method, params = {}) => new Promise(r => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
  const ev = async e => (await send('Runtime.evaluate', { expression: e, returnByValue: true, awaitPromise: true })).result?.value;
  const key = async (k, code, vk) => { await send('Input.dispatchKeyEvent', { type: 'keyDown', key: k, code, windowsVirtualKeyCode: vk }); await send('Input.dispatchKeyEvent', { type: 'keyUp', key: k, code, windowsVirtualKeyCode: vk }); await sleep(250); };
  const ENTER = async () => { await send('Input.dispatchKeyEvent', { type: 'keyDown', key: 'Enter', code: 'Enter', windowsVirtualKeyCode: 13, text: String.fromCharCode(13) }); await send('Input.dispatchKeyEvent', { type: 'keyUp', key: 'Enter', code: 'Enter', windowsVirtualKeyCode: 13 }); await sleep(900); };
  const SPACE = async () => { await key(' ', 'Space', 32); await sleep(900); };
  const TAB = async () => { await key('Tab', 'Tab', 9); await sleep(150); };
  const shot = async (name, clip) => { await ev(SCRUB); const s = await send('Page.captureScreenshot', clip ? { format: 'png', clip } : { format: 'png' }); writeFileSync(join(OUT, name), Buffer.from(s.data, 'base64')); };
  const groups = `JSON.stringify((function(){function g(sel){return [].map.call(document.querySelectorAll(sel),function(e){return (e.textContent||'').trim().slice(0,16)+'='+e.getAttribute('aria-current')})}
     return {seg:g('.seg[aria-current]'),satview:g('.satview-btn[aria-current]'),monitor:g('#page-monitor .subtab[aria-current]'),advanced:g('#page-advanced .subtab[aria-current]'),
       monTotal:document.querySelectorAll('#page-monitor .subtab').length,advTotal:document.querySelectorAll('#page-advanced .subtab').length}})())`;
  await send('Page.enable'); await send('Emulation.setDeviceMetricsOverride', { width: 1400, height: 900, deviceScaleFactor: 1, mobile: false });
  await send('Page.navigate', { url: `http://${IP}/` }); await sleep(3000);
  // check 2a: no saved Monitor tab, fresh load, no click
  await ev(`localStorage.clear();localStorage.setItem('otgw.lang','en');localStorage.setItem('otgw-theme','light');localStorage.setItem('otgw-sim-prompted','1');'ok'`);
  await send('Page.navigate', { url: `http://${IP}/` }); await sleep(10000);
  F.c2_noSaved_noClick = JSON.parse(await ev(groups));
  // check 2b: saved Monitor tab = Values, fresh load, no click; then open Monitor
  await ev(`localStorage.setItem('otgw-v2-montab','mvalues');'ok'`);
  await send('Page.navigate', { url: `http://${IP}/` }); await sleep(10000);
  F.c2_savedValues_noClick = JSON.parse(await ev(groups));
  await ev(`document.querySelector('.seg[data-page=monitor]').click(),'ok'`); await sleep(3000);
  F.c2_savedValues_afterMonitorNav = JSON.parse(await ev(groups));
  F.c2_visiblePanel = await ev(`(document.querySelector('#page-monitor .mpanel.active')||{}).id||null`);
  // check 3: Tab to a .seg in the main nav, dark theme
  await ev(`localStorage.removeItem('otgw-v2-montab');localStorage.setItem('otgw-theme','dark');'ok'`);
  await send('Page.navigate', { url: `http://${IP}/` }); await sleep(10000);
  await ev(`document.activeElement&&document.activeElement.blur(),window.scrollTo(0,0),'ok'`);
  for (let i = 0; i < 30; i++) { await TAB(); if (await ev(`!!(document.activeElement&&document.activeElement.classList.contains('seg'))`)) break; }
  await TAB();   // the next .seg, so the ring is on an inactive tab
  F.c3_ring = JSON.parse(await ev(`JSON.stringify((function(){var e=document.activeElement,cs=getComputedStyle(e),r=e.getBoundingClientRect(),s=e.closest('.switcher')||e.parentElement,sr=s.getBoundingClientRect(),ss=getComputedStyle(s);
     return {text:e.textContent.trim().slice(0,12),cls:e.className,outline:cs.outlineStyle+' '+cs.outlineWidth+' '+cs.outlineColor,offset:cs.outlineOffset,
       btnTopBottom:[r.top,r.bottom].map(Math.round),containerTopBottom:[sr.top,sr.bottom].map(Math.round),containerOverflow:ss.overflowX+'/'+ss.overflowY,
       ringInsideContainer:r.top>=sr.top&&r.bottom<=sr.bottom}})())`));
  await shot('3-seg-focus-ring-dark.png', { x: 0, y: 0, width: 700, height: 110, scale: 2 });
  // check 4: settings rail, keyboard only, short wait per key
  await ev(`localStorage.setItem('otgw-theme','light');'ok'`); await send('Page.navigate', { url: `http://${IP}/` }); await sleep(10000);
  await ev(`document.querySelector('.seg[data-page=settings]').click(),'ok'`); await sleep(3000);
  const rs = `JSON.stringify({focus:(document.activeElement.textContent||'').trim().slice(0,14),focusIsRail:document.activeElement.classList.contains('rail-item'),
     cur:[].map.call(document.querySelectorAll('.rail-item[aria-current]'),function(b){return b.textContent.trim().slice(0,14)}),
     card:(function(){var h=document.querySelector('#setCols h2, #setCols h3, #setCols .stitle');return h?h.textContent.trim().slice(0,20):''})()})`;
  await ev(`document.activeElement&&document.activeElement.blur(),window.scrollTo(0,0),'ok'`);
  F.c4_rail = [];
  let reached = false;
  for (let i = 0; i < 60 && !reached; i++) { await TAB(); reached = await ev(`!!(document.activeElement&&document.activeElement.classList.contains('rail-item'))`); }
  F.c4_rail.push({ step: 'tab reaches rail', ...JSON.parse(await ev(rs)) });
  await TAB(); F.c4_rail.push({ step: 'tab', ...JSON.parse(await ev(rs)) });
  await ENTER(); F.c4_rail.push({ step: 'enter', ...JSON.parse(await ev(rs)) });
  await TAB(); F.c4_rail.push({ step: 'tab', ...JSON.parse(await ev(rs)) });
  await SPACE(); F.c4_rail.push({ step: 'space', ...JSON.parse(await ev(rs)) });
  await TAB(); await TAB(); F.c4_rail.push({ step: 'tab tab', ...JSON.parse(await ev(rs)) });
  await ENTER(); F.c4_rail.push({ step: 'enter', ...JSON.parse(await ev(rs)) });
  await shot('4-settings-rail-keyboard.png');
  writeFileSync(join(OUT, 'facts.json'), JSON.stringify(F, null, 1)); console.log(JSON.stringify(F, null, 1));
} finally { ch.kill(); await sleep(800); try { rmSync(prof, { recursive: true, force: true }); } catch {} }
