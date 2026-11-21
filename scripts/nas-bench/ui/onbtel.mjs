// TASK-2937 onboarding wizard: walk to the remote telemetry step (never Finish), screenshot toggle off/on, light+dark, 1400+390 px.
// Runs in otgw-bench-chromium on the NAS: scripts/nas-bench/uicheck.sh <run> <ip> ui/onbtel.mjs. GET + keyboard only;
// --no-zygote --in-process-gpu: Chromium 136 hangs at start on the NAS kernel (4.4) with the zygote; start takes ~35 s there.
// IP, hostname and MAC are scrubbed from the DOM before each screenshot. Usage: node onbtel.mjs <ip> <outdir>
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
  const shot = async (name) => { await ev(SCRUB); const s = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: false }); writeFileSync(join(OUT, name), Buffer.from(s.data, 'base64')); };
  await send('Page.enable');
  F.runs = [];
  for (const theme of ['light', 'dark']) for (const [w, h] of [[1400, 900], [390, 844]]) {
    const L = `${theme}-${w}`; const R = { label: L, steps: [] };
    await send('Emulation.setDeviceMetricsOverride', { width: w, height: h, deviceScaleFactor: 1, mobile: w < 500 });
    await send('Page.navigate', { url: `http://${IP}/` }); await sleep(2500);
    await ev(`localStorage.setItem('otgw.lang','en');localStorage.setItem('otgw-theme','${theme}');localStorage.setItem('otgw-sim-prompted','1');'ok'`);
    await send('Page.navigate', { url: `http://${IP}/` }); await sleep(10000);
    await ev(`document.querySelector('.seg[data-page=settings]').click(),'ok'`); await sleep(3000);
    R.open = await ev(`(function(){var b=document.querySelector('[data-act=onboarding]');if(!b){var s=document.getElementById('setSearch');s.value='setup';s.dispatchEvent(new Event('input',{bubbles:true}));b=document.querySelector('[data-act=onboarding]')}if(!b)return 'no onboarding button';b.click();return 'clicked'})()`);
    await sleep(1500);
    for (let i = 0; i < 8; i++) {
      const st = await ev(`(function(){var b=document.getElementById('onbBack');if(!b)return 'none';var tx=b.innerText;if(/telemetry/i.test(tx)&&b.querySelector('[data-act=finish-tel]'))return 'telemetry';var c=[].filter.call(b.querySelectorAll('[data-act]'),function(x){var a=x.getAttribute('data-act');return !/^back|test|finish-tel|go$/.test(a)&&x.offsetParent!==null});var p=c[c.length-1];if(!p)return 'stuck';var a=p.getAttribute('data-act');p.click();return 'clicked '+a})()`);
      R.steps.push(st); await sleep(1200);
      if (st === 'telemetry' || st === 'none' || st === 'stuck') break;
    }
    if (R.steps[R.steps.length - 1] === 'telemetry') {
      R.toggle = await ev(`(function(){var i=document.querySelector('#onbBack input[type=checkbox]');return i?JSON.stringify({checked:i.checked}):'no checkbox'})()`);
      await shot(`onb-telemetry-off-${L}.png`);
      await ev(`(function(){var i=document.querySelector('#onbBack input[type=checkbox]');if(i){i.click()}return 'ok'})()`); await sleep(600);
      R.toggleAfter = await ev(`(function(){var i=document.querySelector('#onbBack input[type=checkbox]');return i?JSON.stringify({checked:i.checked}):'no checkbox'})()`);
      await shot(`onb-telemetry-on-${L}.png`);
    }
    await ev(`(function(){var b=document.getElementById('onbBack');if(b)b.remove();return 'closed without finish'})()`);
    F.runs.push(R);
  }
  F.after = await ev(`fetch('/api/v2/settings').then(r=>r.json()).then(j=>JSON.stringify({ui_onboarded:(j.settings||j).ui_onboarded,remotetelemetry:(j.settings||j).remotetelemetry}))`);
  writeFileSync(join(OUT, 'facts.json'), JSON.stringify(F, null, 1)); console.log(JSON.stringify(F, null, 1));
} finally { ch.kill(); await sleep(800); try { rmSync(prof, { recursive: true, force: true }); } catch {} }
