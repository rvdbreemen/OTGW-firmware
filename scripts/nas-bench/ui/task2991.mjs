// TASK-2991 UI checks: the settings rail keeps focus after Enter/Space, #setSearch keeps focus while filtering, plus the TASK-2982 regression.
// Runs in otgw-bench-chromium on the NAS: scripts/nas-bench/uicheck.sh <run> <ip> ui/task2991.mjs. GET + keyboard only;
// --no-zygote --in-process-gpu: Chromium 136 hangs at start on the NAS kernel (4.4) with the zygote; start takes ~35 s there.
// IP, hostname and MAC are scrubbed from the DOM before each screenshot. Usage: node task2991.mjs <ip> <outdir>
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
  const ENTER = async () => { await send('Input.dispatchKeyEvent', { type: 'keyDown', key: 'Enter', code: 'Enter', windowsVirtualKeyCode: 13, text: String.fromCharCode(13) }); await send('Input.dispatchKeyEvent', { type: 'keyUp', key: 'Enter', code: 'Enter', windowsVirtualKeyCode: 13 }); await sleep(400); };
  const SPACE = async () => { await send('Input.dispatchKeyEvent', { type: 'keyDown', key: ' ', code: 'Space', windowsVirtualKeyCode: 32, text: ' ' }); await send('Input.dispatchKeyEvent', { type: 'keyUp', key: ' ', code: 'Space', windowsVirtualKeyCode: 32 }); await sleep(400); };
  const TAB = async () => { await key('Tab', 'Tab', 9); await sleep(150); };
  const TYPE = async (s) => { for (const ch of s) { await send('Input.dispatchKeyEvent', { type: 'keyDown', key: ch, text: ch }); await send('Input.dispatchKeyEvent', { type: 'keyUp', key: ch }); await sleep(300); } };
  const shot = async (name, clip) => { await ev(SCRUB); const s = await send('Page.captureScreenshot', clip ? { format: 'png', clip } : { format: 'png' }); writeFileSync(join(OUT, name), Buffer.from(s.data, 'base64')); };
  const groups = `JSON.stringify((function(){function g(sel){return [].map.call(document.querySelectorAll(sel),function(e){return (e.textContent||'').trim().slice(0,16)+'='+e.getAttribute('aria-current')})}
     return {seg:g('.seg[aria-current]'),satview:g('.satview-btn[aria-current]'),monitor:g('#page-monitor .subtab[aria-current]'),advanced:g('#page-advanced .subtab[aria-current]')}})())`;
  const act = `JSON.stringify((function(){var e=document.activeElement;return {tag:e.tagName,id:e.id,cls:(typeof e.className==='string'?e.className:''),text:(e.textContent||e.value||'').trim().slice(0,16),
     isRail:e.classList.contains('rail-item'),cur:e.getAttribute('aria-current'),railCur:[].map.call(document.querySelectorAll('.rail-item[aria-current]'),function(b){return b.textContent.trim().slice(0,14)}),
     visibleRail:[].filter.call(document.querySelectorAll('.rail-item'),function(b){return b.offsetParent!==null}).map(function(b){return b.textContent.trim().slice(0,10)}),
     card:(function(){var h=document.querySelector('#setCols h2, #setCols h3, #setCols .stitle');return h?h.textContent.trim().slice(0,20):''})()}})())`;
  await send('Page.enable'); await send('Emulation.setDeviceMetricsOverride', { width: 1400, height: 900, deviceScaleFactor: 1, mobile: false });
  await send('Page.navigate', { url: `http://${IP}/` }); await sleep(3000);
  await ev(`localStorage.clear();localStorage.setItem('otgw.lang','en');localStorage.setItem('otgw-theme','light');localStorage.setItem('otgw-sim-prompted','1');'ok'`);
  await send('Page.navigate', { url: `http://${IP}/` }); await sleep(10000);
  // check 4 (regression 2982): fresh load, no click
  F.c4_groups_freshLoad = JSON.parse(await ev(groups));
  // check 2: rail focus after Enter and Space
  await ev(`document.querySelector('.seg[data-page=settings]').click(),'ok'`); await sleep(3000);
  const items = await ev(`document.querySelectorAll('.rail-item').length`);
  F.c2 = [];
  await ev(`document.querySelectorAll('.rail-item')[2].focus(),'ok'`); F.c2.push({ step: 'focus item 3', ...JSON.parse(await ev(act)) });
  await ENTER(); F.c2.push({ step: 'enter', ...JSON.parse(await ev(act)) });
  await ev(`document.querySelectorAll('.rail-item')[5].focus(),'ok'`); F.c2.push({ step: 'focus item 6', ...JSON.parse(await ev(act)) });
  await SPACE(); F.c2.push({ step: 'space', ...JSON.parse(await ev(act)) });
  await TAB(); F.c2.push({ step: 'tab after space', ...JSON.parse(await ev(act)) });
  await ENTER(); F.c2.push({ step: 'enter', ...JSON.parse(await ev(act)) });
  F.c2_items = items;
  await shot('2-rail-focus-after-enter.png');
  // check 3: typing in #setSearch keeps focus and filters the rail
  await ev(`(function(){var s=document.getElementById('setSearch');var i=s&&(s.tagName==='INPUT'?s:s.querySelector('input'));i.focus();return i.id||i.tagName})()`);
  F.c3 = [{ step: 'focus search', ...JSON.parse(await ev(act)) }];
  await TYPE('mq'); F.c3.push({ step: 'typed mq', ...JSON.parse(await ev(act)) });
  await shot('3-search-mq.png');
  // check 4b: .seg focus ring (dark)
  await ev(`localStorage.setItem('otgw-theme','dark');'ok'`); await send('Page.navigate', { url: `http://${IP}/` }); await sleep(10000);
  await ev(`document.activeElement&&document.activeElement.blur(),window.scrollTo(0,0),'ok'`);
  for (let i = 0; i < 30; i++) { await TAB(); if (await ev(`!!(document.activeElement&&document.activeElement.classList.contains('seg'))`)) break; }
  await TAB();
  F.c4_ring = JSON.parse(await ev(`JSON.stringify((function(){var e=document.activeElement,cs=getComputedStyle(e),r=e.getBoundingClientRect(),s=e.closest('.switcher')||e.parentElement,sr=s.getBoundingClientRect();
     return {text:e.textContent.trim().slice(0,12),outline:cs.outlineStyle+' '+cs.outlineWidth+' '+cs.outlineColor,offset:cs.outlineOffset,ringInsideContainer:r.top>=sr.top&&r.bottom<=sr.bottom}})())`));
  await shot('4-seg-focus-ring-dark.png', { x: 0, y: 0, width: 700, height: 110, scale: 2 });
  writeFileSync(join(OUT, 'facts.json'), JSON.stringify(F, null, 1)); console.log(JSON.stringify(F, null, 1));
} finally { ch.kill(); await sleep(800); try { rmSync(prof, { recursive: true, force: true }); } catch {} }
