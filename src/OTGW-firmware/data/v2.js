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
    if (b) b.textContent = dark ? '☀ Light' : '🌙 Dark';
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
    else if (t === 'mstats') renderStats();
    else if (t === 'msupport') renderSupport();
    else if (t === 'mgraph') renderGraph();
    else if (t === 'mconn') fetchConn();
  }

  // ---------- back to the classic UI ----------
  function gotoClassic() {
    // TASK-923: persist the choice device-side in settings.ini (ui_usev2=false)
    // via the settings API, then reload — the firmware serves the classic UI.
    fetch(APIGW + 'v2/settings', {
      method: 'POST', mode: 'cors',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: 'ui_usev2', value: 'false' })
    }).catch(function () { }).finally(function () { location.href = '/'; });
  }

  // ---------- clock ----------
  function tick() {
    var el = document.getElementById('clock');
    if (!el) return;
    var d = new Date();
    function p(n) { return (n < 10 ? '0' : '') + n; }
    el.textContent = p(d.getHours()) + ':' + p(d.getMinutes()) + ':' + p(d.getSeconds());
  }

  // ---------- always-visible connectivity strip (live; fed by fetchConn) ----------
  var STATE_TXT = { 'st-ok': 'up', 'st-warn': 'weak', 'st-down': 'down', 'st-off': 'off', 'st-unknown': '—' };
  function renderConnStrip() {
    var strip = document.getElementById('connStrip');
    if (!strip) return;
    var pills = [
      { k: 'WiFi', s: conn.wifi, v: conn.rssi !== null ? conn.rssi + ' dBm' : STATE_TXT[conn.wifi] },
      { k: 'MQTT', s: conn.mqtt, v: STATE_TXT[conn.mqtt] },
      { k: 'OT bus', s: conn.ot, v: STATE_TXT[conn.ot] },
      { k: 'NTP', s: conn.ntp, v: STATE_TXT[conn.ntp] }
    ];
    strip.textContent = '';   // rebuild with DOM nodes (textContent — no HTML injection)
    pills.forEach(function (p) {
      var btn = document.createElement('button'); btn.className = 'connpill ' + p.s;
      btn.title = 'Connectivity — tap for the detail map';
      btn.addEventListener('click', function () { showPage('monitor'); showTab('mconn'); });
      var dot = document.createElement('span'); dot.className = 'd';
      var k = document.createElement('span'); k.className = 'pk'; k.textContent = p.k + ' ';
      var v = document.createElement('span'); v.className = 'pv'; v.textContent = p.v;
      btn.appendChild(dot); btn.appendChild(k); btn.appendChild(v);
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
    flame: false, ch: false, dhw_on: false, fault: false, fresh: false
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
      case 27: if (isAck) { model.outside = f88(u16); changed = true; } break;    // Toutside
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
    ticker.push(line);
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
  }
  function renderSupport() {
    var grid = document.getElementById('supMatrix'); if (!grid) return;
    if (grid.childElementCount !== 128) {
      grid.innerHTML = '';
      for (var i = 0; i < 128; i++) {
        var c = document.createElement('div'); c.className = 'mcellq'; c.dataset.id = i; c.textContent = i;
        c.addEventListener('click', (function (id) { return function () { showSupportDetail(id); }; })(i));
        c.addEventListener('mouseenter', (function (id) { return function (e) { showSupTip(id, e); }; })(i));
        c.addEventListener('mousemove', (function (id) { return function (e) { showSupTip(id, e); }; })(i));
        c.addEventListener('mouseleave', hideSupTip);
        grid.appendChild(c);
      }
    }
    var seen = 0;
    for (var id = 0; id < 128; id++) {
      var cell = grid.children[id], s = stats[id];
      cell.className = 'mcellq' + (s ? (s.dirT && s.dirB ? ' both' : (s.dirT ? ' tonly' : ' bonly')) : '');
      if (s) seen++;
    }
    var sc = document.getElementById('supCount'); if (sc) sc.textContent = seen;
  }
  function showSupportDetail(id) {
    var el = document.getElementById('supDetail'); if (!el) return;
    var s = stats[id];
    el.innerHTML = '';
    var top = document.createElement('div'); top.className = 'sd-top';
    var num = document.createElement('span'); num.className = 'sd-num'; num.textContent = id; top.appendChild(num);
    var hex = document.createElement('span'); hex.className = 'sd-hex'; hex.textContent = '0x' + ('0' + id.toString(16).toUpperCase()).slice(-2); top.appendChild(hex);
    el.appendChild(top);
    var h5 = document.createElement('h5'); h5.textContent = s ? (s.label || ('ID ' + id)) : ('ID ' + id + ' — not observed'); el.appendChild(h5);
    if (s) {
      var dl = document.createElement('dl');
      [['Value', s.value || '—'], ['Direction', dirBadge(s)[0]], ['Count', '' + s.count], ['Interval', s.interval ? (s.interval / 1000).toFixed(1) + 's' : '—']]
        .forEach(function (kv) { var dt = document.createElement('dt'); dt.textContent = kv[0]; var dd = document.createElement('dd'); dd.textContent = kv[1]; dl.appendChild(dt); dl.appendChild(dd); });
      el.appendChild(dl);
    } else {
      var hint = document.createElement('div'); hint.className = 'sd-hint'; hint.textContent = 'This message ID has not been seen on the bus yet.'; el.appendChild(hint);
    }
  }

  // ---------- Monitor > Log ----------
  var LOG_MAX = 600, logBuf = [], logRecvTimes = [];
  var logPaused = false, logSearch = '', logShowTs = true, logAutoScroll = true;
  function pushLog(line) {
    var now = Date.now();
    logRecvTimes.push(now);
    while (logRecvTimes.length && now - logRecvTimes[0] > 10000) logRecvTimes.shift();
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

  function renderA() {
    var svg = document.getElementById('schem');
    if (svg) {
      svg.classList.toggle('flame-on', model.flame);
      svg.classList.toggle('ch-on', model.ch);
      svg.classList.toggle('dhw-on', model.dhw_on);
      svg.classList.toggle('fault', model.fault);
      // burner height = modulation (0..1 on --fy, consumed by .burnscale)
      if (model.mod !== null) svg.style.setProperty('--fy', Math.max(0.12, Math.min(1, model.mod / 100)).toFixed(2));
    }
    txt('aFlow', fmt(model.flow, 1, '°'));
    txt('aRet', fmt(model.ret, 1, '°'));
    txt('aDhw', model.dhw === null ? '—' : Math.round(model.dhw) + '°');
    txt('aOut', fmt(model.outside, 1, '°'));
    txt('aRoom', fmt(model.room, 1, '°'));
    txt('aRoomSet', model.roomSet === null ? '' : 'set ' + fmt(model.roomSet, 1, '°'));
    txt('aModBig', model.mod === null ? '—' : Math.round(model.flame ? model.mod : 0) + '%');
    txt('aLcd', 'CH ' + (model.mod === null ? '--' : Math.round(model.mod)) + '%');
    txt('aPress', fmt(model.pressure, 2, ' bar'));
    if (model.flow !== null && model.ret !== null) txt('aDt', 'ΔT ' + fmt(model.flow - model.ret, 1, '°'));
    // pressure needle: 0..4 bar over a ~150° sweep centred on the gauge.
    var needle = document.getElementById('aPressNeedle');
    if (needle && model.pressure !== null) {
      var deg = -75 + Math.max(0, Math.min(4, model.pressure)) / 4 * 150;
      needle.setAttribute('transform', 'rotate(' + deg.toFixed(1) + ',150,322)');
    }
    var statusTxt = model.flame ? (model.dhw_on ? 'Heating water' : 'Heating') : (model.dhw_on ? 'Hot water' : 'Idle');
    txt('aStatusTxt', statusTxt);
    var aStatus = document.getElementById('aStatus');
    if (aStatus) aStatus.classList.toggle('heating', model.flame);
    setTile('aTFlame', model.flame, false, model.flame ? 'On' : 'Off');
    setTile('aTCH', model.ch, false, model.ch ? 'Active' : 'Off');
    setTile('aTDHW', model.dhw_on, false, model.dhw_on ? 'Active' : 'Off');
    setTile('aTFault', false, model.fault, model.fault ? 'Fault' : 'OK');
  }

  // Concept B — at a glance (hero dial + stat cards).
  var B_LO = 135, B_HI = 405, B_MIN = 5, B_MAX = 30;  // 270° sweep, 5..30 °C
  function renderB() {
    var track = document.getElementById('bTrack');
    if (track && !track.getAttribute('data-init')) {
      track.setAttribute('d', arcPath(120, 120, 100, B_LO, B_HI));
      track.setAttribute('data-init', '1');
    }
    var arc = document.getElementById('bArc');
    if (arc) {
      if (model.room === null) arc.setAttribute('d', '');
      else {
        var f = Math.max(0, Math.min(1, (model.room - B_MIN) / (B_MAX - B_MIN)));
        arc.setAttribute('d', arcPath(120, 120, 100, B_LO, B_LO + (B_HI - B_LO) * f));
      }
    }
    var tick = document.getElementById('bTick');
    if (tick) {
      if (model.roomSet === null) { tick.setAttribute('x1', 0); tick.setAttribute('y1', 0); tick.setAttribute('x2', 0); tick.setAttribute('y2', 0); }
      else {
        var sf = Math.max(0, Math.min(1, (model.roomSet - B_MIN) / (B_MAX - B_MIN)));
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
    txt('bOut', fmt(model.outside, 1, '°'));
    var fill = document.getElementById('bModFill');
    if (fill) fill.style.height = (model.mod === null ? 0 : Math.max(0, Math.min(100, model.mod))) + '%';
    var dot = document.getElementById('bPressDot');
    if (dot && model.pressure !== null) dot.style.left = Math.max(0, Math.min(100, model.pressure / 4 * 100)) + '%';
    var flame = document.getElementById('bFlame'); if (flame) flame.classList.toggle('on', model.flame);
    var st = document.getElementById('bStatus'); if (st) st.classList.toggle('heating', model.flame);
    txt('bStatusTxt', model.flame ? (model.dhw_on ? 'Heating water' : 'Heating') : (model.dhw_on ? 'Hot water' : 'Idle'));
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
  function renderCGrid() {
    var grid = document.getElementById('cGrid'); if (!grid) return;
    var cells = [
      ['FLOW', fmt(model.flow, 1, '°'), 'hot'],
      ['RETURN', fmt(model.ret, 1, '°'), 'cold'],
      ['ΔT', (model.flow !== null && model.ret !== null) ? fmt(model.flow - model.ret, 1, '°') : '—', ''],
      ['MODULATION', model.mod === null ? '—' : Math.round(model.mod) + '%', model.flame ? 'ok' : ''],
      ['CH SETPOINT', fmt(model.chSet, 1, '°'), ''],
      ['DHW', model.dhw === null ? '—' : fmt(model.dhw, 1, '°'), ''],
      ['PRESSURE', fmt(model.pressure, 2, ''), ''],
      ['FLAME', model.flame ? 'ON' : 'off', model.flame ? 'ok' : '']
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
    // textContent (not innerHTML) — raw frames are device data, treat as text.
    t.textContent = ticker.join('\n');
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

  // ---------- Monitor > Connection map + (P4) connectivity strip ----------
  var conn = { wifi: 'st-unknown', mqtt: 'st-unknown', ot: 'st-unknown', ntp: 'st-unknown',
               mode: '', rssi: null, mqttOn: false, otOn: false };
  function fetchConn() {
    fetch(APIGW + 'v2/health').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      if (!j || !j.health) return;
      var h = j.health;
      conn.mode = h.networkmode || ''; conn.rssi = (typeof h.wifirssi === 'number') ? h.wifirssi : null;
      conn.mqttOn = !!h.mqttconnected; conn.otOn = !!h.otgwconnected;
      conn.wifi = h.networkmode ? (conn.rssi !== null && conn.rssi < -80 ? 'st-warn' : 'st-ok') : 'st-down';
      conn.mqtt = h.mqttconnected ? 'st-ok' : 'st-down';
      conn.ot = h.otgwconnected ? 'st-ok' : 'st-down';
      conn.ntp = 'st-ok'; // device clock is advancing (NTP-driven); refined later if an endpoint exposes it
      renderConnMap(); renderConnStrip();
    }).catch(function () { });
  }
  function setLink(id, flowId, state) {
    var lk = document.getElementById(id);
    if (lk) lk.setAttribute('class', 'cn-link lk-' + state.replace('st-', ''));
    var fl = document.getElementById(flowId);
    if (fl) fl.classList.toggle('on', state === 'st-ok' || state === 'st-warn');
  }
  function setDot(id, state) { var d = document.getElementById(id); if (d) d.setAttribute('class', 'cn-statedot ' + state); }
  function renderConnMap() {
    setLink('lk-wifi', 'fl-wifi', conn.wifi); setDot('dot-wifi', conn.wifi);
    setLink('lk-mqtt', 'fl-mqtt', conn.mqtt); setDot('dot-mqtt', conn.mqtt);
    setLink('lk-ws', 'fl-ws', ws && ws.readyState === 1 ? 'st-ok' : 'st-down');
    setLink('lk-boiler', 'fl-boiler', conn.ot); setDot('dot-boiler', conn.ot);
    setLink('lk-therm', 'fl-therm', conn.ot); setDot('dot-therm', conn.ot);
    var ws2 = document.getElementById('cnWifiSub'); if (ws2) ws2.textContent = conn.rssi === null ? conn.mode : (conn.mode + ' · ' + conn.rssi + ' dBm');
    var md = document.getElementById('cnModeTxt'); if (md) md.textContent = conn.mode || '—';
    renderConnDetail();
  }

  // ---------- Settings (built from GET /api/v2/settings) ----------
  // Categorize flat setting keys by prefix into titled groups.
  var SET_GROUPS = [
    { id: 'device', label: 'Device', icon: '🖥', test: function (k) { return /^(hostname|ssid|ledblink|darktheme|mydebug|nightlyrestart|restarthour|boardmode|httppasswd)/.test(k); } },
    { id: 'network', label: 'Network', icon: '📶', test: function (k) { return /^wifi|^eth/.test(k); } },
    { id: 'mqtt', label: 'MQTT', icon: '📨', test: function (k) { return /^mqtt|^legacyport/.test(k); } },
    { id: 'ntp', label: 'Time / NTP', icon: '🕐', test: function (k) { return /^ntp/.test(k); } },
    { id: 'sat', label: 'SAT thermostat', icon: '🌡', test: function (k) { return /^sat/.test(k); } },
    { id: 'sensors', label: 'Sensors', icon: '🌡', test: function (k) { return /^gpiosensors|^s0|^dallas|^sensor/.test(k); } },
    { id: 'outputs', label: 'Outputs', icon: '🔌', test: function (k) { return /^gpiooutputs|^output/.test(k); } },
    { id: 'webhook', label: 'Webhook', icon: '🪝', test: function (k) { return /^webhook/.test(k); } },
    { id: 'otd', label: 'OT-Direct', icon: '⚙', test: function (k) { return /^otd/.test(k); } },
    { id: 'ui', label: 'Web UI', icon: '🎛', test: function (k) { return /^ui_/.test(k); } },
    { id: 'other', label: 'Other', icon: '•', test: function () { return true; } }
  ];
  var setData = {};   // key -> {value,type,...} as fetched
  var setDirty = {};  // key -> new value (string)
  var setSearch = '';
  function groupFor(k) { for (var i = 0; i < SET_GROUPS.length; i++) if (SET_GROUPS[i].test(k)) return SET_GROUPS[i].id; return 'other'; }
  function fetchSettings() {
    fetch(APIGW + 'v2/settings').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      if (!j) return;
      // settings come as { key: {value, type, ...}, ... } possibly nested under a root
      var src = j.settings || j;
      setData = {}; setDirty = {};
      Object.keys(src).forEach(function (k) {
        var v = src[k];
        if (v && typeof v === 'object' && 'value' in v) setData[k] = v;
      });
      renderSettings();
      fetchBle();
    }).catch(function () { });
  }
  function fieldDirty(k) { return Object.prototype.hasOwnProperty.call(setDirty, k); }
  function curVal(k) { return fieldDirty(k) ? setDirty[k] : ('' + setData[k].value); }
  function renderSettings() {
    var rail = document.getElementById('setRail'), cols = document.getElementById('setCols');
    if (!rail || !cols) return;
    // bucket keys
    var buckets = {};
    Object.keys(setData).forEach(function (k) { (buckets[groupFor(k)] = buckets[groupFor(k)] || []).push(k); });
    var q = setSearch.trim().toLowerCase();
    rail.textContent = ''; cols.textContent = '';
    SET_GROUPS.forEach(function (g) {
      var keys = (buckets[g.id] || []).filter(function (k) { return !q || k.toLowerCase().indexOf(q) !== -1; });
      if (!keys.length) return;
      // rail item
      var ri = document.createElement('div'); ri.className = 'rail-item'; ri.textContent = g.icon + ' ' + g.label;
      var cnt = document.createElement('span'); cnt.className = 'cnt'; cnt.textContent = keys.length; ri.appendChild(cnt);
      ri.addEventListener('click', function () {
        var card = document.getElementById('setcard-' + g.id);
        if (card) card.scrollIntoView({ behavior: 'smooth', block: 'start' });
        document.querySelectorAll('.rail-item').forEach(function (x) { x.classList.remove('active'); });
        ri.classList.add('active');
      });
      rail.appendChild(ri);
      // card
      var card = document.createElement('div'); card.className = 'set-group'; card.id = 'setcard-' + g.id;
      var h = document.createElement('h3'); h.textContent = g.label; card.appendChild(h);
      keys.sort().forEach(function (k) { card.appendChild(settingRow(k)); });
      cols.appendChild(card);
    });
    if (bleData) renderBleCard();
    updateSaveBar();
  }
  function settingRow(k) {
    var meta = setData[k], type = meta.type || 's';
    var row = document.createElement('div'); row.className = 'srow' + (fieldDirty(k) ? ' dirty' : '');
    row.dataset.key = k;
    var lbl = document.createElement('div'); lbl.className = 'slbl'; lbl.textContent = k; row.appendChild(lbl);
    var input;
    if (type === 'b') {
      input = document.createElement('input'); input.type = 'checkbox'; input.className = 'sw';
      input.checked = curVal(k) === 'true' || curVal(k) === '1';
      input.addEventListener('change', function () { markDirty(k, input.checked ? 'true' : 'false', row); });
    } else if (type === 'r') {
      input = document.createElement('input'); input.type = 'text'; input.value = curVal(k); input.disabled = true;
    } else if (ENUM_OPTS[k]) {
      input = document.createElement('select');
      ENUM_OPTS[k].forEach(function (o) {
        var opt = document.createElement('option'); opt.value = '' + o[0]; opt.textContent = o[1]; input.appendChild(opt);
      });
      input.value = '' + (parseInt(curVal(k), 10) || 0);
      input.addEventListener('change', function () { markDirty(k, input.value, row); });
    } else {
      input = document.createElement('input');
      input.type = (type === 'p') ? 'password' : (type === 'i') ? 'number' : 'text';
      input.value = curVal(k);
      input.addEventListener('input', function () { markDirty(k, input.value, row); });
    }
    row.appendChild(input);
    return row;
  }
  function markDirty(k, val, row) {
    if (('' + setData[k].value) === val) delete setDirty[k]; else setDirty[k] = val;
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
  var bleData = null;
  function fetchBle() {
    fetch(APIGW + 'v2/sat/ble/discovery').then(function (r) { return r.ok ? r.json() : null; })
      .then(function (j) { if (j) { bleData = j; renderBleCard(); } }).catch(function () { });
  }
  function bleAction(path, body) {
    return fetch(APIGW + 'v2/sat/ble/' + path, {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: body ? JSON.stringify(body) : undefined
    }).then(function () { setTimeout(fetchBle, 500); }).catch(function () { });
  }
  function renderBleCard() {
    var cols = document.getElementById('setCols'); if (!cols || !bleData) return;
    var old = document.getElementById('setcard-ble'); if (old) old.remove();
    var card = document.createElement('div'); card.className = 'set-group'; card.id = 'setcard-ble';
    var h = document.createElement('h3'); h.textContent = 'BLE sensors'; card.appendChild(h);
    var desc = document.createElement('div'); desc.className = 'ble-desc';
    desc.textContent = (bleData.populated_slots || 0) + ' / ' + (bleData.max_slots || 0) + ' slots used' +
      (bleData.name_prefix ? ' · prefix "' + bleData.name_prefix + '"' : '');
    card.appendChild(desc);
    var rescan = document.createElement('button'); rescan.className = 'tbtn'; rescan.textContent = '🔄 Rescan';
    rescan.addEventListener('click', function () { bleAction('rescan'); });
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
    satsystem: [[0, 'Auto'], [1, 'Radiators'], [2, 'Underfloor']]
  };

  function fetchSeed() {
    fetch(APIGW + 'v2/otgw/otmonitor').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      var o = j && j.otmonitor; if (!o) return;
      function num(k) { var e = o[k]; if (!e || e.value === undefined) return null; var v = parseFloat(e.value); return isNaN(v) ? null : v; }
      function on(k) { var e = o[k]; if (!e || e.value === undefined) return null; return /on|true|1/i.test('' + e.value); }
      var map = { flow: 'boilertemperature', ret: 'returnwatertemperature', dhw: 'dhwtemperature', outside: 'outsidetemperature', room: 'roomtemperature', roomSet: 'roomsetpoint', chSet: 'controlsetpoint', mod: 'relmodlvl', pressure: 'chwaterpressure' };
      var changed = false;
      Object.keys(map).forEach(function (f) { var v = num(map[f]); if (v !== null) { model[f] = v; changed = true; } });
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

  function renderConnDetail() {
    var box = document.getElementById('connDetail'); if (!box) return;
    var wsUp = ws && ws.readyState === 1;
    var rows = [
      { k: 'WiFi / network', s: conn.wifi, d: (conn.mode || '?') + (conn.rssi !== null ? ' · ' + conn.rssi + ' dBm' : '') },
      { k: 'WebSocket (live log)', s: wsUp ? 'st-ok' : 'st-down', d: wsUp ? 'connected' : 'disconnected — values seeded from REST' },
      { k: 'MQTT broker', s: conn.mqtt, d: conn.mqttOn ? 'connected' : 'not connected' },
      { k: 'OpenTherm bus', s: conn.ot, d: conn.otOn ? 'online' : 'no data' },
      { k: 'NTP / clock', s: conn.ntp, d: 'time syncing' }
    ];
    box.textContent = '';
    rows.forEach(function (r) {
      var row = document.createElement('div'); row.className = 'connrow ' + r.s;
      var b = document.createElement('span'); b.className = 'badge'; b.textContent = STATE_TXT[r.s] || '—';
      var body = document.createElement('div'); body.className = 'body';
      var nm = document.createElement('div'); nm.className = 'nm'; nm.textContent = r.k;
      var det = document.createElement('div'); det.className = 'det'; det.textContent = r.d;
      body.appendChild(nm); body.appendChild(det); row.appendChild(b); row.appendChild(body); box.appendChild(row);
    });
  }

  function showSupTip(id, e) {
    var tip = document.getElementById('supTip'); if (!tip) return;
    var sx = stats[id];
    tip.textContent = '';
    var b = document.createElement('b'); b.textContent = id; tip.appendChild(b);
    tip.appendChild(document.createTextNode(' ' + (sx ? (sx.label || 'seen') : 'not seen')));
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

    // Connectivity: poll device health for the strip + connection map.
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
    document.querySelectorAll('#mgraph .tchip').forEach(function (ch) {
      ch.addEventListener('click', function () {
        var t = ch.textContent.toLowerCase();
        graphWindowMs = t.indexOf('10') >= 0 ? 600000 : t.indexOf('4') >= 0 ? 14400000 : t.indexOf('24') >= 0 ? 86400000 : 3600000;
        document.querySelectorAll('#mgraph .tchip').forEach(function (x) { x.classList.toggle('on', x === ch); });
        if (isMonitorVisible('mgraph')) renderGraph();
      });
    });
    document.querySelectorAll('#mgraph .tbtn').forEach(function (b) {
      if (/png/i.test(b.textContent)) b.addEventListener('click', exportPng);
      else if (/csv/i.test(b.textContent)) b.addEventListener('click', exportCsv);
    });
    fetchSeed(); fetchSatStatus();
    setInterval(function () { if (!ws || ws.readyState !== 1) fetchSeed(); }, 20000);
    setInterval(fetchSatStatus, 30000);

    // Hooks for later phases to attach live-data renderers.
    window.OTGWv2 = {
      APIGW: APIGW, showPage: showPage, showDesign: showDesign, showTab: showTab,
      model: model, renderActive: renderActive
    };
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init);
  else init();
})();
