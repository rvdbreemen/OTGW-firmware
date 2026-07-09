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

  // ---------- fetch with 503-retry (TASK-1033) ----------
  // The REST backpressure gate (ADR-165) 503s transiently during load bursts.
  // Retry only on HTTP 503 and network errors, with doubling backoff, and return
  // the final Response (or rethrow the final network error). Retries are strictly
  // sequential — the next attempt fires only after the previous one failed — so
  // this never adds request concurrency (the N<=2 cap stays intact).
  function fetchWithRetry(url, opts, tries, delayMs) {
    tries = (tries === undefined) ? 3 : tries;
    delayMs = (delayMs === undefined) ? 400 : delayMs;
    function again() {
      return new Promise(function (res) { setTimeout(res, delayMs); })
        .then(function () { return fetchWithRetry(url, opts, tries - 1, delayMs * 2); });
    }
    return fetch(url, opts).then(
      function (r) { return (r.status === 503 && tries > 1) ? again() : r; },
      function (err) { if (tries > 1) return again(); throw err; }
    );
  }

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
    ['home', 'sat', 'monitor', 'settings', 'advanced'].forEach(function (k) {
      var el = document.getElementById('page-' + k);
      if (el) el.classList.toggle('active', k === p);
    });
    try { localStorage.setItem('otgw-v2-page', p); } catch (e) { }
    // TASK-986: the SAT page polls /api/v2/sat/status only while it is visible.
    // Stop the poll on every navigation, then (re)arm it when entering the page.
    satPageStop();
    if (p === 'monitor' && isMonitorLogVisible()) renderLog();
    if (p === 'settings') fetchSettings();
    if (p === 'sat') satPageStart();
    // TASK-982: entering Advanced re-asserts its active sub-tab (restore-on-refresh,
    // TASK-808) and loads that tab's data on demand.
    if (p === 'advanced') {
      var at = null; try { at = localStorage.getItem('otgw-v2-advtab'); } catch (e) { }
      showAdvTab(at || 'pic');
    }
  }

  // ---------- Home concept chips (A/B/C) ----------
  var VIEW_LABELS = { a: 'System view', b: 'At a glance', c: 'Mission control' };
  function showDesign(d) {
    document.querySelectorAll('.vp-item').forEach(function (s) {
      s.classList.toggle('active', s.dataset.design === d);
    });
    ['a', 'b', 'c'].forEach(function (k) {
      var el = document.getElementById('design-' + k);
      if (el) el.classList.toggle('active', k === d);
    });
    var lbl = document.getElementById('viewPickLabel');
    if (lbl) lbl.textContent = VIEW_LABELS[d] || VIEW_LABELS.a;
    try { localStorage.setItem('otgw-v2-design', d); } catch (e) { }
    if (typeof renderActive === 'function') renderActive();
  }
  function closeViewMenu() {
    var m = document.getElementById('viewPickMenu'); if (m) m.classList.remove('show');
    var b = document.getElementById('viewPickBtn'); if (b) b.setAttribute('aria-expanded', 'false');
  }

  // ---------- Monitor sub-tabs ----------
  // Scoped to #page-monitor so the Advanced page's own .subtab/.mpanel elements
  // (TASK-982) are never toggled by this switcher, and vice-versa.
  function showTab(t) {
    document.querySelectorAll('#page-monitor .subtab').forEach(function (s) {
      s.classList.toggle('active', s.dataset.tab === t);
    });
    document.querySelectorAll('#page-monitor .mpanel').forEach(function (el) {
      el.classList.toggle('active', el.id === t);
    });
    if (t === 'mlog') renderLog();
    else if (t === 'mstats') { renderStats(); fetchStatsPanels(); }
    else if (t === 'msupport') renderSupport();
    else if (t === 'mgraph') renderGraph();
    else if (t === 'mconn') { fetchConn(); fetchOtdOvr(); }
  }

  // ---------- Advanced sub-tabs (TASK-982): PIC / Debug / File System / System ----------
  function showAdvTab(k) {
    document.querySelectorAll('#advTabs .subtab').forEach(function (s) {
      s.classList.toggle('active', s.dataset.adv === k);
    });
    ['pic', 'debug', 'files', 'system'].forEach(function (x) {
      var el = document.getElementById('adv-' + x);
      if (el) el.classList.toggle('active', x === k);
    });
    try { localStorage.setItem('otgw-v2-advtab', k); } catch (e) { }
    // Fetch-on-demand: each sub-tab loads its own data when opened, not on page init.
    if (k === 'pic') { fetchPic(); fetchPicSettings(); picUpdatePoll(); }
    else if (k === 'debug') fetchDebug();
    else if (k === 'files') fetchFsList(fsCurrentPath);
    else if (k === 'system') fetchSystemStatus();
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
  var simActive = false;   // TASK-982: device/info otgwsimulation, cached via withDeviceInfo
  // TASK-983: header network chip — static identity from the one-shot device/info
  // snapshot; the Wi-Fi bars + dBm refresh live from connRssi (the /health poll).
  var hdrNet = { ip: '', ssid: '', quality: 0, kind: 'wifi' };   // kind: wifi | eth | ap
  function renderConnStrip() {
    var strip = document.getElementById('connStrip'); if (!strip) return;
    // Single worst-of health-summary pill; the per-node detail lives on Monitor › Connection.
    // Mode is excluded from the roll-up — it moved to the header status pill below.
    var nodes = ['ws', 'mqtt', 'wifi', 'boiler', 'therm'];
    var worst = 'ok', issues = 0;
    nodes.forEach(function (k) {
      var c = CONN[k]; if (!c) return;
      if (c.s === 'st-down') { worst = 'down'; issues++; }
      else if (c.s === 'st-warn') { if (worst !== 'down') worst = 'warn'; issues++; }
    });
    var cls = worst === 'down' ? 'down' : (worst === 'warn' ? 'warn' : '');
    var label = worst === 'down' ? (issues + ' connection issue' + (issues > 1 ? 's' : '')) :
                (worst === 'warn' ? (issues + ' degraded') : 'All systems connected');
    strip.textContent = '';   // rebuild with DOM nodes (textContent — no HTML injection)
    var btn = document.createElement('button');
    btn.className = 'connsum' + (cls ? ' ' + cls : '');
    btn.title = 'Connectivity health — open the Connection map';
    btn.addEventListener('click', function () { showPage('monitor'); showTab('mconn'); });
    var dot = document.createElement('span'); dot.className = 'd';
    var lbl = document.createElement('span'); lbl.textContent = label;
    var go = document.createElement('span'); go.className = 'go'; go.textContent = '· Connection ›';
    btn.appendChild(dot); btn.appendChild(lbl); btn.appendChild(go);
    strip.appendChild(btn);

    // Keep the firmware-style header status pill in sync with the live CONN model.
    // v2's mode vocabulary is wider than the mockup's gateway|monitor: 'detecting'
    // (PIC, mode not yet known) and 'n/a' also occur. 'n/a' is overloaded — it is set
    // both for OT-Direct hardware and for a PIC running non-gateway firmware — so we
    // disambiguate via CONN.ot.detail rather than asserting a hardware type blindly.
    var hs = document.getElementById('hdrStatus');
    if (hs) {
      var wd = (CONN.ws.s !== 'st-ok' && CONN.ws.s !== 'st-warn');
      hs.classList.toggle('down', wd);
      var mv = CONN.mode.value;
      var mode;
      if (mv === 'gateway') mode = 'Gateway';
      else if (mv === 'monitor') mode = 'Monitor';
      else if (mv === 'detecting') mode = 'Detecting…';
      else if (mv === 'n/a') mode = (CONN.ot.detail === 'OT-Direct') ? 'OT-Direct' : 'N/A';
      else mode = mv ? (mv.charAt(0).toUpperCase() + mv.slice(1)) : 'Unknown';
      hs.title = (wd ? 'Disconnected' : 'Connected') + ' · ' + mode + ' mode';
      var md = document.getElementById('hdrStatMode');
      if (md) md.textContent = (wd ? 'Disconnected' : mode);
      // TASK-982: keep the Advanced › System 'Device status' badges in sync from the
      // SAME wd/mode mapping (no drift). sysSim reflects the cached device/info flag.
      var sc = document.getElementById('sysConn');
      if (sc) { sc.classList.toggle('down', wd); sc.textContent = '● ' + (wd ? 'Disconnected' : 'Connected'); }
      var sm = document.getElementById('sysMode');
      if (sm) sm.textContent = '⇄ ' + mode;
      var ssim = document.getElementById('sysSim');
      if (ssim) ssim.classList.toggle('hidden', !simActive);
    }
  }

  // ---------- TASK-983: header identity chips (hostname·version + IP·Wi-Fi) ----------
  function wifiQualFromRssi(rssi) {
    // Mirror of the firmware's signal_quality_perc_quad() so the live bar count
    // matches the quality% device/info reports (which /health does not carry).
    var perfect = -50, worst = -85, n = perfect - worst;   // 35
    var q = Math.trunc((100 * n * n - (perfect - rssi) * (15 * n + 62 * (perfect - rssi))) / (n * n));
    return q > 100 ? 100 : (q < 1 ? 0 : q);
  }
  function fillHeaderIdentity(d) {
    // One-shot from the device/info snapshot at startup (never polled). hostname and
    // ssid are device-sourced strings — build with textContent, never innerHTML.
    var host = document.getElementById('hdrHost');
    if (host) {
      var hn = ('' + (d.hostname || '')).trim();
      var ver = ('' + (d.fwversion || '')).split('+')[0];   // drop +buildmetadata for a compact chip
      host.textContent = '';
      host.appendChild(document.createTextNode(hn || 'device'));
      var sm = document.createElement('small'); sm.textContent = '.local';
      host.appendChild(sm);
      host.appendChild(document.createTextNode(' · ' + (ver ? 'v' + ver : '—')));
    }
    // device/info distinguishes Wi-Fi, Ethernet ("Wired") and AP-fallback — render each honestly.
    hdrNet.ip = ('' + (d.ipaddress || '')).trim();
    hdrNet.ssid = ('' + (d.ssid || '')).trim();
    hdrNet.quality = (typeof d.wifiquality === 'number') ? d.wifiquality : 0;
    hdrNet.kind = (d.apfallback === true) ? 'ap'
      : ((d.wifiquality_text === 'Wired' || d.ssid === 'Wired') ? 'eth' : 'wifi');
    renderHdrNet();
  }
  function renderHdrNet() {
    var el = document.getElementById('hdrNet'); if (!el) return;
    el.textContent = ''; el.classList.remove('ap');
    var dot = document.createElement('span'); dot.className = 'cdot'; el.appendChild(dot);
    el.appendChild(document.createTextNode(hdrNet.ip || '—'));
    // Lit-bar count: Ethernet = all 4, AP = 0; Wi-Fi from quality quartiles, with the
    // quality re-derived from the live connRssi once /health has refreshed it.
    var q = hdrNet.quality, lit;
    if (hdrNet.kind === 'eth') { lit = 4; }
    else if (hdrNet.kind === 'ap') { lit = 0; el.classList.add('ap'); }
    else { if (connRssi !== null) q = wifiQualFromRssi(connRssi); lit = q >= 75 ? 4 : q >= 50 ? 3 : q >= 25 ? 2 : q > 0 ? 1 : 0; }
    var bars = document.createElement('span'); bars.className = 'wbars'; bars.style.marginLeft = '8px';
    var hpx = [4, 7, 10, 13];
    for (var i = 0; i < 4; i++) { var b = document.createElement('i'); b.style.height = hpx[i] + 'px'; if (i >= lit) b.className = 'off'; bars.appendChild(b); }
    el.appendChild(bars);
    // Tooltip — honest per network type. DHCP-vs-static read lazily from the settings
    // cache (empty wifistaticip == DHCP); it fills in once fetchSettings() lands.
    var staticip = (setData.wifistaticip && setData.wifistaticip.value != null) ? ('' + setData.wifistaticip.value).trim() : '';
    var mode = staticip === '' ? 'DHCP' : 'static';
    var tip;
    if (hdrNet.kind === 'eth') tip = 'IP ' + (hdrNet.ip || '—') + ' (' + mode + ') · Wired';
    else if (hdrNet.kind === 'ap') tip = 'IP ' + (hdrNet.ip || '—') + ' · AP mode';
    else {
      tip = 'IP ' + (hdrNet.ip || '—') + ' (' + mode + ') · Wi-Fi ' + (hdrNet.ssid || '—');
      if (connRssi !== null) tip += ' · ' + connRssi + ' dBm';
      if (q > 0) tip += ' · ' + q + '%';
    }
    el.title = tip;
  }
  // SIMULATION badge — /api/v2/simulate is PIC-path only; any error (404/503 on
  // OT-Direct, transport failure) is treated as "not simulating" and hides the badge.
  function fetchSim() {
    fetch(APIGW + 'v2/simulate').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      var active = !!(j && j.simulation && j.simulation.active);
      var el = document.getElementById('hdrSim'); if (el) el.classList.toggle('hidden', !active);
    }).catch(function () {
      var el = document.getElementById('hdrSim'); if (el) el.classList.add('hidden');
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

  // TASK-981: classify a timestamped event/narration line — the lines rawFromLine
  // drops. The firmware emits these over the same WS log stream with a one-char
  // prefix after the timestamp: '>' sent command, '<' PIC response, '!' error/NG,
  // '*' system event, 'S' SAT narration. Returns the colour class for the coloured
  // kinds, 'evt' for the pushed-but-uncoloured kinds ('*'/'S'), or '' when the line
  // is not an event line (so arbitrary non-frame text is never mistaken for one).
  function eventLineClass(line) {
    if (typeof line !== 'string') return '';
    var off = 0;
    var ts = line.match(/^\d{2}:\d{2}:\d{2}\.\d{3,6}\s/);
    if (ts) off = ts[0].length;
    var m = line.substring(off).match(/^([><!*S])\s/);
    if (!m) return '';
    return m[1] === '>' ? 'cmd' : m[1] === '<' ? 'rsp' : m[1] === '!' ? 'err' : 'evt';
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
    } else if (eventLineClass(d)) {
      pushLog(d);                   // TASK-981: sent-command echo / PIC ack / NG into the log
    }
  }

  function activeDesign() {
    var d = document.querySelector('.vp-item.active');
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
    // TASK-981: per-line DOM so command-echo lines can be coloured (.cmd/.rsp/.err)
    // without ever feeding device text to innerHTML. .console is white-space:pre-wrap,
    // so the '\n' text nodes are the line separators (matches the old join('\n')).
    // OT frames stay bare text nodes; only the coloured event kinds get a span.
    el.textContent = '';
    var frag = document.createDocumentFragment();
    for (var i = 0; i < lines.length; i++) {
      if (i > 0) frag.appendChild(document.createTextNode('\n'));
      var cls = eventLineClass(lines[i]);
      if (cls === 'cmd' || cls === 'rsp' || cls === 'err') {
        var span = document.createElement('span'); span.className = cls; span.textContent = lines[i];
        frag.appendChild(span);
      } else {
        frag.appendChild(document.createTextNode(lines[i]));
      }
    }
    el.appendChild(frag);
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
    var d = document.querySelector('.vp-item.active');
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
  // TASK-984: append the ⛓ injected-override mark (+ tooltip) to a readout without
  // clobbering its formatted value; clears the mark/title when the datapoint is plain.
  function setMark(id, base, on) {
    var el = document.getElementById(id); if (!el) return;
    el.textContent = on ? base + ' ⛓' : base;
    if (on) el.title = 'This value is being injected onto the OT bus by the gateway';
    else el.removeAttribute('title');
  }
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
    // TASK-984: decorate the outside-temp and room-setpoint readouts with ⛓ when
    // those specific values are being gateway-injected (MsgID 27, resp. 9/16).
    var ovrOut = ovrRows.some(function (o) { return ovrMark(o.id) === 'aOut'; });
    var ovrSet = ovrRows.some(function (o) { return ovrMark(o.id) === 'aRoomSet'; });
    setMark('aOut', fmt(model.outside, 1, '°'), model.outside !== null && ovrOut);
    txt('aRoom', fmt(model.room, 1, '°'));
    var setBase = model.roomSet === null ? '' : 'set ' + fmt(model.roomSet, 1, '°');
    setMark('aRoomSet', setBase, !!setBase && ovrSet);
    // injected-override badge — count from the same polled overrides model.
    var aob = document.getElementById('aOvrBadge');
    if (aob) {
      var no = ovrRows.length;
      aob.classList.toggle('hidden', !no);
      txt('aOvrCount', no);
      if (!aob._wired) { aob._wired = true; aob.addEventListener('click', function (e) { e.stopPropagation(); toggleOvrPanel(); }); }
    }
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
      // TASK-981: command-bar prompt reflects the live interface — 'OT ›' on OT-Direct,
      // 'PIC ›' otherwise (PIC gateway, or unknown/none as a sane default; never hardcoded).
      var cmdPrompt = document.getElementById('otCmdPrompt');
      if (cmdPrompt) cmdPrompt.textContent = (iface === 'OT-Direct') ? 'OT ›' : 'PIC ›';
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
      renderHdrNet();   // TASK-983: refresh Wi-Fi bars + dBm tooltip from the live RSSI
      fetchSim();       // TASK-983: piggyback the SIMULATION-badge poll on this cadence
      fetchOverrides(iface);   // TASK-984: refresh the injected-override count/list (source by interface)
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
    updateCnOvr();   // TASK-984: refresh the on-bus override badge from the cached rows
    renderConnDetail();
  }

  // ---------- Override-injection visibility (TASK-984) ----------
  // The gateway can inject values onto the OT bus (ADR-118: PIC recordOTOverride) or,
  // on OT-Direct hardware, the user's manual stored-response / modifier overrides. We
  // surface the live count on Home (design A) and the connection map, and list the rows
  // in a floating read-only panel. Polled on the fetchConn cadence — no new timer.
  var ovrRows = [];   // normalized [{ id: <OT MsgID>, name, meta }]
  // MsgID -> which Design-A readout the injection decorates. 9 = remote-override room
  // setpoint (TrOverride), 16 = room setpoint (TrSet); 27 = outside temperature (Toutside).
  function ovrMark(id) { return (id === 9 || id === 16) ? 'aRoomSet' : (id === 27 ? 'aOut' : null); }
  function normPicOvr(j) {
    var rows = (j && Array.isArray(j.overrides)) ? j.overrides : [];
    return rows.map(function (r) {
      var name = (r.friendly && r.friendly.length) ? r.friendly : (r.label || ('MsgID ' + r.id));
      var kind = (r.kind === 'answer') ? 'answer' : 'substituted';
      var val = (typeof r.value === 'number') ? r.value.toFixed(2) : r.value;
      return { id: r.id, name: name, meta: kind + ' = ' + val + ' · ' + r.age_s + 's' };
    });
  }
  function normOtdOvr(j) {
    var ov = (j && j.overrides) ? j.overrides : {};
    var out = [];
    function push(arr, label) {
      (arr || []).forEach(function (e) {
        var mid = (e && e.msgid !== undefined) ? e.msgid : e;
        var idn = (typeof mid === 'number') ? mid : parseInt(mid, 10);
        var v = (e && e.value !== undefined) ? otdHex(e.value) : '';
        out.push({ id: idn, name: label, meta: 'MsgID ' + mid + (v ? ' · ' + v : '') });
      });
    }
    push(ov.write, 'Write'); push(ov.response, 'Stored response');
    push(ov.modify, 'Response modifier'); push(ov.unknown, 'Unknown ID');
    return out;
  }
  function fetchOverrides(iface) {
    // On OT-Direct the gateway overrides endpoint is meaningless (no PIC); read the
    // OT-Direct store instead. Both normalize into ovrRows; errors -> empty, silently.
    var otd = (iface === 'OT-Direct');
    var url = APIGW + (otd ? 'v2/otdirect/overrides' : 'v2/otgw/overrides');
    function apply() { updateCnOvr(); scheduleRender(); }   // map badge + Home A badge/marks
    fetch(url).then(function (r) { return r.ok ? r.json() : null; })
      .then(function (j) { ovrRows = otd ? normOtdOvr(j) : normPicOvr(j); apply(); })
      .catch(function () { ovrRows = []; apply(); });
  }
  // On-bus badge in the connection-map SVG (#cnOvr sits on the y=180 shared bus segment).
  function updateCnOvr() {
    var g = document.getElementById('cnOvr'); if (!g) return;
    var n = ovrRows.length;
    g.style.display = n ? '' : 'none';
    var tx = document.getElementById('cnOvrTxt'); if (tx) tx.textContent = '⛓ ' + n + ' injected';
    g.onclick = function (e) { e.stopPropagation(); toggleOvrPanel(); };
  }
  // Floating read-only detail panel. The ADR-118 store carries {id,kind,value,age} only,
  // so no originating-command / pre-override value / Clear affordance is available yet.
  function renderOvrPanel() {
    var p = document.getElementById('ovrPanel'); if (!p) return;
    p.textContent = '';
    var h = document.createElement('h4'); h.textContent = '⛓ Active gateway overrides'; p.appendChild(h);
    var sub = document.createElement('p'); sub.className = 'sub';
    sub.textContent = 'Values the gateway is injecting onto the OT bus — the boiler/thermostat see these, not the original.';
    p.appendChild(sub);
    if (!ovrRows.length) {
      var m = document.createElement('div'); m.className = 'ovr-item'; m.style.color = 'var(--muted)';
      m.textContent = 'No active overrides'; p.appendChild(m); return;
    }
    ovrRows.forEach(function (o) {
      var it = document.createElement('div'); it.className = 'ovr-item';
      var r1 = document.createElement('div'); r1.className = 'r1';
      var nm = document.createElement('span'); nm.textContent = o.name; r1.appendChild(nm);
      it.appendChild(r1);
      var r2 = document.createElement('div'); r2.className = 'r2'; r2.textContent = o.meta;
      it.appendChild(r2);
      p.appendChild(it);
    });
  }
  function toggleOvrPanel() {
    var p = document.getElementById('ovrPanel'); if (!p) return;
    if (!p.classList.contains('show')) renderOvrPanel();
    p.classList.toggle('show');
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
  // TASK-1012: internal onboarding-completion flags. Hidden from the settings LIST only
  // (NOT from setData — satOnbNeeded()/the ui_onboarded logic read setData). Rendering them
  // as editable "Onboarded" checkboxes would let a user tick one and permanently bypass the
  // wizard + its enable-interception. SET_HIDE strips from setData, so it can't be used here.
  var SET_HIDE_ROW = { sat_onboarded: 1, ui_onboarded: 1 };
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
      maybeShowOnboarding();   // TASK-997: first-time wizard, once settings (ui_onboarded) are known
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
      var keys = keysForCat(id, q).filter(function (k) { return !SET_HIDE_ROW[k]; });   // TASK-1012: never render internal onboarding flags
      var bySub = {}; var subOrder = [];
      keys.forEach(function (k) { var s = subFor(k); if (!(s in bySub)) { bySub[s] = []; subOrder.push(s); } bySub[s].push(k); });
      subOrder.forEach(function (s) {
        var card = document.createElement('section'); card.className = 'set-group';
        if (s) { var h = document.createElement('h3'); h.textContent = s; card.appendChild(h); }
        bySub[s].forEach(function (k) { card.appendChild(settingRow(k)); });
        wrap.appendChild(card);
      });
      // TASK-985: give the Webhook group an inline Send-test-call action. renderSettings
      // rebuilds #setCols on every render, so the row is re-appended here (not once) —
      // the webhook category has a single set-group (no sub-groups).
      if (id === 'webhook') {
        var whCard = wrap.querySelector('.set-group');
        if (whCard) whCard.appendChild(webhookTestRow());
      }
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
  // TASK-985: Webhook "Send test call" action, appended to the Webhook settings group
  // by renderSettings(). Fires the REAL POST /api/v2/webhook/test?state=on. The endpoint
  // is fire-and-forget: it returns {"status":"ok"} immediately and sends the SAVED ON
  // URL/payload from its own FreeRTOS task. The target server's HTTP result is not
  // exposed by the firmware, so the success line reports the endpoint's own ack + a
  // local timestamp, not the remote status.
  function webhookTestRow() {
    var wrap = document.createElement('div'); wrap.className = 'wh-actions';
    var btn = document.createElement('button'); btn.type = 'button'; btn.className = 'tbtn';
    btn.textContent = 'Send test call';
    btn.title = 'Fires the saved ON webhook (POST /api/v2/webhook/test)';
    var status = document.createElement('span'); status.className = 'wh-status';
    status.textContent = 'Not fired yet';
    wrap.appendChild(btn); wrap.appendChild(status);
    btn.addEventListener('click', function () {
      // Disabled-guard: mirror the mockup — block only when the enable checkbox is
      // present and unchecked. (The firmware itself fires regardless of bEnabled; this
      // is a UX gate, so an absent checkbox — e.g. filtered out by search — proceeds.)
      var en = document.querySelector('#setCols .srow[data-key="webhookenable"] input.sw');
      if (en && !en.checked) {
        status.className = 'wh-status';
        status.textContent = 'Webhook disabled — enable it above first';
        return;
      }
      status.className = 'wh-status'; status.textContent = 'Sending…';
      btn.disabled = true;
      // credentials:'same-origin' so HTTP Basic-Auth + CSRF same-origin (ADR-056) hold.
      fetch(APIGW + 'v2/webhook/test?state=on', { method: 'POST', credentials: 'same-origin' })
        .then(function (r) {
          btn.disabled = false;
          if (r.status === 401 || r.status === 403) {
            status.className = 'wh-status';
            status.textContent = 'Auth required — sign in first (HTTP ' + r.status + ')';
            return;
          }
          if (!r.ok) {
            status.className = 'wh-status';
            status.textContent = 'Error: HTTP ' + r.status;
            return;
          }
          return r.text().then(function (body) {
            var st = 'ok';
            try { var j = JSON.parse(body); if (j && j.status) st = j.status; }
            catch (e) { /* non-JSON ack still counts as sent */ }
            var ts = new Date().toTimeString().slice(0, 8);
            status.className = 'wh-status ok';
            status.textContent = '✓ ' + st + ' · ' + ts;
          });
        })
        .catch(function (e) {
          btn.disabled = false;
          status.className = 'wh-status';
          status.textContent = 'Error: ' + ((e && e.message) ? e.message : 'network');
        });
    });
    return wrap;
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
    // TASK-1012: intercept satenabled false->on for a not-yet-onboarded SAT. Do NOT
    // enable now — run the wizard, which enables at Finish. Strip the key from this save.
    var satEnabling = ('satenabled' in setDirty) && ('' + setDirty.satenabled) === 'true' &&
      !(setData.satenabled && ('' + setData.satenabled.value) === 'true') && satOnbNeeded();
    if (satEnabling) { delete setDirty.satenabled; keys = Object.keys(setDirty); }
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
      if (satEnabling) startSatOnboarding('enable');   // TASK-1012: wizard owns the actual enable
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
        // TASK-995: no-PSRAM instability consent gate. When the board has no PSRAM,
        // BLE is enabled, but the user hasn't accepted the risk yet, BLE stays dormant
        // (firmware bleActive()==false) — surface the consent dialog once.
        if (j.psram === 0 && j.ble_enable && !j.risk_ack) maybeShowBleConsent();
        var card = document.getElementById('setcard-ble');
        var typing = card && card.contains(document.activeElement) &&
          /^(INPUT|TEXTAREA)$/.test((document.activeElement || {}).tagName || '');
        if (changed && !typing) renderBleCard();
      }).catch(function () { });
  }
  // TASK-995: BLE-without-PSRAM instability consent. On a no-PSRAM board, enabling BLE
  // steals ~64 KB of internal DRAM that the async web server needs, so the Web UI can
  // wedge under concurrent load. Require an explicit risk acknowledgement: accept ->
  // persist SATbleriskack=true (firmware starts scanning); decline -> turn BLE off again.
  function maybeShowBleConsent() {
    if (document.getElementById('bleRiskBackdrop')) return;   // already open
    var bd = document.createElement('div'); bd.id = 'bleRiskBackdrop'; bd.className = 'sim-backdrop';
    var m = document.createElement('div'); m.className = 'sim-modal';
    var h = document.createElement('h3'); h.textContent = '⚠ BLE without PSRAM — instability risk';
    var p = document.createElement('p');
    p.textContent = 'This board has no PSRAM. Enabling BLE takes ~64 KB of internal memory that the web server needs, so the Web UI can become unstable or unreachable under load (you may need to reboot). Enable BLE anyway?';
    var acts = document.createElement('div'); acts.className = 'acts';
    var no = document.createElement('button'); no.textContent = 'Keep BLE off';
    var yes = document.createElement('button'); yes.className = 'primary'; yes.textContent = 'Enable anyway (accept risk)';
    function post(name, value, then) {
      fetch(APIGW + 'v2/settings', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ name: name, value: value }) })
        .then(function (r) { if (r.ok && then) then(); }).catch(function () { });
    }
    function close() { if (bd.parentNode) bd.parentNode.removeChild(bd); }
    no.addEventListener('click', function () { post('satbleenable', false, function () { if (typeof bleToast === 'function') bleToast('BLE kept off'); setTimeout(fetchBle, 500); }); close(); });
    yes.addEventListener('click', function () { post('satbleriskack', true, function () { if (typeof bleToast === 'function') bleToast('BLE enabled — risk accepted'); setTimeout(fetchBle, 800); }); close(); });
    acts.appendChild(no); acts.appendChild(yes);
    m.appendChild(h); m.appendChild(p); m.appendChild(acts); bd.appendChild(m);
    document.body.appendChild(bd);
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
  // TASK-975: clear the whole roster by forgetting every slot (forget also cleans
  // HA discovery). Fire SEQUENTIALLY — concurrent POSTs trip the REST in-flight cap
  // (REST_MAX_INFLIGHT=4) and some forgets 503, leaving slots behind. Re-fetch once.
  function clearBleRoster(macs) {
    if (typeof bleToast === 'function') bleToast('Clearing roster…');
    var list = (macs || []).filter(Boolean);
    (function next(i) {
      if (i >= list.length) { setTimeout(fetchBle, 500); return; }
      fetch(APIGW + 'v2/sat/ble/forget', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ mac: list[i] }) })
        .then(function () { next(i + 1); }).catch(function () { next(i + 1); });
    })(0);
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
    // TASK-975: clear the whole roster (forget every slot; forget also cleans up HA
    // discovery). Two-click confirm avoids a blocking modal dialog.
    var sensorsNow = (bleData.sensors || []).filter(function (s) { return s.mac; });
    if (sensorsNow.length) {
      var clr = document.createElement('button'); clr.className = 'tbtn'; clr.textContent = '🗑 Clear roster';
      clr.style.marginLeft = '6px';
      clr.addEventListener('click', function () {
        if (clr._armed) { clr._armed = false; clearBleRoster(sensorsNow.map(function (s) { return s.mac; })); return; }
        clr._armed = true; clr.textContent = '⚠ Click again to clear all';
        clr._t = setTimeout(function () { if (clr.isConnected) { clr._armed = false; clr.textContent = '🗑 Clear roster'; } }, 3000);
      });
      card.appendChild(clr);
    }
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
          val.textContent = (bt !== undefined ? bt + '°' : '') + (bh !== undefined ? '  ' + (Math.round(bh * 10) / 10) + '%' : '');
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
        // TASK-996: per-row friendly-name input. The firmware persists a label per
        // roster slot (POST /sat/ble/label {mac,label} -> sBleLabel[24]); this exposes
        // it so a discovered sensor can be named and renamed. Pre-filled with the
        // current label; Enter or Save applies; clearing + Save clears the label.
        var nameRow = document.createElement('div'); nameRow.className = 'ble-ctrls';
        var nameIn = document.createElement('input'); nameIn.type = 'text';
        nameIn.placeholder = 'Name (e.g. Living room)'; nameIn.maxLength = 23;
        nameIn.value = s.label || ''; nameIn.autocomplete = 'off';
        nameIn.style.flex = '1'; nameIn.style.minWidth = '12ch';
        var nameBtn = document.createElement('button'); nameBtn.className = 'tbtn'; nameBtn.textContent = '💾 Save name';
        (function (mac, input) {
          function saveName() {
            var v = (input.value || '').trim();
            bleAction('label', { mac: mac, label: v });
            if (typeof bleToast === 'function') bleToast(v ? 'Name saved' : 'Name cleared');
          }
          nameBtn.addEventListener('click', saveName);
          input.addEventListener('keydown', function (e) { if (e.key === 'Enter') { e.preventDefault(); saveName(); } });
        })(s.mac, nameIn);
        nameRow.appendChild(nameIn); nameRow.appendChild(nameBtn); row.appendChild(nameRow);
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
    // The exp line describes the healthy state ("… is answering") — showing it
    // while the node is down/degraded contradicts the fix hint, so omit it there.
    if (c.exp && c.s !== 'st-down' && c.s !== 'st-warn') { var ex = document.createElement('div'); ex.className = 'exp'; ex.textContent = c.exp; body.appendChild(ex); }
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

  // ---------- Advanced: shared device/info snapshot ----------
  // device/info is the heavy chunked endpoint (heap-gated). Fetch it once and
  // cache for a few seconds so opening PIC → Debug → System in quick succession
  // does not hammer it. Never polled — only fetched on Advanced sub-tab open.
  var devInfoCache = null, devInfoTs = 0, devInfoInflight = null;
  function withDeviceInfo(cb) {
    var now = Date.now();
    if (devInfoCache && (now - devInfoTs) < 8000) { cb(devInfoCache); return; }
    if (devInfoInflight) { devInfoInflight.push(cb); return; }
    devInfoInflight = [cb];
    fetchWithRetry(APIGW + 'v2/device/info').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      var d = (j && j.device) || null;
      if (d) {
        devInfoCache = d; devInfoTs = Date.now();
        simActive = (d.otgwsimulation === true || d.otgwsimulation === 'true');
      }
      var cbs = devInfoInflight; devInfoInflight = null;
      // A failed/empty answer passes the stale cache if we have one, else null
      // ("unknown"): callers must never derive a negative capability verdict
      // (e.g. "No PIC") from null — only from an explicit successful snapshot.
      cbs.forEach(function (fn) { try { fn(d || devInfoCache); } catch (e) { } });
    }).catch(function () {
      var cbs = devInfoInflight || []; devInfoInflight = null;
      cbs.forEach(function (fn) { try { fn(devInfoCache); } catch (e) { } });
    });
  }
  function yesno(b) { return (b === undefined || b === null) ? undefined : (b === false || b === 'false' || b === 0) ? 'No' : 'Yes'; }

  // ---------- Advanced > Debug Information (TASK-967/982) ----------
  function fetchDebug() {
    // Grouped device info (mockup devinfo-groups), from the cached device/info snapshot.
    withDeviceInfo(function (d) { renderDebugGroups(d || {}); });
    // Crashlog: green ok-banner when clean; the detail <pre> only when a crash exists.
    fetch(APIGW + 'v2/device/crashlog').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      var c = (j && j.crashlog) || {};
      var ok = document.getElementById('dbgCrashOk'), pre = document.getElementById('dbgCrash');
      if (!c.available) { if (ok) ok.style.display = ''; if (pre) pre.style.display = 'none'; }
      else { if (ok) ok.style.display = 'none'; if (pre) { pre.style.display = ''; pre.textContent = ((c.summary || '') + '\n\n' + (c.details || '')).trim() || 'Crash recorded (no detail).'; } }
    }).catch(function () {
      var ok = document.getElementById('dbgCrashOk'), pre = document.getElementById('dbgCrash');
      if (ok) ok.style.display = 'none'; if (pre) { pre.style.display = ''; pre.textContent = '—'; }
    });
    // Raw /api/v2/debug dump preserved behind the collapsible (nothing lost from the old tab).
    fetch(APIGW + 'v2/debug').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      var el = document.getElementById('dbgInfo'); if (!el) return;
      var d = (j && j.debug) || j || {};
      var keys = Object.keys(d);
      el.textContent = keys.length ? keys.map(function (k) { return k + '  =  ' + d[k]; }).join('\n') : 'No debug data';
    }).catch(function () { var el = document.getElementById('dbgInfo'); if (el) el.textContent = 'Failed to load debug info'; });
  }
  function renderDebugGroups(d) {
    var host = document.getElementById('dbgGroups'); if (!host) return;
    host.textContent = '';
    var mqttEn;
    if (Object.prototype.hasOwnProperty.call(setData, 'mqttenable')) mqttEn = (('' + setData.mqttenable.value) === 'true') ? 'Yes' : 'No';
    var wifiQ;
    if (d.wifiquality_text !== undefined) wifiQ = d.wifiquality_text + (d.wifiquality !== undefined ? ' (' + d.wifiquality + '%)' : '');
    // TASK-995: format a byte count as MB (whole when exact) for the Hardware group.
    function fmtMB(b) { if (b === undefined || b === null) return undefined; var m = b / 1048576; return (m % 1 ? m.toFixed(1) : m) + ' MB'; }
    var psramStr = d.psram_found ? (fmtMB(d.psram_size) + ' (' + fmtMB(d.psram_free) + ' free)') : 'None';
    var groups = [
      ['Firmware', [
        ['Firmware Version', d.fwversion], ['Compiled On', d.compiled],
        ['PIC Firmware Version', d.picfwversion], ['PIC Firmware Type', d.picfwtype],
        ['PIC Device ID', d.picdeviceid], ['Board Type', d.board]
      ]],
      ['Network', [
        ['Hostname (.local)', d.hostname], ['IP Address', d.ipaddress], ['MAC Address', d.macaddress],
        ['Wi-Fi Network (SSID)', d.ssid], ['Wi-Fi Signal Strength (dBm)', d.wifirssi], ['Wi-Fi Quality', wifiQ]
      ]],
      ['Hardware', [
        ['Chip Model (est.)', d.chip_model_est], ['Unique Chip ID', d.chipid],
        ['Arduino Core Version', d.coreversion], ['Espressif SDK Version', d.sdkversion],
        ['CPU Speed (MHz)', d.cpufreq], ['Flash Size', d.flash_size ? fmtMB(d.flash_size) : (d.flashchipsize ? d.flashchipsize + ' MB' : undefined)],
        ['PSRAM', psramStr], ['LittleFS Size (MB)', d.LittleFSsize], ['Flash Mode', d.flashchipmode]
      ]],
      ['Memory & uptime', [
        ['Free Heap Memory (bytes)', d.freeheap], ['Max. Free Block (bytes)', d.maxfreeblock], ['Heap Fragmentation (%)', d.hd_fragmentation_pct],
        ['Uptime Since Boot', d.uptime], ['Number of Reboots', d.bootcount], ['Last Reset Reason', d.lastreset]
      ]],
      ['Integrations', [
        ['MQTT Connected', yesno(d.mqttconnected)], ['MQTT Enabled', mqttEn], ['NTP Enabled', yesno(d.ntpenable)], ['NTP Timezone', d.ntptimezone]
      ]]
    ];
    groups.forEach(function (g) {
      var rows = g[1].filter(function (r) { return r[1] !== undefined && r[1] !== null && r[1] !== ''; });
      if (!rows.length) return;
      var wrap = document.createElement('div'); wrap.className = 'devinfo-group';
      var h = document.createElement('h4'); h.textContent = g[0]; wrap.appendChild(h);
      var bodyEl = document.createElement('div'); bodyEl.className = 'devinfo-rows';
      rows.forEach(function (r) {
        var row = document.createElement('div'); row.className = 'devinforow';
        var c1 = document.createElement('div'); c1.className = 'devinfocolumn1'; c1.textContent = r[0];
        var c2 = document.createElement('div'); c2.className = 'devinfocolumn2'; c2.textContent = '' + r[1];
        row.appendChild(c1); row.appendChild(c2); bodyEl.appendChild(row);
      });
      wrap.appendChild(bodyEl); host.appendChild(wrap);
    });
  }

  // ---------- Advanced > PIC firmware flash (TASK-972/982) ----------
  var PICBASE = location.protocol + '//' + location.host + '/';   // /pic is a top-level route, not under /api/v2
  var picPollTimer = null, picBusy = false, picDevVer = '';
  var PIC_NONE_MSG = 'No PIC detected — PIC firmware flashing is unavailable on this board (OT-Direct / OTGW32).';
  var picGateRetries = 0;
  function fetchPic() {
    var wrap = document.getElementById('picFlash'), none = document.getElementById('picNone');
    withDeviceInfo(function (d) {
      if (!d || d.otcommandinterface === undefined) {
        // Gating snapshot unknown (device/info failed even after retries): never
        // conclude "No PIC" from a missing answer. Show a neutral checking state
        // and re-poll a few times; only a successful snapshot decides the card.
        if (wrap) wrap.style.display = 'none';
        if (none) { none.style.display = ''; none.textContent = 'Checking device capabilities…'; }
        if (picGateRetries < 5) { picGateRetries++; setTimeout(fetchPic, 2000); }
        return;
      }
      picGateRetries = 0;
      var isPic = (d.otcommandinterface || '') === 'PIC';
      if (wrap) wrap.style.display = isPic ? '' : 'none';      // hidden on OTGW32 / OT-Direct
      if (none) { none.style.display = isPic ? 'none' : ''; if (!isPic) none.textContent = PIC_NONE_MSG; }
      var ssim = document.getElementById('sysSim'); if (ssim) ssim.classList.toggle('hidden', !simActive);
      if (!isPic) return;
      picDevVer = ('' + (d.picfwversion || '')).trim();        // for the .newer highlight
      txt('picHeadDev', d.picdeviceid || '?');
      txt('picHeadType', d.picfwtype || '?');
      txt('picHeadVer', d.picfwversion || '?');
      fetchPicFiles();
    });
  }
  function fetchPicFiles() {
    fetch(APIGW + 'v2/firmware/files').then(function (r) { return r.ok ? r.json() : null; }).then(function (files) {
      renderPicFiles(Array.isArray(files) ? files : ((files && files.files) || []));   // firmware/files is a bare array
    }).catch(function () { });
  }
  function picIcon(sym, title, cls) {
    var b = document.createElement('span'); b.className = 'picbtn' + (cls ? ' ' + cls : '');
    b.textContent = sym; b.title = title; b.setAttribute('role', 'button'); b.tabIndex = 0;
    return b;
  }
  function newerThanDevice(fver) {
    var a = parseFloat(fver), b = parseFloat(picDevVer);
    return (!isNaN(a) && !isNaN(b) && a > b);
  }
  function renderPicFiles(files) {
    var el = document.getElementById('picFiles'); if (!el) return; el.textContent = '';
    if (!files.length) {
      var em = document.createElement('div'); em.className = 'picempty';
      em.textContent = 'No firmware files on device. Use Check for updates, or upload a .hex via the File System tab.';
      el.appendChild(em); return;
    }
    var head = document.createElement('div'); head.className = 'picrow head';
    ['Firmware name', 'Version', 'Size', '', '', ''].forEach(function (t) { var s = document.createElement('span'); s.textContent = t; head.appendChild(s); });
    el.appendChild(head);
    files.forEach(function (f) {
      var row = document.createElement('div'); row.className = 'picrow' + (newerThanDevice(f.version) ? ' newer' : '');
      var nm = document.createElement('span'); nm.className = 'fname'; nm.textContent = f.name || '?';
      var ver = document.createElement('span'); ver.className = 'fver'; ver.textContent = 'v' + (f.version || '?');
      var sz = document.createElement('span'); sz.className = 'fsize'; sz.textContent = ((f.size || 0) >= 1024 ? (f.size / 1024).toFixed(1) + ' KB' : (f.size || 0) + ' B');
      row.appendChild(nm); row.appendChild(ver); row.appendChild(sz);
      var refr = picIcon('⟳', 'Re-download latest ' + (f.name || ''), '');
      refr.addEventListener('click', function () { if (!picBusy) picRefresh(f.name, f.version); });
      var flash = picIcon('⇩', 'Flash ' + (f.name || '') + ' onto the PIC', '');
      flash.addEventListener('click', function () {
        if (picBusy) return;
        if (flash._armed) { flash._armed = false; flash.classList.remove('armed'); flash.textContent = '⇩'; flash.title = 'Flash ' + (f.name || '') + ' onto the PIC'; startPicFlash(f.name); return; }
        flash._armed = true; flash.classList.add('armed'); flash.textContent = '⚠'; flash.title = 'Tap again to confirm flashing ' + (f.name || '') + ' — a bad flash can brick the PIC';
        setTimeout(function () { if (flash.isConnected && flash._armed) { flash._armed = false; flash.classList.remove('armed'); flash.textContent = '⇩'; flash.title = 'Flash ' + (f.name || '') + ' onto the PIC'; } }, 3000);
      });
      var del = picIcon('🗑', 'Delete ' + (f.name || ''), 'del');
      del.addEventListener('click', function () { if (!picBusy && window.confirm('Delete firmware file ' + (f.name || '') + '?')) picDelete(f.name); });
      row.appendChild(refr); row.appendChild(flash); row.appendChild(del);
      el.appendChild(row);
    });
  }
  // Update banner. Manual button forces a fresh check (recheck=1); the auto path on
  // PIC-tab open (picUpdatePoll) reads the cached result without re-triggering.
  function renderPicBanner(u) {
    var b = document.getElementById('picBanner'); if (!b) return;
    b.className = 'pic-banner';
    if (u.status === 'checking') { b.style.display = ''; b.textContent = 'Checking for updates…'; return; }
    if (u.update_available) { b.style.display = ''; b.classList.add('warn'); b.textContent = 'PIC firmware update available: ' + (u.current || '?') + ' → ' + (u.latest || '?'); }
    else if (u.status === 'ready' || u.current) { b.style.display = ''; b.classList.add('ok'); b.textContent = '✓ PIC firmware up to date (' + (u.current || '?') + ')'; }
    else { b.style.display = 'none'; b.textContent = ''; }
  }
  function picUpdatePoll() {
    fetch(APIGW + 'v2/pic/update-check').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      var u = (j && j.pic_update) || {};
      if (u.status === 'checking') { setTimeout(picUpdatePoll, 2500); renderPicBanner(u); return; }
      renderPicBanner(u);
    }).catch(function () { });
  }
  function checkPicUpdate() {
    var b = document.getElementById('picBanner'); if (b) { b.className = 'pic-banner'; b.style.display = ''; b.textContent = 'Checking for updates…'; }
    fetch(APIGW + 'v2/pic/update-check?recheck=1').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      var u = (j && j.pic_update) || {};
      if (u.status === 'checking') { setTimeout(picUpdatePoll, 2500); return; }
      renderPicBanner(u);
    }).catch(function () { if (b) { b.className = 'pic-banner'; b.textContent = 'Update check failed'; } });
  }
  // Gateway settings · cached PR= values. The GET returns the cache immediately AND
  // triggers a fresh ~45s PR readout on the device — call once per tab open, no loop.
  function prv(x) { var s = (x === undefined || x === null) ? '' : ('' + x).trim(); return s === '' ? '—' : s; }
  function picModeText(m) {
    if (m === undefined || m === null || m === '') return '—';
    var s = ('' + m).toLowerCase();
    return s === 'on' ? 'Gateway' : s === 'off' ? 'Monitor' : s === 'detecting' ? 'Detecting…' : ('' + m);
  }
  function fetchPicSettings() {
    var el = document.getElementById('picPrTable'); if (!el) return;
    el.textContent = '';
    var ld = document.createElement('span'); ld.className = 'prk'; ld.textContent = 'Reading PR= values…'; el.appendChild(ld);
    fetch(APIGW + 'v2/pic/settings').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      var s = (j && j.pic_settings) || {};
      withDeviceInfo(function (d) {
        d = d || {};
        el.textContent = '';
        var rows = [
          ['PIC firmware version', prv(d.picfwversion)],
          ['Build date', prv(s.builddate)],
          ['Clock speed', prv(s.clock_mhz)],
          ['Reset cause', prv(s.reset_cause)],
          ['Thermostat detected', prv(s.thermostat_detect)],
          ['Voltage reference', prv(s.voltage_ref)],
          ['Standalone interval', prv(s.standalone_interval)],
          ['Gateway/Monitor mode', picModeText(d.otgwmode)]
        ];
        rows.forEach(function (r) {
          var k = document.createElement('span'); k.className = 'prk'; k.textContent = r[0];
          var v = document.createElement('span'); v.className = 'prv'; v.textContent = r[1];
          el.appendChild(k); el.appendChild(v);
        });
      });
    }).catch(function () { el.textContent = ''; var e = document.createElement('span'); e.className = 'prk'; e.textContent = 'Failed to read PIC settings'; el.appendChild(e); });
  }
  function startPicFlash(name) {
    picBusy = true;
    var prog = document.getElementById('picProg'), txt = document.getElementById('picProgTxt'), bar = document.getElementById('picProgBar');
    if (prog) prog.style.display = '';
    if (bar) { bar.style.width = '0%'; bar.style.background = 'var(--accent)'; }
    if (txt) txt.textContent = 'Starting flash of ' + name + '…';
    fetch(PICBASE + 'pic?action=upgrade&name=' + encodeURIComponent(name)).then(function (r) {
      if (!r.ok) throw 0;
      pollPicFlash();
    }).catch(function () { if (txt) txt.textContent = 'Flash failed to start'; picBusy = false; });
  }
  function pollPicFlash() {
    if (picPollTimer) clearTimeout(picPollTimer);
    fetch(APIGW + 'v2/pic/flash-status').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
      var s = (j && j.flashstatus) || {};
      var bar = document.getElementById('picProgBar'), txt = document.getElementById('picProgTxt');
      var p = (typeof s.progress === 'number') ? s.progress : 0;
      if (bar) bar.style.width = Math.max(0, Math.min(100, p)) + '%';
      if (s.flashing) {
        if (txt) txt.textContent = 'Flashing ' + (s.filename || '') + ' — ' + p + '%';
        picPollTimer = setTimeout(pollPicFlash, 1000);
      } else {
        picBusy = false;
        if (s.error && ('' + s.error).length) { if (txt) txt.textContent = 'Error: ' + s.error; if (bar) bar.style.background = 'var(--hot,#c33)'; }
        else { if (bar) bar.style.width = '100%'; if (txt) txt.textContent = 'Flash complete'; }
        setTimeout(fetchPic, 2500);
      }
    }).catch(function () { picBusy = false; var txt = document.getElementById('picProgTxt'); if (txt) txt.textContent = 'Lost contact during flash (device may be rebooting)'; });
  }
  function picRefresh(name, version) {
    var b = document.getElementById('picBanner'); if (b) b.textContent = 'Downloading ' + name + '…';
    fetch(PICBASE + 'pic?action=refresh&name=' + encodeURIComponent(name) + '&version=' + encodeURIComponent(version || '')).then(function () { setTimeout(fetchPicFiles, 2000); }).catch(function () { });
  }
  function picDelete(name) {
    fetch(PICBASE + 'pic?action=delete&name=' + encodeURIComponent(name)).then(function () { setTimeout(fetchPicFiles, 600); }).catch(function () { });
  }

  // ---------- Advanced > File System — in-page FSexplorer (TASK-982) ----------
  // Mirrors the classic FSexplorer.html against /api/v2/filesystem/files. The listing
  // is a heterogeneous JSON array: file/dir entries {name,size,type} followed by ONE
  // trailing storage-summary {usedBytes,totalBytes,freeBytes,truncated}. All names are
  // device-sourced -> DOM-built with textContent only, never innerHTML.
  var fsCurrentPath = '/';
  // Core UI assets: deleting any of these bricks the very page you are looking at, so
  // the Delete affordance is withheld (parity with the classic protectedFiles guard).
  var FS_PROTECTED = [
    'v2.html', 'v2.js', 'v2.css', 'sat.js', 'sat-slider.js', 'theme-toggle.js', 'echarts-theme.js',
    'index.html', 'index.js', 'index_common.css', 'components.css', 'ds-tokens.css', 'graph.js',
    'FSexplorer.html', 'FSexplorer.png', 'design.html', 'favicon.ico', 'settings.png',
    'inter-400.woff2', 'inter-700.woff2', 'jetbrains-mono-400.woff2'
  ];
  function fsFmtBytes(n) {
    n = +n || 0;
    if (n < 1024) return n + ' B';
    if (n < 1048576) return (n / 1024).toFixed(1) + ' KB';
    return (n / 1048576).toFixed(2) + ' MB';
  }
  function fsFullPath(name) {
    var p = (fsCurrentPath === '/') ? name : (fsCurrentPath + '/' + name);
    return (p.charAt(0) === '/') ? p : ('/' + p);   // root-absolute so it serves regardless of the page path
  }
  function fetchFsList(path) {
    if (typeof path === 'string') fsCurrentPath = path;
    fetchWithRetry(APIGW + 'v2/filesystem/files?path=' + encodeURIComponent(fsCurrentPath))
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (json) {
        if (!Array.isArray(json)) { renderFsError('Failed to read the file system.'); return; }
        var summary = json.length ? json[json.length - 1] : null;
        var files = json.slice(0, -1);
        // LittleFS drops empty directories; if this one vanished, fall back to root.
        if (files.length === 0 && fsCurrentPath !== '/') { fetchFsList('/'); return; }
        files.sort(function (a, b) {
          if (a.type === 'dir' && b.type !== 'dir') return -1;
          if (a.type !== 'dir' && b.type === 'dir') return 1;
          return ('' + a.name).localeCompare('' + b.name, undefined, { sensitivity: 'base' });
        });
        renderFsList(files, summary);
      })
      .catch(function () { renderFsError('Failed to read the file system.'); });
  }
  function renderFsError(msg) {
    var body = document.getElementById('fsTableBody'); if (!body) return;
    body.textContent = '';
    var tr = document.createElement('tr'); var td = document.createElement('td');
    td.colSpan = 4; td.style.color = 'var(--muted)'; td.style.padding = '12px'; td.textContent = msg + ' ';
    var retry = document.createElement('button'); retry.className = 'tbtn'; retry.textContent = 'Retry';
    retry.addEventListener('click', function () { fetchFsList(); });
    td.appendChild(retry);
    tr.appendChild(td); body.appendChild(tr);
  }
  function renderFsList(files, summary) {
    var body = document.getElementById('fsTableBody'); if (!body) return;
    body.textContent = '';
    var crumb = document.getElementById('fsCrumb');
    if (crumb) crumb.textContent = 'Current directory: ' + fsCurrentPath + (summary && summary.truncated ? '  · list truncated' : '');

    // Parent row (navigates up one level) whenever we are below root.
    if (fsCurrentPath !== '/') {
      var pr = document.createElement('tr');
      var pc = document.createElement('td'); pc.className = 'fn dir'; pc.textContent = '.. (parent)';
      pc.addEventListener('click', function () { fsNavigate('..'); });
      pr.appendChild(pc);
      var ps = document.createElement('td'); ps.className = 'fsz'; ps.textContent = '<DIR>'; pr.appendChild(ps);
      pr.appendChild(document.createElement('td')); pr.appendChild(document.createElement('td'));
      body.appendChild(pr);
    }

    files.forEach(function (f) {
      var name = '' + (f.name || '');
      var tr = document.createElement('tr');
      if (f.type === 'dir') {
        var dn = document.createElement('td'); dn.className = 'fn dir'; dn.textContent = name + '/';
        dn.addEventListener('click', function () { fsNavigate(name); });
        tr.appendChild(dn);
        var dz = document.createElement('td'); dz.className = 'fsz'; dz.textContent = '<DIR>'; tr.appendChild(dz);
        tr.appendChild(document.createElement('td')); tr.appendChild(document.createElement('td'));
      } else {
        var nn = document.createElement('td'); nn.className = 'fn'; nn.textContent = name; tr.appendChild(nn);
        var sz = document.createElement('td'); sz.className = 'fsz'; sz.textContent = fsFmtBytes(f.size); tr.appendChild(sz);
        var dl = document.createElement('td');
        var a = document.createElement('a'); a.className = 'fsact'; a.textContent = 'Download';
        a.href = fsFullPath(name); a.setAttribute('download', name); a.target = '_blank'; a.rel = 'noopener';
        dl.appendChild(a); tr.appendChild(dl);
        var dc = document.createElement('td');
        if (FS_PROTECTED.indexOf(name) === -1) {
          var del = document.createElement('a'); del.className = 'fsact del'; del.href = '#'; del.textContent = 'Delete';
          del.addEventListener('click', function (e) { e.preventDefault(); fsDelete(fsFullPath(name), name); });
          dc.appendChild(del);
        }
        tr.appendChild(dc);
      }
      body.appendChild(tr);
    });

    if (summary && summary.usedBytes !== undefined) {
      var usage = document.getElementById('fsUsage');
      if (usage) {
        usage.textContent = '';
        var b = document.createElement('b'); b.textContent = 'LittleFS'; usage.appendChild(b);
        usage.appendChild(document.createTextNode(
          ' used ' + fsFmtBytes(summary.usedBytes) + ' of ' + fsFmtBytes(summary.totalBytes) +
          ' — ' + fsFmtBytes(summary.freeBytes) + ' free'));
      }
      var bar = document.getElementById('fsBar');
      if (bar) {
        var pct = summary.totalBytes ? (summary.usedBytes / summary.totalBytes) * 100 : 0;
        bar.style.width = Math.max(0, Math.min(100, pct)).toFixed(0) + '%';
      }
      var hint = document.getElementById('fsUploadHint');
      if (hint) hint.textContent = fsFmtBytes(summary.freeBytes) + ' free · files land in the current directory.';
    }
  }
  function fsNavigate(name) {
    if (name === '..') {
      var parts = fsCurrentPath.split('/'); parts.pop();
      fsCurrentPath = (parts.length <= 1) ? '/' : parts.join('/');
      if (fsCurrentPath === '') fsCurrentPath = '/';
    } else {
      fsCurrentPath = (fsCurrentPath === '/') ? ('/' + name) : (fsCurrentPath + '/' + name);
    }
    fetchFsList();
  }
  function fsDelete(fullPath, name) {
    if (!window.confirm('Delete ' + name + ' from the file system? This cannot be undone.')) return;
    fetch(APIGW + 'v2/filesystem/files?delete=' + encodeURIComponent(fullPath), { credentials: 'same-origin' })
      .then(function (r) {
        if (r.ok) { bleToast('Deleted ' + name); fetchFsList(); return; }
        if (r.status === 401 || r.status === 403) bleToast('Not authorized — open classic /FSexplorer to sign in first');
        else bleToast('Delete failed: ' + r.status);
      })
      .catch(function () { bleToast('Delete failed'); });
  }
  function fsDoUpload() {
    var inp = document.getElementById('fsFile'), btn = document.getElementById('fsUpload');
    if (!inp || !inp.files || !inp.files.length) { bleToast('Choose a file to upload first'); return; }
    var fd = new FormData(); fd.append('file', inp.files[0], inp.files[0].name);
    if (btn) btn.disabled = true;
    // /upload replies 303 -> FSexplorer.html; redirect:'manual' keeps us here and we
    // refresh the listing ourselves instead of following the redirect.
    fetch('/upload?path=' + encodeURIComponent(fsCurrentPath), {
      method: 'POST', body: fd, credentials: 'same-origin', redirect: 'manual'
    }).then(function (r) {
      // An opaqueredirect (type 'opaqueredirect', status 0) is the success path here.
      if (r.type === 'opaqueredirect' || r.ok || r.status === 0) { bleToast('Uploaded ' + inp.files[0].name); }
      else if (r.status === 401 || r.status === 403) { bleToast('Not authorized — open classic /FSexplorer to sign in first'); }
      else { bleToast('Upload failed: ' + r.status); }
      inp.value = ''; if (btn) btn.disabled = true;
      setTimeout(fetchFsList, 400);
    }).catch(function () {
      bleToast('Upload failed'); if (btn) btn.disabled = false;
    });
  }

  // ---------- Advanced > System — device status + system actions (TASK-982) ----------
  // The three status badges are driven from renderConnStrip() (the SAME wd/mode mapping
  // as the TASK-980 header pill), so there is no vocabulary drift. Opening the tab just
  // re-asserts them and refreshes the cached simulation flag.
  function fetchSystemStatus() {
    renderConnStrip();
    withDeviceInfo(function () { renderConnStrip(); });
  }
  function advAction(act) {
    if (act === 'home') { showPage('home'); return; }
    if (act === 'onboarding') { startOnboarding(); return; }   // TASK-997: re-run the first-time setup wizard on demand
    if (act === 'sat-onboarding') { _satOnbShown = false; startSatOnboarding('rerun'); return; }   // TASK-1012: re-run the SAT setup wizard on demand
    if (act === 'update') { window.location.href = '/update'; return; }   // OTA sketch / filesystem upload page
    if (act === 'reboot') {
      if (!window.confirm('Reboot the OTGW device now? The Web UI will be unavailable for a few seconds.')) return;
      bleToast('Device is rebooting…');
      fetch('/ReBoot', { credentials: 'same-origin' })
        .then(function (r) { if (!r.ok) bleToast('Reboot failed: ' + r.status); })
        .catch(function () { });   // socket drops as the device restarts — expected
      return;
    }
    if (act === 'resetwifi') {
      if (!window.confirm('Reset Wireless settings?\n\nThis CLEARS the stored Wi-Fi network and reboots the device into AP-mode setup. You will have to reconnect it to your network afterwards.')) return;
      bleToast('Wi-Fi cleared — rebooting into AP-mode setup…');
      fetch('/ResetWireless', { credentials: 'same-origin' })
        .then(function (r) { if (!r.ok) bleToast('Reset failed: ' + r.status); })
        .catch(function () { });   // socket drops as the device restarts — expected
    }
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
    // OTGW convention: the two-letter command code is upper-case (tt=20.5 -> TT=20.5).
    // Only the leading code is forced; the value is sent verbatim.
    cmd = cmd.replace(/^([a-zA-Z]{2})/, function (m) { return m.toUpperCase(); });
    // TASK-981: unpause the log so the firmware's '>' echo and the PIC ack are visible.
    // Do it before the POST so a paused pushLog can't swallow an echo that races the
    // response. Restore the Pause button label exactly like its own click handler.
    if (logPaused) {
      logPaused = false;
      var lp = document.getElementById('logPause'); if (lp) lp.textContent = '⏸ Pause';
      if (isMonitorLogVisible()) renderLog();
    }
    fetch(APIGW + 'v2/otgw/commands', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command: cmd })
    }).then(function (r) {
      if (r.status === 202 || r.ok) { showCmdStatus('Sent: ' + cmd, true); if (inp) inp.value = ''; return; }
      // TASK-981: surface the firmware's error text ({"error":{"message":...}}) rather
      // than a bare status code (e.g. 413 "Command too long", 503 "No OT interface").
      r.json().then(function (j) {
        var msg = (j && j.error && j.error.message) ? j.error.message : ('Error ' + r.status);
        showCmdStatus(msg, false);
      }).catch(function () { showCmdStatus('Error ' + r.status, false); });
    }).catch(function () { showCmdStatus('Send failed', false); });
  }

  // ======================= SAT page (TASK-986) =======================
  // Owns #page-sat. Polls GET /api/v2/sat/status every 5 s ONLY while the page
  // is visible (satPageStart/Stop, driven by showPage). Renders three cumulative
  // depth layers (Thermostat / Control / Technical) and writes back via the SAT
  // POST routes verified in restAPI.ino: /sat/enable, /sat/target, /sat/preset,
  // /sat/mode, /sat/settings/<name>. Temperature history accumulates client-side
  // across polls (no firmware history endpoint, by design — mirrors classic sat.js).
  var satPollTimer = null;
  var satLastData = null;
  var satMarkers = [];
  var satMarkersLoaded = false;
  var satHist = { set: [], flow: [], room: [], out: [], pid: [] };
  var SAT_HIST_MAX = 720;               // ~1 h at the 5 s poll cadence
  var satDhwDirty = false;              // user dragging the DHW slider — defer status echo
  var satEnPending = false, satDhwEnPending = false, satSimPending = false;  // in-flight POST guards
  var SAT_MODE_LABELS = ['Off', 'Continuous', 'PWM'];      // control_mode enum
  var SAT_CYCLE_LABELS = ['None', 'Good', 'Overshoot', 'Underheat', 'Short', 'Uncertain'];  // last_cycle_class
  var SAT_PRESET_NAMES = ['None', 'Away', 'Eco', 'Comfort', 'Sleep', 'Activity', 'Home'];   // active_preset enum
  var SAT_PRESETS = [['home', '🏠 Home'], ['away', '🚪 Away'], ['eco', '🌱 Eco'], ['comfort', '🛋 Comfort'], ['sleep', '🌙 Sleep'], ['activity', '🏃 Activity']];
  var SAT_BOILER_LABELS = {
    off: 'Off', idle: 'Idle', preheating: 'Preheating', at_setpoint: 'At setpoint',
    modulating_up: 'Modulating up', modulating_down: 'Modulating down', ignition_surge: 'Ignition surge',
    stalled_ignition: 'Stalled ignition', anti_cycling: 'Anti-cycling', pump_starting: 'Pump starting',
    waiting_flame: 'Waiting flame', overshoot_cooling: 'Overshoot cooling', post_cycle: 'Post-cycle',
    heating: 'Heating', cooling: 'Cooling', heating_hot_water: 'Heating hot water'
  };
  // Heating-curve constants — JS port of satCalcHeatingCurve() in SATcontrol.ino.
  var SAT_HC_FLOOR = 20.0, SAT_HC_RAD = 27.2, SAT_HC_REF = 20.0;

  function satCssVar(n) { return getComputedStyle(document.documentElement).getPropertyValue(n).trim() || '#888'; }
  function satNum(v) { return (v !== null && v !== undefined && !isNaN(v)) ? +v : null; }
  function satFmt(v, dp, suf) { var n = satNum(v); return n === null ? '—' : n.toFixed(dp === undefined ? 1 : dp) + (suf || ''); }

  function satPageStart() {
    fetchSatPage();
    if (!satMarkersLoaded) { satMarkersLoaded = true; fetchSatMarkers(); }
    if (satPollTimer) clearInterval(satPollTimer);
    satPollTimer = setInterval(fetchSatPage, 5000);
    // TASK-1012: existing SAT users see the wizard once on first SAT-page visit.
    if (setData && setData.satenabled && ('' + setData.satenabled.value) === 'true') maybeShowSatOnboarding('migrate');
    // Wire the "Re-run setup" button (guarded so re-entry doesn't double-bind).
    var rb = document.getElementById('satRerunSetup');
    if (rb && !rb._wired) { rb._wired = true; rb.addEventListener('click', function () { _satOnbShown = false; startSatOnboarding('rerun'); }); }
  }
  function satPageStop() { if (satPollTimer) { clearInterval(satPollTimer); satPollTimer = null; } }

  // POST helper for the /api/v2/sat/* write routes. Default content-type text/plain
  // (matches how the routes parse the body via satExtractPostValue).
  function satPost(sub, value, ctype) {
    return fetch(APIGW + 'v2/sat/' + sub, {
      method: 'POST', headers: { 'Content-Type': ctype || 'text/plain' }, body: String(value)
    }).then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r; });
  }

  function fetchSatPage() {
    fetchWithRetry(APIGW + 'v2/sat/status')
      .then(function (r) { return r.ok ? r.json().catch(function () { return null; }).then(function (j) { return { status: r.status, json: j }; }) : { status: r.status, json: null }; })
      .then(function (res) {
        var d = res.json;
        var nd = document.getElementById('satNoData'), body = document.getElementById('satBody');
        if (!d || typeof d !== 'object') {
          // Distinguish "this device has no SAT endpoint" (404) from a transient
          // load-shed 503: the latter self-heals on the 5 s poll, so say so.
          if (nd) {
            nd.textContent = (res.status === 404)
              ? 'SAT status is unavailable on this device.'
              : 'Couldn’t load SAT status — retrying…';
            nd.style.display = '';
          }
          if (body) body.style.display = 'none';
          return;
        }
        if (nd) nd.style.display = 'none'; if (body) body.style.display = '';
        satLastData = d;
        try { renderSatPage(d); } catch (e) { }
      })
      .catch(function () {
        // Hard failure (device unreachable) at first paint: show the no-data banner
        // instead of a wall of dashes. Self-heals on the next poll once reachable.
        if (!satLastData) {
          var nd = document.getElementById('satNoData'), body = document.getElementById('satBody');
          if (nd) nd.style.display = ''; if (body) body.style.display = 'none';
        }
      });
    fetchSatPageWeather();
  }

  function fetchSatPageWeather() {
    fetch(APIGW + 'v2/sat/weather')
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (w) { if (w) renderSatWeather(w); })
      .catch(function () { });
  }

  function fetchSatMarkers() {
    fetch(APIGW + 'v2/sat/markers')
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (data) {
        satMarkers = Array.isArray(data) ? data : (data && Array.isArray(data.markers) ? data.markers : []);
        satRenderMarkerList();
        if (satLastData) satDrawCurve(satLastData);
      })
      .catch(function () { satMarkers = []; });
  }

  // Build a .kv card via DOM (textContent — safe for device-provided strings like manufacturer).
  function satKv(id, rows) {
    var box = document.getElementById(id); if (!box) return;
    box.textContent = '';
    rows.forEach(function (r) {
      if (!r) return;
      var row = document.createElement('div'); row.className = 'kv-row';
      var k = document.createElement('span'); k.className = 'kv-k'; k.textContent = r[0];
      var v = document.createElement('span'); v.className = 'kv-v'; v.textContent = r[1];
      row.appendChild(k); row.appendChild(v); box.appendChild(row);
    });
  }

  function renderSatPage(d) {
    var enabled = !!d.enabled, active = !!d.active, heating = enabled && active;

    var pill = document.getElementById('satPill');
    if (pill) {
      pill.textContent = !enabled ? 'Off' : (heating ? 'Heating' : 'Idle');
      pill.className = 'sat-pill' + (!enabled ? ' off' : (heating ? '' : ' idle'));
    }
    txt('satEnableLbl', enabled ? 'Enabled' : 'Disabled');
    var enCb = document.getElementById('satEnable'); if (enCb && !satEnPending) enCb.checked = enabled;

    // L1 — dial + status + presets
    satDialDraw(d);
    var big;
    if (!enabled) big = 'SAT is off';
    else if (d.safety_tripped) big = 'Safety tripped!';
    else if (d.window_open) big = 'Window open';
    else if (d.summer_active) big = 'Summer mode';
    else if (active) big = 'Heating to ' + satFmt(d.target_temp, 1, '°');
    else big = 'At target · idle';
    txt('satStatusBig', big);
    satRenderPresets(d);

    // L2 — history + tiles + dhw + burner cycle + curve + control status
    satHistPush(d); satDrawHistory();
    txt('satOut', satFmt(d.outside_temp, 1, '°'));
    txt('satFlowSet', satFmt(d.final_setpoint, 1, '°'));
    txt('satRoom', satFmt(d.room_temp, 1, '°'));
    txt('satTargetT', satFmt(d.target_temp, 1, '°'));
    satRenderDhw(d);
    satRenderCycle(d);
    satDrawCurve(d);
    satKv('satControlStatus', [
      ['Control mode', SAT_MODE_LABELS[d.control_mode | 0] || '—'],
      ['Boiler status', SAT_BOILER_LABELS[d.boiler_status] || '—'],
      ['Active preset', SAT_PRESET_NAMES[d.active_preset | 0] || '—'],
      ['Manufacturer', d.manufacturer || '—'],
      ['Coefficient', satFmt(d.coefficient, 2)],
      ['Deadband', satFmt(d.deadband, 2, ' °C')],
      ['PID output', satFmt(d.pid_output, 1, ' °C')],
      ['Error', satFmt(d.error, 2, ' °C')],
      ['Modulation', d.current_modulation != null ? d.current_modulation + ' %' : '—'],
      ['Max modulation', d.max_rel_modulation != null ? d.max_rel_modulation + ' %' : '—']
    ]);

    // L3 — health + PID + PWM/cycle + smart + external + simulation + raw JSON
    satRenderHealth(d);
    satKv('satPidKv', [
      ['P term', satFmt(d.pid_p, 2)], ['I term', satFmt(d.pid_i, 2)], ['D term', satFmt(d.pid_d, 2)],
      ['Kp', satFmt(d.kp, 4)], ['Ki', satFmt(d.ki, 4)], ['Kd', satFmt(d.kd, 4)],
      ['Raw derivative', satFmt(d.raw_derivative, 4)]
    ]);
    satKv('satPwmKv', [
      ['Duty cycle', d.pwm_duty != null ? Math.round(d.pwm_duty * 100) + ' %' : '—'],
      ['PWM flame', d.pwm_flame_req ? 'Requesting' : 'Off'],
      ['Cycle count', d.cycle_count != null ? String(d.cycle_count) : '—'],
      ['Cycles this hour', d.cycles_this_hour != null ? String(d.cycles_this_hour) : '—'],
      ['Last cycle', SAT_CYCLE_LABELS[d.last_cycle_class | 0] || '—'],
      ['Cycle max flow', satFmt(d.cycle_max_flow, 1, ' °C')],
      ['Cycle overshoot', d.cycle_overshoot_sec != null ? Math.round(d.cycle_overshoot_sec) + ' s' : '—']
    ]);
    var summer = d.summer_active ? ('Active' + (d.summer_hours_above != null ? ' (' + Math.round(d.summer_hours_above) + ' h)' : '')) : 'Inactive';
    var thermal = (d.thermal_model_valid && d.thermal_coeff != null) ? satFmt(d.thermal_coeff, 3) : 'Learning…';
    satKv('satSmartKv', [
      ['Solar gain', d.solar_gain_active ? 'Active' : 'Inactive'],
      ['Summer mode', summer],
      ['Thermal learning', thermal],
      ['Comfort offset', (d.comfort_offset != null && d.comfort_offset !== 0) ? satFmt(d.comfort_offset, 2, ' °C') : 'Off'],
      ['PV boost', d.pv_boost_active ? ('Active ' + satFmt(d.pv_boost_applied_c, 1, ' °C')) : 'Off'],
      ['Window detection', d.window_detection ? 'On' : 'Off']
    ]);
    satKv('satExtKv', [
      ['Indoor sensor', d.external_temp_valid ? 'External' : 'OT bus'],
      ['Outdoor sensor', d.external_outdoor_valid ? 'External' : 'OT bus / weather']
    ]);
    satRenderSim(d);
    var raw = document.getElementById('satRawJson');
    if (raw && raw.style.display !== 'none') raw.textContent = JSON.stringify(d, null, 2);
  }

  function satDialDraw(d) {
    var svg = document.getElementById('satDial'); if (!svg) return;
    var lo = 5, hi = 30, startDeg = -135, sweepDeg = 270;
    function ang(v) { return startDeg + (Math.max(lo, Math.min(hi, v)) - lo) / (hi - lo) * sweepDeg; }
    var track = satCssVar('--gauge-track'), accent = satCssVar('--accent'), ok = satCssVar('--zone-ok'),
      txtc = satCssVar('--gauge-text'), muted = satCssVar('--muted');
    var tgt = satNum(d.target_temp), room = satNum(d.room_temp);
    var g = '<path d="' + arcPath(120, 120, 92, startDeg, startDeg + sweepDeg) + '" fill="none" stroke="' + track + '" stroke-width="14" stroke-linecap="round"/>';
    if (tgt !== null) g += '<path d="' + arcPath(120, 120, 92, startDeg, ang(tgt)) + '" fill="none" stroke="' + accent + '" stroke-width="14" stroke-linecap="round"/>';
    if (room !== null) {
      var a = polar(120, 120, 79, ang(room)), b = polar(120, 120, 105, ang(room));
      g += '<line x1="' + a[0].toFixed(1) + '" y1="' + a[1].toFixed(1) + '" x2="' + b[0].toFixed(1) + '" y2="' + b[1].toFixed(1) + '" stroke="' + ok + '" stroke-width="4" stroke-linecap="round"/>';
    }
    g += '<text x="120" y="116" text-anchor="middle" font-size="44" font-weight="700" fill="' + txtc + '" font-family="var(--font-sans)">' + (tgt !== null ? tgt.toFixed(1) + '°' : '—') + '</text>';
    g += '<text x="120" y="140" text-anchor="middle" font-size="13" fill="' + muted + '" font-family="var(--font-sans)">target · room ' + (room !== null ? room.toFixed(1) + '°' : '—') + '</text>';
    svg.innerHTML = g;
  }

  function satRenderPresets(d) {
    var box = document.getElementById('satPresets'); if (!box) return;
    var an = SAT_PRESET_NAMES[d.active_preset | 0];
    var activeName = an ? an.toLowerCase() : '';
    box.textContent = '';
    SAT_PRESETS.forEach(function (p) {
      var b = document.createElement('button');
      b.className = 'sat-preset' + (p[0] === activeName ? ' active' : '');
      b.textContent = p[1];
      b.dataset.preset = p[0];
      b.addEventListener('click', function () {
        satPost('preset', p[0]).then(function () { setTimeout(fetchSatPage, 300); }).catch(function () { });
        document.querySelectorAll('#satPresets .sat-preset').forEach(function (x) { x.classList.toggle('active', x === b); });
      });
      box.appendChild(b);
    });
  }

  function satHistPush(d) {
    satHist.set.push(satNum(d.final_setpoint));
    satHist.flow.push(satNum(d.heating_curve));
    satHist.room.push(satNum(d.room_temp));
    satHist.out.push(satNum(d.outside_temp));
    satHist.pid.push(satNum(d.pid_output));
    ['set', 'flow', 'room', 'out', 'pid'].forEach(function (k) { if (satHist[k].length > SAT_HIST_MAX) satHist[k].shift(); });
  }

  function satDrawHistory() {
    var svg = document.getElementById('satHistChart'); if (!svg) return;
    var n = satHist.flow.length;
    var accent = satCssVar('--accent'), hot = satCssVar('--hot'), ok = satCssVar('--zone-ok'),
      cold = satCssVar('--cold'), muted = satCssVar('--muted'), border = satCssVar('--border');
    function Y(t) { return 186 - (Math.max(-5, Math.min(80, t)) + 5) / 85 * 168; }
    var x0 = 8, x1 = 592, dx = n > 1 ? (x1 - x0) / (n - 1) : 0;
    function L(arr) {
      var p = '';
      for (var i = 0; i < n; i++) { var v = arr[i]; if (v === null || v === undefined || isNaN(v)) continue; p += (x0 + i * dx).toFixed(1) + ',' + Y(v).toFixed(1) + ' '; }
      return p.trim();
    }
    function poly(pts, stroke, w, dash) { return pts ? '<polyline points="' + pts + '" fill="none" stroke="' + stroke + '" stroke-width="' + w + '"' + (dash ? ' stroke-dasharray="' + dash + '"' : '') + '/>' : ''; }
    var g = '';
    [0, 20, 40, 60, 80].forEach(function (t) { var y = Y(t).toFixed(1); g += '<line x1="8" y1="' + y + '" x2="592" y2="' + y + '" stroke="' + border + '" stroke-width="1"/>'; });
    g += poly(L(satHist.out), cold, 1.5, '6 4');
    g += poly(L(satHist.pid), muted, 1.5, '1 4');
    g += poly(L(satHist.flow), hot, 2);
    g += poly(L(satHist.set), accent, 2);
    g += poly(L(satHist.room), ok, 2);
    svg.innerHTML = g;
    var lg = document.getElementById('satHistLegend');
    if (lg) lg.innerHTML =
      '<span><i style="background:' + accent + '"></i>Setpoint</span><span><i style="background:' + hot + '"></i>Flow</span>' +
      '<span><i style="background:' + ok + '"></i>Room</span><span><i style="background:' + cold + '"></i>Outside</span>' +
      '<span><i style="background:' + muted + '"></i>PID</span>';
  }

  function satRenderDhw(d) {
    var slider = document.getElementById('satDhwSlider');
    var sp = satNum(d.dhw_setpoint);
    if (slider && !satDhwDirty) {
      if (sp !== null) { slider.value = Math.round(sp); txt('satDhwR', Math.round(sp) + '°C'); }
      else txt('satDhwR', '—');
    }
    var row = document.getElementById('satDhwEnableRow');
    if (row) row.style.display = d.dhw_config_tank ? '' : 'none';
    var cb = document.getElementById('satDhwEnable');
    if (cb && !satDhwEnPending && d.dhw_enable !== undefined) cb.checked = !!d.dhw_enable;
  }

  function satRenderCycle(d) {
    var cm = d.control_mode | 0;
    document.querySelectorAll('#satCycleRow .ds-seg').forEach(function (b) {
      var on = (b.dataset.mode === 'continuous' && cm === 1) || (b.dataset.mode === 'pwm' && cm === 2);
      b.classList.toggle('active', on);
    });
    txt('satCycleSub', cm === 2 ? 'PID modulates the burner in PWM bursts' : (cm === 1 ? 'PID drives the burner continuously' : 'SAT is not actively controlling'));
  }

  function satRenderHealth(d) {
    var box = document.getElementById('satHealth'); if (!box) return;
    var lc = d.last_cycle_class | 0, badCycle = (lc === 2 || lc === 3 || lc === 4);
    var items = [
      ['Device', (d.boiler_status && d.boiler_status !== 'off') ? 'ok' : 'idle'],
      ['Cycle', badCycle ? 'warn' : 'ok'],
      ['Flame', d.safety_tripped ? 'alert' : 'ok'],
      ['Pressure', d.pressure_alarm ? 'alert' : 'ok'],
      ['Setpoint', d.setpoint_mismatch ? 'warn' : 'ok'],
      ['Modulation', d.modulation_reliable ? 'ok' : 'warn']
    ];
    box.textContent = '';
    items.forEach(function (it) {
      var span = document.createElement('span'); span.className = 'health-item';
      var dot = document.createElement('span'); dot.className = 'hdot ' + it[1];
      span.appendChild(dot); span.appendChild(document.createTextNode(it[0]));
      box.appendChild(span);
    });
  }

  function satRenderSim(d) {
    var on = !!d.simulation, avail = (d.sim_available !== false);
    var cb = document.getElementById('satSim');
    if (cb && !satSimPending) cb.checked = on;
    if (cb) cb.disabled = (!avail && !on);
    txt('satSimHint', avail ? '' : 'A real boiler is connected — bench simulation is unavailable.');
    satKv('satSimKv', [
      ['Simulation', on ? 'On (bench)' : 'Off'],
      ['Sim room', on ? satFmt(d.sim_room_temp, 1, ' °C') : '—'],
      ['Sim flow', on ? satFmt(d.sim_flow_temp, 1, ' °C') : '—'],
      ['Sim return', on ? satFmt(d.sim_return_temp, 1, ' °C') : '—'],
      ['Sim flame', on ? (d.sim_flame_on ? 'On' : 'Off') : '—'],
      ['Sim modulation', on && d.sim_modulation != null ? d.sim_modulation + ' %' : '—'],
      ['Last blocked cmd', (on && d.last_blocked_cmd) ? (d.last_blocked_cmd + (d.last_blocked_cmd_age_ms != null ? ' (' + Math.round(d.last_blocked_cmd_age_ms / 1000) + 's ago)' : '')) : '—'],
      ['Fallback active', d.fallback_active ? 'Yes' : 'No'],
      ['Fallback reason', d.fallback_reason != null ? String(d.fallback_reason) : '—']
    ]);
  }

  function renderSatWeather(w) {
    if (!w || !w.enabled) { satKv('satWeatherKv', [['Weather', 'Disabled']]); txt('satCoords', ''); return; }
    var valid = !!w.valid;
    var mins = (valid && w.age_seconds >= 0) ? Math.floor(w.age_seconds / 60) : null;
    satKv('satWeatherKv', [
      ['Outdoor temp', valid ? satFmt(w.temperature, 1, ' °C') : '—'],
      ['Humidity', valid && w.humidity != null ? Math.round(w.humidity) + ' %' : '—'],
      ['Wind', valid && w.wind_speed != null ? satFmt(w.wind_speed, 1, ' km/h') : '—'],
      ['Last update', mins === null ? 'Never' : (mins < 1 ? 'Just now' : mins + ' min ago')],
      ['Fetch errors', String(w.fetch_errors || 0)]
    ]);
    var lat = parseFloat(w.latitude), lon = parseFloat(w.longitude);
    txt('satCoords', (!isNaN(lat) && !isNaN(lon) && (lat !== 0 || lon !== 0)) ? lat.toFixed(3) + ', ' + lon.toFixed(3) : 'No location set');
  }

  // JS port of satCalcHeatingCurve() (SATcontrol.ino): base offset + (coeff/4)·curve.
  function satCalcCurve(outside, target, coeff, system) {
    var base = (system === 2) ? SAT_HC_FLOOR : SAT_HC_RAD;   // 2 = underfloor
    var diff = outside - SAT_HC_REF;
    return base + (coeff / 4.0) * (4.0 * (target - SAT_HC_REF) + 0.03 * diff * diff - 0.4 * diff);
  }

  function satDrawCurve(d) {
    var svg = document.getElementById('satCurveBig'); if (!svg) return;
    var coeff = satNum(d.coefficient), target = satNum(d.target_temp), system = d.heating_system | 0,
      outside = satNum(d.outside_temp), setp = satNum(d.final_setpoint);
    if (coeff === null || target === null) { svg.innerHTML = ''; return; }
    var xmin = -15, xmax = 25, ymin = 20, ymax = 85;
    function X(o) { return 50 + (o - xmin) / (xmax - xmin) * 530; }
    function Y(f) { return 270 - (Math.max(ymin, Math.min(ymax, f)) - ymin) / (ymax - ymin) * 240; }
    var muted = satCssVar('--muted'), border = satCssVar('--border'), accent = satCssVar('--accent'),
      ok = satCssVar('--zone-ok'), hot = satCssVar('--hot'), panel = satCssVar('--panel');
    var g = '';
    [20, 40, 60, 80].forEach(function (f) { var y = Y(f).toFixed(1); g += '<line x1="50" y1="' + y + '" x2="580" y2="' + y + '" stroke="' + border + '" stroke-width="1"/><text x="44" y="' + (+y + 4) + '" text-anchor="end" font-size="11" fill="' + muted + '">' + f + '°</text>'; });
    [-10, 0, 10, 20].forEach(function (o) { var x = X(o).toFixed(1); g += '<line x1="' + x + '" y1="30" x2="' + x + '" y2="270" stroke="' + border + '" stroke-width="1" stroke-dasharray="2 5"/><text x="' + x + '" y="288" text-anchor="middle" font-size="11" fill="' + muted + '">' + o + '°</text>'; });
    var refs = [0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0], matched = false;
    refs.forEach(function (c) {
      var pts = [], activeC = Math.abs(c - coeff) < 0.05; if (activeC) matched = true;
      for (var o = xmin; o <= xmax; o++) pts.push(X(o).toFixed(1) + ',' + Y(satCalcCurve(o, target, c, system)).toFixed(1));
      g += '<polyline points="' + pts.join(' ') + '" fill="none" stroke="' + (activeC ? accent : border) + '" stroke-width="' + (activeC ? 3 : 1.2) + '" opacity="' + (activeC ? 1 : 0.65) + '"/>';
    });
    if (!matched) {
      var apts = [];
      for (var o2 = xmin; o2 <= xmax; o2++) apts.push(X(o2).toFixed(1) + ',' + Y(satCalcCurve(o2, target, coeff, system)).toFixed(1));
      g += '<polyline points="' + apts.join(' ') + '" fill="none" stroke="' + accent + '" stroke-width="3"/>';
    }
    satMarkers.forEach(function (m) {
      var mo = satNum(m.outside_temp), mf = satNum(m.flow_temp);
      if (mo !== null && mf !== null) g += '<circle cx="' + X(mo).toFixed(1) + '" cy="' + Y(mf).toFixed(1) + '" r="4.5" fill="' + ok + '" stroke="' + panel + '" stroke-width="1.5"/>';
    });
    if (outside !== null && setp !== null) g += '<circle cx="' + X(outside).toFixed(1) + '" cy="' + Y(setp).toFixed(1) + '" r="6.5" fill="' + hot + '" stroke="' + panel + '" stroke-width="2"/>';
    svg.innerHTML = g;
    txt('satCurveLbl', (system === 2 ? 'underfloor' : 'radiator') + ' · coeff ' + coeff.toFixed(1) + ' · target ' + target.toFixed(1) + '°');
    var lg = document.getElementById('satCurveLegend');
    if (lg) lg.innerHTML =
      '<span><i style="background:' + accent + '"></i>active curve (c=' + coeff.toFixed(1) + ')</span>' +
      '<span><i style="background:' + ok + '"></i>calibration markers</span>' +
      '<span><i style="background:' + hot + '"></i>current point</span>';
  }

  function satRenderMarkerList() {
    var list = document.getElementById('satMarkerList'); if (!list) return;
    list.textContent = '';
    if (!satMarkers.length) {
      var li0 = document.createElement('li'); li0.textContent = 'No calibration markers yet.'; li0.style.color = 'var(--muted)';
      list.appendChild(li0); return;
    }
    satMarkers.forEach(function (m) {
      var li = document.createElement('li');
      var s = document.createElement('span'); s.textContent = satFmt(m.outside_temp, 0, '° out');
      var b = document.createElement('b'); b.textContent = satFmt(m.flow_temp, 0, '° flow');
      li.appendChild(s); li.appendChild(b); list.appendChild(li);
    });
  }

  function satShowView(v) {
    if (['simple', 'control', 'technical'].indexOf(v) === -1) v = 'simple';
    document.querySelectorAll('.satview-btn').forEach(function (b) { b.classList.toggle('active', b.dataset.sv === v); });
    var body = document.getElementById('satBody'); var cls = 'sv-' + v; if (body) body.className = cls;
    try { localStorage.setItem('otgw-v2-satview', v); } catch (e) { }
    // Redraw charts: a layer that was display:none has no layout, so its SVG must
    // be regenerated once it becomes visible.
    if (satLastData) { satDrawHistory(); satDrawCurve(satLastData); }
  }

  function satStep(delta) {
    var base = (satLastData && satNum(satLastData.target_temp) !== null) ? satNum(satLastData.target_temp) : 20;
    var t = Math.max(5, Math.min(30, Math.round((base + delta) * 2) / 2));
    satPost('target', t.toFixed(1)).then(function () {
      if (satLastData) { satLastData.target_temp = t; satDialDraw(satLastData); }
      setTimeout(fetchSatPage, 300);
    }).catch(function () { });
  }

  // Detect location for the weather card. Sets SATweatherlat/lon (string settings —
  // not the boolean SATweatherenable, which the /api/v2/settings typed-bool path
  // would silently no-op on a "1" string; enabling weather stays a Settings action).
  function satDetectLocation() {
    if (!navigator.geolocation) { txt('satCoords', 'Geolocation not supported'); return; }
    txt('satCoords', 'Detecting…');
    navigator.geolocation.getCurrentPosition(function (pos) {
      var lat = pos.coords.latitude.toFixed(4), lon = pos.coords.longitude.toFixed(4);
      // Land the two coords one at a time (never both at once) so the REST
      // backpressure gate (restAPI.ino, TASK-884) is never tripped.
      [['SATweatherlat', lat], ['SATweatherlon', lon]].reduce(function (c, kv) {
        return c.then(function () {
          return fetch(APIGW + 'v2/settings', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ name: kv[0], value: kv[1] }) })
            .then(function (r) { if (!r.ok) throw new Error(r.status); return r; });
        });
      }, Promise.resolve()).then(function () { txt('satCoords', lat + ', ' + lon); setTimeout(fetchSatPageWeather, 1500); }).catch(function () { txt('satCoords', 'Save failed'); });
    }, function () { txt('satCoords', 'Location error'); }, { timeout: 10000 });
  }

  // Wire the SAT page controls once (elements exist at load; the page is just hidden).
  function wireSat() {
    document.querySelectorAll('.satview-btn').forEach(function (b) {
      b.addEventListener('click', function () { satShowView(b.dataset.sv); });
    });
    var sv = 'simple'; try { sv = localStorage.getItem('otgw-v2-satview') || 'simple'; } catch (e) { }
    satShowView(sv);

    var en = document.getElementById('satEnable');
    if (en) en.addEventListener('change', function () {
      // TASK-1012: intercept enabling a not-yet-onboarded SAT — run the wizard instead
      // of enabling now (it enables at Finish). Revert the toggle meanwhile.
      if (en.checked && !(satLastData && satLastData.enabled) && satOnbNeeded()) {
        en.checked = false; startSatOnboarding('enable'); return;
      }
      satEnPending = true;
      satPost('enable', en.checked ? '1' : '0')
        .then(function () { satEnPending = false; setTimeout(fetchSatPage, 300); })
        .catch(function () { satEnPending = false; if (satLastData) en.checked = !!satLastData.enabled; });
    });

    var up = document.getElementById('satUp'), dn = document.getElementById('satDown');
    if (up) up.addEventListener('click', function () { satStep(0.5); });
    if (dn) dn.addEventListener('click', function () { satStep(-0.5); });

    document.querySelectorAll('#satCycleRow .ds-seg').forEach(function (b) {
      b.addEventListener('click', function () {
        satPost('mode', b.dataset.mode).then(function () { setTimeout(fetchSatPage, 300); }).catch(function () { });
      });
    });

    var ds = document.getElementById('satDhwSlider');
    if (ds) {
      ds.addEventListener('input', function () { satDhwDirty = true; txt('satDhwR', ds.value + '°C'); });
      ds.addEventListener('change', function () {
        satDhwDirty = false;
        satPost('settings/dhw_setpoint', ds.value).then(function () { setTimeout(fetchSatPage, 300); }).catch(function () { });
      });
    }

    var de = document.getElementById('satDhwEnable');
    if (de) de.addEventListener('change', function () {
      satDhwEnPending = true;
      satPost('settings/dhw_enable', de.checked ? 'true' : 'false')
        .then(function () { satDhwEnPending = false; setTimeout(fetchSatPage, 300); })
        .catch(function () { satDhwEnPending = false; if (satLastData) de.checked = !!satLastData.dhw_enable; });
    });

    var sim = document.getElementById('satSim');
    if (sim) sim.addEventListener('change', function () {
      satSimPending = true;
      satPost('settings/simulation', sim.checked ? '1' : '0')
        .then(function () { satSimPending = false; setTimeout(fetchSatPage, 300); })
        .catch(function () { satSimPending = false; if (satLastData) sim.checked = !!satLastData.simulation; txt('satSimHint', 'Could not change simulation (a real boiler may be connected).'); });
    });

    var rt = document.getElementById('satRawToggle');
    if (rt) rt.addEventListener('click', function () {
      var pre = document.getElementById('satRawJson'); if (!pre) return;
      var open = pre.style.display === 'none';
      pre.style.display = open ? 'block' : 'none';
      rt.textContent = (open ? '▾' : '▸') + ' Raw data (JSON)';
      rt.setAttribute('aria-expanded', open ? 'true' : 'false');
      if (open && satLastData) pre.textContent = JSON.stringify(satLastData, null, 2);
    });

    var dl = document.getElementById('satDetectLoc');
    if (dl) dl.addEventListener('click', satDetectLocation);
  }

  // ---------- TASK-990: sensor-discovery toast (PR-649 "OTGW Patterns and Tokens") ----------
  // A newly discovered, still-unassigned BLE sensor surfaces as a floating card
  // (bottom-right, .discover-stack) with one-tap Assign (jump to the Sensors roster)
  // or Ignore, then dismiss/auto-expire. Runs GLOBALLY off the same
  // GET /api/v2/sat/ble/discovery poll the roster uses, so a background discovery is
  // never silent. Each MAC surfaces once (persisted in localStorage otgw-ble-seen).
  var discoverSeen = null;
  function discoverSeenSet() {
    if (discoverSeen) return discoverSeen;
    discoverSeen = {};
    try { (JSON.parse(localStorage.getItem('otgw-ble-seen') || '[]') || []).forEach(function (m) { discoverSeen[m] = 1; }); } catch (e) {}
    return discoverSeen;
  }
  function markDiscoverSeen(mac) {
    discoverSeenSet()[mac] = 1;
    try { localStorage.setItem('otgw-ble-seen', JSON.stringify(Object.keys(discoverSeen))); } catch (e) {}
  }
  function getDiscoverStack() {
    var s = document.getElementById('discoverStack');
    if (!s) { s = document.createElement('div'); s.id = 'discoverStack'; s.className = 'discover-stack'; document.body.appendChild(s); }
    return s;
  }
  function dropDiscoverCard(card, mac) {
    markDiscoverSeen(mac);
    if (card && card.parentNode) card.parentNode.removeChild(card);
  }
  // o = { id, bus:'ble'|'dallas', title, sub }. Bus-agnostic so BLE and 1-Wire
  // (Dallas) both surface through the same card, colour-coded by CSS (.ble/.dallas).
  function showDiscoverCard(o) {
    var id = o.id; if (!id) return;
    var stack = getDiscoverStack();
    if (stack.querySelector('[data-mac="' + id + '"]')) return;   // already shown
    if (stack.children.length >= 4) return;                       // cap the visible stack
    var card = document.createElement('div');
    card.className = 'discover-card ' + (o.bus || 'ble');
    card.setAttribute('data-mac', id);
    var ic = document.createElement('span'); ic.className = 'discover-ic';
    var bd = document.createElement('div'); bd.className = 'discover-bd';
    var t = document.createElement('div'); t.className = 't'; t.textContent = o.title;
    var ss = document.createElement('div'); ss.className = 's'; ss.textContent = o.sub;
    var act = document.createElement('div'); act.className = 'discover-act';
    var assign = document.createElement('button'); assign.className = 'primary'; assign.textContent = 'Assign';
    assign.addEventListener('click', function () {
      dropDiscoverCard(card, id);
      setActiveCat = 'sensors'; showPage('settings'); renderSettings();
      setTimeout(function () { var el = document.querySelector('[class*="ble-row"]'); if (el && el.scrollIntoView) el.scrollIntoView({ behavior: 'smooth', block: 'center' }); }, 350);
    });
    var ign = document.createElement('button'); ign.textContent = 'Ignore';
    ign.addEventListener('click', function () { dropDiscoverCard(card, id); });
    act.appendChild(assign); act.appendChild(ign);
    bd.appendChild(t); bd.appendChild(ss); bd.appendChild(act);
    var x = document.createElement('button'); x.className = 'discover-x'; x.setAttribute('aria-label', 'Dismiss'); x.textContent = '×';
    x.addEventListener('click', function () { dropDiscoverCard(card, id); });
    card.appendChild(ic); card.appendChild(bd); card.appendChild(x);
    stack.appendChild(card);
    // No auto-expire: the card stays until the user acts on it (Assign / Ignore / ×).
  }
  // TASK-1034: BLE discoveries collapse into ONE grouped card ("N new BLE sensors
  // discovered") with a single View action — the Sensors roster is the place to
  // act — and auto-dismiss after 15 s. Nothing is lost: the roster keeps them.
  var bleDiscoverPending = {};   // mac -> sub line of the pending (unshown-seen) sensor
  var bleDiscoverTimer = null;
  function bleDiscoverDismiss() {
    if (bleDiscoverTimer) { clearTimeout(bleDiscoverTimer); bleDiscoverTimer = null; }
    Object.keys(bleDiscoverPending).forEach(function (m) { markDiscoverSeen(m); });
    bleDiscoverPending = {};
    var stack = document.getElementById('discoverStack');
    var card = stack && stack.querySelector('[data-ble-group]');
    if (card && card.parentNode) card.parentNode.removeChild(card);
  }
  function goToSensorRoster() {
    setActiveCat = 'sensors'; showPage('settings'); renderSettings();
    setTimeout(function () { var el = document.querySelector('[class*="ble-row"]'); if (el && el.scrollIntoView) el.scrollIntoView({ behavior: 'smooth', block: 'center' }); }, 350);
  }
  function showBleDiscoverGroup() {
    var macs = Object.keys(bleDiscoverPending);
    if (!macs.length) return;
    var stack = getDiscoverStack();
    var card = stack.querySelector('[data-ble-group]');
    if (!card) {
      card = document.createElement('div');
      card.className = 'discover-card ble';
      card.setAttribute('data-ble-group', '1');
      var ic = document.createElement('span'); ic.className = 'discover-ic';
      var bd = document.createElement('div'); bd.className = 'discover-bd';
      var t = document.createElement('div'); t.className = 't';
      var ss = document.createElement('div'); ss.className = 's';
      var act = document.createElement('div'); act.className = 'discover-act';
      var view = document.createElement('button'); view.className = 'primary'; view.textContent = 'View';
      view.addEventListener('click', function () { bleDiscoverDismiss(); goToSensorRoster(); });
      act.appendChild(view);
      bd.appendChild(t); bd.appendChild(ss); bd.appendChild(act);
      var x = document.createElement('button'); x.className = 'discover-x'; x.setAttribute('aria-label', 'Dismiss'); x.textContent = '×';
      x.addEventListener('click', bleDiscoverDismiss);
      card.appendChild(ic); card.appendChild(bd); card.appendChild(x);
      stack.appendChild(card);
    }
    var tEl = card.querySelector('.t'), sEl = card.querySelector('.s');
    if (tEl) tEl.textContent = (macs.length === 1) ? 'New BLE sensor discovered' : macs.length + ' new BLE sensors discovered';
    if (sEl) sEl.textContent = (macs.length === 1) ? bleDiscoverPending[macs[0]] : 'View them in Settings › Sensors.';
    if (bleDiscoverTimer) clearTimeout(bleDiscoverTimer);
    bleDiscoverTimer = setTimeout(bleDiscoverDismiss, 15000);
  }
  // BLE discovery: /api/v2/sat/ble/discovery (mac + temp/hum).
  function pollDiscovery(retriesLeft) {
    fetch(APIGW + 'v2/sat/ble/discovery', { cache: 'no-store' })
      .then(function (r) { if (!r.ok) throw 0; return r.json(); })
      .then(function (j) {
        if (!j || !Array.isArray(j.sensors)) return;
        var seen = discoverSeenSet();
        var fresh = false;
        j.sensors.forEach(function (s) {
          if (!s || !s.valid || !s.mac || seen[s.mac] || bleDiscoverPending[s.mac]) return;
          if (s.label && s.label.trim()) { markDiscoverSeen(s.mac); return; }   // already named -> not "new"
          var nm = (s.name && s.name.trim()) || '';
          var sub = (nm ? nm + ' · ' : '') + s.mac;
          if (typeof s.temp === 'number') sub += ' · ' + s.temp.toFixed(1) + '°C';
          if (typeof s.hum === 'number') sub += ' / ' + Math.round(s.hum) + '%';
          bleDiscoverPending[s.mac] = sub;
          fresh = true;
        });
        if (fresh) showBleDiscoverGroup();
      })
      .catch(function () {
        // Board momentarily saturated under the page's concurrent load (the
        // discovery fetch colliding with the health/SAT polls + WS starved it).
        // Retry once shortly; the board recovers between bursts.
        if ((retriesLeft | 0) > 0) setTimeout(function () { pollDiscovery(retriesLeft - 1); }, 4000);
      });
  }
  // 1-Wire/Dallas discovery: /api/v2/sensors (sensors.dallas[] with addr + temp).
  // Each detected address surfaces once (seen-tracked, shared with BLE).
  function pollDallasDiscovery(retriesLeft) {
    fetch(APIGW + 'v2/sensors', { cache: 'no-store' })
      .then(function (r) { if (!r.ok) throw 0; return r.json(); })
      .then(function (j) {
        var d = j && j.sensors; if (!d || !Array.isArray(d.dallas)) return;
        var seen = discoverSeenSet();
        d.dallas.forEach(function (s) {
          var addr = s && s.addr; if (!addr || seen[addr]) return;
          var sub = 'DS18B20 · ' + addr;
          if (typeof s.temp === 'number') sub += ' · ' + s.temp.toFixed(1) + '°C';
          showDiscoverCard({ id: addr, bus: 'dallas', title: 'New 1-Wire sensor detected', sub: sub });
        });
      })
      .catch(function () { if ((retriesLeft | 0) > 0) setTimeout(function () { pollDallasDiscovery(retriesLeft - 1); }, 4000); });
  }
  // Gentle scheduler: skip the boot asset-burst (10s), then poll on a 25s cycle
  // OFFSET from the 15s health poll so the two rarely fire together — that
  // collision is what starved the discovery fetch. Dallas polls a beat later so
  // the two discovery fetches don't collide with each other. Each tick retries once.
  function startDiscoveryPolling() {
    setTimeout(function () {
      pollDiscovery(1);
      setInterval(function () { pollDiscovery(1); }, 25000);
    }, 10000);
    setTimeout(function () {
      pollDallasDiscovery(1);
      setInterval(function () { pollDallasDiscovery(1); }, 25000);
    }, 14000);
  }

  // ---------- TASK-991: bench-mode -> simulation prompt ----------
  // No boiler AND no thermostat on the OT bus == a bench setup. On PIC boards
  // (where /api/v2/simulate exists) offer once to enable simulation mode. The
  // firmware itself refuses simulation when a real boiler is present
  // (satBoilerHardwarePresent), so this can never clobber a live rig.
  // ── TASK-997: first-time-setup onboarding wizard (design OTGW Onboarding.dc.html) ──
  // Shown ONCE on a genuinely fresh device (firmware ui_onboarded=false; existing
  // installs are migrated to onboarded so it never appears again). Welcome ->
  // Appliance -> Home Assistant -> Done; writes SATsource + mqtt*, then persists
  // ui_onboarded=true. Re-runnable from Settings > System (see renderSettings).
  var _onbShown = false;
  function maybeShowOnboarding() {
    if (_onbShown || document.getElementById('onbBack')) return;
    var ob = setData.ui_onboarded;
    if (!ob || ('' + ob.value) === 'true') return;   // already onboarded / old firmware
    _onbShown = true; startOnboarding();
  }
  function startOnboarding() {
    var st = {
      screen: 'welcome',
      appliance: (setData.satsource && ('' + setData.satsource.value) === '2') ? 'hp' : 'gas',
      mqtt: !!(setData.mqttenable && ('' + setData.mqttenable.value) === 'true'),
      discovery: true, test: 'idle',
      broker: (setData.mqttbroker && setData.mqttbroker.value) || 'homeassistant.local',
      port: (setData.mqttbrokerport && setData.mqttbrokerport.value) || '1883',
      user: (setData.mqttuser && setData.mqttuser.value) || '', pass: '',
      topic: (setData.mqtttoptopic && setData.mqtttoptopic.value) || 'otgw'
    };
    var back = document.createElement('div'); back.id = 'onbBack';
    back.setAttribute('style', 'position:fixed;inset:0;z-index:200;background:var(--bg);display:flex;flex-direction:column;overflow:auto');
    document.body.appendChild(back);
    var CARD = 'background:var(--panel);border:1px solid var(--border);border-radius:16px;box-shadow:0 8px 26px rgba(0,0,0,.32);padding:28px 26px';
    var PRIM = 'background:var(--accent);border:0;color:#04222c;font-weight:700;font-size:14px;border-radius:11px;padding:12px 26px;cursor:pointer;min-height:46px';
    var GHOST = 'background:none;border:1px solid var(--border);color:var(--text);font-weight:600;font-size:13.5px;border-radius:10px;padding:11px 18px;cursor:pointer;min-height:44px';
    var IN = 'width:100%;border:1px solid var(--border);background:var(--bg);color:var(--text);border-radius:8px;padding:10px 12px;font:400 13.5px inherit';
    var LBL = 'font-weight:600;font-size:11.5px;color:var(--muted);display:block;margin-bottom:5px';
    function esc(s) { return ('' + (s == null ? '' : s)).replace(/"/g, '&quot;').replace(/</g, '&lt;'); }
    function post(name, value) { return fetch(APIGW + 'v2/settings', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ name: name, value: value }) }); }
    function close() { if (back.parentNode) back.parentNode.removeChild(back); }
    function syncInputs() {
      var b = back.querySelector('#onbBroker'); if (b) st.broker = b.value;
      var p = back.querySelector('#onbPort'); if (p) st.port = p.value;
      var u = back.querySelector('#onbUser'); if (u) st.user = u.value;
      var pw = back.querySelector('#onbPass'); if (pw) st.pass = pw.value;
      var t = back.querySelector('#onbTopic'); if (t) st.topic = t.value;
    }
    function commit(then) {
      var kv = [['satsource', st.appliance === 'hp' ? 2 : 1], ['mqttenable', st.mqtt]];
      if (st.mqtt) {
        kv.push(['mqttbroker', st.broker], ['mqttbrokerport', parseInt(st.port, 10) || 1883], ['mqttuser', st.user], ['mqtttoptopic', st.topic]);
        if (st.pass) kv.push(['mqttpasswd', st.pass]);
      }
      kv.push(['ui_onboarded', true]);   // onboarding flag last
      // One POST at a time (never a concurrent burst) so the REST backpressure
      // gate (restAPI.ino, TASK-884) is never tripped by the settings write.
      kv.reduce(function (c, p) { return c.then(function () { return post(p[0], p[1]); }); }, Promise.resolve()).then(then, then);
    }
    function testConn() {
      if (st.test === 'testing') return; syncInputs(); st.test = 'testing'; render();
      var kv = [['mqttenable', true], ['mqttbroker', st.broker], ['mqttbrokerport', parseInt(st.port, 10) || 1883], ['mqttuser', st.user]];
      if (st.pass) kv.push(['mqttpasswd', st.pass]);
      // Land the credentials one at a time (never a burst) before polling for the connection.
      kv.reduce(function (c, p) { return c.then(function () { return post(p[0], p[1]); }); }, Promise.resolve()).then(function () {
        var tries = 0;
        (function poll() {
          fetch(APIGW + 'v2/device/info').then(function (r) { return r.ok ? r.json() : null; }).then(function (j) {
            var c = j && j.device && j.device.mqttconnected;
            if (c === true || c === 'true') { st.test = 'ok'; render(); }
            else if (++tries < 6) setTimeout(poll, 1300); else { st.test = 'fail'; render(); }
          }).catch(function () { if (++tries < 6) setTimeout(poll, 1300); else { st.test = 'fail'; render(); } });
        })();
      });
    }
    function choiceCard(kind, title, desc) {
      var sel = st.appliance === kind;
      return '<button data-act="pick-' + kind + '" style="flex:1;min-width:210px;text-align:left;border-radius:12px;padding:16px;cursor:pointer;font-family:inherit;' +
        (sel ? 'background:color-mix(in srgb,var(--accent) 12%,var(--panel));border:2px solid var(--accent)' : 'background:var(--bg);border:1.5px solid var(--border)') + '">' +
        '<div style="display:flex;align-items:center;gap:9px"><span style="width:16px;height:16px;border-radius:50%;box-sizing:border-box;flex:0 0 auto;' +
        (sel ? 'border:5px solid var(--accent)' : 'border:1.5px solid var(--muted)') + '"></span><span style="font-weight:700;font-size:15px;color:var(--text)">' + title + '</span></div>' +
        '<p style="font:400 11.5px/1.5 inherit;color:var(--muted);margin:9px 0 0">' + desc + '</p></button>';
    }
    function toggle(on, act) {
      return '<button data-act="' + act + '" aria-label="toggle" style="width:44px;height:24px;border-radius:12px;position:relative;flex:0 0 auto;cursor:pointer;border:0;padding:0;background:' + (on ? 'var(--accent)' : 'var(--border)') + '">' +
        '<span style="position:absolute;top:2px;width:20px;height:20px;background:#fff;border-radius:50%;left:' + (on ? '22px' : '2px') + '"></span></button>';
    }
    function render() {
      var h = '';
      h += '<div style="display:flex;align-items:center;gap:11px;padding:12px 18px;background:var(--panel);border-bottom:1px solid var(--border)">' +
        '<span style="font-weight:700;font-size:15px;color:var(--text)">OTGW firmware</span>' +
        '<span style="font-weight:600;font-size:10px;color:var(--muted);letter-spacing:.06em;text-transform:uppercase">First-time setup</span></div>';
      h += '<div style="flex:1;display:flex;align-items:flex-start;justify-content:center;padding:44px 18px 60px"><div style="width:100%;max-width:560px">';
      if (st.screen === 'appliance' || st.screen === 'mqtt') {
        var isM = st.screen === 'mqtt';
        h += '<div style="margin:0 0 20px"><div style="display:flex;justify-content:space-between;align-items:baseline;margin-bottom:8px">' +
          '<span style="font-weight:700;font-size:11px;letter-spacing:.06em;text-transform:uppercase;color:var(--muted)">Step ' + (isM ? 2 : 1) + ' of 2</span>' +
          '<span style="font-weight:600;font-size:12px;color:var(--text)">' + (isM ? 'Home Assistant' : 'Appliance') + '</span></div>' +
          '<div style="height:5px;border-radius:3px;background:var(--border);overflow:hidden"><span style="display:block;height:100%;border-radius:3px;background:var(--accent);width:' + (isM ? 100 : 50) + '%"></span></div></div>';
      }
      if (st.screen === 'welcome') {
        h += '<div style="' + CARD + ';text-align:center">' +
          '<div style="width:54px;height:54px;border-radius:50%;background:color-mix(in srgb,var(--ok,#27c469) 18%,transparent);display:flex;align-items:center;justify-content:center;margin:0 auto 18px"><span style="width:16px;height:16px;border-radius:50%;background:var(--ok,#27c469);box-shadow:0 0 10px var(--ok,#27c469)"></span></div>' +
          '<h1 style="font-weight:700;font-size:25px;color:var(--text);margin:0 0 8px">Your gateway is online</h1>' +
          '<p style="font:400 13px/1.65 inherit;color:var(--muted);margin:0 auto 24px;max-width:400px">Two quick things and you\'re done — everything else is read straight from the OpenTherm bus.</p>' +
          '<button data-act="begin" style="' + PRIM + ';font-size:15px;padding:13px 30px">Get started</button></div>';
      } else if (st.screen === 'appliance') {
        h += '<div style="' + CARD + '"><h1 style="font-weight:700;font-size:21px;color:var(--text);margin:0 0 5px">What are you heating with?</h1>' +
          '<p style="font:400 12.5px/1.55 inherit;color:var(--muted);margin:0 0 18px">Sets how OpenTherm data is read and labelled. Change it anytime in Settings.</p>' +
          '<div style="display:flex;gap:12px;flex-wrap:wrap">' + choiceCard('gas', 'Gas boiler', 'Modulating OpenTherm boiler — flow &amp; return temperature, modulation %, flame &amp; DHW.') + choiceCard('hp', 'Heat pump', 'Air/water heat pump or electric heater — compressor, COP, water temperatures &amp; defrost.') + '</div>' +
          '<div style="display:flex;align-items:center;gap:12px;margin-top:24px"><button data-act="back-welcome" style="' + GHOST + '">Back</button>' +
          '<button data-act="to-mqtt" style="margin-left:auto;' + PRIM + '">Continue</button></div></div>';
      } else if (st.screen === 'mqtt') {
        h += '<div style="' + CARD + '"><div style="display:flex;align-items:flex-start;gap:12px"><div style="flex:1">' +
          '<h1 style="font-weight:700;font-size:21px;color:var(--text);margin:0 0 5px">Connect to Home Assistant</h1>' +
          '<p style="font:400 12.5px/1.55 inherit;color:var(--muted);margin:0">The gateway publishes every value over MQTT; Home Assistant auto-discovers them. Optional — leave off to run standalone.</p></div>' +
          toggle(st.mqtt, 'toggle-mqtt') + '</div>';
        if (st.mqtt) {
          h += '<div style="margin-top:18px;display:flex;flex-direction:column;gap:12px">' +
            '<div style="display:flex;gap:12px"><div style="flex:1"><label style="' + LBL + '">MQTT broker</label><input id="onbBroker" value="' + esc(st.broker) + '" style="' + IN + '"></div>' +
            '<div style="width:116px"><label style="' + LBL + '">Port</label><input id="onbPort" value="' + esc(st.port) + '" style="' + IN + '"></div></div>' +
            '<div style="display:flex;gap:12px"><div style="flex:1"><label style="' + LBL + '">Username</label><input id="onbUser" value="' + esc(st.user) + '" style="' + IN + '"></div>' +
            '<div style="flex:1"><label style="' + LBL + '">Password</label><input id="onbPass" type="password" placeholder="unchanged" style="' + IN + '"></div></div>' +
            '<div style="flex:1"><label style="' + LBL + '">Top topic</label><input id="onbTopic" value="' + esc(st.topic) + '" style="' + IN + '"></div>' +
            '<div style="display:flex;align-items:center;gap:12px;flex-wrap:wrap;min-height:40px">' +
            '<button data-act="test" style="background:none;color:var(--accent);font-weight:600;font-size:12.5px;border-radius:9px;padding:9px 15px;min-height:40px;border:1px solid var(--accent);cursor:pointer">' + (st.test === 'testing' ? 'Testing…' : (st.test === 'ok' || st.test === 'fail') ? 'Test again' : 'Test connection') + '</button>' +
            (st.test === 'testing' ? '<span style="font:500 12px inherit;color:var(--muted)">Testing connection…</span>' : '') +
            (st.test === 'ok' ? '<span style="font-weight:600;font-size:12px;color:var(--ok,#27c469)">✓ Connected</span>' : '') +
            (st.test === 'fail' ? '<span style="font-weight:600;font-size:12px;color:var(--warn,#e0a300)">No response — check broker &amp; port</span>' : '') +
            '</div></div>';
        }
        h += '<div style="display:flex;align-items:center;gap:12px;margin-top:22px"><button data-act="back-appliance" style="' + GHOST + '">Back</button>' +
          '<button data-act="finish" style="margin-left:auto;' + PRIM + '">' + (st.mqtt ? 'Finish' : 'Skip &amp; finish') + '</button></div></div>';
      } else if (st.screen === 'done') {
        h += '<div style="' + CARD + ';text-align:center">' +
          '<div style="width:54px;height:54px;border-radius:50%;background:var(--accent);display:flex;align-items:center;justify-content:center;margin:0 auto 18px;font-weight:700;font-size:24px;color:#04222c">✓</div>' +
          '<h1 style="font-weight:700;font-size:24px;color:var(--text);margin:0 0 6px">You\'re all set</h1>' +
          '<p style="font:400 13px/1.6 inherit;color:var(--muted);margin:0 auto 20px;max-width:400px">The gateway is live and reading the bus.</p>' +
          '<div style="text-align:left;background:var(--bg);border:1px solid var(--border);border-radius:12px;padding:6px 16px;margin:0 0 20px">' +
          '<div style="display:flex;align-items:center;padding:11px 0;border-bottom:1px solid var(--border)"><span style="font:500 12.5px inherit;color:var(--muted)">Appliance</span><span style="margin-left:auto;font-weight:600;font-size:12.5px;color:var(--text)">' + (st.appliance === 'hp' ? 'Heat pump' : 'Gas boiler') + '</span></div>' +
          '<div style="display:flex;align-items:center;padding:11px 0"><span style="font:500 12.5px inherit;color:var(--muted)">Integration</span><span style="margin-left:auto;font-weight:600;font-size:12.5px;color:var(--text)">' + (st.mqtt ? 'Home Assistant · ' + esc(st.broker) : 'Standalone (no MQTT)') + '</span></div></div>' +
          '<button data-act="go" style="' + PRIM + ';font-size:15px;padding:13px 30px">Go to dashboard</button></div>';
      }
      h += '</div></div>';
      back.innerHTML = h;
    }
    back.addEventListener('click', function (ev) {
      var t = ev.target.closest && ev.target.closest('[data-act]'); if (!t) return;
      var a = t.getAttribute('data-act');
      if (a === 'begin') { st.screen = 'appliance'; render(); }
      else if (a === 'pick-gas') { st.appliance = 'gas'; render(); }
      else if (a === 'pick-hp') { st.appliance = 'hp'; render(); }
      else if (a === 'back-welcome') { st.screen = 'welcome'; render(); }
      else if (a === 'to-mqtt') { st.screen = 'mqtt'; render(); }
      else if (a === 'back-appliance') { syncInputs(); st.screen = 'appliance'; render(); }
      else if (a === 'toggle-mqtt') { syncInputs(); st.mqtt = !st.mqtt; st.test = 'idle'; render(); }
      else if (a === 'test') { testConn(); }
      else if (a === 'finish') { syncInputs(); commit(function () { st.screen = 'done'; render(); }); }
      else if (a === 'go') { close(); fetchSettings(); showPage('home'); }
    });
    render();
  }

  // ── TASK-1012: SAT onboarding wizard (design "SAT Onboarding.dc.html") ──
  // Fires when a not-yet-onboarded SAT is enabled (Settings save OR SAT-page toggle,
  // both intercepted so SAT is NOT enabled until Finish), and once for existing SAT
  // users on their first SAT-page visit (migrate). Six screens: Welcome, Heating
  // system, Manufacturer, Tuning, Sources-health (read-only), Done. Writes only
  // existing keys + sat_onboarded. Second instance of the startOnboarding engine.
  var _satOnbShown = false;
  // System choice → satsystem value + starting heating-curve slope (satcoefficient).
  var SAT_SYS = { radiators: { sys: '1', slope: 1.4 }, underfloor: { sys: '2', slope: 0.6 }, hp: { sys: '0', slope: 0.8 } };
  function satOnbNeeded() {
    var ob = setData.sat_onboarded;
    return !(ob && ('' + ob.value) === 'true');   // falsy / absent (old firmware) => needed
  }
  // reason: 'enable' (just switched on) | 'migrate' (existing user, SAT-page visit) | 'rerun' (manual)
  function maybeShowSatOnboarding(reason) {
    if (_satOnbShown || document.getElementById('satOnbBack')) return;
    if (reason !== 'rerun' && !satOnbNeeded()) return;
    startSatOnboarding(reason);
  }
  function startSatOnboarding(reason) {
    if (document.getElementById('satOnbBack')) return;
    _satOnbShown = true;
    var migrate = (reason === 'migrate');
    var curSys = setData.satsystem ? ('' + setData.satsystem.value) : '0';
    var curSrc = setData.satsource ? ('' + setData.satsource.value) : '1';
    var st = {
      screen: 'welcome',
      system: curSys === '1' ? 'radiators' : curSys === '2' ? 'underfloor' : (curSrc === '2' ? 'hp' : 'radiators'),
      mfr: setData.satmanufacturer ? ('' + setData.satmanufacturer.value) : '0',
      tuning: 'auto',
      slope: null,   // lazily defaulted from system
      gains: (setData.satautogains && setData.satautogains.value != null) ? ('' + setData.satautogains.value) : '1.0',
      finishing: false
    };
    if (st.slope == null) st.slope = SAT_SYS[st.system].slope;

    var back = document.createElement('div'); back.id = 'satOnbBack';
    back.setAttribute('style', 'position:fixed;inset:0;z-index:200;background:var(--bg);display:flex;flex-direction:column;overflow:auto');
    document.body.appendChild(back);

    var CARD = 'background:var(--panel);border:1px solid var(--border);border-radius:16px;box-shadow:0 8px 26px rgba(0,0,0,.32)';
    var PRIM = 'margin-left:auto;background:var(--accent);border:0;color:#04222c;font-weight:700;font-size:13.5px;border-radius:10px;padding:11px 24px;cursor:pointer;min-height:44px';
    var GHOST = 'background:none;border:1px solid var(--border);color:var(--text);font-weight:600;font-size:13.5px;border-radius:10px;padding:11px 18px;cursor:pointer;min-height:44px';
    var MONO = 'font-family:ui-monospace,SFMono-Regular,Menlo,monospace';
    function esc(s) { return ('' + (s == null ? '' : s)).replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/</g, '&lt;'); }
    // Reject on non-ok so a 503 (ADR-147 heap-gate under the finish burst), 401/403
    // or reboot does NOT masquerade as success (mirrors the saveSettings r.ok audit fix).
    function post(name, value) {
      return fetch(APIGW + 'v2/settings', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ name: name, value: value }) })
        .then(function (r) { if (!r.ok) throw new Error('POST ' + name + ' -> ' + r.status); return r; });
    }
    function close() { if (back.parentNode) back.parentNode.removeChild(back); }
    function dismiss() { if (migrate) { post('sat_onboarded', true).then(function () { }, function () { }); } close(); }
    function slopePct(v) { return Math.round(((v - 0.2) / (2.4 - 0.2)) * 100); }
    function syncInputs() {
      var mf = back.querySelector('#satOnbMfr'); if (mf) st.mfr = mf.value;
      var gn = back.querySelector('#satOnbGains'); if (gn) st.gains = gn.value;
      var sl = back.querySelector('#satOnbSlope'); if (sl) st.slope = parseFloat(sl.value);
    }
    function optCard(sel) {
      return 'display:flex;align-items:center;gap:13px;width:100%;text-align:left;border-radius:12px;padding:14px 15px;cursor:pointer;font-family:inherit;' +
        (sel ? 'background:color-mix(in srgb,var(--accent) 12%,var(--panel));border:2px solid var(--accent)' : 'background:var(--bg);border:1.5px solid var(--border)');
    }
    function dot(sel) { return 'width:18px;height:18px;border-radius:50%;box-sizing:border-box;flex:0 0 auto;' + (sel ? 'border:5px solid var(--accent)' : 'border:1.5px solid var(--muted)'); }
    function recBadge() { return '<span style="font-weight:600;font-size:9.5px;letter-spacing:.05em;color:var(--accent);border:1px solid var(--accent);border-radius:20px;padding:2px 8px">RECOMMENDED</span>'; }

    // Written at Finish: satsystem + satmanufacturer + satcoefficient (effective slope) +
    // satautogains + satenabled + sat_onboarded. Heat pump also writes satsource=2.
    function commit(then) {
      var m = SAT_SYS[st.system] || SAT_SYS.radiators;
      var eff = (st.tuning === 'manual') ? st.slope : m.slope;
      // Land the settings ONE AT A TIME (never a concurrent burst). The REST
      // backpressure gate (restAPI.ino, TASK-884) caps concurrent /api requests
      // at 4 and tightens toward 1 under heap pressure, so firing all 6-7 writes
      // via Promise.all made the excess 503 and the commit fail ("SAT was not
      // enabled") -- often dropping satenabled itself, leaving a partial write.
      // Sequential keeps exactly one POST in flight, so the gate is never tripped
      // and the write order is guaranteed.
      var kv = [['satsystem', m.sys], ['satmanufacturer', st.mfr], ['satcoefficient', '' + eff]];
      if (st.system === 'hp') kv.push(['satsource', 2]);
      kv.push(['satautogains', st.tuning === 'manual' ? ('' + st.gains) : '1.0']);
      kv.push(['satenabled', true]);       // SAT enabled ONLY here, after the tuning writes land
      kv.push(['sat_onboarded', true]);    // onboarding flag last, once everything else persisted
      // then(true)=all persisted, then(false)=a POST failed (any reject aborts the chain,
      // so a failed write must NOT advance to the "SAT is now controlling your boiler" screen).
      kv.reduce(function (c, p) { return c.then(function () { return post(p[0], p[1]); }); }, Promise.resolve())
        .then(function () { then(true); }, function () { then(false); });
    }

    var health = null, healthErr = false;
    function loadHealth() {
      health = null; healthErr = false;
      // One request at a time -- fetch /health, then /sat/status only after it
      // resolves. No concurrent burst (see the REST backpressure gate, TASK-884).
      fetch(APIGW + 'v2/health').then(function (r) { return r.ok ? r.json() : null; }).catch(function () { return null; })
        .then(function (h0) {
          return fetch(APIGW + 'v2/sat/status').then(function (r) { return r.ok ? r.json() : null; }).catch(function () { return null; })
            .then(function (s0) { return [h0, s0]; });
        }).then(function (res) {
        var h = (res[0] && res[0].health) || res[0] || {}, s = res[1] || {};
        health = {
          ot: !!h.otgwconnected,
          otAge: (typeof h.boiler_age_s === 'number' && h.boiler_age_s >= 0) ? h.boiler_age_s : null,
          mqtt: !!h.mqttconnected,
          room: (s.room_temp == null) ? null : s.room_temp,
          outside: (s.outside_temp == null) ? null : s.outside_temp
        };
        if (back.querySelector('#satOnbHealth')) render();
      }).catch(function () { healthErr = true; if (back.querySelector('#satOnbHealth')) render(); });
    }
    function healthRow(label, ok, valHtml, last) {
      var c = ok ? 'var(--zone-ok)' : 'var(--zone-warn)';   // theme-aware (--ok/--warn are not defined)
      return '<div style="display:flex;align-items:center;gap:12px;padding:13px 0;' + (last ? '' : 'border-bottom:1px solid var(--border)') + '">' +
        '<span style="width:9px;height:9px;border-radius:50%;flex:0 0 auto;background:' + c + ';box-shadow:0 0 7px ' + c + '"></span>' +
        '<span style="font-weight:600;font-size:13px;color:var(--text)">' + label + '</span>' +
        '<span style="margin-left:auto;font-size:11.5px;color:' + c + '">' + valHtml + '</span></div>';
    }

    function render() {
      var steps = ['system', 'mfr', 'tuning', 'sources'];
      var idx = steps.indexOf(st.screen), total = steps.length;
      var h = '';
      h += '<div style="background:var(--nav);display:flex;align-items:center;gap:11px;padding:12px 18px">' +
        '<span style="width:16px;height:16px;border-radius:4px;background:var(--accent);box-shadow:0 0 8px var(--accent);flex:0 0 auto"></span>' +
        '<span style="font-weight:700;font-size:15px;color:#fff">OTGW firmware</span>' +
        '<span style="font-weight:600;font-size:10px;color:rgba(255,255,255,.6);letter-spacing:.05em;' + MONO + '">SAT SETUP</span></div>';
      h += '<div style="flex:1;display:flex;align-items:flex-start;justify-content:center;padding:44px 18px 60px"><div style="width:100%;max-width:580px">';
      if (idx >= 0) {
        h += '<div style="margin:0 0 20px"><div style="display:flex;justify-content:space-between;align-items:baseline;margin-bottom:8px">' +
          '<span style="font-weight:700;font-size:11px;letter-spacing:.06em;text-transform:uppercase;color:var(--muted)">Step ' + (idx + 1) + ' of ' + total + '</span>' +
          '<span style="font-weight:600;font-size:12px;color:var(--text)">' + ['Heating system', 'Manufacturer', 'Tuning', 'Sources health'][idx] + '</span></div>' +
          '<div style="height:5px;border-radius:3px;background:var(--bg);overflow:hidden"><span style="display:block;height:100%;border-radius:3px;background:var(--accent);transition:width .25s;width:' + Math.round(((idx + 1) / total) * 100) + '%"></span></div></div>';
      }
      if (st.screen === 'welcome') {
        h += '<div style="' + CARD + ';padding:34px 30px;text-align:center">' +
          '<div style="width:54px;height:54px;border-radius:14px;background:color-mix(in srgb,var(--accent) 18%,transparent);display:flex;align-items:center;justify-content:center;margin:0 auto 18px"><span style="width:16px;height:16px;border-radius:4px;background:var(--accent);box-shadow:0 0 12px var(--accent)"></span></div>' +
          '<h1 style="font-weight:700;font-size:25px;color:var(--text);margin:0 0 8px">Let\'s tune SAT for your system</h1>' +
          '<p style="font:400 13.5px/1.65 inherit;color:var(--muted);margin:0 auto 8px;max-width:440px">The <b style="color:var(--text)">Smart Autotune Thermostat</b> takes over control of your boiler, driving flow temperature from the room. Four short questions set it up.</p>' +
          '<div style="display:inline-flex;align-items:center;gap:8px;margin:6px auto 22px;padding:8px 14px;background:var(--bg);border:1px solid var(--border);border-radius:10px"><span style="width:9px;height:9px;border-radius:50%;background:var(--zone-warn);box-shadow:0 0 7px var(--zone-warn);flex:0 0 auto"></span><span style="font-weight:500;font-size:12px;color:var(--text)">SAT stays <b>disabled</b> until you finish, nothing changes on your boiler yet.</span></div>' +
          '<div style="display:flex;gap:10px;justify-content:center;flex-wrap:wrap"><button data-sa="notnow" style="' + GHOST + '">Not now</button>' +
          '<button data-sa="begin" style="background:var(--accent);border:0;color:#04222c;font-weight:700;font-size:15px;border-radius:11px;padding:13px 30px;cursor:pointer;min-height:48px">Get started</button></div></div>';
      } else if (st.screen === 'system') {
        function sysCard(k, title, desc) {
          return '<button data-sa="sys-' + k + '" style="' + optCard(st.system === k) + '"><span style="' + dot(st.system === k) + '"></span>' +
            '<div style="text-align:left"><span style="font-weight:700;font-size:14.5px;color:var(--text)">' + title + '</span><p style="font:400 11.5px/1.45 inherit;color:var(--muted);margin:3px 0 0">' + desc + '</p></div></button>';
        }
        h += '<div style="' + CARD + ';padding:26px 26px 22px"><h1 style="font-weight:700;font-size:21px;color:var(--text);margin:0 0 5px">What kind of heating do you have?</h1>' +
          '<p style="font:400 12.5px/1.55 inherit;color:var(--muted);margin:0 0 18px">Sets the heating curve and control interval SAT starts from. <span style="' + MONO + '">satsystem</span></p>' +
          '<div style="display:flex;flex-direction:column;gap:10px">' +
          sysCard('radiators', 'Radiators', 'High-temperature emitters. Steeper curve, faster response.') +
          sysCard('underfloor', 'Underfloor heating', 'Low-temperature, high inertia. Gentle curve, long cycles.') +
          sysCard('hp', 'Heat pump', 'Air/water heat pump. Also sets the appliance source to heat pump.') + '</div>' +
          (st.system === 'hp' ? '<div style="display:flex;align-items:flex-start;gap:9px;margin-top:14px;padding:10px 13px;background:var(--bg);border:1px solid var(--border);border-radius:10px"><span style="width:8px;height:8px;border-radius:50%;background:var(--accent);box-shadow:0 0 6px var(--accent);margin-top:3px;flex:0 0 auto"></span><span style="font-weight:500;font-size:11.5px;line-height:1.5;color:var(--muted)">Writes <span style="' + MONO + ';color:var(--text)">satsource = heat pump</span> alongside <span style="' + MONO + ';color:var(--text)">satsystem</span>.</span></div>' : '') +
          '<div style="display:flex;align-items:center;gap:12px;margin-top:22px"><button data-sa="back-welcome" style="' + GHOST + '">Back</button><button data-sa="to-mfr" style="' + PRIM + '">Continue</button></div></div>';
      } else if (st.screen === 'mfr') {
        var mopts = (typeof ENUM_OPTS !== 'undefined' && ENUM_OPTS.satmanufacturer ? ENUM_OPTS.satmanufacturer : [[0, 'Auto']]).map(function (o) {
          return '<option value="' + o[0] + '"' + (('' + st.mfr) === ('' + o[0]) ? ' selected' : '') + '>' + esc(o[1]) + '</option>';
        }).join('');
        var det = (satLastData && satLastData.manufacturer) ? esc(satLastData.manufacturer) : 'read from the OT bus';
        h += '<div style="' + CARD + ';padding:26px 26px 22px"><h1 style="font-weight:700;font-size:21px;color:var(--text);margin:0 0 5px">Which boiler do you have?</h1>' +
          '<p style="font:400 12.5px/1.55 inherit;color:var(--muted);margin:0 0 18px">Applies known modulation limits for your make. <span style="' + MONO + '">satmanufacturer</span></p>' +
          '<button data-sa="pick-auto" style="' + optCard(st.mfr === '0') + '"><span style="' + dot(st.mfr === '0') + '"></span>' +
          '<div style="text-align:left;flex:1"><div style="display:flex;align-items:center;gap:8px"><span style="font-weight:700;font-size:14.5px;color:var(--text)">Auto-detect</span>' + recBadge() + '</div>' +
          '<p style="font:400 11.5px/1.45 inherit;color:var(--muted);margin:4px 0 0">Reads the OpenTherm member-id from the bus. Detected: <span style="' + MONO + ';color:var(--text)">' + det + '</span></p></div></button>' +
          '<div style="margin-top:14px"><label style="font-weight:600;font-size:11.5px;color:var(--muted);display:block;margin-bottom:6px">Or set it manually</label>' +
          '<select id="satOnbMfr" style="width:100%;min-height:44px;-webkit-appearance:none;appearance:none;border:1px solid var(--border);background:var(--bg);color:var(--text);border-radius:8px;padding:11px 34px 11px 12px;font-weight:500;font-size:13.5px;cursor:pointer">' + mopts + '</select></div>' +
          '<div style="display:flex;align-items:center;gap:12px;margin-top:22px"><button data-sa="back-system" style="' + GHOST + '">Back</button><button data-sa="to-tuning" style="' + PRIM + '">Continue</button></div></div>';
      } else if (st.screen === 'tuning') {
        h += '<div style="' + CARD + ';padding:26px 26px 22px"><h1 style="font-weight:700;font-size:21px;color:var(--text);margin:0 0 5px">How should SAT tune itself?</h1>' +
          '<p style="font:400 12.5px/1.55 inherit;color:var(--muted);margin:0 0 18px">SAT has no raw P/I/D, pick automatic learning, or set the curve slope yourself.</p>' +
          '<div style="display:flex;flex-direction:column;gap:10px">' +
          '<button data-sa="tune-auto" style="' + optCard(st.tuning === 'auto') + '"><span style="' + dot(st.tuning === 'auto') + '"></span><div style="text-align:left;flex:1"><div style="display:flex;align-items:center;gap:8px"><span style="font-weight:700;font-size:14.5px;color:var(--text)">Automatic</span>' + recBadge() + '</div><p style="font:400 11.5px/1.45 inherit;color:var(--muted);margin:4px 0 0">SAT learns your home\'s response over ~2 weeks and adjusts gains. <span style="' + MONO + '">satautogains = 1.0</span></p></div></button>' +
          '<button data-sa="tune-manual" style="' + optCard(st.tuning === 'manual') + '"><span style="' + dot(st.tuning === 'manual') + '"></span><div style="text-align:left;flex:1"><span style="font-weight:700;font-size:14.5px;color:var(--text)">Manual</span><p style="font:400 11.5px/1.45 inherit;color:var(--muted);margin:4px 0 0">Set the heating-curve slope yourself; auto-gains off. For experienced users.</p></div></button></div>';
        if (st.tuning === 'manual') {
          var pct = slopePct(st.slope);
          h += '<div style="margin-top:16px;background:var(--bg);border:1px solid var(--border);border-radius:12px;padding:16px">' +
            '<div style="display:flex;align-items:baseline;margin-bottom:12px"><div><span style="font-weight:600;font-size:13px;color:var(--text)">Heating curve slope</span><p style="font:400 11px inherit;color:var(--muted);margin:2px 0 0">Flow temp rise per °C of outdoor drop · <span style="' + MONO + '">satcoefficient</span></p></div>' +
            '<span id="satOnbSlopeVal" style="margin-left:auto;font-weight:600;font-size:17px;' + MONO + ';color:var(--accent)">' + st.slope.toFixed(1) + '</span></div>' +
            '<input id="satOnbSlope" type="range" class="satonb-slope" min="0.2" max="2.4" step="0.1" value="' + st.slope + '" style="width:100%;background-image:linear-gradient(90deg,var(--accent) ' + pct + '%,var(--tile-off,#54565a) ' + pct + '%)">' +
            '<div style="display:flex;justify-content:space-between;margin-top:7px;font-weight:500;font-size:10px;color:var(--muted)"><span>Gentle · underfloor</span><span>Steep · radiators</span></div>' +
            '<div style="display:flex;align-items:center;gap:12px;margin-top:16px;padding-top:14px;border-top:1px solid var(--border)"><div style="flex:1"><span style="font-weight:600;font-size:13px;color:var(--text)">Auto-gains multiplier</span><p style="font:400 11px inherit;color:var(--muted);margin:2px 0 0">Scales learned corrections · <span style="' + MONO + '">satautogains</span></p></div>' +
            '<input id="satOnbGains" value="' + esc(st.gains) + '" inputmode="decimal" style="width:74px;min-height:44px;text-align:center;border:1px solid var(--border);background:var(--panel);color:var(--text);border-radius:8px;padding:9px 8px;font-weight:600;font-size:13.5px;' + MONO + '"></div></div>';
        }
        h += '<div style="display:flex;align-items:center;gap:12px;margin-top:22px"><button data-sa="back-mfr" style="' + GHOST + '">Back</button><button data-sa="to-sources" style="' + PRIM + '">Continue</button></div></div>';
      } else if (st.screen === 'sources') {
        var rows;
        if (health == null && !healthErr) {
          rows = '<div style="padding:22px 0;text-align:center;color:var(--muted);font-size:12.5px">Checking sources…</div>';
        } else {
          var hh = health || {};
          rows = healthRow('OpenTherm bus', !!hh.ot, hh.ot ? ('Online' + (hh.otAge != null ? ' · ' + hh.otAge + 's ago' : '')) : 'Offline') +
            healthRow('Room temperature', hh.room != null, hh.room != null ? (hh.room.toFixed(1) + ' °C') : 'Not detected') +
            healthRow('Outside temperature', hh.outside != null, hh.outside != null ? (hh.outside.toFixed(1) + ' °C') : 'Not detected') +
            healthRow('MQTT', !!hh.mqtt, hh.mqtt ? 'Connected' : 'Not connected', true);
        }
        var warnOutside = health && health.outside == null;
        h += '<div style="' + CARD + ';padding:26px 26px 22px"><h1 style="font-weight:700;font-size:21px;color:var(--text);margin:0 0 5px">Source health check</h1>' +
          '<p style="font:400 12.5px/1.55 inherit;color:var(--muted);margin:0 0 18px">SAT reads these from your existing setup. This is a read-only check, configure inputs on their own pages. <span style="' + MONO + '">/api/v2/health</span></p>' +
          '<div id="satOnbHealth" style="background:var(--bg);border:1px solid var(--border);border-radius:12px;padding:2px 16px">' + rows + '</div>' +
          (warnOutside ? '<div style="display:flex;align-items:flex-start;gap:9px;margin-top:14px;padding:11px 13px;background:color-mix(in srgb,var(--zone-warn) 12%,var(--bg));border:1px solid color-mix(in srgb,var(--zone-warn) 40%,var(--border));border-radius:10px"><span style="width:8px;height:8px;border-radius:50%;background:var(--zone-warn);box-shadow:0 0 7px var(--zone-warn);margin-top:3px;flex:0 0 auto"></span><span style="font-weight:500;font-size:12px;line-height:1.5;color:var(--text)">No outside temperature. SAT will run without weather compensation until you set a location under <b>SAT ▸ Weather</b>. Not required to continue.</span></div>' : '') +
          (st.finishErr ? '<div style="margin-top:14px;padding:11px 13px;background:color-mix(in srgb,var(--zone-alert) 12%,var(--bg));border:1px solid color-mix(in srgb,var(--zone-alert) 45%,var(--border));border-radius:10px;font-weight:500;font-size:12px;line-height:1.5;color:var(--text)">Could not save the settings, SAT was not enabled. Check the device is reachable and try again.</div>' : '') +
          '<div style="display:flex;align-items:center;gap:12px;margin-top:22px"><button data-sa="back-tuning" style="' + GHOST + '">Back</button><button data-sa="finish" style="' + PRIM + '"' + (st.finishing ? ' disabled' : '') + '>' + (st.finishing ? 'Enabling…' : (st.finishErr ? 'Retry' : 'Enable SAT')) + '</button></div></div>';
      } else if (st.screen === 'done') {
        var sysL = { radiators: 'Radiators', underfloor: 'Underfloor heating', hp: 'Heat pump' }[st.system];
        var mfrL = (typeof ENUM_OPTS !== 'undefined' && ENUM_OPTS.satmanufacturer ? (ENUM_OPTS.satmanufacturer.filter(function (o) { return ('' + o[0]) === ('' + st.mfr); })[0] || [0, 'Auto'])[1] : 'Auto');
        var tuneL = st.tuning === 'auto' ? 'Automatic (auto-gains)' : ('Manual · slope ' + st.slope.toFixed(1));
        h += '<div style="' + CARD + ';padding:30px 28px;text-align:center"><div style="width:54px;height:54px;border-radius:50%;background:var(--accent);display:flex;align-items:center;justify-content:center;margin:0 auto 18px;font-weight:700;font-size:24px;color:#04222c">✓</div>' +
          '<h1 style="font-weight:700;font-size:24px;color:var(--text);margin:0 0 6px">SAT is now controlling your boiler</h1>' +
          '<p style="font:400 13px/1.6 inherit;color:var(--muted);margin:0 auto 20px;max-width:420px">Room-driven control is live. It may take a heating cycle or two to settle.</p>' +
          '<div style="text-align:left;background:var(--bg);border:1px solid var(--border);border-radius:12px;padding:6px 16px;margin:0 0 20px">' +
          '<div style="display:flex;align-items:center;padding:11px 0;border-bottom:1px solid var(--border)"><span style="font-weight:500;font-size:12.5px;color:var(--muted)">Heating system</span><span style="margin-left:auto;font-weight:600;font-size:12.5px;color:var(--text)">' + sysL + '</span></div>' +
          '<div style="display:flex;align-items:center;padding:11px 0;border-bottom:1px solid var(--border)"><span style="font-weight:500;font-size:12.5px;color:var(--muted)">Boiler</span><span style="margin-left:auto;font-weight:600;font-size:12.5px;color:var(--text)">' + esc(mfrL) + '</span></div>' +
          '<div style="display:flex;align-items:center;padding:11px 0"><span style="font-weight:500;font-size:12.5px;color:var(--muted)">Tuning</span><span style="margin-left:auto;font-weight:600;font-size:12.5px;color:var(--text)">' + tuneL + '</span></div></div>' +
          '<p style="font:400 11px/1.6 inherit;color:var(--muted);margin:0 0 20px;text-align:left">Written to settings: <span style="' + MONO + '">satenabled=on · sat_onboarded=on</span> plus every answer above.</p>' +
          '<div style="display:flex;gap:10px;justify-content:center;flex-wrap:wrap"><button data-sa="restart" style="' + GHOST + '">Run setup again</button>' +
          '<button data-sa="go" style="background:var(--accent);border:0;color:#04222c;font-weight:700;font-size:15px;border-radius:11px;padding:13px 28px;cursor:pointer;min-height:48px">Open SAT dashboard</button></div></div>';
      }
      h += '</div></div>';
      back.innerHTML = h;
      // live-update slope without a full re-render (keeps thumb focus)
      var sl = back.querySelector('#satOnbSlope');
      if (sl) sl.addEventListener('input', function () {
        st.slope = parseFloat(sl.value);
        var v = back.querySelector('#satOnbSlopeVal'); if (v) v.textContent = st.slope.toFixed(1);
        sl.style.backgroundImage = 'linear-gradient(90deg,var(--accent) ' + slopePct(st.slope) + '%,var(--tile-off,#54565a) ' + slopePct(st.slope) + '%)';
      });
      var mf = back.querySelector('#satOnbMfr');
      if (mf) mf.addEventListener('change', function () { st.mfr = mf.value; render(); });
    }

    back.addEventListener('click', function (ev) {
      var t = ev.target.closest && ev.target.closest('[data-sa]'); if (!t) return;
      var a = t.getAttribute('data-sa');
      if (a === 'begin') { st.screen = 'system'; render(); }
      else if (a === 'notnow') { dismiss(); }
      else if (a === 'sys-radiators' || a === 'sys-underfloor' || a === 'sys-hp') { st.system = a.slice(4); st.slope = SAT_SYS[st.system].slope; render(); }
      else if (a === 'back-welcome') { st.screen = 'welcome'; render(); }
      else if (a === 'to-mfr') { st.screen = 'mfr'; render(); }
      else if (a === 'pick-auto') { st.mfr = '0'; render(); }
      else if (a === 'back-system') { syncInputs(); st.screen = 'system'; render(); }
      else if (a === 'to-tuning') { syncInputs(); st.screen = 'tuning'; render(); }
      else if (a === 'tune-auto') { syncInputs(); st.tuning = 'auto'; render(); }
      else if (a === 'tune-manual') { syncInputs(); st.tuning = 'manual'; render(); }
      else if (a === 'back-mfr') { syncInputs(); st.screen = 'mfr'; render(); }
      else if (a === 'to-sources') { syncInputs(); st.screen = 'sources'; render(); loadHealth(); }
      else if (a === 'back-tuning') { st.screen = 'tuning'; render(); }
      else if (a === 'finish') {
        if (st.finishing) return; st.finishing = true; st.finishErr = false; render();
        commit(function (ok) { st.finishing = false; if (ok) { st.screen = 'done'; } else { st.finishErr = true; } render(); });
      }
      else if (a === 'restart') { st.screen = 'welcome'; st.system = 'radiators'; st.mfr = '0'; st.tuning = 'auto'; st.slope = SAT_SYS.radiators.slope; st.gains = '1.0'; render(); }
      else if (a === 'go') { close(); fetchSettings(); showPage('sat'); }
    });
    render();
  }

  function showSimModal() {
    if (document.getElementById('simBackdrop')) return;
    var bd = document.createElement('div'); bd.id = 'simBackdrop'; bd.className = 'sim-backdrop';
    var m = document.createElement('div'); m.className = 'sim-modal';
    var h = document.createElement('h3'); h.textContent = '⚙ No boiler detected';
    var p = document.createElement('p'); p.textContent = 'This gateway sees no boiler or thermostat on the OpenTherm bus — looks like a bench setup. Run in simulation mode to preview live-like data?';
    var acts = document.createElement('div'); acts.className = 'acts';
    var no = document.createElement('button'); no.textContent = 'Not now';
    var yes = document.createElement('button'); yes.className = 'primary'; yes.textContent = 'Enable simulation';
    function closeSim() { try { localStorage.setItem('otgw-sim-prompted', '1'); } catch (e) { } if (bd.parentNode) bd.parentNode.removeChild(bd); }
    no.addEventListener('click', closeSim);
    yes.addEventListener('click', function () {
      fetch(APIGW + 'v2/settings', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ name: 'simulation', value: true }) })
        .then(function (r) { if (r.ok && typeof bleToast === 'function') bleToast('Simulation enabled'); })
        .catch(function () { });
      closeSim();
    });
    bd.addEventListener('click', function (e) { if (e.target === bd) closeSim(); });
    acts.appendChild(no); acts.appendChild(yes);
    m.appendChild(h); m.appendChild(p); m.appendChild(acts); bd.appendChild(m);
    document.body.appendChild(bd);
  }
  function maybePromptSimulation() {
    try { if (localStorage.getItem('otgw-sim-prompted') === '1') return; } catch (e) { }
    withDeviceInfo(function (d) {
      if (!d) return;
      if (d.otcommandinterface !== 'PIC') return;                                  // /simulate is PIC-path only
      if (d.otgwsimulation) return;                                                // already simulating
      if (d.boilerconnected || d.thermostatconnected || d.otgwconnected) return;   // something on the bus -> not a bench
      showSimModal();
    });
  }

  // ---------- TASK-992: hide the Advanced > PIC sub-tab when no PIC is present ----------
  // OT-Direct boards (OTGW32) have no PIC firmware, so the PIC tab is dead there.
  function applyPicTabVisibility() {
    withDeviceInfo(function (d) {
      if (!d) return;   // snapshot unknown — keep the tab as-is rather than hide it on a failed fetch
      var isPic = (d.otcommandinterface === 'PIC');
      var btn = document.querySelector('#advTabs [data-adv="pic"]');
      var panel = document.getElementById('adv-pic');
      if (btn) btn.classList.toggle('hidden', !isPic);
      if (!isPic) {
        var wasActive = (btn && btn.classList.contains('active')) || (panel && panel.classList.contains('active'));
        if (panel) panel.classList.remove('active');
        if (wasActive && typeof showAdvTab === 'function') showAdvTab('debug');
      }
    });
  }

  // ---------- init ----------
  function init() {
    initTheme();
    document.querySelectorAll('.seg').forEach(function (s) {
      s.addEventListener('click', function () { showPage(s.dataset.page); });
    });
    document.querySelectorAll('.vp-item').forEach(function (s) {
      s.addEventListener('click', function () { showDesign(s.dataset.design); closeViewMenu(); });
    });
    (function () {
      var btn = document.getElementById('viewPickBtn'), menu = document.getElementById('viewPickMenu');
      if (!btn || !menu) return;
      btn.addEventListener('click', function (e) {
        e.stopPropagation();
        var open = menu.classList.toggle('show');
        btn.setAttribute('aria-expanded', open ? 'true' : 'false');
      });
      document.addEventListener('click', function (e) {
        if (!e.target.closest || !e.target.closest('#viewPick')) closeViewMenu();
      });
    })();
    // Monitor sub-tabs only (scoped so Advanced's own .subtab elements are wired separately).
    document.querySelectorAll('#page-monitor .subtab').forEach(function (s) {
      s.addEventListener('click', function () { showTab(s.dataset.tab); });
    });
    // TASK-982: Advanced sub-tabs (PIC / Debug / File System / System).
    document.querySelectorAll('#advTabs .subtab').forEach(function (s) {
      s.addEventListener('click', function () { showAdvTab(s.dataset.adv); });
    });
    // TASK-982: Advanced > System action buttons.
    document.querySelectorAll('#adv-system [data-act]').forEach(function (b) {
      b.addEventListener('click', function () { advAction(b.dataset.act); });
    });
    // TASK-982: Advanced > File System upload (button enables once a file is chosen).
    var fsFileInp = document.getElementById('fsFile');
    if (fsFileInp) fsFileInp.addEventListener('change', function () {
      var u = document.getElementById('fsUpload'); if (u) u.disabled = !(fsFileInp.files && fsFileInp.files.length);
    });
    var fsUpBtn = document.getElementById('fsUpload');
    if (fsUpBtn) fsUpBtn.addEventListener('click', fsDoUpload);
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
    var pcu = document.getElementById('picCheckUpd'); if (pcu) pcu.addEventListener('click', checkPicUpdate);   // TASK-972
    var bDisc = document.getElementById('btnDiscard'); if (bDisc) bDisc.addEventListener('click', discardSettings);

    wireSat();   // TASK-986: bind SAT-page controls before the page is restored below
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
    // TASK-989: STAGGER the startup API calls. Firing them all at once (plus the
    // three big assets) is the parallel burst that collapses the S3 heap under
    // concurrent serving. Settings first (the connectivity model needs it), then
    // conn shortly after for the first strip render; the heavier device/info and
    // the seed/SAT/weather cluster trickle in behind (see below).
    fetchSettings();
    setTimeout(fetchConn, 400);
    setInterval(fetchConn, 15000);
    // TASK-990: surface newly discovered BLE + 1-Wire sensors as discovery toasts
    // (any page), via a gentle retrying scheduler that stays out of the boot burst.
    startDiscoveryPolling();
    // TASK-983/992: one delayed device/info fetch feeds BOTH the header identity
    // chips and the PIC-tab visibility (withDeviceInfo dedups + caches 8s).
    setTimeout(function () { applyPicTabVisibility(); withDeviceInfo(function (d) { if (d) fillHeaderIdentity(d); }); }, 1200);
    // TASK-991: on a bench setup (no boiler/thermostat on the bus, PIC board), offer
    // simulation mode once. Delayed past the boot burst so the bus state has settled.
    setTimeout(maybePromptSimulation, 12000);

    // TASK-984: close the floating override panel on an outside click. The two triggers
    // (#aOvrBadge on Home, #cnOvr on the connection map) stopPropagation on their own
    // clicks, but whitelist them here too so a bubbled click never self-closes the panel.
    document.addEventListener('click', function (e) {
      var p = document.getElementById('ovrPanel'); if (!p || !p.classList.contains('show')) return;
      if (!p.contains(e.target) && !(e.target.closest && (e.target.closest('#cnOvr') || e.target.closest('#aOvrBadge')))) p.classList.remove('show');
    });

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
    // TASK-989: staggered (see the startup-burst note above) — seed/SAT/weather
    // trail the first-paint fetches one at a time instead of bursting together.
    setTimeout(fetchSeed, 2000); setTimeout(fetchSatStatus, 2800); setTimeout(fetchWeather, 3600);
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
