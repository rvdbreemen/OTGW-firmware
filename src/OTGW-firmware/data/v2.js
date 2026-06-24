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

    // Hooks for later phases to attach live-data renderers.
    window.OTGWv2 = { APIGW: APIGW, showPage: showPage, showDesign: showDesign, showTab: showTab };
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init);
  else init();
})();
