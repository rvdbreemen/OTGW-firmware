// TASK-2998 contrast: wizard primary buttons + Done check (on-accent on accent) and the #cnOvrTxt chip, light + dark.
// Runs in otgw-bench-chromium on the NAS: scripts/nas-bench/uicheck.sh <run> <ip> ui/task2998.mjs. GET + keyboard only;
// --no-zygote --in-process-gpu: Chromium 136 hangs at start on the NAS kernel (4.4) with the zygote; start takes ~35 s there.
// IP, hostname and MAC are scrubbed from the DOM before each screenshot. Usage: node task2998.mjs <ip> <outdir>
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
  const HELP = `window.__c=function(el,prop){if(!el)return null;function p(s){var a=(s.match(/[0-9.]+/g)||[]).map(parseFloat);if(a.length<3)return null;return {r:a[0],g:a[1],b:a[2],a:a.length>3?a[3]:1}}
     function L(c){function f(v){v/=255;return v<=0.03928?v/12.92:Math.pow((v+0.055)/1.055,2.4)}return 0.2126*f(c.r)+0.7152*f(c.g)+0.0722*f(c.b)}
     var fg=p(getComputedStyle(el)[prop||'color']);var e=el,bg=null;while(e&&e.nodeType===1){var b=p(getComputedStyle(e).backgroundColor);if(b&&b.a>0.99){bg=b;break}e=e.parentElement}
     if(!bg)bg={r:255,g:255,b:255,a:1};var l1=L(fg),l2=L(bg);var r=(Math.max(l1,l2)+0.05)/(Math.min(l1,l2)+0.05);
     return {fg:'rgb('+fg.r+','+fg.g+','+fg.b+')',bg:'rgb('+bg.r+','+bg.g+','+bg.b+')',ratio:Math.round(r*100)/100,text:(el.textContent||'').trim().slice(0,24)}};'ok'`;
  const shot = async (name) => { await ev(SCRUB); const s = await send('Page.captureScreenshot', { format: 'png' }); writeFileSync(join(OUT, name), Buffer.from(s.data, 'base64')); };
  const J = async (e) => JSON.parse(await ev(e));
  await send('Page.enable'); await send('Emulation.setDeviceMetricsOverride', { width: 1400, height: 900, deviceScaleFactor: 1, mobile: false });
  await send('Page.navigate', { url: `http://${IP}/` }); await sleep(3000);
  F.c1 = await ev(`fetch('/api/v2/device/info').then(r=>r.json()).then(j=>JSON.stringify(j.device||j).match(/"fwversion":"[^"]*"/)[0])`);
  for (const theme of ['light', 'dark']) {
    const R = (F[theme] = {});
    await ev(`localStorage.setItem('otgw.lang','en');localStorage.setItem('otgw-theme','${theme}');localStorage.setItem('otgw-sim-prompted','1');'ok'`);
    await send('Page.navigate', { url: `http://${IP}/` }); await sleep(10000); await ev(HELP);
    // onboarding wizard (never finished)
    await ev(`document.querySelector('.seg[data-page=settings]').click(),'ok'`); await sleep(3000);
    R.onbOpen = await ev(`(function(){var b=document.querySelector('[data-act=onboarding]');if(!b){var s=document.getElementById('setSearch');s.value='setup';s.dispatchEvent(new Event('input',{bubbles:true}));b=document.querySelector('[data-act=onboarding]')}if(!b)return 'no button';b.click();return 'clicked'})()`);
    await sleep(1500); await ev(HELP);
    R.onbPrimary = await J(`JSON.stringify(__c([].filter.call(document.querySelectorAll('#onbBack [data-act]'),function(x){return /on-accent/.test(x.getAttribute('style')||'')})[0]))`);
    R.onbDoneProbe = await J(`(function(){var b=document.getElementById('onbBack');b.insertAdjacentHTML('beforeend','<div id="__done" style="width:54px;height:54px;border-radius:50%;background:var(--accent);color:var(--on-accent);font-weight:700;font-size:24px">✓</div>');return JSON.stringify(__c(document.getElementById('__done')))})()`);
    await shot(`onboarding-${theme}.png`);
    await ev(`(function(){var b=document.getElementById('onbBack');if(b)b.remove();return 'ok'})()`);
    // SAT setup wizard (never finished)
    await ev(`document.querySelector('.seg[data-page=sat]').click(),'ok'`); await sleep(3000);
    R.satOpen = await ev(`(function(){var b=document.getElementById('satRerunSetup');if(!b)return 'no #satRerunSetup';b.click();return 'clicked'})()`);
    await sleep(1500); await ev(HELP);
    R.satBegin = await J(`JSON.stringify(__c(document.querySelector('[data-sa=begin]')))`);
    R.satDoneProbe = await J(`(function(){var w=(document.querySelector('[data-sa=begin]')||document.body).closest('div[style*=fixed]')||document.body;w.insertAdjacentHTML('beforeend','<div id="__sdone" style="width:54px;height:54px;border-radius:50%;background:var(--accent);color:var(--on-accent);font-weight:700;font-size:24px">✓</div>');return JSON.stringify(__c(document.getElementById('__sdone')))})()`);
    await shot(`sat-wizard-${theme}.png`);
    await ev(`(function(){var b=document.querySelector('[data-sa=begin]');var w=b&&b.closest('div[style*=fixed]');if(w)w.remove();return 'ok'})()`);
    // Monitor connection map chip
    await send('Page.navigate', { url: `http://${IP}/` }); await sleep(9000); await ev(HELP);
    await ev(`document.querySelector('.seg[data-page=monitor]').click(),'ok'`); await sleep(2500);
    await ev(`(function(){var t=[].filter.call(document.querySelectorAll('#page-monitor .subtab'),function(x){return /connection/i.test(x.textContent)})[0];if(t)t.click();return 'ok'})()`); await sleep(2500);
    R.cnOvr = await J(`(function(){var g=document.getElementById('cnOvr');if(!g)return JSON.stringify({error:'no #cnOvr'});g.style.display='';var tx=document.getElementById('cnOvrTxt');if(tx&&!tx.textContent)tx.textContent='⛓ 1 override';return JSON.stringify(__c(tx,'fill'))})()`);
    await shot(`monitor-connection-${theme}.png`);
  }
  F.verdict = [];
  for (const theme of ['light', 'dark']) for (const k of ['onbPrimary', 'onbDoneProbe', 'satBegin', 'satDoneProbe', 'cnOvr']) { const v = F[theme][k]; F.verdict.push(`${theme} ${k}: ${v && v.ratio ? (v.ratio >= 4.5 ? 'PASS' : 'FAIL') + ' ' + v.ratio + ' (' + v.fg + ' on ' + v.bg + ')' : 'NOT FOUND ' + JSON.stringify(v)}`); }
  writeFileSync(join(OUT, 'facts.json'), JSON.stringify(F, null, 1)); console.log(F.c1); console.log(F.verdict.join('\n'));
} finally { ch.kill(); await sleep(800); try { rmSync(prof, { recursive: true, force: true }); } catch {} }
