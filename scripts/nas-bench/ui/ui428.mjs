// beta.428 candidate screenshots (otgw-webui list bench_screenshots_beta.txt). GET + keyboard only, synthetic scrub of IP/host/MAC, lat/lon masked.
// Runs in otgw-bench-chromium on the NAS: bin/uicheck.sh <run> <ip> ui/ui428.mjs; usage inside: node ui428.mjs <ip> <outdir> <mode> <shotnames>
// mode a: 1400 px, shots home/monitor graph/SAT control loop in light+dark, names from <shotnames> (e.g. S3,S4,S5, '-' skips). mode b: the 390 px scroll-fade set (S10-S13) plus S14 at 1400.
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
  const MASK = `(function(){[].forEach.call(document.querySelectorAll('input'),function(i){if(/zaphod|keepout|otgw-[0-9a-f]{6}|marvin/i.test(i.value)){i.value='xxx'}var k=(i.closest('[data-key]')||{getAttribute:function(){return ''}}).getAttribute('data-key')||'';if(/lat|lon/i.test(k+' '+(i.name||'')+' '+(i.id||''))||/^-?\d{1,3}\.\d{3,}$/.test(i.value)){i.value='xx.xxxx'}});return 'ok'})()`;
  const shot = async (name) => { await ev(SCRUB); await ev(MASK); const s = await send('Page.captureScreenshot', { format: 'png' }); writeFileSync(join(OUT, name), Buffer.from(s.data, 'base64')); };
  const nav = async (p) => { await ev(`document.querySelector('.seg[data-page=${p}]').click(),'ok'`); await sleep(3500); };
  const text = (sel, n) => J(`JSON.stringify((function(){var e=document.querySelector('${sel}');return e?e.innerText.slice(0,${n || 1500}):null})())`);
  const clickText = (scope, re) => J(`JSON.stringify((function(){var a=[].filter.call(document.querySelectorAll('${scope} a,${scope} button,${scope} [role=link]'),function(x){return ${re}.test(x.textContent)&&(x.offsetWidth||x.offsetHeight)})[0];if(!a)return {found:false};var t=a.textContent.trim();a.click();return {found:true,text:t}})())`);
  const errs = []; ws.addEventListener('message', m => { const d = JSON.parse(m.data); if (d.method === 'Runtime.exceptionThrown') errs.push('EXC ' + JSON.stringify(d.params.exceptionDetails).slice(0, 300)); if (d.method === 'Runtime.consoleAPICalled' && d.params.type === 'error') errs.push('error ' + JSON.stringify(d.params.args).slice(0, 300)); });
  const [MODE, NAMES] = process.argv.slice(4); const NM = (NAMES || '').split(',');
  const setTheme = async t => { await ev(`localStorage.setItem('otgw-theme','${t}'),'ok'`); await send('Page.navigate', { url: `http://${IP}/` }); await sleep(7000); };
  const clickVis = (re) => J(`JSON.stringify((function(){var a=[].filter.call(document.querySelectorAll('button,a,[role=tab],.seg,.tab'),function(x){return ${re}.test((x.textContent||'').trim())&&(x.offsetWidth||x.offsetHeight)})[0];if(!a)return {found:false};a.click();return {found:true,text:a.textContent.trim().slice(0,40)}})())`);
  const resize = (w, h) => send('Emulation.setDeviceMetricsOverride', { width: w, height: h || 900, deviceScaleFactor: 1, mobile: false });
  await send('Page.enable'); await send('Runtime.enable'); await resize(1400);
  await send('Page.navigate', { url: `http://${IP}/` }); await sleep(4000);
  F.fw = await ev(`fetch('/api/v2/device/info').then(r=>r.json()).then(j=>JSON.stringify(j.device||j).match(/"fwversion":"[^"]*"/)[0])`);
  F.mode = MODE; F.shots = {};
  for (const th of ['light', 'dark']) {
    await setTheme(th);
    if (MODE === 'a') {
      if (NM[0] !== '-') { await nav('home'); F.shots[`${NM[0]}-${th}`] = { homeText: await text('#page-home,[data-page-id=home],main', 900) }; await shot(`${NM[0]}-${th}-1400.png`); }
      if (NM[1] !== '-') { await nav('monitor'); F.clickGraph = await clickVis('/^\s*graph\s*$/i'); await sleep(3000); F.shots[`${NM[1]}-${th}`] = { monText: await text('#page-monitor,main', 1200) }; await shot(`${NM[1]}-${th}-1400.png`); }
      if (NM[2] !== '-') { await nav('sat'); await sleep(2500); F.dismiss = [await clickVis('/^\s*Not now\s*$/i'), await clickVis('/^\s*[×x]\s*$/i'), await clickVis('/Dismiss for this session/i')]; await sleep(1200); await shot(`${NM[2]}-${th}-1400-top.png`); F.satScroll = await ev(`(function(){var e=[].filter.call(document.querySelectorAll('h2,h3,.card-title,.title,legend,div'),function(x){return x.children.length<3&&/SAT control loop/i.test(x.textContent)})[0];if(e){e.scrollIntoView({block:'start'});return 'scrolled'}return 'not found'})()`); await sleep(1500); F.shots[`${NM[2]}-${th}`] = { satText: await text('#page-sat,main', 1500) }; await shot(`${NM[2]}-${th}-1400.png`); }
    } else {
      await resize(390, 844); await send('Page.navigate', { url: `http://${IP}/` }); await sleep(6000);
      await shot(`S10-${th}-390.png`);
      F[`navScroll_${th}`] = await ev(`(function(){var s=document.querySelector('.seg[data-page=home]');var n=s&&s.parentElement;if(!n)return 'no nav';n.scrollLeft=9999;return 'scrollLeft '+n.scrollLeft+' of '+(n.scrollWidth-n.clientWidth)})()`); await sleep(800);
      await shot(`S11-${th}-390.png`);
      await nav('advanced'); await sleep(1500); await shot(`S12-${th}-390.png`);
      await nav('settings'); await sleep(2500); await clickVis('/^\s*Not now\s*$/i'); await sleep(800); await clickVis('/^\s*[×x]\s*$/i'); await sleep(800); await shot(`S13a-${th}-390.png`);
      F[`railLast_${th}`] = await ev(`(function(){var r=document.querySelector('[class*=rail]');if(!r)return 'no rail';var it=r.querySelectorAll('button,a,[role=tab],li');var l=it[it.length-1];if(l){l.click();return 'clicked '+(l.textContent||'').trim().slice(0,30)}return 'no items'})()`); await sleep(1500);
      await shot(`S13-${th}-390.png`);
      await resize(1400); await send('Page.navigate', { url: `http://${IP}/` }); await sleep(6000); await nav('settings'); await sleep(2500); await clickVis('/^\s*Not now\s*$/i'); await sleep(800); await clickVis('/^\s*[×x]\s*$/i'); await sleep(800); await shot(`S14-${th}-1400.png`);
    }
  }
  F.consoleErrors = errs;
} catch (e) { F.error = String(e && e.stack || e); }
writeFileSync(join(OUT, 'facts.json'), JSON.stringify(F, null, 1));
ch.kill('SIGKILL'); try { rmSync(prof, { recursive: true, force: true }); } catch {}
process.exit(0);
