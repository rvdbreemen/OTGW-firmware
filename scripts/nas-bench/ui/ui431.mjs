// beta.431 web UI bench (otgw-webui list): rail keyboard focus S20/S21, Tab order + Enter, satroomsource help S22, 390 px fades S23-S25, crash-log ok-line S27, toast after save S28. Real mouse/key events via CDP Input; IP/host/MAC scrubbed, hostname/SSID inputs masked.
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
const SCRUB = `(function(){var re=/192\\.168\\.88\\.\\d+|zaphod-classic-s3|marvin-otgw32|otgw-[0-9A-Fa-f]{12}|([0-9A-F]{2}:){5}[0-9A-F]{2}|-?\\d{1,3}\\.\\d{3,}, ?-?\\d{1,3}\\.\\d{3,}/g;
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
  const setTheme = async t => { await ev(`localStorage.setItem('otgw-theme','${t}'),'ok'`); await send('Page.navigate', { url: `http://${IP}/` }); await sleep(7000); };
  const resize = (w, h) => send('Emulation.setDeviceMetricsOverride', { width: w, height: h || 900, deviceScaleFactor: 1, mobile: false });
  const mouse = async (r) => { if (r && r.found) { for (const t of ['mouseMoved', 'mousePressed', 'mouseReleased']) await send('Input.dispatchMouseEvent', { type: t, x: r.x, y: r.y, button: 'left', clickCount: 1 }); } return r; };
  const clickVis = async (re) => mouse(await J(`JSON.stringify((function(){var a=[].filter.call(document.querySelectorAll('button,a,[role=tab],.seg,.tab'),function(x){return ${re}.test((x.textContent||'').trim())&&(x.offsetWidth||x.offsetHeight)})[0];if(!a)return {found:false};var b=a.getBoundingClientRect();return {found:true,text:a.textContent.trim().slice(0,40),x:b.x+b.width/2,y:b.y+b.height/2}})())`));
  const clickSel = async (sel, idx) => mouse(await J(`JSON.stringify((function(){var a=document.querySelectorAll('${sel}')[${idx || 0}];if(!a)return {found:false};a.scrollIntoView({block:'center'});var b=a.getBoundingClientRect();return {found:true,x:b.x+b.width/2,y:b.y+b.height/2}})())`));
  const key = async (k, mods) => { const code = { Tab: 9, Enter: 13 }[k]; const o = { key: k, code: k, windowsVirtualKeyCode: code, modifiers: mods || 0 }; await send('Input.dispatchKeyEvent', { type: 'keyDown', ...o, text: k === 'Enter' ? '\r' : undefined }); await send('Input.dispatchKeyEvent', { type: 'keyUp', ...o }); await sleep(150); };
  const act = () => J(`JSON.stringify((function(){var e=document.activeElement;if(!e)return null;return (e.tagName+'.'+(e.className||'')+'#'+(e.id||'')+' | '+((e.getAttribute('aria-label')||e.textContent||e.value||'').trim().slice(0,28))+' | focus-visible='+e.matches(':focus-visible'))})())`);
  const dismiss = async () => { const a = await clickVis('/^\\s*Not now\\s*$/i'); await sleep(600); const b = await clickVis('/^\\s*[×x]\\s*$/i'); await sleep(600); return [a && a.found, b && b.found]; };
  const clip = async (name, sel, pad) => { const p = pad || 6; const b = await J(`JSON.stringify((function(){var e=document.querySelector('${sel}');if(!e)return null;var r=e.getBoundingClientRect();return {x:r.x,y:r.y,w:r.width,h:r.height}})())`); if (!b) return; await ev(SCRUB); await ev(MASK); const s2 = await send('Page.captureScreenshot', { format: 'png', clip: { x: Math.max(0, b.x - p), y: Math.max(0, b.y - p), width: b.w + 2 * p, height: b.h + 2 * p, scale: 2 } }); writeFileSync(join(OUT, name), Buffer.from(s2.data, 'base64')); };
  await send('Page.enable'); await send('Runtime.enable');
  await send('Page.navigate', { url: `http://${IP}/` }); await sleep(4000);
  F.fw = await ev(`fetch('/api/v2/device/info').then(r=>r.json()).then(j=>JSON.stringify(j.device||j).match(/"fwversion":"[^"]*"/)[0])`);
  F.versionHash = await ev(`fetch('/version.hash').then(r=>r.text())`);
  for (const th of ['light', 'dark']) {
    const G = F[th] = {};
    if (process.env.ONLY === 's23') { await setTheme(th); await resize(390, 844); await send('Page.navigate', { url: `http://${IP}/` }); await sleep(6000); await nav('home'); await sleep(2500); G.dismissHome = await dismiss(); G.homeNav = await ev(`(function(){var s=document.querySelector('.seg[data-page=home]');var n=s&&s.parentElement;return n?JSON.stringify({scrollLeft:n.scrollLeft,max:n.scrollWidth-n.clientWidth}):null})()`); await shot(`S23-${th}-390.png`); continue; }
    await setTheme(th); await resize(1400);
    G.footer = await ev(`(function(){var m=document.body.innerText.match(/v2\\.0\\.0-beta\\.\\d+/);return m?m[0]:null})()`);
    G.badge = await ev(`(function(){var e=document.querySelector('header');return e?e.innerText.replace(/\\s+/g,' ').slice(0,160):null})()`);
    // S20/S21 rail focus ring: category 2 active (mouse), focus on item 1 via Tab from the search box
    await nav('settings'); await sleep(2000); G.dismissSettings = await dismiss();
    for (let tr = 0; tr < 4; tr++) { await clickSel('#setRail .rail-item', 1); await sleep(900); if (await ev(`document.querySelectorAll('#setRail .rail-item')[1].classList.contains('active')`)) break; }
    await ev(`document.getElementById('setSearch').focus(),1`); await key('Tab'); await sleep(300);
    G.focusAfterTab = await act();
    G.railState = await J(`JSON.stringify([].map.call(document.querySelectorAll('#setRail .rail-item'),function(e,i){return i+':'+(e.classList.contains('active')?'active':'')+(e===document.activeElement?'+focus':'')}).filter(function(x){return /active|focus/.test(x)}))`);
    G.ring = await J(`JSON.stringify((function(){var e=document.activeElement;var s=getComputedStyle(e);return {outline:s.outline,outlineOffset:s.outlineOffset,boxShadow:s.boxShadow.slice(0,120),zIndex:s.zIndex,bg:s.backgroundColor}})())`);
    const n20 = th === 'light' ? 20 : 21;
    await shot(`S${n20}-${th}-1400.png`); await clip(`S${n20}-${th}-1400-railzoom.png`, '#setRail', 8);
    // Tab order from the top of the page
    await send('Page.navigate', { url: `http://${IP}/` }); await sleep(5000); await nav('settings'); await sleep(2500); await dismiss();
    await ev(`document.activeElement&&document.activeElement.blur(),window.scrollTo(0,0),1`);
    G.tabOrder = []; for (let i = 0; i < 30; i++) { await key('Tab'); G.tabOrder.push(await act()); }
    // Enter on a rail item: focus the search box, Tab to the 4th item, Enter
    await ev(`document.getElementById('setSearch').focus(),1`); for (let i = 0; i < 4; i++) await key('Tab');
    G.enterOn = await act(); await key('Enter'); await sleep(900);
    G.enterResult = await ev(`(document.querySelector('.set-cat-title')||{}).textContent`);
    // S22 satroomsource help
    await nav('sat'); await sleep(2000); await dismiss(); G.satTab = await clickVis('/OPERATIONAL/i'); await sleep(2500);
    await sleep(3500); G.dismissSat2 = await dismiss();
    G.satRow = await ev(`!!document.querySelector('.srow[data-key=satroomsource]')`);
    G.satInfoClick = await mouse(await J(`JSON.stringify((function(){var r=document.querySelector('.srow[data-key=satroomsource] .sinfo');if(!r)return {found:false};r.scrollIntoView({block:'center'});var b=r.getBoundingClientRect();return {found:true,x:b.x+b.width/2,y:b.y+b.height/2}})())`));
    await sleep(1200);
    G.helpText = await ev(`(function(){var p=document.getElementById('shelp');if(!p)return null;var r=p.getBoundingClientRect();return JSON.stringify({text:p.textContent,shown:p.classList.contains('show'),rect:[Math.round(r.x),Math.round(r.y),Math.round(r.width),Math.round(r.height)],scrollH:p.scrollHeight,clientH:p.clientHeight,vw:innerWidth,vh:innerHeight})})()`);
    await shot(`S22-${th}-1400.png`); await clip(`S22-${th}-1400-help.png`, '#shelp', 10);
    if (process.env.ONLY === 's20s22') continue;
    // S27 crash-log ok-line colour
    await nav('advanced'); await sleep(1500); G.dbgTab = await clickVis('/Debug Information/i'); await sleep(3500);
    G.crashOk = await J(`JSON.stringify((function(){var e=document.getElementById('dbgCrashOk');if(!e)return null;e.scrollIntoView({block:'center'});var s=getComputedStyle(e);function bgUp(x){while(x&&x.nodeType===1){var b=getComputedStyle(x).backgroundColor;if(b&&!/rgba\\(0, 0, 0, 0\\)|transparent/.test(b))return b;x=x.parentElement}return 'none'}return {shown:e.offsetWidth>0,text:e.textContent.trim().slice(0,40),color:s.color,bg:s.backgroundColor,effBg:bgUp(e),border:s.borderColor}})())`);
    await sleep(500); await shot(`S27-${th}-1400.png`);
    // S28 toast after saving a harmless setting (ledblink), restore afterwards
    await nav('settings'); await sleep(2000); await dismiss(); await clickSel('#setRail .rail-item', 9); await sleep(800);
    G.led0 = await ev(`document.querySelector('.srow[data-key=ledblink] input').checked`);
    await clickSel('.srow[data-key=ledblink] .sw', 0); await sleep(500);
    G.led1 = await ev(`document.querySelector('.srow[data-key=ledblink] input').checked`);
    G.save1 = await clickSel('#btnSave', 0); await sleep(700);
    G.toast = await ev(`(function(){var t=document.getElementById('toast');return t?JSON.stringify({text:t.textContent,cls:t.className,shown:getComputedStyle(t).opacity+'/'+getComputedStyle(t).visibility}):null})()`);
    await shot(`S28-${th}-1400.png`); await clip(`S28-${th}-1400-toast.png`, '#toast', 10);
    await sleep(3500);
    await clickSel('.srow[data-key=ledblink] .sw', 0); await sleep(500);
    G.led2 = await ev(`document.querySelector('.srow[data-key=ledblink] input').checked`);
    G.save2 = await clickSel('#btnSave', 0); await sleep(3500);
    // 390 px fades (S23-S25)
    await resize(390, 844); await send('Page.navigate', { url: `http://${IP}/` }); await sleep(6500);
    await nav('home'); await sleep(2500); await shot(`S23-${th}-390.png`);
    await nav('advanced'); await sleep(1500); await shot(`S24-${th}-390.png`);
    G.advNavScroll = await ev(`(function(){var s=document.querySelector('.seg[data-page=home]');var n=s&&s.parentElement;if(!n)return 'no nav';var a=document.querySelector('.seg[data-page=advanced]').getBoundingClientRect();var nb=n.getBoundingClientRect();return JSON.stringify({scrollLeft:n.scrollLeft,advRight:Math.round(a.right),navRight:Math.round(nb.right),inside:a.right<=nb.right+1})})()`);
    await nav('settings'); await sleep(2500); G.dismiss390 = await dismiss(); await shot(`S25a-${th}-390.png`);
    await clickSel('#setRail .rail-item', 10); await sleep(1200); await shot(`S25-${th}-390.png`);
    G.rail390 = await ev(`(function(){var n=document.getElementById('setRail');return JSON.stringify({scrollLeft:n.scrollLeft,max:n.scrollWidth-n.clientWidth})})()`);
  }
  F.consoleErrors = errs;
} catch (e) { F.error = String(e && e.stack || e); }
writeFileSync(join(OUT, 'facts.json'), JSON.stringify(F, null, 1));
ch.kill('SIGKILL'); try { rmSync(prof, { recursive: true, force: true }); } catch {}
process.exit(0);
