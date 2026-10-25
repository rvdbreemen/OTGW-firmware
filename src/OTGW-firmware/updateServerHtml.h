
#ifndef UPDATESERVERHTML_H
#define UPDATESERVERHTML_H

// TASK-1283: the /update page. It lives in app flash (PROGMEM), so it is served
// whatever the state of LittleFS. It carries two ways to update:
//  1. The one-action block: the browser takes the release zip (or the two .bin
//     files), recognises them by content, hands the unit over to the safeboot
//     updater, uploads the firmware there, waits for the new firmware, uploads
//     the filesystem to it and verifies. Safeboot and the OTA endpoints are not
//     changed (ADR-191); the browser tab runs the sequence.
//  2. The manual forms below it (advanced): one upload per form, as before.
// The first <script> (id updLogic) holds the page logic without DOM access
// (file recognition, zip reader, phase engine). tests/webui/update-orchestrator.test.mjs
// extracts it and runs it under node, so keep it free of document/window use.
// The raw string uses its own delimiter: the JS contains the sequence )" .
static const char UpdateServerIndex[] PROGMEM =
  R"UPD(<!DOCTYPE html>
<html charset="UTF-8">
<head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<script>
(function() {
 try {
   if (localStorage.getItem('theme') === 'dark') {
     document.documentElement.className = 'dark';
   }
 } catch (e) { console.error(e); }
})();
</script>
<style type='text/css'>
   body { font-family: sans-serif; max-width: 920px; margin: 20px auto; padding: 0 12px; box-sizing: border-box; }
   form, #oneBox, details { margin: 12px 0; padding: 12px; border: 1px solid #c7d7ea; border-radius: 6px; background: #f8fbff; box-sizing: border-box; }
   html.dark form, html.dark #oneBox, html.dark details { border-color: #555; background: #2b2b2b; }
   details form { margin: 8px 0 0 0; }
   summary { cursor: pointer; font-weight: bold; }
   input[type='file'] { margin: 8px 0 10px; max-width: 100%; }
   #fwSubmit, #fsSubmit, #oneStart {
     min-width: 170px;
     padding: 10px 14px;
     border: 1px solid #9aa7b5;
     border-radius: 6px;
     font-weight: 700;
     background: #d7dde3;
     color: #6c757d;
     cursor: not-allowed;
   }
   #fwSubmit:enabled, #fsSubmit:enabled, #oneStart:enabled {
     color: #fff;
     cursor: pointer;
     background: #2e9d57;
     border-color: #207240;
   }
   #fwSubmit:enabled:hover, #fsSubmit:enabled:hover, #oneStart:enabled:hover { background: #27854a; }
   #oneBox h2 { margin: 0 0 8px 0; font-size: 20px; }
   #oneBox p, #oneBox ul { margin: 6px 0; }
   .note { margin: 8px 0; padding: 8px 10px; border-left: 3px solid #7aaad6; background: #eef6ff; border-radius: 4px; font-size: 14px; }
   html.dark .note { border-left-color: #4f89c1; background: #3a3a3a; }
   .warn { border-left-color: #d9a400; background: #fff8e1; }
   html.dark .warn { border-left-color: #d9a400; background: #4a4020; }
   #dropZone { margin: 10px 0; padding: 10px; border: 2px dashed #9aa7b5; border-radius: 6px; }
   #dropZone.over { border-color: #2e9d57; }
   #oneSteps { margin: 10px 0; padding-left: 22px; }
   #oneSteps li { color: #888; }
   #oneSteps li.now { color: inherit; font-weight: bold; }
   #oneSteps li.done { color: #2e7d32; }
   html.dark #oneSteps li.done { color: #6fcf7a; }
   #pageProgress { display: none; }
   #updatePanel { margin-top: 10px; padding: 15px; background: #e8f4ff; border: 1px solid #7aaad6; color: black; }
   html.dark #updatePanel { background: #333; border: 1px solid #555; color: white; }
   #progressContainer, .pb { position: relative; width: 100%; height: 40px; background-color: #e0e0e0; border: 1px solid #999; border-radius: 4px; margin: 12px 0; overflow: hidden; box-sizing: border-box; }
   html.dark #progressContainer, html.dark .pb { background-color: #444; border-color: #666; }
   #progressBar, .pbf { height: 100%; background-color: #2196F3; transition: width 0.3s ease, background-color 0.5s ease; width: 0%; }
   #progressText, .pbt { position: absolute; top: 0; left: 0; width: 100%; height: 100%; display: flex; align-items: center; justify-content: center; font-weight: bold; font-size: 14px; color: #fff; text-shadow: 0 0 4px rgba(0,0,0,0.9); }
   #updateError, #oneErr { color: #b00020; font-weight: bold; margin: 12px 0; }
   html.dark #updateError, html.dark #oneErr { color: #ff5555; }
   #oneRec { margin: 8px 0; }
   #retryButton, #oneRetry, #oneBack { margin: 10px 8px 0 0; background-color: #d32f2f; color: white; border: none; padding: 10px 20px; cursor: pointer; border-radius: 4px; font-weight: bold; }
   #oneBack { background-color: #607d8b; }
   #oneOpen { display: inline-block; margin-top: 10px; padding: 10px 20px; background: #2e9d57; color: #fff; border-radius: 4px; font-weight: bold; text-decoration: none; }
</style>
</head>
<body>
<div id='pageForm'>
  <h1>OTGW firmware Flash utility</h1>
  <div id='oneBox'>
    <h2>Update to a new release</h2>
    <div id='oneIntro'>
    <p>Choose the release zip and press <b>Update</b>. The update takes 2 to 3 minutes. <b>Keep this tab open until it says Done.</b></p>
    <p class='note'>The unit does not control heating while the updater runs (1 to 2 minutes); on an OTGW32 the boiler then runs on its own settings.</p>
    <div id='unitInfo' class='note'>Reading the unit...</div>
    <details id='oneDetails'>
      <summary>Details: what is kept, what to expect</summary>
      <ul>
        <li><b>Kept:</b> your settings and your sensor names, the SAT learned values (energy totals, heating-curve data and markers, controller state), the learned OpenTherm profiles, the reboot log and the hot-water meter total. WiFi is kept too. A file that has grown past its size limit (2 to 3 KB) is left out.</li>
        <li><b>Starts fresh:</b> files you uploaded in the File Explorer, the boot detection log and the MQTT topic cleanup state.</li>
        <li>A board with the PIC gateway keeps passing OpenTherm traffic during the update.</li>
        <li>With an admin password set, the browser asks for it again. The updater uses WiFi with an address from DHCP, so a unit on Ethernet or with a fixed IP address can show up at another address.</li>
      </ul>
    </details>
    </div>
    <div id='oneWarn'></div>
    <div id='oneIntake'>
      <div id='dropZone'>
        <input type='file' id='oneFiles' multiple accept='.zip,.bin'>
        <div>Release zip (or the <b>.ino.bin</b> and <b>.littlefs.bin</b>). You can drop files here.</div>
      </div>
      <div id='oneFilesInfo'></div>
      <input id='oneStart' type='button' value='Update' disabled>
    </div>
    <div id='oneRun' style='display:none;'>
      <ol id='oneSteps'>
        <li>Switch to the firmware updater</li>
        <li>Install the firmware</li>
        <li>Restart and check the new firmware</li>
        <li>Install the web files</li>
        <li>Final check</li>
      </ol>
      <div class='pb'><div id='oneBar' class='pbf'></div><div id='oneBarText' class='pbt'></div></div>
      <div id='oneStatus'></div>
      <div id='oneErr'></div>
      <div id='oneRec'></div>
      <input id='oneRetry' type='button' value='Try again' style='display:none;'>
      <input id='oneBack' type='button' value='Back' style='display:none;'>
      <a id='oneOpen' href='./' style='display:none;'>Open web UI</a>
    </div>
    <p><a id='bkLink' href='/settings.ini' download>Download a settings backup (optional)</a></p>
  </div>
  <details id='manual'>
    <summary>Advanced: update the firmware or the web files by hand</summary>
    <form id='fwForm' method='POST' action='?cmd=0' enctype='multipart/form-data'>
         Select a "<b>.ino.bin</b>" file to flash<br/>
         <input type='file' accept='.ino.bin' name='firmware' required>
         <br/>
         <input id='fwSubmit' type='submit' value='Flash Firmware' disabled>
    </form>
    <form id='fsForm' method='POST' action='?cmd=100' enctype='multipart/form-data'>
         Select a "<b>.littlefs.bin</b>" file to flash<br/>
         <input type='file' accept='.littlefs.bin' name='filesystem' required>
         <br/>
         <input id='fsSubmit' type='submit' value='Flash LittleFS' disabled>
    </form>
  </details>
  <div id='formError' style='color: #b00020; font-weight: bold; margin-top: 8px;'></div>
</div>
<div id='pageProgress'>
  <h1>OTGW firmware Flash utility</h1>
  <div id='updatePanel'>
    <div>File: <strong><span id='updateFilename'>-</span></strong></div>
    <div>Target: <strong><span id='updateTarget'>-</span></strong></div>
    <div id='progressContainer'>
      <div id='progressBar'></div>
      <div id='progressText'>Preparing...</div>
    </div>
    <div id='updateError'></div>
    <button id='retryButton' onclick='retryFlash()' style='display:none;'>Try Again</button>
  </div>
</div>
<script id='updLogic'>
var OTGWUpd = (function () {
  var APP_MAX = 0x230000, FS_SIZE = 0xC0000, MAGIC = 0xABCD5432;
  var VER_RE = /\d+\.\d+\.\d+(?:-[a-z]+\.\d+)?\+[0-9a-f]{7}/g;
  var NOUNZIP = 'Your browser cannot unpack zip files. Unzip the file and choose the two .bin files.';
  var WHY = {
    usb: 'is a USB image (for flash_otgw), not a WiFi update file. Choose the .ino.bin and .littlefs.bin files, or the release zip.',
    safeboot: 'is the firmware updater itself. It is not updated over WiFi.',
    foreign: 'is not an OTGW firmware image.',
    noversion: 'has no version in it, so it cannot be checked.',
    fssize: 'is a filesystem image of the wrong size for this unit.',
    zip: 'is a zip file. Choose the zip on its own, or the two .bin files inside it.',
    unknown: 'is not a firmware or filesystem image from the release zip.'
  };
  function u16(b, o) { return b[o] | (b[o + 1] << 8); }
  function u32(b, o) { return (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0; }
  function bstr(b) {
    var s = '', i, n = b.length;
    for (i = 0; i < n; i += 8192) s += String.fromCharCode.apply(null, b.subarray(i, Math.min(n, i + 8192)));
    return s;
  }
  function has(b, str) { return bstr(b).indexOf(str) >= 0; }
  var crcT = null;
  function crc32(b) {
    var c, i, k, n = b.length;
    if (!crcT) {
      crcT = [];
      for (i = 0; i < 256; i++) { c = i; for (k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1); crcT[i] = c >>> 0; }
    }
    c = 0xFFFFFFFF;
    for (i = 0; i < n; i++) c = crcT[(c ^ b[i]) & 255] ^ (c >>> 8);
    return (c ^ 0xFFFFFFFF) >>> 0;
  }
  function classify(b) {
    var n = b.length, s, m, vers = [], head;
    if (n >= 0x24 && b[0] === 0xE9 && u16(b, 12) === 9 && u32(b, 0x20) === MAGIC && n <= APP_MAX) {
      s = bstr(b);
      // Safeboot's boot log line. Built at run time and not 'start': this page's own text is inside every main
      // image, so no marker literal may appear in it (a main image must never count as the updater).
      if (s.indexOf('[safe' + 'boot] app valid') >= 0) return { kind: 'safeboot' };
      if (s.indexOf('OTGW-firmware') < 0) return { kind: 'foreign' };
      VER_RE.lastIndex = 0;
      while ((m = VER_RE.exec(s)) && vers.length < 20) vers.push(m[0]);
      return vers.length ? { kind: 'app', vers: vers } : { kind: 'noversion' };
    }
    if (b[0] === 0x50 && b[1] === 0x4B && b[2] === 3 && b[3] === 4) return { kind: 'zip' };
    head = bstr(b.subarray(0, Math.min(n, 4112)));
    if (b[0] !== 0xE9 && n <= APP_MAX && (head.substr(8, 8) === 'littlefs' || head.substr(4104, 8) === 'littlefs')) {
      if (n !== FS_SIZE) return { kind: 'fssize' };
      return has(b, 'version.hash') ? { kind: 'fs' } : { kind: 'foreign' };
    }
    if (b[0] === 0xE9 || n > APP_MAX) return { kind: 'usb' };
    return { kind: 'unknown' };
  }
  function slugOf(name) { var m = /^OTGW-firmware-(esp32-[a-z0-9]+)-/.exec(name.split('/').pop()); return m ? m[1] : ''; }
  function hintOf(name) { var m = /(\d+\.\d+\.\d+(?:-[a-z]+\.\d+)?\+[0-9a-f]{7})/.exec(name); return m ? m[1] : ''; }
  function hwOk(slug, hw) {
    if (!slug || !hw || slug === 'esp32-combo') return true;
    return (slug === 'esp32-classic' && hw === 'otgw-classic') || (slug === 'esp32-otgw32' && hw === 'otgw32');
  }
  function isIPv4(h) { return /^\d{1,3}(\.\d{1,3}){3}$/.test(h); }
  function parseZip(b) {
    var n = b.length, i, eocd = -1, lim = Math.max(0, n - 65557), cnt, p, k, nl, el, cl, out = [];
    for (i = n - 22; i >= lim; i--) { if (u32(b, i) === 0x06054b50) { eocd = i; break; } }
    if (eocd < 0) throw new Error('This is not a valid zip file.');
    cnt = u16(b, eocd + 10); p = u32(b, eocd + 16);
    if (cnt === 0xFFFF || p === 0xFFFFFFFF) throw new Error('Zip64 archives are not supported. Choose the two .bin files.');
    for (k = 0; k < cnt; k++) {
      if (p + 46 > n || u32(b, p) !== 0x02014b50) throw new Error('The zip file is damaged.');
      nl = u16(b, p + 28); el = u16(b, p + 30); cl = u16(b, p + 32);
      out.push({ name: bstr(b.subarray(p + 46, p + 46 + nl)), flags: u16(b, p + 8), method: u16(b, p + 10), crc: u32(b, p + 16), csize: u32(b, p + 20), usize: u32(b, p + 24), lho: u32(b, p + 42) });
      p += 46 + nl + el + cl;
    }
    return out;
  }
  function pickZip(es) {
    var apps = [], fss = [], i, m, j;
    for (i = 0; i < es.length; i++) {
      m = /^OTGW-firmware-(esp32-[a-z0-9]+)-(.+)\.(ino|littlefs)\.bin$/.exec(es[i].name.split('/').pop());
      if (m) (m[3] === 'ino' ? apps : fss).push({ e: es[i], slug: m[1], stem: m[2] });
    }
    if (apps.length === 1) {
      for (j = 0; j < fss.length; j++) { if (fss[j].slug === apps[0].slug && fss[j].stem === apps[0].stem) return { app: apps[0], fs: fss[j] }; }
    }
    throw new Error('The zip does not contain exactly one .ino.bin file with its matching .littlefs.bin file.');
  }
  // Inflate with a byte cap: stop reading as soon as the output passes the declared size.
  // 'deflate-raw' is missing in older browsers: the constructor throws, which becomes the unzip advice.
  function inflateCapped(raw, cap) {
    var rd, parts = [], total = 0, dmg = function (m) { var x = new Error(m); x.zip = 1; return x; };
    try { rd = new Blob([raw]).stream().pipeThrough(new DecompressionStream('deflate-raw')).getReader(); }
    catch (err) { return Promise.reject(dmg(NOUNZIP)); }
    function pump() {
      return rd.read().then(function (r) {
        var out, o = 0, i;
        if (r.done) {
          out = new Uint8Array(total);
          for (i = 0; i < parts.length; i++) { out.set(parts[i], o); o += parts[i].length; }
          return out;
        }
        total += r.value.length;
        if (total > cap) { rd.cancel().then(null, function () {}); throw dmg('The zip file is damaged (an entry is larger than declared).'); }
        parts.push(r.value);
        return pump();
      });
    }
    return pump().then(null, function (err) { throw (err && err.zip) ? err : dmg('The zip file is damaged.'); });
  }
  function entryBytes(b, e) {
    var o = e.lho, st, raw, p, bad = function (m) { return Promise.reject(new Error(m)); };
    if (e.flags & 1) return bad('Encrypted zip files are not supported.');
    if (e.csize === 0xFFFFFFFF || e.usize === 0xFFFFFFFF || o === 0xFFFFFFFF) return bad('Zip64 archives are not supported. Choose the two .bin files.');
    if (u32(b, o) !== 0x04034b50) return bad('The zip file is damaged.');
    if (e.usize > APP_MAX) return bad('The zip file is damaged (an entry is larger than any firmware file).');
    st = o + 30 + u16(b, o + 26) + u16(b, o + 28);
    raw = b.subarray(st, st + e.csize);
    if (raw.length !== e.csize) return bad('The zip file is damaged.');
    if (e.method === 0) p = Promise.resolve(raw);
    else if (e.method === 8) {
      if (typeof DecompressionStream === 'undefined') return bad(NOUNZIP);
      p = inflateCapped(raw, e.usize);
    } else return bad('This zip uses a compression method that is not supported. Choose the two .bin files.');
    return p.then(function (u) {
      if (u.length !== e.usize || crc32(u) !== e.crc) throw new Error('The zip file is damaged (checksum mismatch).');
      return u;
    });
  }
  // Two files in, one verdict out. a/b: {name, u8}; hint: version from the zip entry name.
  function judge(a, b, slug, hint) {
    var ca = classify(a.u8), cb = classify(b.u8), app, fsf, capp, E, i, warns = [];
    if (ca.kind === 'app' && cb.kind === 'fs') { app = a; fsf = b; capp = ca; }
    else if (cb.kind === 'app' && ca.kind === 'fs') { app = b; fsf = a; capp = cb; }
    else {
      if (ca.kind !== 'app' && ca.kind !== 'fs') throw new Error('"' + a.name + '" ' + (WHY[ca.kind] || WHY.unknown));
      if (cb.kind !== 'app' && cb.kind !== 'fs') throw new Error('"' + b.name + '" ' + (WHY[cb.kind] || WHY.unknown));
      throw new Error('Choose one firmware file (.ino.bin) and one filesystem file (.littlefs.bin).');
    }
    hint = hint || hintOf(app.name);
    E = capp.vers[0];
    for (i = 0; i < capp.vers.length; i++) { if (capp.vers[i] === hint) E = hint; }
    if (!has(fsf.u8, E.split('+')[1])) warns.push('These two files look like they come from different builds.');
    return { app: app, fs: fsf, E: E, G: E.split('+')[1], slug: slug || slugOf(app.name), warns: warns };
  }
  // files: [{name}], rd(file) -> Promise<Uint8Array>
  function intake(files, rd) {
    var f = [], i;
    for (i = 0; i < files.length; i++) f.push(files[i]);
    if (f.length === 1 && /\.zip$/i.test(f[0].name)) {
      return rd(f[0]).then(function (u) {
        var pk = pickZip(parseZip(u));
        return entryBytes(u, pk.app.e).then(function (a) {
          return entryBytes(u, pk.fs.e).then(function (s) {
            return judge({ name: pk.app.e.name.split('/').pop(), u8: a }, { name: pk.fs.e.name.split('/').pop(), u8: s }, pk.app.slug, hintOf(pk.app.stem));
          });
        });
      });
    }
    if (f.length === 2) {
      return rd(f[0]).then(function (a) {
        return rd(f[1]).then(function (b) { return judge({ name: f[0].name, u8: a }, { name: f[1].name, u8: b }, '', ''); });
      });
    }
    return Promise.reject(new Error('Choose the release .zip, or both the .ino.bin and the .littlefs.bin file.'));
  }
  function parse(t) { try { var o = JSON.parse(t); return (o && typeof o === 'object') ? o : null; } catch (e) { return null; } }
  // GET and parse a JSON object. Never rejects. j is null unless the body is a JSON object:
  // safeboot answers unknown paths with a 302 to an HTML page, so a status is never proof.
  function jget(io, url, to) {
    return io.req('GET', url + (url.indexOf('?') < 0 ? '?' : '&') + 't=' + io.now(), { timeout: to || 4000 })
      .then(function (r) { return { status: r.status, j: r.status === 200 ? parse(r.text) : null }; },
            function () { return { status: 0, j: null }; });
  }
  function fwOf(j) { var d = j && (j.device || j); return (d && typeof d.fwversion === 'string') ? d.fwversion : null; }
  function snapshot(io) {
    var s = { ok: false };
    return jget(io, '/api/v2/device/info', 8000).then(function (r) {
      var d = r.j && (r.j.device || r.j);
      if (!d || typeof d.fwversion !== 'string') return s;
      s.ok = true; s.ver = d.fwversion; s.host = d.hostname || ''; s.ip = d.ipaddress || ''; s.net = d.networkmode || '';
      s.board = d.board || ''; s.hw = d.hardware_type || ''; s.sb = d.safeboot_available === true; s.appOta = d.app_ota_available === true;
      return jget(io, '/api/v2/filesystem/hash-check', 8000).then(function (h) {
        var c = h.j && h.j.filesystem_check;
        s.fsMatch = c ? c.match === true : null; s.fsHash = (c && c.fs_hash) || '';
        return s;
      });
    });
  }
  // What the page offers once the files are recognised (r, from intake) against a fresh unit snapshot s.
  // The web files are always sent, also when the unit already reports this githash: the filesystem
  // identity is only that 7-character hash, so a match is not proof the files are there.
  function planFor(s, r, host) {
    var w = r.warns.slice(), ok = !!(s && s.ok), same = ok && s.ver === r.E, mb = function (n) { return (n / 1048576).toFixed(2) + ' MB'; };
    var p = { from: 'handover', info: '', warns: w, start: ok && s.sb && !s.appOta && isIPv4(host) };
    if (ok && !hwOk(r.slug, s.hw)) w.push('These files are for another board type (' + r.slug + '); this unit reports ' + s.hw + '.');
    if (same && s.fsMatch === true && s.fsHash === r.G) { p.from = 'fs'; p.info = 'This unit already runs ' + r.E + ' with matching web files. Press Update to send the web files again anyway.'; }
    else if (same) { p.from = 'fs'; p.info = 'The unit already runs ' + r.E + '. Only the web files will be installed.'; }
    else p.info = 'Ready: ' + r.E + ' (' + mb(r.app.u8.length) + ' firmware, ' + mb(r.fs.u8.length) + ' web files).' + (ok ? ' The unit runs ' + s.ver + '.' : '');
    return p;
  }
  // The phase engine. io: {req(method,url,opts)->Promise<{status,text}> (rejects {kind,sentAll}), sleep(ms), now()};
  // ui: {step(stage), say(text), progress(loaded,total)}; c: {app,fs:{blob,name,size}, E, G, host, beforeFs(), afterFs(), t}.
  // Resolves {ok:true} or {ok:false, stage, code, msg, rec}; never rejects.
  function run(io, ui, c, from) {
    var order = ['handover', 'upload', 'boot', 'settle', 'fs', 'verify'], t = c.t || {}, cur = from;
    var tSb = t.sb || 60000, tBoot = t.boot || 120000, tSettle = t.settle === undefined ? 35000 : t.settle;
    var host = c.host || 'OTGW2', pw = ' Check the admin password and press Try again.';
    var recSb = 'The updater may be at another address (a unit on Ethernet or with a fixed IP address gets an address by DHCP over WiFi), or it opened its own WiFi network "' + host + '-safeboot" (join it and open http://192.168.4.1/). Upload the .ino.bin in the updater, then open this page again and choose the same files to finish the web files. Or switch the unit off and on: it returns to the old firmware.';
    var recBoot = 'Wait a minute and reload this page. If the unit does not come back, find it in your router, or join its WiFi network "' + host + '-safeboot" and open http://192.168.4.1/ to upload the .ino.bin again.';
    var recFs = 'Press Try again now and do not switch the unit off: a half-written filesystem loses the settings when the unit restarts. The new firmware is already installed.';
    function todo(s) { return order.indexOf(cur) <= order.indexOf(s); }
    function F(code, msg, rec, stage) { return { upd: 1, stage: stage || cur, code: code, msg: msg, rec: rec || '' }; }
    function authErr(r) { return F('auth', 'The unit refused the request (HTTP ' + r.status + ').', pw); }
    function waitSafeboot(ms) {
      var end = io.now() + ms, to = 15000;
      function loop() {
        return jget(io, '/info', to).then(function (r) {
          if (r.j && typeof r.j.safeboot === 'string') return r.j;
          if (r.status === 401) { ui.say('Waiting for the admin password in your browser...'); to = 60000; } else to = 15000;
          if (io.now() > end) return null;
          return io.sleep(1000).then(loop);
        });
      }
      return io.sleep(1500).then(loop);
    }
    function waitMain(ms) {
      var end = io.now() + ms, sbSince = 0;
      function loop() {
        return jget(io, '/api/v2/device/info').then(function (r) {
          var v = fwOf(r.j);
          if (v !== null) return { state: v === c.E ? 'up' : 'wrong', ver: v };
          return jget(io, '/info').then(function (s) {
            var sb = s.j && typeof s.j.safeboot === 'string';
            if (sb && s.j.app_rolled_back === true) return { state: 'rollback' };
            // The updater restarts within a second of a complete upload. Still there 8 s later: it did not get the whole image.
            if (!sb) sbSince = 0; else if (!sbSince) sbSince = io.now(); else if (io.now() - sbSince > 8000) return { state: 'stuck' };
            if (io.now() > end) return { state: 'timeout' };
            return io.sleep(1500).then(loop);
          });
        });
      }
      return loop();
    }
    function mainUp(r) {
      if (r.state === 'up') return;
      if (r.state === 'rollback') throw F('rollback', 'The new firmware failed to start and the updater took over again.', 'The web files were not changed. Open http://' + (c.ip || 'the unit address') + '/ (the updater) and upload a working .ino.bin there, for example the previous release.');
      if (r.state === 'stuck') throw F('fwincomplete', 'The updater did not receive the whole firmware.', 'The updater is still waiting for a firmware. Press Try again to send it again.', 'upload');
      if (r.state === 'wrong') throw F('wrongver', 'The unit runs ' + r.ver + ' instead of ' + c.E + '.', 'Do not repeat the update. Report this.');
      throw F('boottimeout', 'The unit did not come back within two minutes.', recBoot);
    }
    function sendForm(url, field, f) {
      return io.req('POST', url, { timeout: 300000, form: { field: field, blob: f.blob, name: f.name }, onProgress: ui.progress });
    }
    function doHandover() {
      cur = 'handover'; ui.step(cur); ui.say('Switching to the firmware updater...');
      return io.req('POST', '/safeboot', { timeout: 10000 }).then(function (r) {
        if (r.status === 200) return;
        if (r.status === 409) throw F('nosb', 'This unit has no firmware updater.', 'Update over USB with flash_otgw.');
        if (r.status === 401 || r.status === 403) throw authErr(r);
        throw F('handover', 'Unexpected answer from the unit: HTTP ' + r.status + '.', 'Reload this page and try again.');
      }, function () { /* the link may drop as the unit restarts; the poll below decides */ })
      .then(function () { ui.say('Waiting for the updater to start...'); return waitSafeboot(tSb); })
      .then(function (info) { if (!info) throw F('sbtimeout', 'The updater did not answer at this address within a minute.', recSb); });
    }
    function doUpload() {
      cur = 'upload'; ui.step(cur); ui.say('Checking the updater...');
      return waitSafeboot(15000).then(function (info) {
        if (!info) throw F('sbgone', 'The updater is not reachable.', recSb);
        ui.say('Sending the firmware...');
        return sendForm('/update', 'firmware', c.app).then(function (r) {
          if (r.status === 200) return;
          if (r.status === 401 || r.status === 403) throw authErr(r);
          throw F('fwfail', 'The updater refused the firmware: ' + r.text.slice(0, 160), 'The updater stays up. Press Try again.');
        }, function (e) {
          if (e && e.sentAll) return;
          throw F('fwnet', 'The connection broke while sending the firmware.', 'The updater stays up and waits for a good firmware. Press Try again.');
        });
      });
    }
    function doBoot() {
      cur = 'boot'; ui.step(cur); ui.say('Waiting for the new firmware to start...');
      return waitMain(t.boot || 120000).then(mainUp);
    }
    function doSettle() {
      var left = Math.round(tSettle / 1000);
      cur = 'settle'; ui.step(cur);
      function tick() {
        if (left <= 0) return waitMain(60000).then(mainUp);
        ui.say('The new firmware is running. Letting it settle: ' + left + ' s');
        left--;
        return io.sleep(1000).then(tick);
      }
      return tick();
    }
    function doFs() {
      cur = 'fs'; ui.step(cur); ui.say('Preparing the web files...');
      return Promise.resolve(c.beforeFs ? c.beforeFs() : 0).then(function () {
        ui.say('Sending the web files...');
        return sendForm('/update?cmd=100&size=' + c.fs.size, 'filesystem', c.fs).then(function (r) {
          if (r.status >= 200 && r.status < 300) {
            if (r.text.indexOf('Flash error') >= 0) throw F('fserr', 'The unit refused the web files: ' + r.text.replace(/^[^:]*Flash error:?\s*/, '').slice(0, 160), recFs);
            return;
          }
          if (r.status === 401 || r.status === 403) throw authErr(r);
          throw F('fshttp', 'The unit answered HTTP ' + r.status + ' to the web files.', recFs);
        }, function (e) {
          if (e && e.sentAll) return;
          throw F('fsnet', 'The connection broke while sending the web files.', recFs);
        });
      });
    }
    // One failed read is not a verdict: null means unknown, only a different version is wrong.
    function readVer(n) {
      return jget(io, '/api/v2/device/info', 8000).then(function (r) {
        var v = fwOf(r.j);
        if (v !== null || n <= 1) return v;
        return io.sleep(2000).then(function () { return readVer(n - 1); });
      });
    }
    function doVerify() {
      var end, seen = false, stale = false;
      cur = 'verify'; ui.step(cur); ui.say('The unit restarts with the new web files...');
      // UP with the old hash can be the instance from before the restart: keep polling until the deadline.
      function check() {
        return jget(io, '/api/v2/health').then(function (r) {
          var h = r.j && r.j.health;
          if (h) seen = true;
          if (!h || h.status !== 'UP') return false;
          return jget(io, '/api/v2/filesystem/hash-check', 8000).then(function (k) {
            var m = k.j && k.j.filesystem_check;
            if (m && m.match === true && m.fs_hash === c.G) return true;
            stale = true;
            return false;
          });
        }).then(function (ok) {
          if (ok) return 'ok';
          if (io.now() > end) return stale ? 'mismatch' : (seen ? 'degraded' : 'gone');
          return io.sleep(1500).then(check);
        });
      }
      return io.sleep(t.v0 === undefined ? 10000 : t.v0).then(function () {
        end = io.now() + (t.verify || 120000);
        return check();
      }).then(function (res) {
        if (res === 'degraded') throw F('degraded', 'The unit is back, but the web files did not mount.', 'Press Try again to send the web files once more.', 'fs');
        if (res === 'gone') throw F('fstimeout', 'The unit did not come back within two minutes.', recBoot);
        if (res === 'mismatch') throw F('fsmismatch', 'The web files on the unit do not match the firmware.', 'Press Try again to send the web files once more.', 'fs');
        return readVer(5);
      }).then(function (v) {
        if (v === null) throw F('unknownver', 'The unit is back but did not report its version.', 'Press Try again to check once more.');
        if (v !== c.E) throw F('wrongver', 'The unit runs ' + v + ' instead of ' + c.E + '.', 'Report this.');
        return Promise.resolve(c.afterFs ? c.afterFs() : 0);
      });
    }
    var p = Promise.resolve();
    if (todo('handover')) p = p.then(doHandover);
    if (todo('upload')) p = p.then(doUpload);
    if (todo('boot')) p = p.then(doBoot);
    if (todo('settle')) p = p.then(doSettle);
    if (todo('fs')) p = p.then(doFs);
    p = p.then(doVerify);
    return p.then(function () { return { ok: true }; }, function (e) {
      if (e && e.upd) { e.ok = false; return e; }
      return { ok: false, stage: cur, code: 'internal', msg: String((e && e.message) || e), rec: 'Reload this page and try again.' };
    });
  }
  return { classify: classify, parseZip: parseZip, pickZip: pickZip, entryBytes: entryBytes, intake: intake, judge: judge, crc32: crc32,
           snapshot: snapshot, planFor: planFor, run: run, jget: jget, hwOk: hwOk, isIPv4: isIPv4, slugOf: slugOf, has: has };
})();
</script>
<script>
(function() {
  var U = OTGWUpd;
  var pageForm = document.getElementById('pageForm');
  var pageProgress = document.getElementById('pageProgress');
  var fileEl = document.getElementById('updateFilename');
  var targetEl = document.getElementById('updateTarget');
  var progressBar = document.getElementById('progressBar');
  var progressText = document.getElementById('progressText');
  var errorEl = document.getElementById('updateError');
  var retryBtn = document.getElementById('retryButton');
  var formErrorEl = document.getElementById('formError');

  function $(id) { return document.getElementById(id); }
  function setText(id, t) { var e = $(id); if (e) e.textContent = t; }
  function show(id, on) { var e = $(id); if (e) e.style.display = on ? '' : 'none'; }

  // Plain XHR: fetch has no upload progress. One request at a time is the caller's job.
  // The phase engine names device paths from the root ('/safeboot', '/info', ...).
  // Resolve them against the page's directory, so the update also runs behind a
  // reverse proxy that serves the gateway on a path prefix (TASK-1377.5).
  var PAGE_BASE = location.pathname.replace(/[^/]*$/, '');
  // Served through a reverse proxy: on a path prefix (HA ingress, nginx sub-path) or over HTTPS.
  var PROXIED = PAGE_BASE !== '/' || location.protocol === 'https:';
  // The updater (safeboot) has no mDNS name, so the browser must reach the unit by IP, or a
  // proxy must, which the page cannot check: it warns instead (see the snapshot notes).
  function hostReachable() { return U.isIPv4(location.hostname) || PROXIED; }
  function req(method, url, o) {
    o = o || {};
    if (url.charAt(0) === '/') url = PAGE_BASE + url.slice(1);
    return new Promise(function (resolve, reject) {
      var x = new XMLHttpRequest(), sent = false, fd = null;
      x.open(method, url, true);
      x.timeout = o.timeout || 10000;
      if (o.form) { fd = new FormData(); fd.append(o.form.field, o.form.blob, o.form.name); }
      x.upload.onprogress = function (e) {
        if (e.lengthComputable && o.onProgress) o.onProgress(e.loaded, e.total);
      };
      x.upload.onload = function () { sent = true; };
      x.onload = function () { resolve({ status: x.status, text: x.responseText || '' }); };
      x.onerror = function () { reject({ kind: 'net', sentAll: sent }); };
      x.ontimeout = function () { reject({ kind: 'timeout', sentAll: sent }); };
      x.send(fd);
    });
  }
  var io = {
    req: req,
    now: function () { return Date.now(); },
    sleep: function (ms) { return new Promise(function (r) { setTimeout(r, ms); }); }
  };

  function restoreDallasLabelsFromStorage() {
    try {
      var stored = localStorage.getItem('otgw_dallas_restore');
      if (!stored) return Promise.resolve();
      var entry;
      try { entry = JSON.parse(stored); } catch(e) { localStorage.removeItem('otgw_dallas_restore'); return Promise.resolve(); }
      // Discard if older than 24 hours (stale / wrong-device guard)
      if (!entry || !entry.data || !entry.savedAt || (Date.now() - entry.savedAt) > 86400000 ||
          typeof entry.data !== 'object' || Object.keys(entry.data).length === 0) {
        localStorage.removeItem('otgw_dallas_restore');
        return Promise.resolve();
      }
      return fetch('api/v2/sensors/labels', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(entry.data)
      }).then(function(res) {
        if (res.ok) localStorage.removeItem('otgw_dallas_restore');
      }).catch(function(err) { console.error('[OTA] Label restore error:', err); });
    } catch (e) { return Promise.resolve(); }
  }

  // The firmware keeps up to 2 KB of Dallas labels across a filesystem update;
  // this copy in the browser covers larger label files.
  function saveDallasLabelsToStorage() {
    return fetch('api/v2/sensors/labels', { cache: 'no-store' })
      .then(function(res) { return res.ok ? res.json() : null; })
      .then(function(labels) {
        if (!labels || typeof labels !== 'object' || Object.keys(labels).length === 0) return;
        localStorage.setItem('otgw_dallas_restore', JSON.stringify({ data: labels, savedAt: Date.now() }));
      })
      .catch(function(e) { console.log('[OTA] Dallas labels save skipped:', e); });
  }

  function redirectToHome(delayMs) {
    setTimeout(function() { window.location.href = PAGE_BASE + '?_ota=' + Date.now(); }, delayMs || 1000);
  }

  function formatBytes(bytes) {
    if (!bytes || bytes < 0) return '0 B';
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / 1024 / 1024).toFixed(2) + ' MB';
  }

  //===== One-action update =====
  var snap = null, plan = null, running = false;

  function guard(e) { e.preventDefault(); e.returnValue = 'Keep this tab open until the update is done.'; return e.returnValue; }
  function note(text, cls, link) {
    var d = document.createElement('div');
    d.className = 'note' + (cls ? ' ' + cls : '');
    d.textContent = text;
    if (link) { var a = document.createElement('a'); a.href = link; a.textContent = link; d.appendChild(document.createTextNode(' ')); d.appendChild(a); }
    $('oneWarn').appendChild(d);
  }
  function blockOne(msg, link) {
    show('oneIntake', false);
    note(msg, 'warn', link);
  }
  function readFile(f) {
    return new Promise(function (resolve, reject) {
      var r = new FileReader();
      r.onload = function () { resolve(new Uint8Array(r.result)); };
      r.onerror = function () { reject(new Error('Could not read "' + f.name + '".')); };
      r.readAsArrayBuffer(f);
    });
  }

  function preflight() {
    snap = null; plan = null;
    $('oneWarn').textContent = '';
    setText('unitInfo', 'Reading the unit...');
    show('oneIntake', true);
    $('oneStart').disabled = true;
    return U.snapshot(io).then(function (s) {
      snap = s;
      if (!s.ok) {
        setText('unitInfo', 'The unit did not answer. Reload this page. The manual forms below still work.');
        blockOne('Could not read the unit, so the one-action update is off.');
        return;
      }
      var fsTxt = s.fsMatch === true ? 'match the firmware' : (s.fsMatch === false ? 'do NOT match the firmware (hash ' + (s.fsHash || 'unknown') + ')' : 'could not be checked');
      setText('unitInfo', 'Unit ' + (s.host || '?') + ' (' + (s.ip || '?') + ') runs ' + s.ver + '. Its web files ' + fsTxt + '.');
      if (s.appOta) { blockOne('This unit takes a firmware image directly (two firmware slots). Use the manual forms below.'); }
      else if (!s.sb) { blockOne('This unit cannot update its firmware over WiFi. Use USB (flash_otgw) or, for the web files only, the manual form below.'); }
      else if (PROXIED && !U.isIPv4(location.hostname)) {
        note('This page was opened through a reverse proxy. The update works only if the proxy reaches the unit by IP address (' + (s.ip || '?') + '): the updater has no network name.', 'warn');
      }
      else if (!U.isIPv4(location.hostname)) {
        blockOne('This page was opened as "' + location.hostname + '". The updater can only be reached by IP address, so open this page by IP:', s.ip ? 'http://' + s.ip + '/update' : '');
      }
      if (s.net === 'Ethernet') note('This unit is connected by Ethernet. The updater only uses WiFi, so it may come up at another address. See the recovery text if this page loses the unit.', 'warn');
      if (s.fsMatch === false) note('The web files on the unit do not match its firmware, so an earlier update was not finished. Choose the files of the release you want and press Update.', 'warn');
      applyManual(s);
      refreshStart();
    });
  }

  function refreshStart() {
    $('oneStart').disabled = !plan || !snap || !snap.ok || !snap.sb || snap.appOta || !hostReachable();
  }

  function setFiles(files) {
    plan = null;
    $('oneStart').disabled = true;
    setText('oneFilesInfo', '');
    if (!files || !files.length) return;
    setText('oneFilesInfo', 'Reading the files...');
    U.intake(files, readFile).then(function (r) {
      // Behind a proxy the browser never talks to the unit's address itself; the proxy does,
      // so the unit's IP stands in for the host check.
      var p = U.planFor(snap, r, PROXIED && snap && snap.ip ? snap.ip : location.hostname);
      plan = { r: r, from: p.from };
      setText('oneFilesInfo', p.info + (p.warns.length ? ' Warning: ' + p.warns.join(' ') : ''));
      refreshStart();
    }, function (e) {
      setText('oneFilesInfo', 'Not accepted: ' + (e && e.message ? e.message : String(e)));
    });
  }

  function setSteps(stage) {
    var map = { handover: 0, upload: 1, boot: 2, settle: 2, fs: 3, verify: 4, done: 9 }, i = map[stage], items = $('oneSteps').getElementsByTagName('li'), k;
    for (k = 0; k < items.length; k++) items[k].className = k < i ? 'done' : (k === i ? 'now' : '');
  }
  var ui = {
    step: function (s) { setSteps(s); $('oneBar').style.width = '0%'; setText('oneBarText', ''); },
    say: function (t) { setText('oneStatus', t); },
    progress: function (l, t) {
      var p = t ? Math.min(100, Math.round(l / t * 100)) : 0;
      $('oneBar').style.width = p + '%';
      setText('oneBarText', p + '% (' + formatBytes(l) + ' / ' + formatBytes(t) + ')');
    }
  };

  function go(from) {
    var r = plan.r;
    running = true;
    window.addEventListener('beforeunload', guard);
    show('oneIntake', false); show('oneIntro', false); show('oneWarn', false); show('manual', false); show('oneRun', true);
    show('oneRetry', false); show('oneBack', false); show('oneOpen', false);
    setText('oneErr', ''); setText('oneRec', '');
    $('oneBar').style.width = '0%'; setText('oneBarText', '');
    var c = {
      app: { blob: new Blob([r.app.u8]), name: r.app.name }, fs: { blob: new Blob([r.fs.u8]), name: r.fs.name, size: r.fs.u8.length },
      E: r.E, G: r.G, host: snap.host, ip: snap.ip, beforeFs: saveDallasLabelsToStorage, afterFs: restoreDallasLabelsFromStorage
    };
    c.app.size = r.app.u8.length;
    U.run(io, ui, c, from).then(function (res) {
      running = false;
      window.removeEventListener('beforeunload', guard);
      if (res.ok) {
        setSteps('done'); ui.say('Done. The unit runs ' + r.E + ' with matching web files.');
        $('oneBar').style.width = '100%'; setText('oneBarText', 'Done');
        $('oneOpen').href = PAGE_BASE + '?_ota=' + Date.now(); show('oneOpen', true);
        return;
      }
      setText('oneErr', res.msg); setText('oneRec', res.rec);
      ui.say('Stopped at: ' + res.stage);
      if (res.code !== 'rollback' && res.code !== 'wrongver' && res.code !== 'nosb') {
        $('oneRetry').onclick = function () { go(res.stage); };
        show('oneRetry', true);
      }
      $('oneBack').onclick = function () { show('oneRun', false); show('oneIntake', true); show('oneIntro', true); show('oneWarn', true); show('manual', true); preflight().then(function () { setFiles($('oneFiles').files); }); };
      show('oneBack', true);
    });
  }

  var picker = $('oneFiles');
  if (picker) {
    picker.addEventListener('change', function () { setFiles(picker.files); });
    $('oneStart').addEventListener('click', function () { if (plan && !running) go(plan.from); });
    var dz = $('dropZone');
    dz.addEventListener('dragover', function (e) { e.preventDefault(); dz.className = 'over'; });
    dz.addEventListener('dragleave', function () { dz.className = ''; });
    dz.addEventListener('drop', function (e) {
      e.preventDefault(); dz.className = '';
      if (e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files.length) setFiles(e.dataTransfer.files);
    });
  }

  //===== Manual forms (advanced) =====
  function pollUntilHealthy(options) {
    var remaining = options.timeoutSeconds || 60;
    if (options.onTick) options.onTick(remaining);
    var timer = setInterval(function() {
      remaining--;
      if (remaining <= 0) { clearInterval(timer); if (options.onTimeout) options.onTimeout(); return; }
      if (options.onTick) options.onTick(remaining);
      fetch('api/v2/health?t=' + Date.now(), { method: 'GET', cache: 'no-store', headers: { 'Accept': 'application/json' } })
        .then(function(res) { if (res.ok) return res.json(); throw new Error('HTTP ' + res.status); })
        .then(function(data) {
          if (data && data.health && data.health.status === 'UP') { clearInterval(timer); options.onHealthy(); }
        })
        .catch(function() { /* device still rebooting */ });
    }, 1000);
    return timer;
  }

  function showProgressPage() {
    pageForm.style.display = 'none';
    pageProgress.style.display = 'block';
    retryBtn.style.display = 'none';
    errorEl.textContent = '';
    progressBar.style.backgroundColor = '';
  }

  window.retryFlash = function() {
    pageProgress.style.display = 'none';
    pageForm.style.display = 'block';
    retryBtn.style.display = 'none';
    errorEl.textContent = '';
    formErrorEl.textContent = '';
  };

  function waitForDeviceReboot() {
    progressBar.style.width = '100%';
    pollUntilHealthy({
      timeoutSeconds: 60,
      onTick: function(remaining) { progressText.textContent = 'Device rebooting... (' + remaining + 's)'; },
      onHealthy: function() {
        progressText.textContent = 'Device is back online!';
        restoreDallasLabelsFromStorage().then(function() {
          progressText.textContent = 'Redirecting...';
          redirectToHome(1000);
        });
      },
      onTimeout: function() {
        progressText.textContent = 'Redirecting...';
        window.location.href = PAGE_BASE + '?_ota=' + Date.now();
      }
    });
  }

  function initUploadForm(formId, targetName) {
    var form = document.getElementById(formId);
    if (!form) return;
    var input = form.querySelector('input[type="file"]');
    var submit = form.querySelector('input[type="submit"]');
    if (input && submit) {
      submit.disabled = !input.files || input.files.length === 0;
      input.addEventListener('change', function() {
        submit.disabled = !input.files || input.files.length === 0;
        if (formErrorEl) formErrorEl.textContent = '';
      });
    }

    form.addEventListener('submit', function(e) {
      if (!input || !input.files || input.files.length === 0) {
        if (formErrorEl) formErrorEl.textContent = 'Select a file first.';
        e.preventDefault();
        return;
      }
      e.preventDefault();
      formErrorEl.textContent = '';
      showProgressPage();
      fileEl.textContent = input.files[0].name || '-';
      targetEl.textContent = targetName;
      progressBar.style.width = '0%';
      progressText.textContent = 'Preparing upload...';
      errorEl.textContent = '';

      if (!window.FormData || !window.XMLHttpRequest) {
        errorEl.textContent = 'Browser not supported';
        retryBtn.style.display = 'block';
        return;
      }

      // Filesystem flash: keep a browser copy of the Dallas labels (the unit keeps up to 2 KB itself)
      var preFlight = formId === 'fsForm' ? saveDallasLabelsToStorage() : Promise.resolve();

      preFlight.then(function() {
        var action = form.action;
        if (action.indexOf('size=') === -1) {
          action += (action.indexOf('?') === -1 ? '?' : '&') + 'size=' + encodeURIComponent(input.files[0].size);
        }
        var xhr = new XMLHttpRequest();
        xhr.open('POST', action, true);
        xhr.timeout = 300000; // 5 minutes for flash operations

        xhr.upload.onprogress = function(ev) {
          if (ev.lengthComputable) {
            var pct = Math.round((ev.loaded / ev.total) * 100);
            if (pct > 100) pct = 100;
            progressBar.style.width = pct + '%';
            progressText.textContent = 'Uploading: ' + pct + '% (' + formatBytes(ev.loaded) + ' / ' + formatBytes(ev.total) + ')';
          }
        };

        xhr.onload = function() {
          if (xhr.status >= 200 && xhr.status < 300) {
            var responseText = xhr.responseText || '';
            if (responseText.indexOf('Flash error') !== -1) {
              progressText.textContent = 'Flash error';
              errorEl.textContent = responseText;
              retryBtn.style.display = 'block';
            } else {
              progressBar.style.width = '100%';
              progressBar.style.backgroundColor = '#4CAF50';
              progressText.textContent = 'Flash complete! Device rebooting...';
              waitForDeviceReboot();
            }
          } else {
            progressText.textContent = 'Upload failed';
            errorEl.textContent = 'Upload failed: HTTP ' + xhr.status;
            retryBtn.style.display = 'block';
          }
        };

        xhr.ontimeout = function() {
          progressText.textContent = 'Upload timeout';
          errorEl.textContent = 'Connection timeout - flash may still be in progress. Wait 60 seconds and check device manually.';
          retryBtn.style.display = 'block';
        };

        xhr.onerror = function() {
          progressText.textContent = 'Upload error';
          errorEl.textContent = 'Upload connection lost - flash may still be in progress. Wait 60 seconds and check device manually.';
          retryBtn.style.display = 'block';
        };

        xhr.send(new FormData(form));
      }).catch(function(e) {
        console.error('[OTA] Error: Upload cancelled or pre-flight failed', e);
        progressText.textContent = 'Cancelled';
        errorEl.textContent = 'Cancelled';
        retryBtn.style.display = 'block';
      });
    });
  }

  initUploadForm('fwForm', 'flash');
  initUploadForm('fsForm', 'filesystem');

  // TASK-959: a board without a spare app slot cannot receive a firmware write
  // without overwriting the running partition (the backend refuses it too).
  // ADR-191: on the 4MB safeboot layout the firmware goes through the updater.
  function applyManual(info) {
    var fwForm = document.getElementById('fwForm');
    if (!fwForm) return;
    if (info.appOta) return;
    var box = document.createElement('div');
    box.className = 'note';
    if (info.sb) {
      var title = document.createElement('b');
      title.textContent = 'Firmware update by hand runs in the safeboot updater';
      var text = document.createElement('p');
      text.textContent = 'The device restarts into a small updater at the same address. ' +
        'Upload the .ino.bin firmware file there; it then restarts into the new firmware. ' +
        'The gateway does not run while the updater is active.';
      var btn = document.createElement('button');
      btn.type = 'button';
      btn.textContent = 'Restart into the firmware updater';
      var msg = document.createElement('p');
      btn.addEventListener('click', function() {
        if (!confirm('Restart into the firmware updater now? The gateway stops until you upload a firmware or restart.')) return;
        btn.disabled = true;
        fetch('safeboot', { method: 'POST' })
          .then(function(r) { return r.text().then(function(t) { return { ok: r.ok, t: t }; }); })
          .then(function(res) {
            msg.textContent = res.t;
            if (!res.ok) { btn.disabled = false; return; }
            var left = 20;
            var tick = setInterval(function() {
              left--;
              msg.textContent = 'Restarting into the updater... ' + left + 's';
              if (left <= 0) { clearInterval(tick); location.href = './'; }
            }, 1000);
          })
          .catch(function() { msg.textContent = 'Request failed.'; btn.disabled = false; });
      });
      box.appendChild(title); box.appendChild(text); box.appendChild(btn); box.appendChild(msg);
    } else {
      box.innerHTML = '<b>Firmware (app) update is USB-only on this board</b><br>' +
        'This board has a single app slot (no spare OTA partition), so writing a firmware image over WiFi ' +
        'would overwrite the running app and could brick the device. Use <code>flash_otgw.bat</code> over USB instead. ' +
        'Filesystem (LittleFS) updates below are unaffected.';
    }
    fwForm.parentNode.replaceChild(box, fwForm);
  }

  preflight();
})();
</script>
</html>)UPD";

static const char UpdateServerSuccess[] PROGMEM =
  // NOTE: This page is the HTTP response body for a traditional (non-XHR) form POST.
  // In the normal XHR flow the main upload page handles polling and label restore itself;
  // this page's script only runs when the browser navigates here via a legacy form submit.
  R"SUCCESS(<html charset="UTF-8">
      <head>
      <script>
        (function() {
          try {
            if (localStorage.getItem('theme') === 'dark') { document.documentElement.className = 'dark'; }
          } catch (e) {}
        })();
      </script>
      <style type='text/css'>body { font-family: sans-serif; }</style>
      </head>
      <body>
      <h1>OTGW firmware Flash utility</h1>
      <h2>Flashing successful!</h2>
      <p>Wait for the OTGW firmware to reboot and start the HTTP server.</p>
      <p><span id="s" style="font-weight:bold; color:#666;">Waiting for device... (60s)</span></p>
      <p>If nothing happens: <b><a href="./">click here</a></b>.</p>
      </body>
      <script>
        // Fallback path only — restore Dallas labels from localStorage then redirect.
        // saveDallasLabelsToStorage() must have run on the upload page before the flash.
        (function() {
          function tryRestoreLabels() {
            try {
              var raw = localStorage.getItem('otgw_dallas_restore');
              if (!raw) return;
              var e = JSON.parse(raw);
              if (!e || !e.data || !e.savedAt || (Date.now() - e.savedAt) > 86400000) {
                localStorage.removeItem('otgw_dallas_restore'); return;
              }
              if (typeof e.data !== 'object' || Object.keys(e.data).length === 0) {
                localStorage.removeItem('otgw_dallas_restore'); return;
              }
              fetch('api/v2/sensors/labels', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(e.data)
              }).then(function(r) {
                if (r.ok) { localStorage.removeItem('otgw_dallas_restore'); console.log('[OTA] Dallas labels restored (fallback path)'); }
                else { console.error('[OTA] Label restore failed: HTTP ' + r.status); }
              }).catch(function(err) { console.error('[OTA] Label restore error:', err); });
            } catch(ex) { console.log('[OTA] Label restore skipped:', ex); }
          }

          var s = document.getElementById('s'), n = 60;
          var t = setInterval(function() {
            n--;
            if (n <= 0) { clearInterval(t); window.location.href = './'; return; }
            s.textContent = 'Waiting for device... (' + n + 's)';
            fetch('api/v2/health?t=' + Date.now(), { cache: 'no-store' })
              .then(function(r) { return r.ok ? r.json() : null; })
              .then(function(d) {
                if (d && d.health && d.health.status === 'UP') {
                  clearInterval(t);
                  s.textContent = 'Device is back online!';
                  tryRestoreLabels();
                  setTimeout(function() { window.location.href = './'; }, 1000);
                }
              }).catch(function() {});
          }, 1000);
        })();
      </script>
    </html>)SUCCESS";


#endif // UPDATESERVERHTML_H
