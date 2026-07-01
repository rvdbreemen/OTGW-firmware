/*
 * OTGW-firmware — v2 Web UI (TASK-908)
 *
 * Coexisting redesign served as /v2.html when settings.ui.bUseV2 is set.
 * This file is the interaction shell: page navigation, Home concept chips,
 * Monitor sub-tabs, theme toggle (shared 'otgw-theme' key with the classic
 * UI), clock, and the "back to classic UI" toggle.
 *
 * Data wiring (OT-log WebSocket + /api/v2 REST) is layered on in later
 * phases via window.OTGWv2 hooks; the static mockup values remain visible
 * until each page is wired. No external libraries.
 */
(function () {
  'use strict';

  var APIGW = location.protocol + '//' + location.host + '/api/';

  // ---------- theme (shared key with classic UI) ----------
  function applyTheme(dark) {
    document.documentElement.setAttribute('data-theme', dark ? 'dark' : 'light');
    var b = document.getElementById('themeToggle');
    if (b) { b.textContent = dark ? '☀ Light' : '🌙 Dark'; b.setAttribute('aria-pressed', dark ? 'true' : 'false'); }
  }
  function initTheme() {
    applyTheme(document.documentElement.getAttribute('data-theme') === 'dark');
    var b = document.getElementById('themeToggle');
    if (b) b.addEventListener('click', function () {
      var nowDark = document.documentElement.getAttribute('data-theme') !== 'dark';
      applyTheme(nowDark);
      try { localStorage.setItem('otgw-theme', nowDark ? 'dark' : 'light'); } catch (e) { }
    });
  }

  // ---------- page navigation ----------
  function showPage(p) {
    document.querySelectorAll('.seg').forEach(function (s) {
      s.classList.toggle('active', s.dataset.page === p);
    });
    ['home', 'monitor', 'settings'].forEach(function (k) {
      var el = document.getElementById('page-' + k);
      if (el) el.classList.toggle('active', k === p);
    });
    try { localStorage.setItem('otgw-v2-page', p); } catch (e) { }
    if (p === 'monitor' && isMonitorLogVisible()) renderLog();
    if (p === 'settings') fetchSettings();
  }

  // ---------- Home concept chips (A/B/C) ----------
  function showDesign(d) {
    document.querySelectorAll('.cchip').forEach(function (s) {
      s.classList.toggle('active', s.dataset.design === d);
    });
    ['a', 'b', 'c'].forEach(function (k) {
      var el = document.getElementById('design-' + k);
      if (el) el.classList.toggle('active', k === d);
    });
    try { localStorage.setItem('otgw-v2-design', d); } catch (e) { }
    if (typeof renderActive === 'function') renderActive();
  }

  // ---------- Monitor sub-tabs ----------
  function showTab(t) {
    document.querySelectorAll('.subtab').forEach(function (s) {
      s.classList.toggle('active', s.dataset.tab === t);
    });
    document.querySelectorAll('.mpanel').forEach(function (el) {
      el.classList.toggle('active', el.id === t);
    });
    if (t === 'mlog') renderLog();
    else if (t === 'mstats') { renderStats(); fetchStatsPanels(); }
    else if (t === 'msupport') renderSupport();
    else if (t === 'mgraph') renderGraph();
    else if (t === 'mconn') { fetchConn(); fetchOtdOvr(); }
    else if (t === 'mdebug') fetchDebug();
  }

  // ---------- back to the classic UI ----------
  function gotoClassic() {
    // TASK-923: persist the choice device-side in settings.ini (ui_usev2=false)
    // via the settings API, then reload so the firmware serves the classic UI.
    // TASK-960: retry on 503/network and reload ONLY after a confirmed 200. The
    // old handler reloaded unconditionally (.finally) without checking
    // response.ok, so a 503 from the REST backpressure gate silently dropped the
    // switch and reloaded back to v2.
    var attempt = 0;
    function trySet() {
      fetch(APIGW + 'v2/settings', {
        method: 'POST', mode: 'cors',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: 'ui_usev2', value: 'false' })
      }).then(function (r) {
        if (r.ok) { location.href = '/'; }
        else if (++attempt <= 8) { setTimeout(trySet, 250 * attempt); }
      }).catch(function () {
        if (++attempt <= 8) { setTimeout(trySet, 250 * attempt); }
      });
    }
    trySet();
  }

  // ---------- clock ----------
  function tick() {
    var el = document.getElementById('clock');
    if (!el) return;
    var d = new Date();
    function p(n) { return (n < 10 ? '0' : '') + n; }
    el.textContent = p(d.getHours()) + ':' + p(d.getMinutes()) + ':' + p(d.getSeconds());
  }

  // ---------- connectivity model + always-visible strip (live; fed by fetchConn) ----------
  // One five-state vocabulary used everywhere (colour + icon + text), ported from
  // the mockup. MODE (gateway/monitor) is a separate kind, never a green/red light.
  var STICON = { 'st-ok': '✓', 'st-warn': '!', 'st-down': '✕', 'st-off': '○', 'st-unknown': '?', 'st-mode': '⇄' };
  var STLABEL = { 'st-ok': 'Connected', 'st-warn': 'Degraded', 'st-down': 'Disconnected', 'st-off': 'Off', 'st-unknown': 'Unknown', 'st-mode': 'Mode' };
  var STV = { 'st-ok': 'var(--zone-ok)', 'st-warn': 'var(--zone-warn)', 'st-down': 'var(--zone-alert)', 'st-off': 'var(--tile-off)', 'st-unknown': 'var(--muted)', 'st-mode': 'var(--accent)' };
  // CONN: static metadata (group / icon / name / explanation / fix hint) + live
  // state (s / value / detail), bound to the real /api/v2/health + settings. The OT
  // bus is two independent links (thermostat vs boiler) so "boiler not answering"
  // is distinguishable from "thermostat not asking".
  var CONN = {
    ws:    { grp: 'ses', s: 'st-unknown', icon: '📱', name: 'Live link', detail: '',
             exp: 'This page has a live link to the gateway — the log updates in real time.',
             fix: 'Live link dropped — the page is reconnecting; data may be paused.' },
    mode:  { grp: 'bus', s: 'st-mode', icon: '⇄', name: 'Gateway mode', value: '',
             exp: 'Gateway actively rewrites OT messages (lets the UI set temperatures); Monitor passively observes only.' },
    mqtt:  { grp: 'int', s: 'st-unknown', icon: '📡', name: 'MQTT broker', detail: '',
             exp: 'Connected to the broker; the online (birth) message is published.',
             fix: 'Cannot reach the broker — check address, port and credentials.' },
    wifi:  { grp: 'net', s: 'st-unknown', icon: '📶', name: 'Wi-Fi', detail: '',
             exp: 'The gateway is on your network.',
             fix: 'Weak or no signal — move the device or add a repeater.' },
    boiler:{ grp: 'bus', s: 'st-unknown', icon: '🔥', name: 'Boiler', detail: '',
             exp: 'The boiler (OT slave) is answering.',
             fix: 'Boiler not answering — check the OT wiring / boiler power.' },
    therm: { grp: 'bus', s: 'st-unknown', icon: '🌡️', name: 'Thermostat', detail: '',
             exp: 'The room thermostat (OT master) is talking to the gateway.',
             fix: 'No frames from the thermostat — check the OT wiring.' },
    ot:    { grp: 'bus', s: 'st-unknown', icon: '🔌', name: 'OpenTherm interface', detail: '',
             exp: 'The active OpenTherm command interface (PIC gateway or OT-Direct).',
             fix: 'No OT interface available — check the hardware / board mode.' },
    ntp:   { grp: 'int', s: 'st-unknown', icon: '🕐', name: 'Time (NTP)', detail: '',
             exp: 'The clock is synced; timestamps and time-to-thermostat are correct.',
             fix: 'No time sync — check the NTP server and internet access.' },
    api:   { grp: 'ses', s: 'st-ok', icon: '🔁', name: 'REST API', detail: 'polling',
             exp: 'Values on this page are fetched from the device API.' }
  };
  var connRssi = null;
  function renderConnStrip() {
    var strip = document.getElementById('connStrip'); if (!strip) return;
    var order = ['ws', 'mode', 'mqtt', 'wifi', 'boiler', 'therm'];
    var short = { ws: 'Live', mode: 'Mode', mqtt: 'MQTT', wifi: 'Wi-Fi', boiler: 'Boiler', therm: 'Thermostat' };
    strip.textContent = '';   // rebuild with DOM nodes (textContent — no HTML injection)
    order.forEach(function (key) {
      var c = CONN[key]; if (!c) return;
      var btn = document.createElement('button'); btn.className = 'connpill ' + c.s;
      btn.title = 'Connectivity — tap for the detail map';
      btn.addEventListener('click', function () { showPage('monitor'); showTab('mconn'); });
      var dot = document.createElement('span'); dot.className = 'd';
      var pk = document.createElement('span'); pk.className = 'pk'; pk.textContent = short[key] + ' ';
      var pv = document.createElement('span'); pv.className = 'pv';
      if (c.s === 'st-mode') pv.textContent = (c.value || 'unknown').toUpperCase();
      else if (key === 'wifi' && connRssi !== null) pv.textContent = connRssi + ' dBm';
      else pv.textContent = STLABEL[c.s] || '—';
      btn.appendChild(dot); btn.appendChild(pk); btn.appendChild(pv);
      strip.appendChild(btn);
    });
  }

  // ============================================================
  // Live data layer — OT-log WebSocket → model (TASK-908 P1+)
  // ============================================================
  // The firmware streams column-formatted OT log lines on the live-log
  // WebSocket. Each line carries a 9-char raw OT frame "Xnnhhll":
  //   X  = direction/type letter (T/B/R/A...)
  //   nn = data byte 0 — its high nibble (bits 6..4) is the OT message type
  //   id, hb, lb follow. We parse the raw frame directly for precision.
  // Only boiler READ-ACK (type 4) frames carry real sensor values; the
  // matching thermostat READ request carries zero data, so we filter by
  // message type per msgid to avoid clobbering a reading with a request.

  var WS_PATH = '/ws';            // overwritten by discovery below if needed
  var ws = null, wsTimer = null;
  var model = {
    flow: null, ret: null, dhw: null, outside: null, room: null,
    roomSet: null, chSet: null, mod: null, pressure: null,
    flame: false, ch: false, dhw_on: false, fault: false, fresh: false,
    wxValid: false   // TASK-954: weather API drives OUTSIDE when its last fetch is valid
  };

  function f88(u16) { var v = (u16 > 32767) ? u16 - 65536 : u16; return v / 256; }

  // OT message types (data byte 0, bits 6..4): 0=READ-DATA,1=WRITE-DATA,
  // 4=READ-ACK,5=WRITE-ACK,6=DATA-INVALID,7=UNKNOWN-DATAID.
  function applyFrame(raw) {
    if (!raw || raw.length < 9) return;
    var b0 = parseInt(raw.substr(1, 2), 16);
    var id = parseInt(raw.substr(3, 2), 16);
    var hb = parseInt(raw.substr(5, 2), 16);
    var lb = parseInt(raw.substr(7, 2), 16);
    if (isNaN(b0) || isNaN(id)) return;
    var type = (b0 >> 4) & 0x7;
    var u16 = (hb << 8) | lb;
    var isAck = (type === 4 || type === 5);       // boiler answer carries data
    var isWrite = (type === 1 || type === 5);     // thermostat/boiler write
    var changed = false;
    switch (id) {
      case 0:  // Status — slave status in LB
        if (isAck) {
          model.ch = !!(lb & 0x02); model.dhw_on = !!(lb & 0x04);
          model.flame = !!(lb & 0x08); model.fault = !!(lb & 0x01);
          changed = true;
        }
        break;
      case 1:  if (isWrite) { model.chSet = f88(u16); changed = true; } break;   // TSet (CH setpoint)
      case 16: if (isWrite) { model.roomSet = f88(u16); changed = true; } break;  // TrSet (room setpoint)
      case 17: if (isAck) { model.mod = f88(u16); changed = true; } break;        // RelModLevel
      case 18: if (isAck) { model.pressure = f88(u16); changed = true; } break;   // CH water pressure
      case 24: if (isAck || isWrite) { model.room = f88(u16); changed = true; } break; // Troom
      case 25: if (isAck) { model.flow = f88(u16); changed = true; } break;       // Tboiler (flow)
      case 26: if (isAck) { model.dhw = f88(u16); changed = true; } break;        // Tdhw
      case 27: if (isAck && !model.wxValid) { model.outside = f88(u16); changed = true; } break;    // Toutside (yields to weather API when valid, TASK-954)
      case 28: if (isAck) { model.ret = f88(u16); changed = true; } break;        // Tret (return)
    }
    if (changed) { model.fresh = true; pushSample(); scheduleRender(); }
  }

  // Column offsets mirror parseLogLine() in the classic UI: the raw frame
  // sits at column 19 (after an 18-char source field + space), possibly
  // shifted by a leading "HH:MM:SS.mmmmmm " timestamp.
  function rawFromLine(line) {
    if (typeof line !== 'string') return null;
    var off = 0;
    var ts = line.match(/^(\d{2}:\d{2}:\d{2}\.\d{3,6})\s/);
    if (ts) off = ts[0].length;
    if (/^[><!*S]\s/.test(line.substring(off))) return null; // narration/event line
    var raw = line.substr(19 + off, 9).trim();
    return /^[A-Za-z][0-9A-Fa-f]{8}$/.test(raw) ? raw : null;
  }

  function onWsMessage(ev) {
    var d = ev.data;
    if (typeof d !== 'string') return;
    if (d.indexOf('"type":"keepalive"') !== -1) return;
    if (d.charAt(0) === '{' || d.charAt(0) === '[') return; // JSON status, not a frame
    var raw = rawFromLine(d);
    if (raw) {
      applyFrame(raw);
      pushTicker(d);                // mission-control raw-frame ticker (concept C)
      pushLog(d);                   // Monitor > Log console
      updateStats(d, raw);          // Monitor > Stats + OT Support
      if (activeDesign() === 'c') scheduleRender();
    }
  }

  function activeDesign() {
    var d = document.querySelector('.cchip.active');
    return d ? d.dataset.design : 'a';
  }

  // Concept C buffers: strip-chart samples + raw-frame ticker.
  var SAMPLES_MAX = 1800, samples = [];
  var TICKER_MAX = 60, ticker = [];
  function pushSample() {
    samples.push({ t: Date.now(), flow: model.flow, ret: model.ret, sp: model.chSet, mod: model.mod });
    if (samples.length > SAMPLES_MAX) samples.shift();
  }
  function pushTicker(line) {
    // store parsed {ts,dir,body} so renderTicker can colour timestamp/request/answer
    // (T/R = request, B/A = answer) without ever feeding device data to innerHTML.
    var ts = '', body = line, tm = line.match(/^(\d{2}:\d{2}:\d{2}\.\d{3,6})\s+/);
    if (tm) { ts = tm[1]; body = line.substring(tm[0].length); }
    var raw = rawFromLine(line);
    ticker.push({ ts: ts, dir: raw ? raw.charAt(0).toUpperCase() : '', body: body });
    if (ticker.length > TICKER_MAX) ticker.shift();
  }

  // ---------- Monitor > Stats + OT Support ----------
  // Per-msgID telemetry built from the decoded log lines. We reuse the
  // firmware's own "> label = value" decoding rather than a static table.
  var stats = {};            // id -> {label, value, dirT, dirB, count, lastMs, interval}
  var statsSearch = '';
  function updateStats(line, raw) {
    var id = parseInt(raw.substr(3, 2), 16);
    if (isNaN(id)) return;
    var s = stats[id] || (stats[id] = { label: '', value: '', dirT: false, dirB: false, count: 0, lastMs: 0, interval: 0 });
    // direction from the raw frame's leading letter (T/R = request, B/A = answer)
    var dl = raw.charAt(0).toUpperCase();
    if (dl === 'T' || dl === 'R') s.dirT = true;
    if (dl === 'B' || dl === 'A') s.dirB = true;
    // label = value from the part after '>'
    var gt = line.indexOf('>');
    if (gt !== -1) {
      var rest = line.substring(gt + 1).trim();
      var eq = rest.indexOf('=');
      if (eq !== -1) { s.label = rest.substring(0, eq).trim(); s.value = rest.substring(eq + 1).trim(); }
      else if (rest) { s.label = rest; }
    }
    var now = Date.now();
    if (s.lastMs) s.interval = now - s.lastMs;
    s.lastMs = now; s.count++;
    if (isMonitorVisible('mstats')) renderStats();
    if (isMonitorVisible('msupport')) renderSupport();
  }
  function isMonitorVisible(panelId) {
    var p = document.getElementById('page-monitor');
    var el = document.getElementById(panelId);
    return p && el && p.classList.contains('active') && el.classList.contains('active');
  }
  function dirBadge(s) {
    if (s.dirT && s.dirB) return ['T+B', 'dir-tb'];
    if (s.dirT) return ['T', 'dir-t'];
    if (s.dirB) return ['B', 'dir-b'];
    return ['—', ''];
  }
  function renderStats() {
    var tb = document.querySelector('#statsTable tbody'); if (!tb) return;
    var ids = Object.keys(stats).map(Number).sort(sortStatsCmp);
    var q = statsSearch.trim().toLowerCase();
    tb.innerHTML = '';
    var shown = 0;
    ids.forEach(function (id) {
      var s = stats[id];
      if (q && ('' + id).indexOf(q) === -1 && s.label.toLowerCase().indexOf(q) === -1) return;
      shown++;
      var db = dirBadge(s);
      var tr = document.createElement('tr');
      function td(txt, cls) { var e = document.createElement('td'); if (cls) e.className = cls; e.textContent = txt; return e; }
      var idTd = document.createElement('td'); var b = document.createElement('span'); b.className = 'idbadge'; b.textContent = id; idTd.appendChild(b);
      tr.appendChild(idTd);
      tr.appendChild(td(s.label || ('ID ' + id)));
      var dTd = document.createElement('td'); var badge = document.createElement('span'); badge.className = 'dirbadge ' + db[1]; badge.textContent = db[0]; dTd.appendChild(badge); tr.appendChild(dTd);
      tr.appendChild(td(s.interval ? (s.interval / 1000).toFixed(1) + 's' : '—'));
      tr.appendChild(td('' + s.count));
      tr.appendChild(td(s.value || '—', 'statval'));
      tr.lastChild.style.textAlign = 'right';
      tb.appendChild(tr);
    });
    var cnt = document.getElementById('statsCount'); if (cnt) cnt.textContent = shown;
    updateSortIndicators();
  }
  // Reflect the active sort column/direction on the header (aria-sort + ▲/▼ glyph)
  // so the user can see which column is sorted and which way.
  function updateSortIndicators() {
    var ths = document.querySelectorAll('#statsTable thead th');
    var cols = ['id', 'label', 'dir', 'interval', 'count', 'value'];
    ths.forEach(function (th, i) {
      var on = cols[i] === statsSort.col;
      th.classList.toggle('sorted', on);
      th.classList.toggle('asc', on && statsSort.asc);
      th.classList.toggle('desc', on && !statsSort.asc);
      if (on) th.setAttribute('aria-sort', statsSort.asc ? 'ascending' : 'descending');
      else th.removeAttribute('aria-sort');
    });
  }
  // OpenTherm message-ID metadata (msgID 0..127), extracted verbatim from the
  // firmware OTmap[] in OTGW-Core.h. Drives the OT Support map detail panel + tooltip
  // so spec name / data type / direction / conclusion are authentic even for IDs the
  // bus has never shown (TASK-933 iter 3).
  var OTMSG = [
  /*   0 */ { name: "Master and Slave status", spec: "Status", type: "flag8/flag8", dir: "Read · boiler → thermostat", unit: "", concl: "Master/slave status flags: CH and DHW demand plus the boiler's flame, fault and mode bits." },
  /*   1 */ { name: "Control setpoint", spec: "TSet", type: "f8.8", dir: "Write · thermostat → boiler", unit: "°C", concl: "Thermostat's commanded boiler flow (CH water) setpoint." },
  /*   2 */ { name: "Master Config / Member ID", spec: "MasterConfigMemberIDcode", type: "flag8/u8", dir: "Write · thermostat → boiler", unit: "", concl: "Thermostat announces its configuration flags and OpenTherm member ID." },
  /*   3 */ { name: "Slave Config / Member ID", spec: "SlaveConfigMemberIDcode", type: "flag8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Boiler announces its configuration flags and OpenTherm member ID." },
  /*   4 */ { name: "Command-Code", spec: "Command", type: "u8/u8", dir: "Write · thermostat → boiler", unit: "", concl: "Thermostat sends a remote command to the boiler (e.g. boiler reset)." },
  /*   5 */ { name: "Application-specific fault", spec: "ASFflags", type: "flag8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Boiler's application-specific fault flags plus an OEM fault code." },
  /*   6 */ { name: "Remote-parameter flags", spec: "RBPflags", type: "flag8/flag8", dir: "Read · boiler → thermostat", unit: "", concl: "Boiler advertises which remote boiler parameters are transfer-enabled and read/write." },
  /*   7 */ { name: "Cooling control signal", spec: "CoolingControl", type: "f8.8", dir: "Write · thermostat → boiler", unit: "%", concl: "Thermostat's cooling demand signal to the boiler." },
  /*   8 */ { name: "Control setpoint for 2e CH circuit", spec: "TsetCH2", type: "f8.8", dir: "Write · thermostat → boiler", unit: "°C", concl: "Thermostat's flow setpoint for the second CH circuit." },
  /*   9 */ { name: "Remote override room setpoint", spec: "TrOverride", type: "f8.8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Remote-supplied room-setpoint override; a value of 0 means no override is active." },
  /*  10 */ { name: "Number of TSPs", spec: "TSP", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Number of transparent slave parameters the boiler exposes." },
  /*  11 */ { name: "Index number / Value of referred-to transparent slave parameter", spec: "TSPindexTSPvalue", type: "u8/u8", dir: "Read / Write", unit: "", concl: "Reads or writes one transparent slave parameter by index." },
  /*  12 */ { name: "Size of Fault-History-Buffer supported by slave", spec: "FHBsize", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Size of the boiler's fault-history buffer." },
  /*  13 */ { name: "Index number / Value of referred-to fault-history buffer entry", spec: "FHBindexFHBvalue", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Reads one fault-history buffer entry by index." },
  /*  14 */ { name: "Maximum relative modulation level setting", spec: "MaxRelModLevelSetting", type: "f8.8", dir: "Write · thermostat → boiler", unit: "%", concl: "Thermostat caps the boiler's maximum relative modulation." },
  /*  15 */ { name: "Maximum boiler capacity (kW) / Minimum boiler modulation level(%)", spec: "MaxCapacityMinModLevel", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "kW/%", concl: "Boiler reports its maximum capacity (kW) and minimum modulation level (%)." },
  /*  16 */ { name: "Room Setpoint", spec: "TrSet", type: "f8.8", dir: "Write · thermostat → boiler", unit: "°C", concl: "Thermostat's current room setpoint." },
  /*  17 */ { name: "Relative Modulation Level", spec: "RelModLevel", type: "f8.8", dir: "Read · boiler → thermostat", unit: "%", concl: "Boiler's actual relative modulation level (burner output, %)." },
  /*  18 */ { name: "CH water pressure", spec: "CHPressure", type: "f8.8", dir: "Read · boiler → thermostat", unit: "bar", concl: "Boiler's central-heating water pressure." },
  /*  19 */ { name: "DHW flow rate", spec: "DHWFlowRate", type: "f8.8", dir: "Read · boiler → thermostat", unit: "l/min", concl: "Domestic-hot-water flow rate through the boiler." },
  /*  20 */ { name: "Day of Week and Time of Day", spec: "DayTime", type: "special", dir: "Read / Write", unit: "", concl: "Day-of-week and time-of-day clock exchanged between thermostat and boiler." },
  /*  21 */ { name: "Calendar date", spec: "Date", type: "u8/u8", dir: "Read / Write", unit: "", concl: "Calendar month and day." },
  /*  22 */ { name: "Calendar year", spec: "Year", type: "u16", dir: "Read / Write", unit: "", concl: "Calendar year." },
  /*  23 */ { name: "Room Setpoint CH2", spec: "TrSetCH2", type: "f8.8", dir: "Write · thermostat → boiler", unit: "°C", concl: "Thermostat's room setpoint for the second CH circuit." },
  /*  24 */ { name: "Room Temperature", spec: "Tr", type: "f8.8", dir: "Write · thermostat → boiler", unit: "°C", concl: "Thermostat's measured room temperature." },
  /*  25 */ { name: "Boiler water temperature", spec: "Tboiler", type: "f8.8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Boiler's actual flow (CH water) temperature." },
  /*  26 */ { name: "DHW temperature", spec: "Tdhw", type: "f8.8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Domestic-hot-water temperature." },
  /*  27 */ { name: "Outside temperature", spec: "Toutside", type: "f8.8", dir: "Read / Write", unit: "°C", concl: "Outside air temperature used for weather-compensated control." },
  /*  28 */ { name: "Return water temperature", spec: "Tret", type: "f8.8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Central-heating return water temperature." },
  /*  29 */ { name: "Solar storage temperature", spec: "Tsolarstorage", type: "f8.8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Solar storage tank temperature." },
  /*  30 */ { name: "Solar collector temperature", spec: "Tsolarcollector", type: "s16", dir: "Read · boiler → thermostat", unit: "°C", concl: "Solar collector temperature." },
  /*  31 */ { name: "Flow water temperature CH2", spec: "TflowCH2", type: "f8.8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Flow water temperature of the second CH circuit." },
  /*  32 */ { name: "DHW2 temperature", spec: "Tdhw2", type: "f8.8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Second domestic-hot-water circuit temperature." },
  /*  33 */ { name: "Exhaust temperature", spec: "Texhaust", type: "s16", dir: "Read · boiler → thermostat", unit: "°C", concl: "Boiler exhaust-gas temperature." },
  /*  34 */ { name: "Boiler heat exchanger temperature", spec: "Theatexchanger", type: "f8.8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Boiler heat-exchanger temperature." },
  /*  35 */ { name: "Boiler fan speed and setpoint", spec: "FanSpeed", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "Hz", concl: "Boiler fan speed and its setpoint." },
  /*  36 */ { name: "Electrical current through burner flame", spec: "ElectricalCurrentBurnerFlame", type: "f8.8", dir: "Read · boiler → thermostat", unit: "µA", concl: "Flame-ionisation current measured at the burner." },
  /*  37 */ { name: "Room temperature for 2nd CH circuit", spec: "TRoomCH2", type: "f8.8", dir: "Write · thermostat → boiler", unit: "°C", concl: "Room temperature reported for the second CH circuit." },
  /*  38 */ { name: "Relative Humidity", spec: "RelativeHumidity", type: "f8.8", dir: "Read / Write", unit: "%", concl: "Relative humidity measurement." },
  /*  39 */ { name: "Remote override room setpoint 2", spec: "TrOverride2", type: "f8.8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Remote room-setpoint override for a second zone; 0 means no override is active." },
  /*  40 */ { name: "Reserved", spec: "", type: "—", dir: "Undefined", unit: "", concl: "" },
  /*  41 */ { name: "Reserved", spec: "", type: "—", dir: "Undefined", unit: "", concl: "" },
  /*  42 */ { name: "Reserved", spec: "", type: "—", dir: "Undefined", unit: "", concl: "" },
  /*  43 */ { name: "Reserved", spec: "", type: "—", dir: "Undefined", unit: "", concl: "" },
  /*  44 */ { name: "Reserved", spec: "", type: "—", dir: "Undefined", unit: "", concl: "" },
  /*  45 */ { name: "Reserved", spec: "", type: "—", dir: "Undefined", unit: "", concl: "" },
  /*  46 */ { name: "Reserved", spec: "", type: "—", dir: "Undefined", unit: "", concl: "" },
  /*  47 */ { name: "Reserved", spec: "", type: "—", dir: "Undefined", unit: "", concl: "" },
  /*  48 */ { name: "DHW setpoint upper & lower bounds for adjustment", spec: "TdhwSetUBTdhwSetLB", type: "s8/s8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Allowed upper/lower bounds for the DHW setpoint." },
  /*  49 */ { name: "Max CH water setpoint upper & lower bounds for adjustment", spec: "MaxTSetUBMaxTSetLB", type: "s8/s8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Allowed upper/lower bounds for the maximum CH setpoint." },
  /*  50 */ { name: "OTC heat curve ratio upper & lower bounds for adjustment", spec: "HcratioUBHcratioLB", type: "s8/s8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Allowed upper/lower bounds for the OTC heat-curve ratio." },
  /*  51 */ { name: "Remote parameter 4 boundaries", spec: "Remoteparameter4boundaries", type: "s8/s8", dir: "Read · boiler → thermostat", unit: "", concl: "Upper/lower bounds for remote parameter 4." },
  /*  52 */ { name: "Remote parameter 5 boundaries", spec: "Remoteparameter5boundaries", type: "s8/s8", dir: "Read · boiler → thermostat", unit: "", concl: "Upper/lower bounds for remote parameter 5." },
  /*  53 */ { name: "Remote parameter 6 boundaries", spec: "Remoteparameter6boundaries", type: "s8/s8", dir: "Read · boiler → thermostat", unit: "", concl: "Upper/lower bounds for remote parameter 6." },
  /*  54 */ { name: "Remote parameter 7 boundaries", spec: "Remoteparameter7boundaries", type: "s8/s8", dir: "Read · boiler → thermostat", unit: "", concl: "Upper/lower bounds for remote parameter 7." },
  /*  55 */ { name: "Remote parameter 8 boundaries", spec: "Remoteparameter8boundaries", type: "s8/s8", dir: "Read · boiler → thermostat", unit: "", concl: "Upper/lower bounds for remote parameter 8." },
  /*  56 */ { name: "DHW setpoint", spec: "TdhwSet", type: "f8.8", dir: "Read / Write", unit: "°C", concl: "Domestic-hot-water setpoint." },
  /*  57 */ { name: "Max CH water setpoint", spec: "MaxTSet", type: "f8.8", dir: "Read / Write", unit: "°C", concl: "Maximum central-heating water setpoint." },
  /*  58 */ { name: "OTC heat curve ratio", spec: "Hcratio", type: "f8.8", dir: "Read / Write", unit: "°C", concl: "Outside-temperature-compensation (weather) heat-curve ratio." },
  /*  59 */ { name: "Remote parameter 4", spec: "Remoteparameter4", type: "f8.8", dir: "Read / Write", unit: "", concl: "Value of remote parameter 4." },
  /*  60 */ { name: "Remote parameter 5", spec: "Remoteparameter5", type: "f8.8", dir: "Read / Write", unit: "", concl: "Value of remote parameter 5." },
  /*  61 */ { name: "Remote parameter 6", spec: "Remoteparameter6", type: "f8.8", dir: "Read / Write", unit: "", concl: "Value of remote parameter 6." },
  /*  62 */ { name: "Remote parameter 7", spec: "Remoteparameter7", type: "f8.8", dir: "Read / Write", unit: "", concl: "Value of remote parameter 7." },
  /*  63 */ { name: "Remote parameter 8", spec: "Remoteparameter8", type: "f8.8", dir: "Read / Write", unit: "", concl: "Value of remote parameter 8." },
  /*  64 */ { name: "Reserved", spec: "", type: "—", dir: "Undefined", unit: "", concl: "" },
  /*  65 */ { name: "Reserved", spec: "", type: "—", dir: "Undefined", unit: "", concl: "" },
  /*  66 */ { name: "Reserved", spec: "", type: "—", dir: "Undefined", unit: "", concl: "" },
  /*  67 */ { name: "Reserved", spec: "", type: "—", dir: "Undefined", unit: "", concl: "" },
  /*  68 */ { name: "Reserved", spec: "", type: "—", dir: "Undefined", unit: "", concl: "" },
  /*  69 */ { name: "Reserved", spec: "", type: "—", dir: "Undefined", unit: "", concl: "" },
  /*  70 */ { name: "Status Ventilation/Heat recovery", spec: "StatusVH", type: "flag8/flag8", dir: "Read · boiler → thermostat", unit: "", concl: "Master/slave status flags for the ventilation/heat-recovery unit." },
  /*  71 */ { name: "Control setpoint V/H", spec: "ControlSetpointVH", type: "u8", dir: "Write · thermostat → boiler", unit: "%", concl: "Commanded ventilation/heat-recovery setpoint." },
  /*  72 */ { name: "Application-specific Fault Flags/Code V/H", spec: "ASFFaultCodeVH", type: "flag8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Ventilation/heat-recovery fault flags and OEM fault code." },
  /*  73 */ { name: "Diagnostic code V/H", spec: "DiagnosticCodeVH", type: "u16", dir: "Read · boiler → thermostat", unit: "", concl: "Ventilation/heat-recovery diagnostic code." },
  /*  74 */ { name: "Config/Member ID V/H", spec: "ConfigMemberIDVH", type: "flag8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Ventilation/heat-recovery configuration flags and member ID." },
  /*  75 */ { name: "OpenTherm version V/H", spec: "OpenthermVersionVH", type: "f8.8", dir: "Read · boiler → thermostat", unit: "", concl: "OpenTherm protocol version of the ventilation/heat-recovery unit." },
  /*  76 */ { name: "Product version & type V/H", spec: "VersionTypeVH", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Product version and type of the ventilation/heat-recovery unit." },
  /*  77 */ { name: "Relative ventilation", spec: "RelativeVentilation", type: "u8", dir: "Read · boiler → thermostat", unit: "%", concl: "Actual relative ventilation level." },
  /*  78 */ { name: "Relative humidity exhaust air", spec: "RelativeHumidityExhaustAir", type: "u8", dir: "Read / Write", unit: "%", concl: "Relative humidity of the exhaust air." },
  /*  79 */ { name: "CO2 level exhaust air", spec: "CO2LevelExhaustAir", type: "u16", dir: "Read / Write", unit: "ppm", concl: "CO2 concentration of the exhaust air." },
  /*  80 */ { name: "Supply inlet temperature", spec: "SupplyInletTemperature", type: "f8.8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Ventilation supply-air inlet temperature." },
  /*  81 */ { name: "Supply outlet temperature", spec: "SupplyOutletTemperature", type: "f8.8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Ventilation supply-air outlet temperature." },
  /*  82 */ { name: "Exhaust inlet temperature", spec: "ExhaustInletTemperature", type: "f8.8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Ventilation exhaust-air inlet temperature." },
  /*  83 */ { name: "Exhaust outlet temperature", spec: "ExhaustOutletTemperature", type: "f8.8", dir: "Read · boiler → thermostat", unit: "°C", concl: "Ventilation exhaust-air outlet temperature." },
  /*  84 */ { name: "Actual exhaust fan speed", spec: "ActualExhaustFanSpeed", type: "u16", dir: "Read · boiler → thermostat", unit: "rpm", concl: "Measured exhaust fan speed." },
  /*  85 */ { name: "Actual supply fan speed", spec: "ActualSupplyFanSpeed", type: "u16", dir: "Read · boiler → thermostat", unit: "rpm", concl: "Measured supply fan speed." },
  /*  86 */ { name: "Remote Parameter Setting V/H", spec: "RemoteParameterSettingVH", type: "flag8/flag8", dir: "Read · boiler → thermostat", unit: "", concl: "Transfer-enable & read/write flags for the V/H remote parameters." },
  /*  87 */ { name: "Nominal Ventilation Value", spec: "NominalVentilationValue", type: "u8", dir: "Read / Write", unit: "%", concl: "Nominal ventilation level setting." },
  /*  88 */ { name: "TSP Number V/H", spec: "TSPNumberVH", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Number of transparent slave parameters for the V/H unit." },
  /*  89 */ { name: "TSP setting V/H", spec: "TSPEntryVH", type: "u8/u8", dir: "Read / Write", unit: "", concl: "Reads or writes one V/H transparent slave parameter." },
  /*  90 */ { name: "Fault Buffer Size V/H", spec: "FaultBufferSizeVH", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Size of the V/H fault-history buffer." },
  /*  91 */ { name: "Fault Buffer Entry V/H", spec: "FaultBufferEntryVH", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Reads one V/H fault-history buffer entry." },
  /*  92 */ { name: "Reserved", spec: "", type: "—", dir: "Undefined", unit: "", concl: "" },
  /*  93 */ { name: "Boiler brand name (index/char)", spec: "Brand", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Boiler brand name, transferred one character at a time by index." },
  /*  94 */ { name: "Boiler brand version (index/char)", spec: "BrandVersion", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Boiler brand/version string, transferred one character at a time." },
  /*  95 */ { name: "Boiler brand serial number (index/char)", spec: "BrandSerialNumber", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Boiler serial number, transferred one character at a time." },
  /*  96 */ { name: "Cooling operation hours", spec: "CoolingOperationHours", type: "u16", dir: "Read / Write", unit: "hrs", concl: "Cumulative hours spent in cooling mode." },
  /*  97 */ { name: "Power cycles", spec: "PowerCycles", type: "u16", dir: "Read / Write", unit: "", concl: "Number of boiler power cycles." },
  /*  98 */ { name: "RF sensor status information", spec: "RFstrengthbatterylevel", type: "special", dir: "Write · thermostat → boiler", unit: "", concl: "RF sensor signal strength and battery level." },
  /*  99 */ { name: "Remote Override Operating Mode (Heating/DHW)", spec: "OperatingMode_HC1_HC2_DHW", type: "special", dir: "Read / Write", unit: "", concl: "Remote override of the heating/DHW operating mode; 0 means no override is active." },
  /* 100 */ { name: "Function of manual and program changes in master and remote room setpoint.", spec: "RoomRemoteOverrideFunction", type: "flag8", dir: "Read · boiler → thermostat", unit: "", concl: "Defines whether manual and program room-setpoint changes may overrule an active remote override." },
  /* 101 */ { name: "Solar Storage Master mode", spec: "SolarStorageMaster", type: "flag8/flag8", dir: "Read · boiler → thermostat", unit: "", concl: "Master/slave status flags for the solar-storage unit." },
  /* 102 */ { name: "Solar Storage Application-specific flags and OEM fault", spec: "SolarStorageASFflags", type: "flag8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Solar-storage application-specific fault flags and OEM fault code." },
  /* 103 */ { name: "Solar Storage Slave Config / Member ID", spec: "SolarStorageSlaveConfigMemberIDcode", type: "flag8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Solar-storage configuration flags and member ID." },
  /* 104 */ { name: "Solar Storage product version number and type", spec: "SolarStorageVersionType", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Solar-storage product version and type." },
  /* 105 */ { name: "Solar Storage Number of Transparent-Slave-Parameters supported", spec: "SolarStorageTSP", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Number of solar-storage transparent slave parameters." },
  /* 106 */ { name: "Solar Storage Index number / Value of referred-to transparent slave parameter", spec: "SolarStorageTSPindexTSPvalue", type: "u8/u8", dir: "Read / Write", unit: "", concl: "Reads or writes one solar-storage transparent slave parameter by index." },
  /* 107 */ { name: "Solar Storage Size of Fault-History-Buffer supported by slave", spec: "SolarStorageFHBsize", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Size of the solar-storage fault-history buffer." },
  /* 108 */ { name: "Solar Storage Index number / Value of referred-to fault-history buffer entry", spec: "SolarStorageFHBindexFHBvalue", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Reads one solar-storage fault-history buffer entry by index." },
  /* 109 */ { name: "Electricity producer starts", spec: "ElectricityProducerStarts", type: "u16", dir: "Read / Write", unit: "", concl: "Number of micro-CHP electricity-producer starts." },
  /* 110 */ { name: "Electricity producer hours", spec: "ElectricityProducerHours", type: "u16", dir: "Read / Write", unit: "", concl: "Operating hours of the electricity producer." },
  /* 111 */ { name: "Electricity production", spec: "ElectricityProduction", type: "u16", dir: "Read · boiler → thermostat", unit: "", concl: "Instantaneous electricity production." },
  /* 112 */ { name: "Cumulative Electricity production", spec: "CumulativeElectricityProduction", type: "u16", dir: "Read / Write", unit: "", concl: "Cumulative electricity produced." },
  /* 113 */ { name: "Unsuccessful burner starts", spec: "BurnerUnsuccessfulStarts", type: "u16", dir: "Read / Write", unit: "", concl: "Count of failed burner ignition attempts." },
  /* 114 */ { name: "Flame signal too low count", spec: "FlameSignalTooLow", type: "u16", dir: "Read / Write", unit: "", concl: "Count of low-flame-signal events." },
  /* 115 */ { name: "OEM-specific diagnostic/service code", spec: "OEMDiagnosticCode", type: "u16", dir: "Read · boiler → thermostat", unit: "", concl: "Manufacturer-specific diagnostic/service code." },
  /* 116 */ { name: "Burner starts", spec: "BurnerStarts", type: "u16", dir: "Read / Write", unit: "", concl: "Total number of burner ignitions." },
  /* 117 */ { name: "CH pump starts", spec: "CHPumpStarts", type: "u16", dir: "Read / Write", unit: "", concl: "Total number of CH pump starts." },
  /* 118 */ { name: "DHW pump/valve starts", spec: "DHWPumpValveStarts", type: "u16", dir: "Read / Write", unit: "", concl: "Total number of DHW pump/valve starts." },
  /* 119 */ { name: "DHW burner starts", spec: "DHWBurnerStarts", type: "u16", dir: "Read / Write", unit: "", concl: "Number of burner starts while in DHW mode." },
  /* 120 */ { name: "Burner operation hours", spec: "BurnerOperationHours", type: "u16", dir: "Read / Write", unit: "hrs", concl: "Total hours the burner has run (flame on)." },
  /* 121 */ { name: "CH pump operation hours", spec: "CHPumpOperationHours", type: "u16", dir: "Read / Write", unit: "hrs", concl: "Total hours the CH pump has run." },
  /* 122 */ { name: "DHW pump/valve operation hours", spec: "DHWPumpValveOperationHours", type: "u16", dir: "Read / Write", unit: "hrs", concl: "Total hours the DHW pump has run or the DHW valve has been open." },
  /* 123 */ { name: "DHW burner operation hours", spec: "DHWBurnerOperationHours", type: "u16", dir: "Read / Write", unit: "hrs", concl: "Total burner hours while in DHW mode." },
  /* 124 */ { name: "Master Version OpenTherm Protocol Specification", spec: "OpenThermVersionMaster", type: "f8.8", dir: "Write · thermostat → boiler", unit: "", concl: "OpenTherm protocol version implemented by the thermostat (master)." },
  /* 125 */ { name: "Slave Version OpenTherm Protocol Specification", spec: "OpenThermVersionSlave", type: "f8.8", dir: "Read · boiler → thermostat", unit: "", concl: "OpenTherm protocol version implemented by the boiler (slave)." },
  /* 126 */ { name: "Master product version number and type", spec: "MasterVersion", type: "u8/u8", dir: "Write · thermostat → boiler", unit: "", concl: "Thermostat (master) product version number and type." },
  /* 127 */ { name: "Slave product version number and type", spec: "SlaveVersion", type: "u8/u8", dir: "Read · boiler → thermostat", unit: "", concl: "Boiler (slave) product version number and type." }
  ];
  var supPinned = null;   // msgID currently shown in the detail panel
  function renderSupport() {
    var grid = document.getElementById('supMatrix'); if (!grid) return;
    if (grid.childElementCount !== 128) {
      grid.innerHTML = '';
      for (var i = 0; i < 128; i++) {
        var c = document.createElement('div'); c.className = 'mcellq'; c.dataset.id = i; c.textContent = i;
        c.addEventListener('click', (function (id) { return function () { showSupportDetail(id); renderSupport(); }; })(i));
        c.addEventListener('mouseenter', (function (id) { return function (e) { showSupTip(id, e); }; })(i));
        c.addEventListener('mousemove', (function (id) { return function (e) { showSupTip(id, e); }; })(i));
        c.addEventListener('mouseleave', hideSupTip);
        grid.appendChild(c);
      }
      // Pin a default cell so the detail panel is never blank (mockup pins one on load).
      showSupportDetail(supPinned == null ? 0 : supPinned);
    }
    var seen = 0;
    for (var id = 0; id < 128; id++) {
      var cell = grid.children[id], s = stats[id];
      cell.className = 'mcellq' + (s ? (s.dirT && s.dirB ? ' both' : (s.dirT ? ' tonly' : ' bonly')) : '') + (id === supPinned ? ' sel' : '');
      if (s) seen++;
    }
    var sc = document.getElementById('supCount'); if (sc) sc.textContent = seen;
    // refresh the pinned panel so observed value/count track the live bus
    if (supPinned != null) showSupportDetail(supPinned);
  }
  // Support state of a msgID: who uses it on THIS bus (colours the badge + panel).
  function supState(id) {
    var s = stats[id];
    if (!s) return { cls: 'off', label: 'Not seen', sv: 'var(--tile-off)' };
    if (s.dirT && s.dirB) return { cls: 'both', label: 'Both sides', sv: 'var(--zone-ok)' };
    if (s.dirT) return { cls: 'tonly', label: 'Thermostat only', sv: 'var(--accent)' };
    return { cls: 'bonly', label: 'Boiler only', sv: 'var(--zone-warn)' };
  }
  var SUP_BADGE = { both: '✓', tonly: '↑', bonly: '↓', off: '○' };
  function supConcl(id, st) {
    var m = (typeof OTMSG !== 'undefined' && OTMSG[id]) ? OTMSG[id] : null;
    if (st.cls === 'both') return 'Both the thermostat and the boiler use this message.';
    if (st.cls === 'tonly') return 'The thermostat asks for this, but the boiler never answers — your boiler may not implement it.';
    if (st.cls === 'bonly') return 'The boiler reports this, but the thermostat never asks for it.';
    if (m && m.spec === '') return 'Reserved / undefined in the OpenTherm spec — not used.';
    return (m && m.concl) ? (m.concl + ' (Not seen on your bus yet.)') : 'Not seen on your bus yet.';
  }
  function showSupportDetail(id) {
    var el = document.getElementById('supDetail'); if (!el) return;
    supPinned = id;
    var s = stats[id];
    var m = (typeof OTMSG !== 'undefined' && OTMSG[id]) ? OTMSG[id] : { name: 'Reserved', spec: '', type: '—', dir: 'Undefined', unit: '', concl: '' };
    var st = supState(id);
    el.style.setProperty('--s', st.sv);
    el.innerHTML = '';
    // top: decimal + hex + spec mnemonic chip
    var top = document.createElement('div'); top.className = 'sd-top';
    var num = document.createElement('span'); num.className = 'sd-num'; num.textContent = id; top.appendChild(num);
    var hex = document.createElement('span'); hex.className = 'sd-hex'; hex.textContent = '0x' + ('0' + id.toString(16).toUpperCase()).slice(-2); top.appendChild(hex);
    if (m.spec) { var sp = document.createElement('span'); sp.className = 'sd-spec'; sp.textContent = m.spec; top.appendChild(sp); }
    el.appendChild(top);
    // human name
    var h5 = document.createElement('h5'); h5.textContent = m.name || ('ID ' + id); el.appendChild(h5);
    // coloured support badge — after the name, matching the mockup detail order
    var badge = document.createElement('div'); badge.className = 'sd-badge'; badge.textContent = (SUP_BADGE[st.cls] || '') + ' ' + st.label; el.appendChild(badge);
    // spec + observed fields
    var dl = document.createElement('dl');
    var rows = [['Data type', m.type || '—'], ['Direction', m.dir || '—']];
    if (m.unit) rows.push(['Unit', m.unit]);
    if (s) {
      rows.push(['Value', s.value || '—']);
      rows.push(['Seen', s.count + '×' + (s.interval ? ' · ~' + (s.interval / 1000).toFixed(1) + 's' : '')]);
    }
    rows.forEach(function (kv) { var dt = document.createElement('dt'); dt.textContent = kv[0]; var dd = document.createElement('dd'); dd.textContent = kv[1]; dl.appendChild(dt); dl.appendChild(dd); });
    el.appendChild(dl);
    // plain-language conclusion
    var concl = document.createElement('div'); concl.className = 'sd-concl'; concl.textContent = supConcl(id, st); el.appendChild(concl);
    // provenance footnote — mockup closes every detail render with this
    var hint = document.createElement('div'); hint.className = 'sd-hint'; hint.textContent = 'From the OpenTherm spec table + your bus traffic this session.'; el.appendChild(hint);
  }

  // ---------- Monitor > Log ----------
  var LOG_MAX = 600, logBuf = [], logRecvTimes = [];
  var logPaused = false, logSearch = '', logShowTs = true, logAutoScroll = true;
  // TASK-970: SAT-only filter, auto-download timer, stream-to-file writer
  var logSatOnly = false, logAutoDlTimer = null, logStreamWriter = null;
  var SAT_LINE_RE = /^\d{2}:\d{2}:\d{2}\s+S\s/;   // "HH:MM:SS S ..." SAT narration prefix
  function pushLog(line) {
    // WS frames arrive with a trailing "\r\n"; strip it so renderLog's join('\n')
    // produces ONE newline per frame (dense) instead of frame-newline + join-newline
    // = a blank line between every frame. Also keeps the stream-to-file output clean.
    line = ('' + line).replace(/[\r\n]+$/, '');
    var now = Date.now();
    logRecvTimes.push(now);
    while (logRecvTimes.length && now - logRecvTimes[0] > 10000) logRecvTimes.shift();
    // Stream to file captures every frame regardless of the display Pause.
    if (logStreamWriter) { try { logStreamWriter.write(line + '\n'); } catch (e) { } }
    if (logPaused) return;
    logBuf.push(line);
    if (logBuf.length > LOG_MAX) logBuf.shift();
    if (isMonitorLogVisible()) renderLog();
  }
  function isMonitorLogVisible() {
    var p = document.getElementById('page-monitor');
    var l = document.getElementById('mlog');
    return p && l && p.classList.contains('active') && l.classList.contains('active');
  }
  function renderLog() {
    var el = document.getElementById('monLog'); if (!el) return;
    var q = logSearch.trim().toLowerCase();
    var lines = logBuf;
    if (q) lines = lines.filter(function (l) { return l.toLowerCase().indexOf(q) !== -1; });
    if (logSatOnly) lines = lines.filter(function (l) { return SAT_LINE_RE.test(l); });   // TASK-970
    if (!logShowTs) lines = lines.map(function (l) { return l.replace(/^\d{2}:\d{2}:\d{2}\.\d{3,6}\s+/, ''); });
    el.textContent = lines.join('\n');     // textContent: device data, not HTML
    if (logAutoScroll) el.scrollTop = el.scrollHeight;
    var cnt = document.getElementById('logCount'); if (cnt) cnt.textContent = logBuf.length;
    var rate = document.getElementById('logRate'); if (rate) rate.textContent = (logRecvTimes.length / 10).toFixed(1);
  }

  function connectWs() {
    var url = (location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + WS_PATH;
    try {
      ws = new WebSocket(url);
      ws.onmessage = onWsMessage;
      ws.onclose = function () { ws = null; scheduleReconnect(); fetchSeed(); };
      ws.onerror = function () { try { if (ws) ws.close(); } catch (e) { } };
    } catch (e) { scheduleReconnect(); }
  }
  function scheduleReconnect() {
    if (wsTimer) return;
    wsTimer = setTimeout(function () { wsTimer = null; connectWs(); }, 5000);
  }

  // ---------- render scheduling ----------
  var renderPending = false;
  function scheduleRender() {
    if (renderPending) return;
    renderPending = true;
    requestAnimationFrame(function () { renderPending = false; renderActive(); });
  }
  function renderActive() {
    var d = document.querySelector('.cchip.active');
    var which = d ? d.dataset.design : 'a';
    if (which === 'a') renderA();
    else if (which === 'b') renderB();
    else if (which === 'c') renderC();
  }

  // SVG arc helper: point on a circle (deg, 0 = 12 o'clock, clockwise).
  function polar(cx, cy, r, deg) {
    var a = (deg - 90) * Math.PI / 180;
    return [cx + r * Math.cos(a), cy + r * Math.sin(a)];
  }
  function arcPath(cx, cy, r, startDeg, endDeg) {
    var s = polar(cx, cy, r, endDeg), e = polar(cx, cy, r, startDeg);
    var large = (endDeg - startDeg) <= 180 ? 0 : 1;
    return 'M' + s[0].toFixed(1) + ' ' + s[1].toFixed(1) +
      ' A' + r + ' ' + r + ' 0 ' + large + ' 0 ' + e[0].toFixed(1) + ' ' + e[1].toFixed(1);
  }

  function txt(id, s) { var el = document.getElementById(id); if (el) el.textContent = s; }
  function fmt(v, dp, suffix) {
    if (v === null || v === undefined || isNaN(v)) return '—';
    return v.toFixed(dp === undefined ? 1 : dp) + (suffix || '');
  }
  function setTile(id, on, fault, text) {
    var el = document.getElementById(id); if (!el) return;
    el.classList.toggle('on', !!on && !fault);
    el.classList.toggle('fault', !!fault);
    var v = el.querySelector('.val'); if (v) v.textContent = text;
  }

  // Plain-language status line (mockup hero headline): load + action, appliance-aware.
  function statusSentence(isHP) {
    var burn = isHP ? 'compressor' : 'flame';
    if (model.fault) return 'Fault reported — check the appliance display.';
    if (model.dhw_on) return 'Hot water running · tap open';
    if (model.flame) return 'Heating · ' + burn + ' on' + (model.mod !== null ? ' · ' + Math.round(model.mod) + '% modulation' : '');
    if (model.ch) return 'Heating idle · pump running, ' + burn + ' off';
    return 'Standby · everything quiet';
  }
  function renderA() {
    var isHP = effectiveSource() === 2;  // 2 = Heat Pump (electric → bolt + HP labels)
    var svg = document.getElementById('schem');
    if (svg) {
      svg.classList.toggle('flame-on', model.flame);
      svg.classList.toggle('ch-on', model.ch);
      svg.classList.toggle('dhw-on', model.dhw_on);
      svg.classList.toggle('fault', model.fault);
      // burner size tracks modulation: dramatic height (--fy 0.32..1.27) + a touch
      // of width (--fx) so a short bolt at 34% reads differently from a tall flame.
      var m = (model.mod === null) ? 0 : Math.max(0, Math.min(1, model.mod / 100));
      svg.style.setProperty('--fy', (0.32 + m * 0.95).toFixed(2));
      svg.style.setProperty('--fx', (0.70 + m * 0.32).toFixed(2));
    }
    txt('aFlow', fmt(model.flow, 1, '°'));
    txt('aRet', fmt(model.ret, 1, '°'));
    txt('aDhw', model.dhw === null ? '—' : Math.round(model.dhw) + '°');
    txt('aOut', fmt(model.outside, 1, '°'));
    txt('aRoom', fmt(model.room, 1, '°'));
    txt('aRoomSet', model.roomSet === null ? '' : 'set ' + fmt(model.roomSet, 1, '°'));
    txt('aModBig', model.mod === null ? '—' : Math.round(model.flame ? model.mod : 0) + '%');
    txt('aModTag', isHP ? 'COMPRESSOR' : 'MODULATION');
    // LCD context line: FAULT / DHW xx° / HP|CH xx% / HP|CH idle
    var lead = isHP ? 'HP' : 'CH';
    var lcd = model.fault ? 'FAULT'
            : model.dhw_on ? ('DHW ' + (model.dhw === null ? '--' : Math.round(model.dhw)) + '°')
            : model.flame ? (lead + ' ' + (model.mod === null ? '--' : Math.round(model.mod)) + '%')
            : (model.ch ? lead + ' idle' : 'STANDBY');
    txt('aLcd', lcd);
    txt('aPress', fmt(model.pressure, 2, ' bar'));
    if (model.flow !== null && model.ret !== null) txt('aDt', 'ΔT ' + fmt(model.flow - model.ret, 1, '°'));
    // radiator bars tint from the flow temperature (cool track → hot), but only
    // while CH is actually heating — residual boiler heat must not paint them red.
    var rads = document.querySelectorAll('#schem .rad-bar');
    if (rads.length) {
      var radFill = 'var(--gauge-track)';
      if (model.flow !== null && model.ch) {
        var radPct = Math.max(0, Math.min(100, (model.flow - 20) / 60 * 100));
        radFill = 'color-mix(in srgb, var(--hot) ' + radPct.toFixed(0) + '%, var(--gauge-track))';
      }
      for (var ri = 0; ri < rads.length; ri++) rads[ri].setAttribute('fill', radFill);
    }
    // pressure needle: 0..4 bar over the 180° top semicircle. The needle points
    // west at rotate(0); the arc runs west(low) → north(2 bar OK) → east(high), so
    // the angle is pressure/4*180 (matching the gauge geometry + the mockup).
    var needle = document.getElementById('aPressNeedle');
    if (needle && model.pressure !== null) {
      var deg = Math.max(0, Math.min(4, model.pressure)) / 4 * 180;
      needle.setAttribute('transform', 'rotate(' + deg.toFixed(1) + ',150,322)');
    }
    txt('aStatusTxt', statusSentence(isHP));
    var aStatus = document.getElementById('aStatus');
    if (aStatus) aStatus.classList.toggle('heating', model.flame);
    // first tile relabels to Compressor in heat-pump mode; tile values uppercase.
    var t0lbl = document.querySelector('#aTFlame .lbl'); if (t0lbl) t0lbl.textContent = isHP ? 'Compressor' : 'Flame';
    setTile('aTFlame', model.flame, false, model.flame ? 'ON' : 'OFF');
    setTile('aTCH', model.ch, false, model.ch ? 'ON' : 'OFF');
    setTile('aTDHW', model.dhw_on, false, model.dhw_on ? 'ON' : 'OFF');
    setTile('aTFault', false, model.fault, model.fault ? 'FAULT' : 'OK');
    // mobile chip strip (≤719px) — same six values as the schematic
    var strip = document.getElementById('aStrip');
    if (strip) {
      var chips = [['Flow', fmt(model.flow, 1, '°'), 'var(--hot)'], ['Return', fmt(model.ret, 1, '°'), 'var(--cold)'], ['DHW', model.dhw === null ? '—' : Math.round(model.dhw) + '°', ''], ['Pressure', fmt(model.pressure, 2, ' bar'), ''], ['Room', fmt(model.room, 1, '°'), ''], ['Outside', fmt(model.outside, 1, '°'), '']];
      strip.textContent = '';
      chips.forEach(function (c) {
        var ch = document.createElement('span'); ch.className = 'mchip';
        var bb = document.createElement('b'); bb.textContent = c[1]; if (c[2]) bb.style.color = c[2];
        ch.appendChild(document.createTextNode(c[0] + ' ')); ch.appendChild(bb);
        strip.appendChild(ch);
      });
    }
  }

  // Concept B — at a glance (hero dial + stat cards).
  var B_LO = 135, B_HI = 405, B_MIN = 5, B_MAX = 30;  // 270° sweep, 5..30 °C
  function renderB() {
    var track = document.getElementById('bTrack');
    if (track && !track.getAttribute('data-init')) {
      track.setAttribute('d', arcPath(120, 120, 100, B_LO, B_HI));
      track.setAttribute('data-init', '1');
    }
    // Mockup semantics: the coloured arc fills to the SETPOINT (the target), the
    // tick marks the measured ROOM temperature.
    var arc = document.getElementById('bArc');
    if (arc) {
      if (model.roomSet === null) arc.setAttribute('d', '');
      else {
        var f = Math.max(0, Math.min(1, (model.roomSet - B_MIN) / (B_MAX - B_MIN)));
        arc.setAttribute('d', arcPath(120, 120, 100, B_LO, B_LO + (B_HI - B_LO) * f));
      }
    }
    var tick = document.getElementById('bTick');
    if (tick) {
      if (model.room === null) { tick.setAttribute('x1', 0); tick.setAttribute('y1', 0); tick.setAttribute('x2', 0); tick.setAttribute('y2', 0); }
      else {
        var sf = Math.max(0, Math.min(1, (model.room - B_MIN) / (B_MAX - B_MIN)));
        var deg = B_LO + (B_HI - B_LO) * sf, p1 = polar(120, 120, 88, deg), p2 = polar(120, 120, 108, deg);
        tick.setAttribute('x1', p1[0].toFixed(1)); tick.setAttribute('y1', p1[1].toFixed(1));
        tick.setAttribute('x2', p2[0].toFixed(1)); tick.setAttribute('y2', p2[1].toFixed(1));
      }
    }
    txt('bRoom', fmt(model.room, 1, '°'));
    txt('bSet', 'target ' + (model.roomSet === null ? '—' : fmt(model.roomSet, 1, '°')));
    txt('bFlow', fmt(model.flow, 1, '°'));
    txt('bRet', fmt(model.ret, 1, '°'));
    txt('bDt', 'ΔT ' + (model.flow !== null && model.ret !== null ? fmt(model.flow - model.ret, 1, '°') : '—'));
    txt('bPress', fmt(model.pressure, 2, ' bar'));
    txt('bMod', model.mod === null ? '—' : Math.round(model.mod) + '%');
    txt('bDhw', model.dhw === null ? '—' : Math.round(model.dhw) + '°');
    txt('bDhwState', model.dhw_on ? 'running' : 'standby');
    txt('bOut', 'Outside ' + fmt(model.outside, 1, '°') + ' · OTGW firmware');
    var fill = document.getElementById('bModFill');
    if (fill) fill.style.height = (model.mod === null ? 0 : Math.max(0, Math.min(100, model.mod))) + '%';
    var dot = document.getElementById('bPressDot');
    if (dot && model.pressure !== null) dot.style.left = Math.max(0, Math.min(100, model.pressure / 4 * 100)) + '%';
    var flame = document.getElementById('bFlame'); if (flame) flame.classList.toggle('on', model.flame);
    var st = document.getElementById('bStatus'); if (st) st.classList.toggle('heating', model.flame);
    // mockup reuses the rich status sentence here (surfaces fault + modulation),
    // not a reduced 4-state string that hides a boiler fault in Concept B.
    txt('bStatusTxt', statusSentence(effectiveSource() === 2));
  }

  // Concept C — mission control: live strip chart + metric cells + frame ticker.
  // strip svg: viewBox 0 0 780 210, plot x 34..772, temp axis y = 170-(T-10)*2
  // (T=10->170, 50->90, 90->10).
  var C_X0 = 34, C_X1 = 772;
  function tempY(t) { return 170 - (t - 10) * 2; }
  function seriesPoints(key) {
    if (samples.length < 2) return '';
    var n = samples.length, span = C_X1 - C_X0, pts = [];
    for (var i = 0; i < n; i++) {
      var v = samples[i][key];
      if (v === null || v === undefined || isNaN(v)) continue;
      var x = C_X0 + (n === 1 ? 0 : (i / (n - 1)) * span);
      pts.push(x.toFixed(1) + ',' + tempY(v).toFixed(1));
    }
    return pts.join(' ');
  }
  function setPoints(id, key) { var el = document.getElementById(id); if (el) el.setAttribute('points', seriesPoints(key)); }
  function renderC() {
    setPoints('cFlow', 'flow');
    setPoints('cRet', 'ret');
    // setpoint: horizontal line at the latest chSet
    var sp = document.getElementById('cSpLine');
    if (sp) {
      if (model.chSet === null) { sp.setAttribute('y1', -10); sp.setAttribute('y2', -10); }
      else { var y = tempY(model.chSet).toFixed(1); sp.setAttribute('y1', y); sp.setAttribute('y2', y); }
    }
    // modulation: filled area along the bottom (0% at y=170, 100% at y=130)
    var mod = document.getElementById('cMod');
    if (mod) {
      if (samples.length < 2) mod.setAttribute('points', '');
      else {
        var n = samples.length, span = C_X1 - C_X0, p = [C_X0 + ',170'];
        for (var i = 0; i < n; i++) {
          var m = samples[i].mod; if (m === null || m === undefined || isNaN(m)) m = 0;
          var x = C_X0 + (i / (n - 1)) * span;
          p.push(x.toFixed(1) + ',' + (170 - Math.max(0, Math.min(100, m)) * 0.4).toFixed(1));
        }
        p.push(C_X1 + ',170');
        mod.setAttribute('points', p.join(' '));
      }
    }
    txt('cFlowLbl', model.flow === null ? '' : 'flow ' + fmt(model.flow, 1, '°'));
    txt('cRetLbl', model.ret === null ? '' : 'ret ' + fmt(model.ret, 1, '°'));
    renderCGrid();
    renderTicker();
  }
  // Pressure severity (5-band, HA-style): green around the 2.0 optimum.
  function pressClass(p) {
    if (p === null || p === undefined || isNaN(p)) return '';
    if (p < 1.0 || p >= 3.0) return 'alert';
    if (p < 1.2 || p >= 2.5) return 'warn';
    return 'ok';
  }
  function renderCGrid() {
    var grid = document.getElementById('cGrid'); if (!grid) return;
    var cells = [
      ['FLOW', fmt(model.flow, 1, '°'), 'hot'],
      ['RETURN', fmt(model.ret, 1, '°'), 'cold'],
      ['ΔT', (model.flow !== null && model.ret !== null) ? fmt(model.flow - model.ret, 1, '°') : '—', ''],
      ['T ROOM', fmt(model.room, 1, '°'), ''],
      ['ROOM SET', fmt(model.roomSet, 1, '°'), ''],
      ['T OUTSIDE', fmt(model.outside, 1, '°'), ''],
      ['MODULATION', model.mod === null ? '—' : Math.round(model.mod) + '%', model.flame ? 'ok' : ''],
      ['CH SETPOINT', fmt(model.chSet, 1, '°'), ''],
      ['DHW', model.dhw === null ? '—' : fmt(model.dhw, 1, '°'), ''],
      ['PRESSURE', fmt(model.pressure, 2, ' bar'), pressClass(model.pressure)],
      ['FLAME', model.flame ? 'ON' : 'OFF', model.flame ? 'ok' : ''],
      ['STATUS', model.fault ? 'FAULT' : 'OK', model.fault ? 'alert' : 'ok']
    ];
    // Build with textContent to avoid HTML injection from values.
    grid.innerHTML = '';
    cells.forEach(function (c) {
      var d = document.createElement('div'); d.className = 'mc-cell';
      var l = document.createElement('div'); l.className = 'lbl'; l.textContent = c[0];
      var v = document.createElement('div'); v.className = 'val ' + (c[2] || ''); v.textContent = c[1];
      d.appendChild(l); d.appendChild(v); grid.appendChild(d);
    });
  }
  function renderTicker() {
    var t = document.getElementById('cTicker'); if (!t) return;
    // One block line per frame, coloured via DOM spans (textContent on each node —
    // raw frames are device data, never innerHTML). Grey timestamp (.ts), request
    // T/R in blue (.t), answer B/A in green (.b) — the mockup's ticker palette.
    t.textContent = '';
    ticker.forEach(function (e) {
      var ln = document.createElement('div');
      if (typeof e === 'string') { ln.textContent = e; t.appendChild(ln); return; }
      if (e.ts) { var sp = document.createElement('span'); sp.className = 'ts'; sp.textContent = e.ts + ' '; ln.appendChild(sp); }
      var bd = document.createElement('span');
      bd.className = (e.dir === 'B' || e.dir === 'A') ? 'b' : (e.dir === 'T' || e.dir === 'R') ? 't' : '';
      bd.textContent = e.body;
      ln.appendChild(bd);
      t.appendChild(ln);
    });
    t.scrollTop = t.scrollHeight;
  }

  // ---------- Monitor > Graph (same strip chart, g-prefixed ids) ----------
  function setG(id, key) { var el = document.getElementById(id); if (el) el.setAttribute('points', seriesPoints(key)); }
  function windowedSamples() {
    if (!graphWindowMs) return samples;
    var cutoff = Date.now() - graphWindowMs;
    return samples.filter(function (s) { return !s.t || s.t >= cutoff; });
  }
  function winSeries(arr, key) {
    if (arr.length < 2) return '';
    var span = C_X1 - C_X0, pts = [];
    for (var i = 0; i < arr.length; i++) {
      var v = arr[i][key]; if (v === null || v === undefined || isNaN(v)) continue;
      pts.push((C_X0 + (i / (arr.length - 1)) * span).toFixed(1) + ',' + tempY(v).toFixed(1));
    }
    return pts.join(' ');
  }
  function renderGraph() {
    var arr = windowedSamples();
    var ef = document.getElementById('gFlow2'); if (ef) ef.setAttribute('points', winSeries(arr, 'flow'));
    var er = document.getElementById('gRet2'); if (er) er.setAttribute('points', winSeries(arr, 'ret'));
    var sp = document.getElementById('gSpLine');
    if (sp) { var y = model.chSet === null ? -10 : tempY(model.chSet); sp.setAttribute('y1', y); sp.setAttribute('y2', y); }
    var mod = document.getElementById('gMod2');
    if (mod) {
      if (arr.length < 2) mod.setAttribute('points', '');
      else {
        var span = C_X1 - C_X0, p = [C_X0 + ',170'];
        for (var i = 0; i < arr.length; i++) {
          var m = arr[i].mod; if (m === null || m === undefined || isNaN(m)) m = 0;
          p.push((C_X0 + (i / (arr.length - 1)) * span).toFixed(1) + ',' + (170 - Math.max(0, Math.min(100, m)) * 0.4).toFixed(1));
        }
        p.push(C_X1 + ',170'); mod.setAttribute('points', p.join(' '));
      }
    }
  }

  // ---------- Monitor > Connection map + detail (live; from /health + settings) ----------
  function st5ok(b) { return b ? 'st-ok' : 'st-down'; }
  // OT-link state with recency (ADR-155 degraded/stale): connected + fresh -> ok;
  // connected but ageing past OT_STALE_S (approaching the firmware's 30 s window) ->
  // degraded (st-warn); not connected -> down. ageS null = no recency data.
  var OT_STALE_S = 20;
  function otLinkState(connected, ageS) {
    if (!connected) return 'st-down';
    if (ageS != null && ageS >= OT_STALE_S) return 'st-warn';
    return 'st-ok';
  }
  function fetchConn() {
    fetch(APIGW + 'v2/health').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      if (!j || !j.health) return;
      var h = j.health;
      connRssi = (typeof h.wifirssi === 'number' && h.wifirssi !== 0) ? h.wifirssi : null;
      var iface = h.otcommandinterface || '';
      var isPic = iface === 'PIC';
      // Two-link OT bus: on PIC use the independent thermostat/boiler flags; on
      // OT-Direct the firmware does not populate the sub-states, so both follow
      // bOnline (otgwconnected) — honest single-bus health on OT-Direct hardware.
      var thermOk = isPic ? !!h.thermostatconnected : !!h.otgwconnected;
      var boilerOk = isPic ? !!h.boilerconnected : !!h.otgwconnected;
      // Per-link recency (age in s from /health; -1 -> null = no data, e.g. OT-Direct).
      // On PIC a connected-but-ageing link degrades to st-warn; on OT-Direct we keep
      // the binary bOnline fallback (ADR-155 — split is presentational there).
      var thermAge = (typeof h.thermostat_age_s === 'number' && h.thermostat_age_s >= 0) ? h.thermostat_age_s : null;
      var boilerAge = (typeof h.boiler_age_s === 'number' && h.boiler_age_s >= 0) ? h.boiler_age_s : null;
      CONN.therm.s = isPic ? otLinkState(thermOk, thermAge) : st5ok(thermOk);
      CONN.boiler.s = isPic ? otLinkState(boilerOk, boilerAge) : st5ok(boilerOk);
      CONN.therm.seen = thermAge; CONN.boiler.seen = boilerAge;
      // OpenTherm command interface row (PIC link vs OT-Direct)
      CONN.ot.name = isPic ? 'PIC link' : (iface === 'OT-Direct' ? 'OT-Direct' : 'OpenTherm interface');
      CONN.ot.detail = iface || 'none';
      CONN.ot.s = (iface && iface !== 'None') ? 'st-ok' : 'st-down';
      // TASK-964: the OT-Direct override panel is meaningful only on OT-Direct
      // hardware (there is no direct bus to override behind a PIC gateway).
      var otdOvrEl = document.getElementById('otdOvr');
      if (otdOvrEl) {
        if (iface === 'OT-Direct') { otdOvrEl.style.display = ''; wireOtdOvrApply(); }
        else otdOvrEl.style.display = 'none';
      }
      // Gateway MODE (PIC only). otgwmode is absent on OT-Direct -> N/A.
      if (h.otgwmode === undefined) { CONN.mode.value = isPic ? 'detecting' : 'n/a'; }
      else { var m = ('' + h.otgwmode).toLowerCase(); CONN.mode.value = (m === 'on') ? 'gateway' : (m === 'off') ? 'monitor' : m; }
      // Wi-Fi health from RSSI
      CONN.wifi.detail = (h.networkmode || '') + (connRssi !== null ? ' · ' + connRssi + ' dBm' : '');
      CONN.wifi.s = h.networkmode ? (connRssi !== null && connRssi < -72 ? 'st-warn' : 'st-ok') : 'st-down';
      // MQTT five-state: connected -> ok; otherwise off when disabled in settings,
      // down when enabled-but-unreachable, unknown when the enabled-state has not
      // loaded yet (setData is empty until Settings is opened — never show a red
      // fault for a not-yet-known integration on the first Home render).
      var mqttKnown = Object.prototype.hasOwnProperty.call(setData, 'mqttenable');
      var mqttEnabled = !mqttKnown || ('' + setData.mqttenable.value) === 'true';
      CONN.mqtt.s = h.mqttconnected ? 'st-ok' : (!mqttKnown ? 'st-unknown' : (mqttEnabled ? 'st-down' : 'st-off'));
      CONN.mqtt.detail = h.mqttconnected ? 'connected' : (!mqttKnown ? 'checking…' : (mqttEnabled ? 'not connected' : 'disabled in settings'));
      // NTP from ntpenable
      var ntpEnabled = (h.ntpenable === undefined) ? true : !!h.ntpenable;
      CONN.ntp.s = !ntpEnabled ? 'st-off' : 'st-ok';
      CONN.ntp.detail = ntpEnabled ? 'time syncing' : 'disabled';
      // Live link (WS) + REST API — this browser session
      var wsUp = ws && ws.readyState === 1;
      CONN.ws.s = wsUp ? 'st-ok' : 'st-down';
      CONN.ws.detail = wsUp ? 'WebSocket · streaming' : 'WebSocket · reconnecting…';
      CONN.api.s = 'st-ok';
      renderConnMap(); renderConnStrip(); renderConnDetail();
    }).catch(function () { });
  }
  // ---------- Monitor > Connection > OT-Direct manual overrides (TASK-964) ----------
  function otdHex(v) { return '0x' + ('0000' + ((v >>> 0)).toString(16)).slice(-4).toUpperCase(); }
  function fetchOtdOvr() {
    fetch(APIGW + 'v2/otdirect/overrides').then(function (r) { return r.ok ? r.json() : null; })
      .then(function (j) { if (j && j.overrides) renderOtdOvr(j.overrides); }).catch(function () { });
  }
  function otdOvrRow(label, msgid, valueTxt, clearAction) {
    var row = document.createElement('div'); row.className = 'ble-row';
    var nm = document.createElement('div'); nm.className = 'ble-nm';
    nm.appendChild(document.createTextNode(label + ' · MsgID ' + msgid));
    if (valueTxt) { var v = document.createElement('span'); v.className = 'ble-mac'; v.textContent = valueTxt; nm.appendChild(v); }
    row.appendChild(nm);
    if (clearAction) {
      var ctr = document.createElement('div'); ctr.className = 'ble-ctrls';
      var btn = document.createElement('button'); btn.className = 'tbtn'; btn.textContent = 'Clear';
      btn.addEventListener('click', function () { otdOvrPost(clearAction, msgid); });
      ctr.appendChild(btn); row.appendChild(ctr);
    }
    return row;
  }
  function renderOtdOvr(ov) {
    var el = document.getElementById('otdOvrList'); if (!el) return;
    el.textContent = ''; var any = false;
    (ov.write || []).forEach(function (e) { any = true; el.appendChild(otdOvrRow('Write', e.msgid, otdHex(e.value), null)); });
    (ov.response || []).forEach(function (e) { any = true; el.appendChild(otdOvrRow('SR', e.msgid, otdHex(e.value), 'cr')); });
    (ov.modify || []).forEach(function (e) { any = true; el.appendChild(otdOvrRow('RM', e.msgid, otdHex(e.value), 'cm')); });
    (ov.unknown || []).forEach(function (id) { any = true; el.appendChild(otdOvrRow('UI', (id && id.msgid !== undefined) ? id.msgid : id, '', 'ki')); });
    if (!any) { var e = document.createElement('div'); e.className = 'ble-row'; e.style.color = 'var(--muted)'; e.textContent = 'No active overrides'; el.appendChild(e); }
  }
  function otdOvrPost(action, msgid, value) {
    var url = APIGW + 'v2/otdirect/overrides?action=' + action + '&msgid=' + encodeURIComponent(msgid);
    if ((action === 'sr' || action === 'rm') && value) url += '&value=' + encodeURIComponent(value);
    fetch(url, { method: 'POST', mode: 'cors' }).then(function (r) { return r.ok ? r.json() : null; })
      .then(function (j) { if (j && j.overrides) renderOtdOvr(j.overrides); else fetchOtdOvr(); }).catch(function () { });
  }
  function wireOtdOvrApply() {
    var btn = document.getElementById('otdOvrApply'); if (!btn || btn._wired) return; btn._wired = true;
    var sel = document.getElementById('otdOvrAction'), vi = document.getElementById('otdOvrValue');
    // Hex value only applies to the "set" actions (SR/RM); hide it for the rest.
    if (sel && vi) { var upd = function () { vi.style.display = (sel.value === 'sr' || sel.value === 'rm') ? '' : 'none'; }; sel.addEventListener('change', upd); upd(); }
    btn.addEventListener('click', function () {
      var a = sel ? sel.value : '', mi = document.getElementById('otdOvrMsgid');
      var m = mi ? mi.value : '';
      if (!a || m === '' || m == null) return;
      otdOvrPost(a, m, vi ? vi.value : '');
      if (mi) mi.value = ''; if (vi) vi.value = '';
    });
  }
  function connRecency(c) {
    if (c.s === 'st-off') return 'disabled';
    if (c.s === 'st-down') return 'no link';
    if (c.seen != null) return 'seen ' + c.seen + 's ago';
    return '';
  }
  function setLink(id, flowId, state) {
    var lk = document.getElementById(id);
    if (lk) lk.setAttribute('class', 'cn-link lk-' + state.replace('st-', ''));
    var fl = document.getElementById(flowId);
    if (fl) fl.classList.toggle('on', state === 'st-ok' || state === 'st-warn');
  }
  // Colour the SVG state dots directly (the .cn-statedot CSS has no fill rule, so
  // a class alone would leave them uncoloured — set fill from the state palette).
  function setDot(id, state) { var d = document.getElementById(id); if (d) d.setAttribute('fill', STV[state] || STV['st-unknown']); }
  function renderConnMap() {
    setLink('lk-therm', 'fl-therm', CONN.therm.s); setDot('dot-therm', CONN.therm.s);
    setLink('lk-boiler', 'fl-boiler', CONN.boiler.s); setDot('dot-boiler', CONN.boiler.s);
    setLink('lk-wifi', 'fl-wifi', CONN.wifi.s); setDot('dot-wifi', CONN.wifi.s);
    setLink('lk-mqtt', 'fl-mqtt', CONN.mqtt.s); setDot('dot-mqtt', CONN.mqtt.s);
    setLink('lk-ws', 'fl-ws', CONN.ws.s); setDot('dot-ws', CONN.ws.s);
    setDot('dot-pic', CONN.ot.s);
    function roleSub(role, c) { var r = connRecency(c); return r ? (role + ' · ' + r) : role; }
    var ts2 = document.getElementById('cnThermSub'); if (ts2) ts2.textContent = roleSub('master', CONN.therm);
    var bs = document.getElementById('cnBoilerSub'); if (bs) bs.textContent = roleSub('slave', CONN.boiler);
    var ps = document.getElementById('cnPicSub'); if (ps) ps.textContent = CONN.ot.name;
    var wf = document.getElementById('cnWifiSub'); if (wf) wf.textContent = connRssi !== null ? connRssi + ' dBm' : (CONN.wifi.detail || '—');
    var md = document.getElementById('cnModeTxt');
    if (md) md.textContent = CONN.mode.value === 'gateway' ? 'GATEWAY MODE' : CONN.mode.value === 'monitor' ? 'MONITOR MODE' : CONN.mode.value === 'n/a' ? 'NO PIC' : ('MODE: ' + (CONN.mode.value || '?').toUpperCase());
    var dm = document.getElementById('dot-mode'); if (dm) dm.setAttribute('fill', (CONN.mode.value === 'gateway' || CONN.mode.value === 'monitor') ? STV['st-mode'] : STV['st-unknown']);
    renderConnDetail();
  }

  // ---------- Settings (built from GET /api/v2/settings) ----------
  // Curated catalog (the mockup's SET_CATS, re-keyed to the LOWERCASE REST names
  // that GET /api/v2/settings actually emits — the mockup used the mixed-case
  // persisted spellings, which never bind by === against the GET keys). Rows show
  // human-readable labels + hints grouped into categories. Any exposed key NOT in
  // the catalog is still shown (categorised by prefix, label humanised) so nothing
  // the firmware exposes ever disappears from the page.
  var SET_CATS = [
    { id: 'system',   title: 'System',                 icon: '🖥', reboot: false, desc: 'Device identity and access.' },
    { id: 'network',  title: 'Network',                icon: '📶', reboot: true,  desc: 'Wi-Fi / Ethernet addressing. Changes apply after a reboot.' },
    { id: 'mqtt',     title: 'MQTT',                   icon: '📨', reboot: false, desc: 'Broker connection and Home Assistant publishing.' },
    { id: 'ntp',      title: 'Time / NTP',             icon: '🕐', reboot: false, desc: 'Clock synchronisation.' },
    { id: 'otgw',     title: 'OpenTherm Gateway',      icon: '🔥', reboot: false, desc: 'Commands sent to the PIC at boot.' },
    { id: 'otd',      title: 'OT-Direct',              icon: '🎚', reboot: false, desc: 'Direct OpenTherm control (OTGW32).' },
    { id: 'sensors',  title: 'Sensors',                icon: '🌡', reboot: false, desc: '1-Wire / GPIO temperature sensors and the BLE roster.' },
    { id: 'outputs',  title: 'GPIO Outputs',           icon: '🔌', reboot: false, desc: 'Drive a GPIO from OT state.' },
    { id: 's0',       title: 'S0 Pulse Counter',       icon: '⚡', reboot: false, desc: 'Energy pulse counting.' },
    { id: 'webhook',  title: 'Webhook',                icon: '🪝', reboot: false, desc: 'HTTP callbacks on OT state.' },
    { id: 'behavior', title: 'Behavior',               icon: '🔧', reboot: false, desc: 'LED, theme and restart behaviour.' },
    { id: 'ui',       title: 'User Interface',         icon: '🎛', reboot: false, desc: 'Web UI preferences.' },
    { id: 'sat',      title: 'SAT — Smart Thermostat', icon: '🧠', reboot: false, desc: 'Standalone Adaptive Thermostat.' }
  ];
  // lowercase REST key -> { cat, sub?, label, hint? }. Reboot is per-category.
  var SET_META = {
    hostname:        { cat: 'system', label: 'Hostname', hint: 'mDNS name on your network' },
    ssid:            { cat: 'system', label: 'Wi-Fi network', hint: 'Connected SSID (read-only)' },
    httppasswd:      { cat: 'system', label: 'Admin password', hint: 'Protects Settings / Advanced' },
    boardmode:       { cat: 'system', label: 'Board mode', hint: 'Hardware interface override' },
    wifistaticip:    { cat: 'network', sub: 'Wi-Fi', label: 'Static IP', hint: 'Leave empty for DHCP' },
    wifisubnet:      { cat: 'network', sub: 'Wi-Fi', label: 'Subnet mask' },
    wifigateway:     { cat: 'network', sub: 'Wi-Fi', label: 'Gateway' },
    wifidns1:        { cat: 'network', sub: 'Wi-Fi', label: 'DNS 1' },
    wifidns2:        { cat: 'network', sub: 'Wi-Fi', label: 'DNS 2', hint: 'Optional' },
    ethstaticip:     { cat: 'network', sub: 'Ethernet', label: 'Use static IP' },
    ethipaddress:    { cat: 'network', sub: 'Ethernet', label: 'IP address' },
    ethsubnet:       { cat: 'network', sub: 'Ethernet', label: 'Subnet mask' },
    ethgateway:      { cat: 'network', sub: 'Ethernet', label: 'Gateway' },
    ethdns:          { cat: 'network', sub: 'Ethernet', label: 'DNS' },
    mqttenable:      { cat: 'mqtt', sub: 'Connection', label: 'Enable MQTT' },
    mqttbroker:      { cat: 'mqtt', sub: 'Connection', label: 'Broker' },
    mqttbrokerport:  { cat: 'mqtt', sub: 'Connection', label: 'Port' },
    mqttuser:        { cat: 'mqtt', sub: 'Connection', label: 'Username' },
    mqttpasswd:      { cat: 'mqtt', sub: 'Connection', label: 'Password' },
    mqtttoptopic:    { cat: 'mqtt', sub: 'Connection', label: 'Top topic' },
    mqttuniqueid:    { cat: 'mqtt', sub: 'Connection', label: 'Unique id' },
    mqtthaprefix:    { cat: 'mqtt', sub: 'HA & publishing', label: 'HA discovery prefix' },
    mqttharebootdetection:  { cat: 'mqtt', sub: 'HA & publishing', label: 'HA reboot detection' },
    mqttonchangepublishing: { cat: 'mqtt', sub: 'HA & publishing', label: 'Publish on change' },
    mqttinterval:    { cat: 'mqtt', sub: 'HA & publishing', label: 'Publish interval', hint: 'Seconds' },
    mqttotmessage:   { cat: 'mqtt', sub: 'HA & publishing', label: 'Publish raw OT messages' },
    mqttseparatesources:   { cat: 'mqtt', sub: 'HA & publishing', label: 'Separate thermostat/boiler topics' },
    mqttuselegacyottopics: { cat: 'mqtt', sub: 'HA & publishing', label: 'Legacy OT topic layout' },
    legacyport25238enabled:{ cat: 'otgw', label: 'Legacy TCP port 25238' },
    ntpenable:       { cat: 'ntp', label: 'Enable NTP' },
    ntptimezone:     { cat: 'ntp', label: 'Timezone' },
    ntphostname:     { cat: 'ntp', label: 'NTP server' },
    ntpsendtime:     { cat: 'ntp', label: 'Send time to thermostat' },
    otgwcommandenable: { cat: 'otgw', label: 'Run commands at boot' },
    otgwcommands:    { cat: 'otgw', label: 'Boot commands', hint: 'Semicolon separated, e.g. PS=0;GW=1' },
    gpiosensorsenabled:   { cat: 'sensors', label: 'Enable sensors' },
    gpiosensorspin:       { cat: 'sensors', label: 'GPIO pin' },
    gpiosensorsinterval:  { cat: 'sensors', label: 'Poll interval', hint: 'Seconds' },
    gpiosensorslegacyformat: { cat: 'sensors', label: 'Legacy topic format' },
    gpiooutputsenabled:   { cat: 'outputs', label: 'Enable output' },
    gpiooutputspin:       { cat: 'outputs', label: 'GPIO pin' },
    gpiooutputstriggerbit:{ cat: 'outputs', label: 'Trigger on' },
    s0counterenabled:     { cat: 's0', label: 'Enable S0 counter' },
    s0counterpin:         { cat: 's0', label: 'GPIO pin' },
    s0counterpulsekw:     { cat: 's0', label: 'Pulses per kWh' },
    s0counterinterval:    { cat: 's0', label: 'Report interval', hint: 'Seconds' },
    s0counterdebouncetime:{ cat: 's0', label: 'Debounce time', hint: 'Milliseconds' },
    webhookenable:   { cat: 'webhook', label: 'Enable webhook' },
    webhooktriggerbit: { cat: 'webhook', label: 'Trigger on' },
    webhookurlon:    { cat: 'webhook', label: 'URL when ON' },
    webhookurloff:   { cat: 'webhook', label: 'URL when OFF' },
    webhookcontenttype: { cat: 'webhook', label: 'Content type' },
    webhookpayload:  { cat: 'webhook', label: 'Payload template' },
    ledblink:        { cat: 'behavior', label: 'Blink LED on OT traffic' },
    darktheme:       { cat: 'behavior', label: 'Dark theme by default' },
    nightlyrestart:  { cat: 'behavior', label: 'Nightly restart' },
    nightlyrestarthour: { cat: 'behavior', label: 'Restart hour', hint: '0–23' },
    ui_autoscroll:   { cat: 'ui', label: 'Log auto-scroll' },
    ui_timestamps:   { cat: 'ui', label: 'Show timestamps' },
    ui_capture:      { cat: 'ui', label: 'Large capture mode' },
    ui_autodownloadlog: { cat: 'ui', label: 'Auto-download log' },
    ui_autoexport:   { cat: 'ui', label: 'Auto-export CSV' },
    ui_autoscreenshot:  { cat: 'ui', label: 'Auto-save graph PNG' },
    ui_graphtimewindow: { cat: 'ui', label: 'Default graph window' },
    // Most-used SAT fields get curated labels; the long tail falls back to the
    // humanised key under the SAT category (Phase 2: port full SAT metadata).
    satenabled:      { cat: 'sat', label: 'SAT enabled' },
    satsystem:       { cat: 'sat', label: 'Heating system' },
    satsource:       { cat: 'sat', label: 'Appliance type' },
    sattargettemp:   { cat: 'sat', label: 'Target temperature', hint: '°C' },
    sattempstep:     { cat: 'sat', label: 'Setpoint step', hint: '°C' },
    satcoefficient:  { cat: 'sat', label: 'Heating curve slope' },
    satdeadband:     { cat: 'sat', label: 'Deadband', hint: '°C' },
    satheatingmode:  { cat: 'sat', label: 'Heating mode' },
    satmanufacturer: { cat: 'sat', label: 'Manufacturer' },
    satinterval:     { cat: 'sat', label: 'Update interval', hint: 'Seconds' },
    sathpcycle:      { cat: 'sat', label: 'Heat-pump min cycle', hint: 'Seconds (1800 = 2/hr, 2400 = 1.5/hr)' },
    otdmode:         { cat: 'otd', label: 'OT-Direct mode' },
    // OT-Direct heating-curve / PI room-comp + ventilation (TASK-933 Phase 2)
    otdchmode:           { cat: 'otd', label: 'CH mode' },
    otdflowtemp:         { cat: 'otd', label: 'Flow setpoint', hint: '°C' },
    otdflowmax:          { cat: 'otd', label: 'Max flow temperature', hint: '°C' },
    otdroomsetpoint:     { cat: 'otd', label: 'Room setpoint', hint: '°C' },
    otdgradient:         { cat: 'otd', label: 'Heat-curve gradient' },
    otdexponent:         { cat: 'otd', label: 'Curve exponent' },
    otdoffset:           { cat: 'otd', label: 'Curve offset', hint: '°C' },
    otdroomcomp:         { cat: 'otd', label: 'Room compensation' },
    otdkp:               { cat: 'otd', label: 'Kp (proportional)' },
    otdki:               { cat: 'otd', label: 'Ki (integral)' },
    otdkboost:           { cat: 'otd', label: 'K boost' },
    otdhysteresisenable: { cat: 'otd', label: 'Hysteresis enabled' },
    otdhysteresis:       { cat: 'otd', label: 'Hysteresis', hint: '°C' },
    otdventenable:       { cat: 'otd', label: 'Ventilation enabled' },
    otdopenbypass:       { cat: 'otd', label: 'Open bypass now' },
    otdautobypass:       { cat: 'otd', label: 'Auto bypass' },
    otdfreeventenable:   { cat: 'otd', label: 'Free ventilation' },
    otdventsetpoint:     { cat: 'otd', label: 'Ventilation setpoint', hint: '%' },
    otdhasbypassrelay:   { cat: 'otd', label: 'Bypass relay fitted' },
    // TASK-935: remaining OT-Direct config keys (were falling back to humanizeKey)
    otdautodetect:       { cat: 'otd', label: 'Auto-detect OTGW32 mode' },
    otdsetbacktemp:      { cat: 'otd', label: 'Setback temperature', hint: '°C' },
    otdsetbacktimeout:   { cat: 'otd', label: 'Setback timeout', hint: 'Minutes' },
    otdenableslave:      { cat: 'otd', label: 'Enable slave (thermostat) passthrough' },
    otdsummermode:       { cat: 'otd', label: 'Summer mode (CH off)' },
    otdfailsafe:         { cat: 'otd', label: 'Fail-safe on OT bus loss' },
    otdmsginterval:      { cat: 'otd', label: 'Message interval', hint: 'ms' },
    // SAT: full metadata (TASK-933 Phase 2)
    satforcepwm:         { cat: 'sat', label: 'Force PWM' },
    satpushsetpoint:     { cat: 'sat', label: 'Push setpoint to thermostat' },
    satwindowdetect:     { cat: 'sat', label: 'Window detection' },
    satwindowminsec:     { cat: 'sat', label: 'Window min open time', hint: 'Seconds' },
    satthermalcomfort:   { cat: 'sat', label: 'Thermal comfort model' },
    sathumiditytimeout:  { cat: 'sat', label: 'Humidity sensor timeout', hint: 'Seconds' },
    satflushtreshold:    { cat: 'sat', label: 'Flush threshold', hint: 'Hours' },
    satdhwenabled:       { cat: 'sat', label: 'DHW enabled (standalone)' },
    satdhwenable:        { cat: 'sat', label: 'DHW enable (HW= master)' },
    satzonecount:        { cat: 'sat', label: 'Zone count' },
    satzonetimeout:      { cat: 'sat', label: 'Zone sensor timeout', hint: 'Seconds' },
    satzoneheadroom:     { cat: 'sat', label: 'Zone headroom', hint: '°C' },
    satsensorarea0:      { cat: 'sat', label: 'Area 0 sensor' },
    satsensorarea1:      { cat: 'sat', label: 'Area 1 sensor' },
    satsensorarea2:      { cat: 'sat', label: 'Area 2 sensor' },
    satsensorarea3:      { cat: 'sat', label: 'Area 3 sensor' },
    satdhwsetpoint:      { cat: 'sat', label: 'DHW setpoint', hint: '°C (0 = inactive)' },
    satflameoffset:      { cat: 'sat', label: 'Flame-off offset', hint: '°C' },
    satflowoffset:       { cat: 'sat', label: 'Flow offset', hint: '°C' },
    satmodsupdelay:      { cat: 'sat', label: 'Modulation suppression delay', hint: 'Seconds' },
    satmodsupoffset:     { cat: 'sat', label: 'Modulation suppression offset', hint: '°C' },
    satminpressure:      { cat: 'sat', label: 'Min pressure', hint: 'bar' },
    satmaxpressure:      { cat: 'sat', label: 'Max pressure', hint: 'bar' },
    satmaxpressdrop:     { cat: 'sat', label: 'Max pressure drop', hint: 'bar/hr' },
    satsolarminelev:     { cat: 'sat', label: 'Solar min elevation', hint: '°' },
    satboilerratedkw:    { cat: 'sat', label: 'Boiler rated output', hint: 'kW (0 = off)' },
    satboilerefficiency: { cat: 'sat', label: 'Boiler efficiency', hint: 'fraction 0.5–1.0' },
    // TASK-933 P2: keys that already had GET + dispatch but lacked a whitelist token
    // (now POST-able) plus the masked weather API key.
    satcyclesperhour:    { cat: 'sat', label: 'Cycles per hour' },
    satvalveoffset:      { cat: 'sat', label: 'Valve offset', hint: '°C' },
    satsensormaxage:     { cat: 'sat', label: 'Sensor max age', hint: 'Seconds' },
    saterrormon:         { cat: 'sat', label: 'Error monitoring' },
    satautogains:        { cat: 'sat', label: 'Auto-gain factor' },
    satsolarfreezeint:   { cat: 'sat', label: 'Solar freeze integral' },
    satmaxmodulation:    { cat: 'sat', label: 'Max modulation', hint: '%' },
    satweatherapikey:    { cat: 'sat', label: 'OWM API key' },
    // --- SAT long-tail: curated labels + hints + sub-groups for the older SAT keys
    // the firmware GET exposes (presets / weather / solar / summer / comfort /
    // multi-area / PV-boost / auto-tune / simulation / BLE). These previously fell
    // back to humanizeKey under a flat SAT card (TASK-933 mockup-alignment). ---
    satexternaltemp:     { cat: 'sat', label: 'Use external temp source' },
    satovershootmargin:  { cat: 'sat', label: 'Overshoot margin', hint: '°C' },
    satpwmautoswitch:    { cat: 'sat', label: 'Auto PWM switch' },
    satboilercapacity:   { cat: 'sat', label: 'Boiler capacity', hint: 'kW' },
    satthermalcoeff:     { cat: 'sat', label: 'Thermal-drop coefficient' },
    satpresetcomfort:    { cat: 'sat', sub: 'Presets', label: 'Comfort preset', hint: '°C' },
    satpreseteco:        { cat: 'sat', sub: 'Presets', label: 'Eco preset', hint: '°C' },
    satpresetaway:       { cat: 'sat', sub: 'Presets', label: 'Away preset', hint: '°C' },
    satpresetsleep:      { cat: 'sat', sub: 'Presets', label: 'Sleep preset', hint: '°C' },
    satpresetactivity:   { cat: 'sat', sub: 'Presets', label: 'Activity preset', hint: '°C' },
    satpresethome:       { cat: 'sat', sub: 'Presets', label: 'Home preset', hint: '°C' },
    satpresetsync:       { cat: 'sat', sub: 'Presets', label: 'Sync presets via MQTT' },
    satpresetsynctopic:  { cat: 'sat', sub: 'Presets', label: 'Preset sync topic' },
    satweatherenable:    { cat: 'sat', sub: 'Weather', label: 'Enable weather compensation' },
    satweatherlat:       { cat: 'sat', sub: 'Weather', label: 'Latitude', hint: '°' },
    satweatherlon:       { cat: 'sat', sub: 'Weather', label: 'Longitude', hint: '°' },
    satweatherinterval:  { cat: 'sat', sub: 'Weather', label: 'Update interval', hint: 'Seconds' },
    satsolargain:        { cat: 'sat', sub: 'Solar gain', label: 'Enable solar gain' },
    satsolarminrise:     { cat: 'sat', sub: 'Solar gain', label: 'Min rise rate', hint: '°C' },
    satsolaroffset:      { cat: 'sat', sub: 'Solar gain', label: 'Setpoint offset', hint: '°C' },
    satsummersimmer:     { cat: 'sat', sub: 'Summer simmer', label: 'Enable summer simmer' },
    satsummerthreshold:  { cat: 'sat', sub: 'Summer simmer', label: 'Summer threshold', hint: '°C' },
    satsummerminhours:   { cat: 'sat', sub: 'Summer simmer', label: 'Min summer hours', hint: 'Hours' },
    satcomfortadjust:    { cat: 'sat', sub: 'Thermal comfort', label: 'Comfort humidity adjust' },
    satcomforthumidity:  { cat: 'sat', sub: 'Thermal comfort', label: 'Target humidity', hint: '%' },
    satcomfortmaxoffset: { cat: 'sat', sub: 'Thermal comfort', label: 'Max comfort offset', hint: '°C' },
    satmultiarea:        { cat: 'sat', sub: 'Multi-area', label: 'Enable multi-area' },
    satmultiareacount:   { cat: 'sat', sub: 'Multi-area', label: 'Area count' },
    satareaweight0:      { cat: 'sat', sub: 'Multi-area', label: 'Area 0 weight' },
    satareaweight1:      { cat: 'sat', sub: 'Multi-area', label: 'Area 1 weight' },
    satareaweight2:      { cat: 'sat', sub: 'Multi-area', label: 'Area 2 weight' },
    satareaweight3:      { cat: 'sat', sub: 'Multi-area', label: 'Area 3 weight' },
    satpvboostenabled:   { cat: 'sat', sub: 'PV boost', label: 'Enable PV boost' },
    satpvboostthresholdw:{ cat: 'sat', sub: 'PV boost', label: 'Surplus threshold', hint: 'W' },
    satpvboostholds:     { cat: 'sat', sub: 'PV boost', label: 'Hold time', hint: 'Seconds' },
    satpvboostdeltac:    { cat: 'sat', sub: 'PV boost', label: 'Setpoint boost', hint: '°C' },
    satpvboostmaxindoorc:{ cat: 'sat', sub: 'PV boost', label: 'Max indoor temp', hint: '°C' },
    satpvboostmaxdurationmin: { cat: 'sat', sub: 'PV boost', label: 'Max duration', hint: 'Minutes' },
    satautotune:         { cat: 'sat', sub: 'PID auto-tune', label: 'Enable auto-tune' },
    satautotunerate:     { cat: 'sat', sub: 'PID auto-tune', label: 'Auto-tune rate' },
    satsimulation:       { cat: 'sat', sub: 'Simulation', label: 'Enable simulation' },
    satsimheatrate:      { cat: 'sat', sub: 'Simulation', label: 'Heat rate', hint: '°C/min' },
    satsimcoolrate:      { cat: 'sat', sub: 'Simulation', label: 'Cool rate', hint: '°C/min' },
    // TASK-974: BLE sensors are sensors, independent of SAT — grouped under the
    // Sensors category (not SAT) so users find the enable toggle next to the roster.
    // The firmware already scans on satbleenable alone (no satenabled needed) and
    // lazy-inits the scan at runtime, so enabling here starts it without a reboot.
    satbleenable:        { cat: 'sensors', sub: 'BLE sensors', label: 'Enable BLE sensors' },
    satblefailover:      { cat: 'sensors', sub: 'BLE sensors', label: 'BLE failover to OT' },
    satblemac:           { cat: 'sensors', sub: 'BLE sensors', label: 'BLE sensor MAC' },
    satbleinterval:      { cat: 'sensors', sub: 'BLE sensors', label: 'BLE poll interval', hint: 'Seconds' }
  };
  // ui_usev2 is the UI-switch flag (owned by the "Classic UI" control), not a
  // normal toggle — never list it as an editable setting.
  var SET_HIDE = { ui_usev2: 1 };
  function humanizeKey(k) {
    var s = k.replace(/^(sat|otd|gpiosensors|gpiooutputs|s0counter|mqtt|wifi|eth|ntp|webhook|ui_|dallas)/, '');
    if (!s) s = k;
    s = s.replace(/_/g, ' ').replace(/([a-z])([A-Z])/g, '$1 $2').trim();
    return s ? s.charAt(0).toUpperCase() + s.slice(1) : k;
  }
  function catFor(k) {
    if (SET_META[k]) return SET_META[k].cat;
    if (/^wifi|^eth/.test(k)) return 'network';
    if (/^mqtt|^legacyport/.test(k)) return 'mqtt';
    if (/^ntp/.test(k)) return 'ntp';
    if (/^otgw/.test(k)) return 'otgw';
    if (/^otd/.test(k)) return 'otd';
    if (/^gpiosensors|^dallas|^sensor/.test(k)) return 'sensors';
    if (/^gpiooutputs/.test(k)) return 'outputs';
    if (/^s0/.test(k)) return 's0';
    if (/^webhook/.test(k)) return 'webhook';
    if (/^sat/.test(k)) return 'sat';
    if (/^ui_/.test(k)) return 'ui';
    return 'system';
  }
  function labelFor(k) { return (SET_META[k] && SET_META[k].label) || humanizeKey(k); }
  function hintFor(k) { return SET_META[k] && SET_META[k].hint; }
  function subFor(k) { return (SET_META[k] && SET_META[k].sub) || ''; }
  function catById(id) { for (var i = 0; i < SET_CATS.length; i++) if (SET_CATS[i].id === id) return SET_CATS[i]; return null; }

  var setData = {};   // key -> {value,type,...} as fetched
  var setDirty = {};  // key -> new value (string)
  var setSearch = '';
  var setActiveCat = '';  // id of the currently-shown category (empty => first non-empty)
  function fetchSettings() {
    fetch(APIGW + 'v2/settings').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      if (!j) return;
      // settings come as { key: {value, type, ...}, ... } possibly nested under a root
      var src = j.settings || j;
      setData = {}; setDirty = {};
      Object.keys(src).forEach(function (k) {
        var v = src[k];
        if (v && typeof v === 'object' && 'value' in v && !SET_HIDE[k]) setData[k] = v;
      });
      renderSettings();
      fetchBle();
    }).catch(function () { });
  }
  function fieldDirty(k) { return Object.prototype.hasOwnProperty.call(setDirty, k); }
  // Password fields: GET returns a masked sentinel ("password=N"), never the real
  // secret. Show an empty field; only POST when the user actually typed something.
  function isPwd(k) { return setData[k] && setData[k].type === 'p'; }
  function curVal(k) {
    if (fieldDirty(k)) return setDirty[k];
    if (isPwd(k)) return '';
    return '' + setData[k].value;
  }
  // keys per category, honoring the search filter
  function keysForCat(id, q) {
    return Object.keys(setData).filter(function (k) {
      if (catFor(k) !== id) return false;
      if (!q) return true;
      return labelFor(k).toLowerCase().indexOf(q) !== -1 || k.toLowerCase().indexOf(q) !== -1;
    });
  }
  function renderSettings() {
    var rail = document.getElementById('setRail'), cols = document.getElementById('setCols');
    if (!rail || !cols) return;
    var q = setSearch.trim().toLowerCase();
    rail.textContent = ''; cols.textContent = '';
    // Build the rail (one item per category that has at least one field), with
    // per-category counts and a REBOOT badge on reboot-sensitive categories.
    var firstNonEmpty = '';
    SET_CATS.forEach(function (c) {
      var n = keysForCat(c.id, q).length;
      if (!n) return;
      if (!firstNonEmpty) firstNonEmpty = c.id;
      var ri = document.createElement('div'); ri.className = 'rail-item';
      ri.appendChild(document.createTextNode(c.icon + ' ' + c.title + ' '));
      if (c.reboot) { var rb = document.createElement('span'); rb.className = 'badge-reboot'; rb.textContent = 'REBOOT'; ri.appendChild(rb); }
      var cnt = document.createElement('span'); cnt.className = 'cnt'; cnt.textContent = n; ri.appendChild(cnt);
      ri.addEventListener('click', function () { setActiveCat = c.id; renderSettings(); });
      ri.dataset.cat = c.id;
      rail.appendChild(ri);
    });
    // When searching, show every matching category at once; otherwise show the one
    // active category (mockup's show-one-at-a-time behaviour).
    var showCats;
    if (q) { showCats = SET_CATS.filter(function (c) { return keysForCat(c.id, q).length; }).map(function (c) { return c.id; }); }
    else {
      var active = setActiveCat && keysForCat(setActiveCat, '').length ? setActiveCat : firstNonEmpty;
      setActiveCat = active;
      showCats = active ? [active] : [];
      var act = rail.querySelector('.rail-item[data-cat="' + active + '"]'); if (act) act.classList.add('active');
    }
    showCats.forEach(function (id) {
      var c = catById(id); if (!c) return;
      // category header (title + REBOOT + description)
      var head = document.createElement('div');
      var title = document.createElement('div'); title.className = 'set-cat-title';
      title.appendChild(document.createTextNode(c.icon + ' ' + c.title + ' '));
      if (c.reboot) { var rb2 = document.createElement('span'); rb2.className = 'badge-reboot'; rb2.textContent = 'REBOOT'; title.appendChild(rb2); }
      head.appendChild(title);
      if (c.desc) { var d = document.createElement('div'); d.className = 'cat-desc'; d.textContent = c.desc; head.appendChild(d); }
      cols.appendChild(head);
      // masonry container; cards grouped by sub-group
      var wrap = document.createElement('div'); wrap.className = 'set-cards';
      var keys = keysForCat(id, q);
      var bySub = {}; var subOrder = [];
      keys.forEach(function (k) { var s = subFor(k); if (!(s in bySub)) { bySub[s] = []; subOrder.push(s); } bySub[s].push(k); });
      subOrder.forEach(function (s) {
        var card = document.createElement('section'); card.className = 'set-group';
        if (s) { var h = document.createElement('h3'); h.textContent = s; card.appendChild(h); }
        bySub[s].forEach(function (k) { card.appendChild(settingRow(k)); });
        wrap.appendChild(card);
      });
      cols.appendChild(wrap);
    });
    if (bleData) renderBleCard();
    updateSaveBar();
  }
  function settingRow(k) {
    var meta = setData[k], type = meta.type || 's';
    var row = document.createElement('div'); row.className = 'srow' + (fieldDirty(k) ? ' dirty' : '');
    row.dataset.key = k;
    var lbl = document.createElement('div'); lbl.className = 'slbl';
    lbl.appendChild(document.createTextNode(labelFor(k)));
    var hint = hintFor(k);
    if (hint) { var hs = document.createElement('span'); hs.className = 'shint'; hs.textContent = hint; lbl.appendChild(hs); }
    row.appendChild(lbl);
    var input;
    if (type === 'b') {
      input = document.createElement('input'); input.type = 'checkbox'; input.className = 'sw';
      input.checked = curVal(k) === 'true' || curVal(k) === '1';
      input.addEventListener('change', function () { markDirty(k, input.checked ? 'true' : 'false', row); });
    } else if (type === 'r') {
      input = document.createElement('input'); input.type = 'text'; input.value = curVal(k); input.disabled = true;
    } else if (ENUM_OPTS[k]) {
      input = document.createElement('select');
      var cv = '' + curVal(k);
      // Match the current value against the option set. Numeric enums store "2";
      // string enums store the literal value. Try raw first, then int-normalised —
      // the old parseInt-only path coerced every string enum to the 0th option.
      var hasRaw = ENUM_OPTS[k].some(function (o) { return ('' + o[0]) === cv; });
      var cvInt = '' + parseInt(cv, 10);
      var hasInt = !hasRaw && cv !== '' && ENUM_OPTS[k].some(function (o) { return ('' + o[0]) === cvInt; });
      ENUM_OPTS[k].forEach(function (o) {
        var opt = document.createElement('option'); opt.value = '' + o[0]; opt.textContent = o[1]; input.appendChild(opt);
      });
      // Preserve an out-of-list current value as its own option so a custom setting
      // (e.g. a timezone not in the list) is never silently dropped to the 0th entry.
      if (!hasRaw && !hasInt && cv !== '') {
        var cur = document.createElement('option'); cur.value = cv; cur.textContent = cv + ' (current)'; input.appendChild(cur);
      }
      input.value = hasRaw ? cv : hasInt ? cvInt : cv;
      input.addEventListener('change', function () { markDirty(k, input.value, row); });
    } else {
      input = document.createElement('input');
      input.type = (type === 'p') ? 'password' : (type === 'i') ? 'number' : 'text';
      input.value = curVal(k);
      if (type === 'p') input.placeholder = '••••••••';
      input.addEventListener('input', function () { markDirty(k, input.value, row); });
    }
    row.appendChild(input);
    return row;
  }
  function markDirty(k, val, row) {
    // Password fields show empty; an empty value means "leave unchanged", so it is
    // never dirty and never POSTed (the GET-side value is a mask, not the secret).
    if ((isPwd(k) && val === '') || ('' + setData[k].value) === val) delete setDirty[k];
    else setDirty[k] = val;
    if (row) row.classList.toggle('dirty', fieldDirty(k));
    updateSaveBar();
  }
  function updateSaveBar() {
    var bar = document.getElementById('saveBar'), cnt = document.getElementById('dirtyCount');
    var n = Object.keys(setDirty).length;
    if (cnt) cnt.textContent = n;
    if (bar) bar.classList.toggle('show', n > 0);
  }
  function saveSettings() {
    var keys = Object.keys(setDirty);
    if (!keys.length) return;
    var chain = Promise.resolve();
    keys.forEach(function (k) {
      chain = chain.then(function () {
        return fetch(APIGW + 'v2/settings', {
          method: 'POST', headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ name: k, value: setDirty[k] })
        }).then(function (r) {
          // audit fix: only treat as saved when the HTTP status is OK; a 401/403
          // (auth/CSRF) or 4xx/5xx must NOT report success or clear the dirty mark.
          if (!r.ok) throw new Error('save ' + k + ' -> HTTP ' + r.status);
          if (setData[k]) setData[k].value = setDirty[k];
        });
      });
    });
    chain.then(function () {
      setDirty = {};
      var t = document.getElementById('toast'); if (t) { t.textContent = 'Settings saved'; t.classList.add('show'); setTimeout(function () { t.classList.remove('show'); }, 2000); }
      renderSettings();
    }).catch(function () {
      var t = document.getElementById('toast'); if (t) { t.textContent = 'Save failed'; t.classList.add('show'); setTimeout(function () { t.classList.remove('show'); }, 2500); }
    });
  }
  function discardSettings() { setDirty = {}; renderSettings(); }

  // ---------- Settings > BLE sensor roster (GET/POST /api/v2/sat/ble/*) ----------
  var bleData = null, blePollTimer = null;
  function bleToast(msg) {
    var t = document.getElementById('toast');
    if (t) { t.textContent = msg; t.classList.add('show'); setTimeout(function () { t.classList.remove('show'); }, 2000); }
  }
  function fetchBle() {
    fetch(APIGW + 'v2/sat/ble/discovery').then(function (r) { return r.ok ? r.json() : null; })
      .then(function (j) {
        if (!j) return;
        // TASK-963: re-render only when the roster actually changed, and never
        // while an input inside the card is focused (a 4s poll would otherwise
        // clobber a half-typed bindkey / steal focus).
        var changed = !bleData || bleData.populated_slots !== j.populated_slots ||
          JSON.stringify(bleData.sensors || []) !== JSON.stringify(j.sensors || []);
        bleData = j;
        var card = document.getElementById('setcard-ble');
        var typing = card && card.contains(document.activeElement) &&
          /^(INPUT|TEXTAREA)$/.test((document.activeElement || {}).tagName || '');
        if (changed && !typing) renderBleCard();
      }).catch(function () { });
  }
  // TASK-963: continuous detection. The firmware scans passive-continuous (ADR-151),
  // so sensors appear over time; poll the roster while the Sensors category is open
  // so they surface on their own. Self-stops when the user leaves Sensors.
  function startBlePoll() {
    if (blePollTimer) return;
    blePollTimer = setInterval(function () {
      if (setActiveCat !== 'sensors' || !document.getElementById('setcard-ble')) { stopBlePoll(); return; }
      fetchBle();
    }, 4000);
  }
  function stopBlePoll() { if (blePollTimer) { clearInterval(blePollTimer); blePollTimer = null; } }
  function bleAction(path, body) {
    return fetch(APIGW + 'v2/sat/ble/' + path, {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: body ? JSON.stringify(body) : undefined
    }).then(function () { setTimeout(fetchBle, 500); }).catch(function () { });
  }
  function renderBleCard() {
    var cols = document.getElementById('setCols'); if (!cols || !bleData) return;
    var old = document.getElementById('setcard-ble'); if (old) old.remove();
    // TASK-963: the BLE roster belongs to the Sensors category ONLY. The old code
    // appended unconditionally (called here AND from the async fetchBle()), so the
    // card leaked onto every category. Bail (and stop the poll) when not on Sensors.
    if (setActiveCat !== 'sensors') { stopBlePoll(); return; }
    var card = document.createElement('div'); card.className = 'set-group'; card.id = 'setcard-ble';
    var h = document.createElement('h3'); h.textContent = 'BLE sensors'; card.appendChild(h);
    var desc = document.createElement('div'); desc.className = 'ble-desc';
    desc.textContent = (bleData.populated_slots || 0) + ' / ' + (bleData.max_slots || 0) + ' slots used' +
      (bleData.name_prefix ? ' · prefix "' + bleData.name_prefix + '"' : '');
    card.appendChild(desc);
    var rescan = document.createElement('button'); rescan.className = 'tbtn'; rescan.textContent = '🔄 Rescan';
    rescan.addEventListener('click', function () {
      // TASK-963: the POST already fires (200) but gave NO visible feedback, so the
      // button read as dead. Show a scanning state + toast; the continuous poll
      // surfaces any newly-found sensors. Re-enable after a short window.
      rescan.disabled = true; rescan.textContent = '⏳ Scanning…';
      bleToast('Scanning for BLE sensors…');
      bleAction('rescan');
      setTimeout(function () { if (rescan.isConnected) { rescan.disabled = false; rescan.textContent = '🔄 Rescan'; } }, 3000);
    });
    card.appendChild(rescan);
    var sensors = bleData.sensors || [];
    if (!sensors.length) {
      var empty = document.createElement('div'); empty.className = 'ble-row'; empty.style.color = 'var(--muted)';
      empty.textContent = 'No BLE sensors discovered yet. Press Rescan and put a sensor in range.';
      card.appendChild(empty);
    } else {
      sensors.forEach(function (s) {
        var row = document.createElement('div'); row.className = 'ble-row';
        var nm = document.createElement('div'); nm.className = 'ble-nm';
        nm.textContent = s.name || s.label || '(unnamed)';
        var mac = document.createElement('span'); mac.className = 'ble-mac'; mac.textContent = s.mac || ''; nm.appendChild(mac);
        if (s.mac && s.mac === bleData.selected_mac) { var tag = document.createElement('span'); tag.className = 'ble-tag temp'; tag.textContent = 'selected'; nm.appendChild(tag); }
        row.appendChild(nm);
        // Firmware emits per-sensor "temp"/"hum" (SATble roster); accept the
        // longer aliases too for forward-compat. (audit fix: was reading
        // s.temperature/s.humidity which the firmware never sends.)
        var bt = (s.temp !== undefined) ? s.temp : s.temperature;
        var bh = (s.hum !== undefined) ? s.hum : s.humidity;
        if (bt !== undefined || bh !== undefined) {
          var val = document.createElement('div'); val.className = 'ble-val';
          val.textContent = (bt !== undefined ? bt + '°' : '') + (bh !== undefined ? '  ' + bh + '%' : '');
          row.appendChild(val);
        }
        // TASK-931: bindkey state — encrypted Xiaomi MiBeacon sensors show a lock
        // when a key is set. The key itself is write-only (never sent back).
        if (s.has_key) { var kt = document.createElement('span'); kt.className = 'ble-tag'; kt.textContent = '🔒 key'; nm.appendChild(kt); }
        var ctr = document.createElement('div'); ctr.className = 'ble-ctrls';
        var sel = document.createElement('button'); sel.className = 'tbtn'; sel.textContent = 'Select';
        sel.addEventListener('click', function () { bleAction('select', { mac: s.mac }); });
        var forget = document.createElement('button'); forget.className = 'tbtn'; forget.textContent = 'Forget';
        forget.addEventListener('click', function () { bleAction('forget', { mac: s.mac }); });
        ctr.appendChild(sel); ctr.appendChild(forget); row.appendChild(ctr);
        // TASK-931: per-row bindkey input (encrypted MiBeacon). Paste the 32-hex
        // beaconkey from the pvvx/Xiaomi-cloud tooling; empty clears it.
        var keyRow = document.createElement('div'); keyRow.className = 'ble-ctrls';
        var keyIn = document.createElement('input'); keyIn.type = 'password'; keyIn.placeholder = '32-hex bindkey';
        keyIn.maxLength = 32; keyIn.autocomplete = 'off'; keyIn.style.flex = '1'; keyIn.style.minWidth = '12ch';
        var keyBtn = document.createElement('button'); keyBtn.className = 'tbtn'; keyBtn.textContent = '🔑 Set';
        keyBtn.addEventListener('click', function () {
          var k = (keyIn.value || '').trim().toLowerCase();
          if (k && !/^[0-9a-f]{32}$/.test(k)) { keyIn.style.borderColor = 'var(--hot,#c33)'; return; }
          bleAction('bindkey', { mac: s.mac, key: k }); keyIn.value = '';
        });
        keyRow.appendChild(keyIn); keyRow.appendChild(keyBtn); row.appendChild(keyRow);
        card.appendChild(row);
      });
    }
    // TASK-931: provision an ENCRYPTED sensor that is not in the roster yet — an
    // encrypted Mijia cannot self-announce until it has a key, so paste its MAC +
    // 32-hex bindkey together (both from the pvvx/Xiaomi-cloud tooling).
    var add = document.createElement('div'); add.className = 'ble-row';
    var addLbl = document.createElement('div'); addLbl.className = 'ble-nm'; addLbl.textContent = 'Add encrypted sensor';
    add.appendChild(addLbl);
    var addCtr = document.createElement('div'); addCtr.className = 'ble-ctrls';
    var macIn = document.createElement('input'); macIn.type = 'text'; macIn.placeholder = 'AA:BB:CC:DD:EE:FF';
    macIn.autocomplete = 'off'; macIn.style.minWidth = '15ch';
    var bkIn = document.createElement('input'); bkIn.type = 'password'; bkIn.placeholder = '32-hex bindkey';
    bkIn.maxLength = 32; bkIn.autocomplete = 'off'; bkIn.style.minWidth = '12ch';
    var addBtn = document.createElement('button'); addBtn.className = 'tbtn'; addBtn.textContent = '➕ Add';
    addBtn.addEventListener('click', function () {
      var m = (macIn.value || '').trim().toUpperCase();
      var k = (bkIn.value || '').trim().toLowerCase();
      if (!/^[0-9A-F]{2}(:[0-9A-F]{2}){5}$/.test(m)) { macIn.style.borderColor = 'var(--hot,#c33)'; return; }
      if (!/^[0-9a-f]{32}$/.test(k)) { bkIn.style.borderColor = 'var(--hot,#c33)'; return; }
      bleAction('bindkey', { mac: m, key: k }); macIn.value = ''; bkIn.value = '';
    });
    addCtr.appendChild(macIn); addCtr.appendChild(bkIn); addCtr.appendChild(addBtn);
    add.appendChild(addCtr); card.appendChild(add);
    cols.appendChild(card);
    startBlePoll();   // TASK-963: continuous roster polling while the Sensors category is open
  }

  // ============================================================
  // P9: heat-source control, REST seed, target steppers, conn detail,
  // support hover, graph window/export, settings enum options.
  // ============================================================
  var sat = { source: 0, detected: 1, target: null };  // source: 0=auto,1=gas,2=hp,3=hybrid
  function effectiveSource() { return sat.source === 0 ? sat.detected : sat.source; }
  var statsSort = { col: 'id', asc: true };
  function sortStatsCmp(a, b) {
    var sa = stats[a], sb = stats[b], dir = statsSort.asc ? 1 : -1;
    switch (statsSort.col) {
      case 'label': return (sa.label || '').localeCompare(sb.label || '') * dir;
      case 'dir': return (dirBadge(sa)[0]).localeCompare(dirBadge(sb)[0]) * dir;
      case 'interval': return ((sa.interval || 0) - (sb.interval || 0)) * dir;
      case 'count': return (sa.count - sb.count) * dir;
      case 'value': return ('' + (sa.value || '')).localeCompare('' + (sb.value || '')) * dir;
      default: return (a - b) * dir;
    }
  }
  var graphWindowMs = 3600000;
  var ENUM_OPTS = {
    satsource: [[0, 'Auto'], [1, 'Gas Boiler'], [2, 'Heat Pump'], [3, 'Hybrid']],
    satsystem: [[0, 'Auto'], [1, 'Radiators'], [2, 'Underfloor']],
    // boardmode is the real 3-value hardware override (ADR-127); the mockup's
    // 4-option ESP8266/OTGW32/LOLIN list was semantically wrong.
    boardmode: [[0, 'Auto-detect'], [1, 'PIC (Classic)'], [2, 'OT-Direct (OTGW32)']],
    gpiooutputstriggerbit: [[0, 'Flame'], [1, 'CH active'], [2, 'DHW active'], [3, 'Fault']],
    webhooktriggerbit: [[0, 'Flame'], [1, 'CH active'], [2, 'DHW active'], [3, 'Fault']],
    satheatingmode: [[0, 'Comfort'], [1, 'Eco']],
    // OT-Direct gateway mode — labels from OTDirecttypes.h (NOT the mockup's
    // Off/On-Off/PID list, which was a different, wrong concept).
    otdmode: [[0, 'Bypass'], [1, 'Gateway'], [2, 'Monitor'], [3, 'Master'], [4, 'Loopback']],
    // OT-Direct CH mode (TASK-933 P2): 0=off, 1=fixed flow, 2=heating curve (auto)
    otdchmode: [[0, 'Off'], [1, 'Fixed flow'], [2, 'Heating curve (auto)']],
    // SAT manufacturer presets — labels from the firmware satManufacturerTable[].
    satmanufacturer: [[0, 'Auto'], [1, 'Atag'], [2, 'Baxi'], [3, 'Brotje'], [4, 'De Dietrich'], [5, 'Ferroli'], [6, 'Geminox'], [7, 'Ideal'], [8, 'Immergas'], [9, 'Intergas'], [10, 'Itho'], [11, 'Nefit'], [12, 'Radiant'], [13, 'Remeha'], [14, 'Sime'], [15, 'Vaillant'], [16, 'Viessmann'], [17, 'Worcester'], [18, 'Other']],
    // Friendly select for the default graph window (firmware stores minutes, 0-1440).
    ui_graphtimewindow: [[10, '10 min'], [30, '30 min'], [60, '1 hour'], [240, '4 hours'], [1440, '24 hours']],
    // Webhook POST content type — standard closed set (string-valued enum; the
    // string-safe select path preserves any custom value not in this list).
    webhookcontenttype: [['application/json', 'application/json'], ['application/x-www-form-urlencoded', 'form-urlencoded'], ['text/plain', 'text/plain']]
  };

  function fetchSeed() {
    fetch(APIGW + 'v2/otgw/otmonitor').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      var o = j && j.otmonitor; if (!o) return;
      function num(k) { var e = o[k]; if (!e || e.value === undefined) return null; var v = parseFloat(e.value); return isNaN(v) ? null : v; }
      function on(k) { var e = o[k]; if (!e || e.value === undefined) return null; return /on|true|1/i.test('' + e.value); }
      var map = { flow: 'boilertemperature', ret: 'returnwatertemperature', dhw: 'dhwtemperature', outside: 'outsidetemperature', room: 'roomtemperature', roomSet: 'roomsetpoint', chSet: 'controlsetpoint', mod: 'relmodlvl', pressure: 'chwaterpressure' };
      var changed = false;
      Object.keys(map).forEach(function (f) { if (f === 'outside' && model.wxValid) return; var v = num(map[f]); if (v !== null) { model[f] = v; changed = true; } });
      var fl = on('flamestatus'); if (fl !== null) { model.flame = fl; changed = true; }
      var ch = on('chmodus'); if (ch !== null) { model.ch = ch; changed = true; }
      var dh = on('dhwmode'); if (dh !== null) { model.dhw_on = dh; changed = true; }
      if (changed) { pushSample(); scheduleRender(); }
    }).catch(function () { });
  }

  function fetchSatStatus() {
    fetch(APIGW + 'v2/sat/status').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      var st = j && (j.sat || j); if (!st) return;
      if (st.heating_source !== undefined) sat.source = st.heating_source | 0;
      if (st.heating_source_detected !== undefined) sat.detected = st.heating_source_detected | 0;
      if (st.target_temp !== undefined) sat.target = parseFloat(st.target_temp);
      applyApplianceToggle();
      if (activeDesign() === 'a') renderA();
    }).catch(function () { });
  }

  // TASK-954: wire the OUTSIDE reading to the actual weather-data API. When
  // weather compensation is enabled and the last OpenWeatherMap fetch is valid,
  // its temperature drives OUTSIDE; otherwise OUTSIDE falls back to the OT-bus
  // Toutside (MsgID 27) / otmonitor value (the OT case 27 + fetchSeed both yield
  // to model.wxValid). Polled slowly — weather changes on the minute scale.
  function fetchWeather() {
    fetch(APIGW + 'v2/sat/weather').then(function (r) { return r.ok ? r.json() : null; }).then(function (w) {
      if (!w) return;
      if (w.enabled && w.valid) {
        var t = parseFloat(w.temperature);
        if (!isNaN(t)) { model.outside = t; model.wxValid = true; scheduleRender(); return; }
      }
      model.wxValid = false;
    }).catch(function () { });
  }
  function applyApplianceToggle() {
    var hp = effectiveSource() === 2;
    var svg = document.getElementById('schem'); if (svg) svg.classList.toggle('heatpump', hp);
    document.querySelectorAll('#applToggle .seg2btn').forEach(function (b) {
      var isHp = (b.dataset.appl === 'hp');
      b.classList.toggle('active', isHp === hp);
    });
  }
  function setSource(v) {
    fetch(APIGW + 'v2/settings', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ name: 'satsource', value: '' + v }) })
      .then(function (r) { if (r.ok) { sat.source = v; applyApplianceToggle(); setTimeout(fetchSatStatus, 400); } }).catch(function () { });
  }

  function stepTarget(delta) {
    var base = (sat.target !== null && !isNaN(sat.target)) ? sat.target : (model.roomSet !== null ? model.roomSet : 20);
    var t = Math.max(5, Math.min(30, Math.round((base + delta) * 2) / 2));
    txt('bCmd', 'set target ' + t.toFixed(1) + '°…');
    fetch(APIGW + 'v2/sat/target', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ value: '' + t }) })
      .then(function (r) {
        if (r.ok) { sat.target = t; txt('bSet', 'target ' + t.toFixed(1) + '°'); txt('bCmd', 'target set ' + t.toFixed(1) + '°'); }
        else txt('bCmd', 'set failed (HTTP ' + r.status + ')');
      }).catch(function () { txt('bCmd', 'set failed'); });
  }

  // Grouped chain map: Heating bus / Network / Integrations / This browser, each
  // row with state badge (icon + word), explanation, per-link fix hint when down,
  // and recency — the mockup's "locate the break and tell the user how to fix it".
  var CONN_GROUPS = [
    { id: 'bus', title: 'Heating bus (OpenTherm)' },
    { id: 'net', title: 'Network' },
    { id: 'int', title: 'Integrations' },
    { id: 'ses', title: 'This browser session' }
  ];
  function connDetailRow(key) {
    var c = CONN[key];
    var row = document.createElement('div'); row.className = 'connrow ' + c.s;
    var badge = document.createElement('span'); badge.className = 'badge';
    badge.textContent = (c.s === 'st-mode') ? ('⇄ ' + (c.value || '?').toUpperCase()) : ((STICON[c.s] || '?') + ' ' + (STLABEL[c.s] || '—'));
    var body = document.createElement('div'); body.className = 'body';
    var nm = document.createElement('div'); nm.className = 'nm'; nm.textContent = c.icon + ' ' + c.name;
    body.appendChild(nm);
    var det = c.detail || (c.seen != null ? connRecency(c) : '');
    if (det) { var dd = document.createElement('div'); dd.className = 'det'; dd.textContent = det; body.appendChild(dd); }
    if (c.exp) { var ex = document.createElement('div'); ex.className = 'exp'; ex.textContent = c.exp; body.appendChild(ex); }
    if ((c.s === 'st-down' || c.s === 'st-warn') && c.fix) { var fx = document.createElement('div'); fx.className = 'fix'; fx.textContent = '▸ ' + c.fix; body.appendChild(fx); }
    row.appendChild(badge); row.appendChild(body);
    return row;
  }
  function renderConnDetail() {
    var box = document.getElementById('connDetail'); if (!box) return;
    box.textContent = '';
    CONN_GROUPS.forEach(function (g) {
      var keys = Object.keys(CONN).filter(function (k) { return CONN[k].grp === g.id; });
      if (!keys.length) return;
      var grp = document.createElement('div'); grp.className = 'conngroup';
      var h4 = document.createElement('h4'); h4.textContent = g.title; grp.appendChild(h4);
      keys.forEach(function (k) { grp.appendChild(connDetailRow(k)); });
      box.appendChild(grp);
    });
  }

  function showSupTip(id, e) {
    var tip = document.getElementById('supTip'); if (!tip) return;
    var sx = stats[id];
    tip.textContent = '';
    var b = document.createElement('b'); b.textContent = id; tip.appendChild(b);
    // Spec name from the firmware OTmap for ALL cells (even never-seen ones).
    var m = (typeof OTMSG !== 'undefined' && OTMSG[id]) ? OTMSG[id] : null;
    var nm = (m && m.name) ? m.name : (sx ? (sx.label || 'seen') : 'not seen');
    tip.appendChild(document.createTextNode(' ' + nm));
    tip.style.left = e.clientX + 'px'; tip.style.top = e.clientY + 'px'; tip.style.opacity = '1';
  }
  function hideSupTip() { var tip = document.getElementById('supTip'); if (tip) tip.style.opacity = '0'; }

  function downloadBlob(blob, name) {
    var a = document.createElement('a'); a.href = URL.createObjectURL(blob); a.download = name;
    document.body.appendChild(a); a.click(); setTimeout(function () { URL.revokeObjectURL(a.href); a.remove(); }, 1000);
  }
  function ts() { var d = new Date(); function p(n) { return (n < 10 ? '0' : '') + n; } return d.getFullYear() + p(d.getMonth() + 1) + p(d.getDate()) + '-' + p(d.getHours()) + p(d.getMinutes()) + p(d.getSeconds()); }
  function exportCsv() {
    var arr = windowedSamples();
    var lines = ['epoch_ms,flow,return,setpoint,modulation'];
    arr.forEach(function (s) { lines.push([s.t || '', s.flow, s.ret, s.sp, s.mod].map(function (x) { return x === null || x === undefined ? '' : x; }).join(',')); });
    downloadBlob(new Blob([lines.join('\n')], { type: 'text/csv' }), 'otgw-graph-' + ts() + '.csv');
  }
  function exportPng() {
    var svg = document.getElementById('gstrip'); if (!svg) return;
    var xml = new XMLSerializer().serializeToString(svg);
    var img = new Image();
    img.onload = function () {
      var c = document.createElement('canvas'); c.width = 780; c.height = 210;
      var ctx = c.getContext('2d'); ctx.fillStyle = '#0d1117'; ctx.fillRect(0, 0, 780, 210); ctx.drawImage(img, 0, 0);
      c.toBlob(function (blob) { if (blob) downloadBlob(blob, 'otgw-graph-' + ts() + '.png'); });
    };
    img.src = 'data:image/svg+xml;base64,' + btoa(unescape(encodeURIComponent(xml)));
  }
  // ---------- Monitor > Graph auto-save (TASK-971) ----------
  var gAutoPngTimer = null, gAutoCsvTimer = null;
  function toggleAutoPng(chip) {
    if (gAutoPngTimer) { clearInterval(gAutoPngTimer); gAutoPngTimer = null; chip.classList.remove('on'); }
    else { gAutoPngTimer = setInterval(exportPng, 300000); chip.classList.add('on'); }   // every 5 min
  }
  function toggleAutoCsv(chip) {
    if (gAutoCsvTimer) { clearInterval(gAutoCsvTimer); gAutoCsvTimer = null; chip.classList.remove('on'); }
    else { gAutoCsvTimer = setInterval(exportCsv, 300000); chip.classList.add('on'); }   // every 5 min
  }

  // ---------- Monitor > Log power-features (TASK-970) ----------
  function downloadLog() {
    downloadBlob(new Blob([logBuf.join('\n')], { type: 'text/plain' }), 'otgw-log-' + ts() + '.txt');
  }
  function toggleAutoDl(chip) {
    if (logAutoDlTimer) { clearInterval(logAutoDlTimer); logAutoDlTimer = null; chip.classList.remove('on'); }
    else { logAutoDlTimer = setInterval(downloadLog, 900000); chip.classList.add('on'); }   // every 15 min
  }
  function toggleStream(chip) {
    if (logStreamWriter) { try { logStreamWriter.close(); } catch (e) { } logStreamWriter = null; chip.classList.remove('on'); return; }
    if (!window.showSaveFilePicker) return;   // Chrome/Edge only (feature-detected in init)
    window.showSaveFilePicker({ suggestedName: 'otgw-log-' + ts() + '.txt', types: [{ description: 'Text log', accept: { 'text/plain': ['.txt'] } }] })
      .then(function (h) { return h.createWritable(); })
      .then(function (w) { logStreamWriter = w; chip.classList.add('on'); })
      .catch(function () { });
  }

  // ---------- Monitor > Statistics > gateway overrides + boiler-unsupported (TASK-969) ----------
  function statMuted(t) { var d = document.createElement('div'); d.className = 'ble-row'; d.style.color = 'var(--muted)'; d.textContent = t; return d; }
  function statRow(title, meta) {
    var row = document.createElement('div'); row.className = 'ble-row';
    var nm = document.createElement('div'); nm.className = 'ble-nm'; nm.textContent = title;
    if (meta) { var m = document.createElement('span'); m.className = 'ble-mac'; m.textContent = meta; nm.appendChild(m); }
    row.appendChild(nm); return row;
  }
  function fetchStatsPanels() {
    fetch(APIGW + 'v2/otgw/overrides').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      var el = document.getElementById('ovStatList'); if (!el) return; el.textContent = '';
      var rows = (j && Array.isArray(j.overrides)) ? j.overrides : [];
      if (!rows.length) { el.appendChild(statMuted('No active gateway overrides')); return; }
      rows.forEach(function (r) {
        var name = (r.friendly && r.friendly.length) ? r.friendly : (r.label || 'Unknown');
        var kind = (r.kind === 'answer') ? 'answer' : 'substituted';
        var val = (typeof r.value === 'number') ? r.value.toFixed(2) : r.value;
        el.appendChild(statRow('MsgID ' + r.id + ' · ' + name, kind + ' = ' + val + ' · ' + r.age_s + 's'));
      });
    }).catch(function () { });
    fetch(APIGW + 'v2/otgw/boiler-support').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      var el = document.getElementById('boilerUnsupList'); if (!el) return; el.textContent = '';
      var rd = (j && Array.isArray(j.unsupported_read)) ? j.unsupported_read : [];
      var wr = (j && Array.isArray(j.unsupported_write)) ? j.unsupported_write : [];
      if (!rd.length && !wr.length) { el.appendChild(statMuted('None — boiler supports all observed messages (or no boiler seen).')); return; }
      function add(arr, mode) { arr.forEach(function (e) { var name = (e.friendly && e.friendly.length) ? e.friendly : (e.label || 'Unknown'); el.appendChild(statRow('MsgID ' + e.id + ' · ' + name, mode)); }); }
      add(rd, 'read'); add(wr, 'write');
    }).catch(function () { });
  }

  // ---------- Monitor > Debug page (TASK-967) ----------
  function fetchDebug() {
    fetch(APIGW + 'v2/debug').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      var el = document.getElementById('dbgInfo'); if (!el) return;
      var d = (j && j.debug) || j || {};
      var keys = Object.keys(d);
      el.textContent = keys.length ? keys.map(function (k) { return k + '  =  ' + d[k]; }).join('\n') : 'No debug data';
    }).catch(function () { var el = document.getElementById('dbgInfo'); if (el) el.textContent = 'Failed to load debug info'; });
    fetch(APIGW + 'v2/device/crashlog').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      var el = document.getElementById('dbgCrash'); if (!el) return;
      var c = (j && j.crashlog) || {};
      if (!c.available) { el.textContent = 'No crash recorded.'; return; }
      el.textContent = ((c.summary || '') + '\n\n' + (c.details || '')).trim() || 'Crash recorded (no detail).';
    }).catch(function () { var el = document.getElementById('dbgCrash'); if (el) el.textContent = '—'; });
  }

  // ---------- Monitor > Log > send raw OTGW command (TASK-965) ----------
  function showCmdStatus(msg, ok) {
    var st = document.getElementById('otCmdStatus'); if (!st) return;
    st.textContent = msg; st.style.display = ''; st.classList.toggle('on', !!ok);
    clearTimeout(st._t); st._t = setTimeout(function () { st.style.display = 'none'; }, 2600);
  }
  function sendOtgwCmd() {
    var inp = document.getElementById('otCmdInput');
    var cmd = ((inp && inp.value) || '').trim();
    if (!cmd) { showCmdStatus('Enter a command', false); return; }
    fetch(APIGW + 'v2/otgw/commands', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command: cmd })
    }).then(function (r) {
      if (r.status === 202 || r.ok) { showCmdStatus('Sent: ' + cmd, true); if (inp) inp.value = ''; }
      else { showCmdStatus('Error ' + r.status, false); }
    }).catch(function () { showCmdStatus('Send failed', false); });
  }

  // ---------- init ----------
  function init() {
    initTheme();
    document.querySelectorAll('.seg').forEach(function (s) {
      s.addEventListener('click', function () { showPage(s.dataset.page); });
    });
    document.querySelectorAll('.cchip').forEach(function (s) {
      s.addEventListener('click', function () { showDesign(s.dataset.design); });
    });
    document.querySelectorAll('.subtab').forEach(function (s) {
      s.addEventListener('click', function () { showTab(s.dataset.tab); });
    });
    var ut = document.getElementById('uiToggle');
    if (ut) ut.addEventListener('click', gotoClassic);

    // Monitor > Log controls
    var lp = document.getElementById('logPause');
    if (lp) lp.addEventListener('click', function () { logPaused = !logPaused; lp.textContent = logPaused ? '▶ Resume' : '⏸ Pause'; });
    var ls = document.getElementById('logSearch');
    if (ls) ls.addEventListener('input', function () { logSearch = ls.value; if (isMonitorLogVisible()) renderLog(); });
    // TASK-965: send raw OTGW command (button + Enter key)
    var ocs = document.getElementById('otCmdSend'), oci = document.getElementById('otCmdInput');
    if (ocs) ocs.addEventListener('click', sendOtgwCmd);
    if (oci) oci.addEventListener('keydown', function (e) { if (e.key === 'Enter') { e.preventDefault(); sendOtgwCmd(); } });
    // TASK-970: Clear + Download (were rendered but unwired) + SAT-only / auto-download / stream chips
    var lcl = document.getElementById('logClear');
    if (lcl) lcl.addEventListener('click', function () { logBuf = []; logRecvTimes = []; renderLog(); });
    var ldl = document.getElementById('logDownload');
    if (ldl) ldl.addEventListener('click', downloadLog);
    var cso = document.getElementById('chipSatOnly');
    if (cso) cso.addEventListener('click', function () { logSatOnly = !logSatOnly; cso.classList.toggle('on', logSatOnly); if (isMonitorLogVisible()) renderLog(); });
    var cad = document.getElementById('chipAutoDl');
    if (cad) cad.addEventListener('click', function () { toggleAutoDl(cad); });
    var cst = document.getElementById('chipStream');
    if (cst) { if (!window.showSaveFilePicker) { cst.style.opacity = '.5'; cst.title = 'Stream to file needs Chrome / Edge'; } cst.addEventListener('click', function () { toggleStream(cst); }); }
    var cs = document.getElementById('chipScroll');
    if (cs) cs.addEventListener('click', function () { logAutoScroll = !logAutoScroll; cs.classList.toggle('on', logAutoScroll); });
    var ct = document.getElementById('chipTs');
    if (ct) ct.addEventListener('click', function () { logShowTs = !logShowTs; ct.classList.toggle('on', logShowTs); if (isMonitorLogVisible()) renderLog(); });
    document.querySelectorAll('#mlog .tbtn').forEach(function (b) {
      if (/clear/i.test(b.textContent)) b.addEventListener('click', function () { logBuf = []; renderLog(); });
    });
    var ss = document.getElementById('statsSearch');
    if (ss) ss.addEventListener('input', function () { statsSearch = ss.value; if (isMonitorVisible('mstats')) renderStats(); });

    // Settings controls
    var setS = document.getElementById('setSearch');
    if (setS) setS.addEventListener('input', function () { setSearch = setS.value; renderSettings(); });
    var bSave = document.getElementById('btnSave'); if (bSave) bSave.addEventListener('click', saveSettings);
    var bDisc = document.getElementById('btnDiscard'); if (bDisc) bDisc.addEventListener('click', discardSettings);

    var sp = localStorage.getItem('otgw-v2-page'); showPage(sp || 'home');
    var sd = localStorage.getItem('otgw-v2-design'); showDesign(sd || 'a');
    renderConnStrip();
    tick();
    setInterval(tick, 1000);

    // Live OT data: connect the log WebSocket and render the active Home concept.
    connectWs();
    renderActive();

    // Connectivity: poll device health for the strip + connection map. Prefetch
    // settings once so the connectivity model knows which integrations are disabled
    // (MQTT off must read grey "Off", not a red "Disconnected", on the first Home
    // render — setData is otherwise empty until the Settings page is opened).
    fetchSettings();
    fetchConn();
    setInterval(fetchConn, 15000);

    // P9 wiring -----------------------------------------------------------
    document.querySelectorAll('#applToggle .seg2btn').forEach(function (b) {
      b.addEventListener('click', function () { setSource(b.dataset.appl === 'hp' ? 2 : 1); });
    });
    var bU = document.getElementById('bUp'); if (bU) bU.addEventListener('click', function () { stepTarget(0.5); });
    var bD = document.getElementById('bDown'); if (bD) bD.addEventListener('click', function () { stepTarget(-0.5); });
    document.querySelectorAll('#statsTable thead th').forEach(function (th, idx) {
      var cols = ['id', 'label', 'dir', 'interval', 'count', 'value'];
      th.style.cursor = 'pointer';
      th.addEventListener('click', function () {
        var col = cols[idx] || 'id';
        if (statsSort.col === col) statsSort.asc = !statsSort.asc; else { statsSort.col = col; statsSort.asc = true; }
        if (isMonitorVisible('mstats')) renderStats();
      });
    });
    document.querySelectorAll('#mlog .tbtn').forEach(function (b) {
      if (/download/i.test(b.textContent)) b.addEventListener('click', function () {
        downloadBlob(new Blob([logBuf.join('\n')], { type: 'text/plain' }), 'otgw-log-' + ts() + '.txt');
      });
    });
    document.querySelectorAll('#mgraph .tchip:not([id])').forEach(function (ch) {
      ch.addEventListener('click', function () {
        // Parse the leading number + unit. indexOf('4') used to match the '4' in
        // '24 h' and silently apply a 4 h window to the 24 h chip.
        var t = ch.textContent.toLowerCase();
        var n = parseInt(t, 10) || 1;
        graphWindowMs = /min/.test(t) ? n * 60000 : n * 3600000;
        document.querySelectorAll('#mgraph .tchip:not([id])').forEach(function (x) { x.classList.toggle('on', x === ch); });
        if (isMonitorVisible('mgraph')) renderGraph();
      });
    });
    document.querySelectorAll('#mgraph .tbtn').forEach(function (b) {
      if (/png/i.test(b.textContent)) b.addEventListener('click', exportPng);
      else if (/csv/i.test(b.textContent)) b.addEventListener('click', exportCsv);
    });
    var apng = document.getElementById('chipAutoPng');   // TASK-971
    if (apng) apng.addEventListener('click', function () { toggleAutoPng(apng); });
    var acsv = document.getElementById('chipAutoCsv');
    if (acsv) acsv.addEventListener('click', function () { toggleAutoCsv(acsv); });
    fetchSeed(); fetchSatStatus(); fetchWeather();
    setInterval(function () { if (!ws || ws.readyState !== 1) fetchSeed(); }, 20000);
    setInterval(fetchSatStatus, 30000);
    setInterval(fetchWeather, 60000);   // TASK-954: refresh weather-API OUTSIDE

    // Hooks for later phases to attach live-data renderers.
    window.OTGWv2 = {
      APIGW: APIGW, showPage: showPage, showDesign: showDesign, showTab: showTab,
      model: model, renderActive: renderActive
    };
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init);
  else init();
})();
