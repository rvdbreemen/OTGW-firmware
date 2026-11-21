// TASK-2937 remote telemetry UI: header badge #hdrTel state/colours and the Settings > General switch, screenshots.
// Runs in otgw-bench-chromium on the NAS: scripts/nas-bench/uicheck.sh <run> <ip> ui/telbadge.mjs. GET + keyboard only;
// --no-zygote --in-process-gpu: Chromium 136 hangs at start on the NAS kernel (4.4) with the zygote; start takes ~35 s there.
// IP, hostname and MAC are scrubbed from the DOM before each screenshot. Usage: node telbadge.mjs <ip> <outdir>
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
  const stamp = new Date().toTimeString().slice(0, 8).replace(/:/g, '');
  const shot = async (name, clip) => { await ev(SCRUB); const s = await send('Page.captureScreenshot', clip ? { format: 'png', clip } : { format: 'png' }); writeFileSync(join(OUT, `${stamp}-${name}`), Buffer.from(s.data, 'base64')); };
  await send('Page.enable'); await send('Emulation.setDeviceMetricsOverride', { width: 1400, height: 900, deviceScaleFactor: 1, mobile: false });
  await send('Page.navigate', { url: `http://${IP}/` }); await sleep(3000);
  await ev(`localStorage.setItem('otgw.lang','en');localStorage.setItem('otgw-theme','light');localStorage.setItem('otgw-sim-prompted','1');'ok'`);
  await send('Page.navigate', { url: `http://${IP}/` }); await sleep(25000);   // > one 15 s health poll
  F.health = await ev(`fetch('/api/v2/health').then(r=>r.json()).then(j=>JSON.stringify(j.remote_telemetry||(j.health&&j.health.remote_telemetry)||null))`);
  F.badge = await ev(`JSON.stringify((function(){var e=document.getElementById('hdrTel');if(!e)return null;var cs=getComputedStyle(e);return {cls:e.className,text:e.textContent.trim(),hidden:e.classList.contains('hidden')||e.offsetParent===null,color:cs.color,bg:cs.backgroundColor}})())`);
  await shot('header.png', { x: 0, y: 0, width: 1400, height: 110, scale: 1 });
  await ev(`document.querySelector('.seg[data-page=settings]').click(),'ok'`); await sleep(3000);
  await ev(`(function(){var s=document.getElementById('setSearch');s.value='telemetry';s.dispatchEvent(new Event('input',{bubbles:true}));return 'ok'})()`); await sleep(1500);
  F.setting = await ev(`JSON.stringify((function(){var i=document.querySelector('#setCols .srow[data-key=remotetelemetry] input');var row=i&&(i.closest('.srow,.set-row,label,div'));return i?{type:i.type,checked:i.checked,disabled:i.disabled,row:(row?row.textContent.trim().replace(/\s+/g,' ').slice(0,80):'')}:null})())`);
  await ev(`(function(){var i=document.querySelector('#setCols .srow[data-key=remotetelemetry] input');if(i)i.scrollIntoView({block:'center'});return 'ok'})()`); await sleep(800);
  await shot('settings-general.png');
  writeFileSync(join(OUT, `${stamp}-facts.json`), JSON.stringify(F, null, 1)); console.log(JSON.stringify(F, null, 1));
} finally { ch.kill(); await sleep(800); try { rmSync(prof, { recursive: true, force: true }); } catch {} }
