// TASK-3017 UI bench: the web UI closes a /ws socket that stays silent for 90 s and reconnects (incoming frames muted in the page); based on the TASK-2957/2959 UI bench: SAT display states, control status, messages badge, Home status, weather card, setting hint, warn pill contrast, Check connection links; light + dark.
// Runs in otgw-bench-chromium on the NAS: scripts/nas-bench/uicheck.sh <run> <ip> ui/ui2959.mjs. GET + keyboard only;
// --no-zygote --in-process-gpu: Chromium 136 hangs at start on the NAS kernel (4.4) with the zygote; start takes ~35 s there.
// IP, hostname and MAC are scrubbed from the DOM before each screenshot. Usage: node ui2959.mjs <ip> <outdir>
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
  const J = async (e) => { const v = await ev(e); try { return JSON.parse(v); } catch { return v; } };
  // contrast helper: text colour against the first opaque background up the tree
  const HELP = `window.__c=function(el){if(!el)return null;function p(s){var a=(s.match(/[0-9.]+/g)||[]).map(parseFloat);if(a.length<3)return null;return {r:a[0],g:a[1],b:a[2],a:a.length>3?a[3]:1}}
     function L(c){function f(v){v/=255;return v<=0.03928?v/12.92:Math.pow((v+0.055)/1.055,2.4)}return 0.2126*f(c.r)+0.7152*f(c.g)+0.0722*f(c.b)}
     var fg=p(getComputedStyle(el).color);var e=el,bg=null;while(e&&e.nodeType===1){var b=p(getComputedStyle(e).backgroundColor);if(b&&b.a>0.99){bg=b;break}e=e.parentElement}
     if(!bg)bg={r:255,g:255,b:255,a:1};var l1=L(fg),l2=L(bg);return {fg:'rgb('+fg.r+','+fg.g+','+fg.b+')',bg:'rgb('+bg.r+','+bg.g+','+bg.b+')',ratio:Math.round((Math.max(l1,l2)+0.05)/(Math.min(l1,l2)+0.05)*100)/100,text:(el.textContent||'').trim().slice(0,30)}};'ok'`;
  const MASK = `(function(){[].forEach.call(document.querySelectorAll('input'),function(i){var k=(i.closest('[data-key]')||{getAttribute:function(){return ''}}).getAttribute('data-key')||'';if(/lat|lon/i.test(k+' '+(i.name||'')+' '+(i.id||''))||/^-?\d{1,3}\.\d{3,}$/.test(i.value)){i.value='xx.xxxx'}});return 'ok'})()`;
  const shot = async (name) => { await ev(SCRUB); await ev(MASK); const s = await send('Page.captureScreenshot', { format: 'png' }); writeFileSync(join(OUT, name), Buffer.from(s.data, 'base64')); };
  const nav = async (p) => { await ev(`document.querySelector('.seg[data-page=${p}]').click(),'ok'`); await sleep(3500); };
  const text = (sel, n) => J(`JSON.stringify((function(){var e=document.querySelector('${sel}');return e?e.innerText.slice(0,${n || 1500}):null})())`);
  const clickText = (scope, re) => J(`JSON.stringify((function(){var a=[].filter.call(document.querySelectorAll('${scope} a,${scope} button,${scope} [role=link]'),function(x){return ${re}.test(x.textContent)&&(x.offsetWidth||x.offsetHeight)})[0];if(!a)return {found:false};var t=a.textContent.trim();a.click();return {found:true,text:t}})())`);
  await send('Page.enable'); await send('Emulation.setDeviceMetricsOverride', { width: 1400, height: 900, deviceScaleFactor: 1, mobile: false });
  await send('Page.navigate', { url: `http://${IP}/` }); await sleep(3000);
  F.fw = await ev(`fetch('/api/v2/device/info').then(r=>r.json()).then(j=>JSON.stringify(j.device||j).match(/"fwversion":"[^"]*"/)[0])`);
  F.satStatusApi = await ev(`fetch('/api/v2/sat/status').then(r=>r.text()).then(t=>t.slice(0,2000))`);
  const HOOK = `(function(){var N=window.WebSocket;window.__wsLog=[];window.__mute=false;
    function W(u,p){var w=p?new N(u,p):new N(u);var id=window.__wsLog.length;window.__wsLog.push({ev:'create',t:Date.now(),id:id});
      w.addEventListener('close',function(){window.__wsLog.push({ev:'close',t:Date.now(),id:id})});
      var real=null;Object.defineProperty(w,'onmessage',{get:function(){return real},set:function(f){real=f;N.prototype.__lookupSetter__?0:0;w.__h&&w.removeEventListener('message',w.__h);w.__h=function(e){if(!window.__mute&&real)real.call(w,e)};w.addEventListener('message',w.__h)}});
      var ael=w.addEventListener.bind(w);w.addEventListener=function(t,f,o){if(t==='message'&&f!==w.__h){return ael(t,function(e){if(!window.__mute)f.call(w,e)},o)}return ael(t,f,o)};
      return w}
    W.prototype=N.prototype;W.CONNECTING=0;W.OPEN=1;W.CLOSING=2;W.CLOSED=3;window.WebSocket=W})()`;
  await send('Page.addScriptToEvaluateOnNewDocument', { source: HOOK });
  await ev(`localStorage.setItem('otgw.lang','en');localStorage.setItem('otgw-sim-prompted','1');'ok'`);
  await send('Page.navigate', { url: `http://${IP}/` }); await sleep(20000);
  F.before = await J(`JSON.stringify(window.__wsLog)`);
  const t0 = Date.now(); await ev(`window.__mute=true;window.__muteAt=Date.now();'ok'`);
  for (let k = 0; k < 16; k++) { await sleep(10000); }
  F.duringMute = await J(`JSON.stringify(window.__wsLog.map(function(x){return {ev:x.ev,id:x.id,s:((x.t-window.__muteAt)/1000).toFixed(1)}}))`);
  await ev(`window.__mute=false;'ok'`); await sleep(20000);
  F.after = await J(`JSON.stringify(window.__wsLog.map(function(x){return {ev:x.ev,id:x.id,s:((x.t-window.__muteAt)/1000).toFixed(1)}}))`);
  F.muteSeconds = (Date.now() - t0) / 1000;
  await shot('after-reconnect.png');
} catch (e) { F.error = String(e && e.stack || e); }
writeFileSync(join(OUT, 'facts.json'), JSON.stringify(F, null, 1));
ch.kill('SIGKILL'); try { rmSync(prof, { recursive: true, force: true }); } catch {}
process.exit(0);
