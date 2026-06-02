// Headless validation of the "SAT only" live-log filter (TASK-815).
// Serves the LittleFS data dir, drives system Chrome, seeds the REAL log buffer
// with the production shape (parseLogLine output: OT-frame objects + SAT event
// objects with prefix 'S'), runs the REAL updateFilteredBuffer(), and asserts
// only the SAT narration events survive. Also checks the parser tags an 'S'
// prefixed raw line as a SAT event, and the string-seed fallback path.
import http from 'node:http'; import fs from 'node:fs'; import path from 'node:path';
import { chromium } from 'playwright';
const DATA = process.env.OTGW_DATA_DIR, PORT = 8139;
const MIME = {'.html':'text/html','.js':'text/javascript','.css':'text/css','.json':'application/json','.svg':'image/svg+xml','.ico':'image/x-icon','.png':'image/png'};
const srv = http.createServer((q,s)=>{let p=decodeURIComponent(new URL(q.url,`http://x:${PORT}`).pathname);if(p==='/')p='/index.html';const f=path.join(DATA,p);if(fs.existsSync(f)&&fs.statSync(f).isFile()){s.writeHead(200,{'content-type':MIME[path.extname(f)]||'application/octet-stream'});fs.createReadStream(f).pipe(s);}else{s.writeHead(404);s.end('nf');}});
await new Promise(r=>srv.listen(PORT,'127.0.0.1',r));
const b = await chromium.launch({channel:'chrome',headless:true});
const pg = await b.newPage();
await pg.route('**/api/**', r=>r.fulfill({status:200,contentType:'application/json',body:'{}'}));
await pg.goto(`http://127.0.0.1:${PORT}/index.html`,{waitUntil:'domcontentloaded'});
let ok=true; const log=(p,m)=>{console.log((p?'PASS':'FAIL')+': '+m); if(!p)ok=false;};

// 1. The parser must tag an 'S'-prefixed raw WS line as a SAT event.
const parsed = await pg.evaluate(() => {
  const o = parseLogLine('19:42:05.123456 S Flame lit: flow 22C, setpoint 45C');
  return o ? { isEvent: o.isEvent, prefix: o.prefix, label: o.label } : null;
});
log(parsed && parsed.isEvent === true && parsed.prefix === 'S',
    "parseLogLine tags 'S' line as event prefix 'S' (got "+JSON.stringify(parsed)+")");

// 2. Production path: filter parsed objects (OT frames + SAT events).
const prod = await pg.evaluate(() => {
  otLogBuffer = [
    {data:{isEvent:false, source:'Boiler', raw:'80000100', id:0,  dir:'Read-Data', label:'Status',      value:'Master', valid:'>'}},
    {data:{isEvent:true,  prefix:'S', label:'Flame lit: flow 22C, setpoint 45C'}},
    {data:{isEvent:false, source:'Boiler', raw:'401C8000', id:17, dir:'Read-Ack',  label:'RelModLevel', value:'78', valid:'>'}},
    {data:{isEvent:true,  prefix:'S', label:'Flame off: cycle GOOD, 410s on, peak flow 47C'}},
    {data:{isEvent:true,  prefix:'!', label:'some other event, not SAT'}}
  ];
  satOnlyFilter = true;
  updateFilteredBuffer();
  return otLogFilteredBuffer.map(e => formatLogLine(e.data));
});
log(prod.length === 2, 'production: SAT-only keeps exactly the 2 SAT events (got '+prod.length+')');
log(prod.every(l => /^S\s/.test(l)), 'production: every kept line renders with the S prefix (got '+JSON.stringify(prod)+')');

// 3. Toggle off restores the full buffer.
const offCount = await pg.evaluate(() => { satOnlyFilter = false; updateFilteredBuffer(); return otLogFilteredBuffer.length; });
log(offCount === 5, 'toggle off restores all 5 lines (got '+offCount+')');

// 4. String-seed fallback path still works (headless convenience).
const strRes = await pg.evaluate(() => {
  otLogBuffer = [ {data:'19:42:02 . Request Boiler R80000100 Status'}, {data:'19:42:05 S Flame lit'} ];
  satOnlyFilter = true; updateFilteredBuffer();
  return otLogFilteredBuffer.length;
});
log(strRes === 1, 'string-seed fallback keeps the 1 S line (got '+strRes+')');

await b.close(); srv.close();
console.log(ok ? 'RESULT: ALL PASS' : 'RESULT: FAILURES'); process.exitCode = ok ? 0 : 1;
