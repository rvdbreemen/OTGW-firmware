// TASK-2956 contrast checks (WCAG ratios from computed colours) in light and dark theme, with screenshots.
// Runs in otgw-bench-chromium on the NAS: scripts/nas-bench/uicheck.sh <run> <ip> ui/task2956.mjs. GET + keyboard only;
// --no-zygote --in-process-gpu: Chromium 136 hangs at start on the NAS kernel (4.4) with the zygote; start takes ~35 s there.
// IP, hostname and MAC are scrubbed from the DOM before each screenshot. Usage: node task2956.mjs <ip> <outdir>
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
  const shot = async (name) => { await ev(SCRUB); const s = await send('Page.captureScreenshot', { format: 'png' }); writeFileSync(join(OUT, name), Buffer.from(s.data, 'base64')); };
  // In-page helper: computed text colour, first opaque ancestor background, WCAG ratio.
  const HELP = `window.__c=function(el,prop){if(!el)return null;function p(s){var a=(s.match(/[0-9.]+/g)||[]).map(parseFloat);if(a.length<3)return null;return {r:a[0],g:a[1],b:a[2],a:a.length>3?a[3]:1}}
     function L(c){function f(v){v/=255;return v<=0.03928?v/12.92:Math.pow((v+0.055)/1.055,2.4)}return 0.2126*f(c.r)+0.7152*f(c.g)+0.0722*f(c.b)}
     var fg=p(getComputedStyle(el)[prop||'color']);var e=el,bg=null;while(e&&e.nodeType===1){var b=p(getComputedStyle(e).backgroundColor);if(b&&b.a>0.99){bg=b;break}e=e.parentElement}
     if(!bg)bg={r:255,g:255,b:255,a:1};var l1=L(fg),l2=L(bg);var r=(Math.max(l1,l2)+0.05)/(Math.min(l1,l2)+0.05);
     return {fg:'rgb('+fg.r+','+fg.g+','+fg.b+')',bg:'rgb('+bg.r+','+bg.g+','+bg.b+')',ratio:Math.round(r*100)/100,text:(el.textContent||'').trim().slice(0,24)}};'ok'`;
  await send('Page.enable'); await send('Emulation.setDeviceMetricsOverride', { width: 1400, height: 900, deviceScaleFactor: 1, mobile: false });
  await send('Page.navigate', { url: `http://${IP}/` }); await sleep(3000);
  F.c0 = await ev(`fetch('/api/v2/device/info').then(r=>r.json()).then(j=>JSON.stringify(j.device||j).match(/"fwversion":"[^"]*"/)[0])`);
  for (const theme of ['light', 'dark']) {
    const R = (F[theme] = {});
    await ev(`localStorage.clear();localStorage.setItem('otgw.lang','en');localStorage.setItem('otgw-theme','${theme}');localStorage.setItem('otgw-sim-prompted','1');sessionStorage.removeItem('otgw-v2-satbeta-dismissed');'ok'`);
    await send('Page.navigate', { url: `http://${IP}/` }); await sleep(10000);
    R.dataTheme = await ev(`document.documentElement.getAttribute('data-theme')`);
    await ev(HELP);
    // 5: nav + accent buttons on the home page
    R.c5_segActiveText = await ev(`JSON.stringify(__c(document.querySelector('.seg.active')))`).then(JSON.parse);
    R.c5_segActiveBorder = await ev(`JSON.stringify(__c(document.querySelector('.seg.active'),'borderBottomColor'))`).then(JSON.parse);
    R.c5_themeBtnActive = await ev(`(function(){var b=[].filter.call(document.querySelectorAll('button'),function(x){return /^(☀|☾)?\s*(Light|Dark)$/.test(x.textContent.trim().replace(/\s+/g,' '))&&(x.classList.contains('active')||x.getAttribute('aria-pressed')==='true')})[0];return JSON.stringify(__c(b))})()`).then(JSON.parse);
    // SAT page, Control view
    await ev(`document.querySelector('.seg[data-page=sat]').click(),'ok'`); await sleep(2500);
    R.controlBtn = await ev(`(function(){var b=[].filter.call(document.querySelectorAll('.satview-btn'),function(x){return /control/i.test(x.textContent)})[0];if(b){b.click();return b.textContent.trim().slice(0,20)}return 'no control view button'})()`);
    await sleep(2500); await ev(HELP);
    R.c1_heating = await ev(`(function(){var p=document.getElementById('satPill');if(!p)return JSON.stringify({error:'no #satPill'});p.className='sat-pill';p.textContent='Heating';return JSON.stringify(__c(p))})()`).then(JSON.parse);
    await shot(`sat-control-heating-${theme}.png`);
    R.c2_idle = await ev(`(function(){var p=document.getElementById('satPill');p.className='sat-pill idle';p.textContent='Idle';return JSON.stringify(__c(p))})()`).then(JSON.parse);
    R.c2_off = await ev(`(function(){var p=document.getElementById('satPill');p.className='sat-pill off';p.textContent='Off';return JSON.stringify(__c(p))})()`).then(JSON.parse);
    R.c3_tileTag = await ev(`JSON.stringify(__c(document.querySelector('.sat-tile .tag')))`).then(JSON.parse);
    R.c4a_betaBanner = await ev(`JSON.stringify(__c(document.getElementById('satBetaNoticeText')))`).then(JSON.parse);
    await shot(`sat-control-${theme}.png`);
    // Settings: injected reboot badge, Save button
    await ev(`document.querySelector('.seg[data-page=settings]').click(),'ok'`); await sleep(3000); await ev(HELP);
    R.c4b_rebootBadge = await ev(`(function(){var pg=document.querySelector('#page-settings');pg.insertAdjacentHTML('afterbegin','<span class="badge-reboot" id="__rb">reboot</span>');return JSON.stringify(__c(document.getElementById('__rb')))})()`).then(JSON.parse);
    R.c5_saveBtn = await ev(`JSON.stringify(__c(document.querySelector('.savebar .save')))`).then(JSON.parse);
    await shot(`settings-${theme}.png`);
    // Advanced: command send button
    await ev(`document.querySelector('.seg[data-page=advanced]').click(),'ok'`); await sleep(3000); await ev(HELP);
    R.c5_cmdSend = await ev(`JSON.stringify(__c(document.querySelector('.cmdbar-send')))`).then(JSON.parse);
    await shot(`advanced-${theme}.png`);
  }
  const thr = { c1_heating: 4.5, c2_idle: 4.5, c2_off: 4.5, c3_tileTag: 4.5, c4a_betaBanner: 4.5, c4b_rebootBadge: 4.5, c5_segActiveText: 3, c5_segActiveBorder: 3, c5_themeBtnActive: 4.5, c5_saveBtn: 4.5, c5_cmdSend: 4.5 };
  F.verdict = [];
  for (const theme of ['light', 'dark']) for (const [k, t] of Object.entries(thr)) { const v = F[theme][k]; F.verdict.push(`${theme} ${k}: ${v && v.ratio ? (v.ratio >= t ? 'PASS' : 'FAIL') + ' ' + v.ratio + ' (' + v.fg + ' on ' + v.bg + ', need ' + t + ')' : 'NOT FOUND'}`); }
  writeFileSync(join(OUT, 'facts.json'), JSON.stringify(F, null, 1)); console.log(F.c0); console.log(F.verdict.join('\n')); console.log('dataTheme', F.light.dataTheme, F.dark.dataTheme, 'controlBtn', F.light.controlBtn);
} finally { ch.kill(); await sleep(800); try { rmSync(prof, { recursive: true, force: true }); } catch {} }
