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
  }

  // ---------- back to the classic UI ----------
  function gotoClassic() {
    // Flip the device-wide default to classic, then land on the root path.
    fetch(APIGW + 'v2/settings', {
      method: 'POST', mode: 'cors',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: 'ui_usev2', value: 'false' })
    }).catch(function () { /* navigate regardless */ })
      .finally(function () { location.href = '/'; });
  }

  // ---------- clock ----------
  function tick() {
    var el = document.getElementById('clock');
    if (!el) return;
    var d = new Date();
    function p(n) { return (n < 10 ? '0' : '') + n; }
    el.textContent = p(d.getHours()) + ':' + p(d.getMinutes()) + ':' + p(d.getSeconds());
  }

  // ---------- connectivity strip (static placeholder; P4 wires live data) ----------
  function renderConnStrip() {
    var strip = document.getElementById('connStrip');
    if (!strip) return;
    var pills = [
      { k: 'WiFi', v: '—', s: 'st-unknown' },
      { k: 'MQTT', v: '—', s: 'st-unknown' },
      { k: 'OT bus', v: '—', s: 'st-unknown' },
      { k: 'NTP', v: '—', s: 'st-unknown' }
    ];
    strip.innerHTML = pills.map(function (p) {
      return '<button class="connpill ' + p.s + '"><span class="d"></span>' +
        '<span class="pk">' + p.k + '</span> <span class="pv">' + p.v + '</span></button>';
    }).join('');
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
    if (changed) { model.fresh = true; scheduleRender(); }
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
    if (raw) applyFrame(raw);
  }

  function connectWs() {
    var url = (location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + WS_PATH;
    try {
      ws = new WebSocket(url);
      ws.onmessage = onWsMessage;
      ws.onclose = function () { ws = null; scheduleReconnect(); };
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
    // B and C renderers are added in later phases.
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

    var sp = localStorage.getItem('otgw-v2-page'); showPage(sp || 'home');
    var sd = localStorage.getItem('otgw-v2-design'); showDesign(sd || 'a');
    renderConnStrip();
    tick();
    setInterval(tick, 1000);

    // Live OT data: connect the log WebSocket and render the active Home concept.
    connectWs();
    renderActive();

    // Hooks for later phases to attach live-data renderers.
    window.OTGWv2 = {
      APIGW: APIGW, showPage: showPage, showDesign: showDesign, showTab: showTab,
      model: model, renderActive: renderActive
    };
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init);
  else init();
})();
