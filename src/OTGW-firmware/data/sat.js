/*
***************************************************************************
**  Module   : sat.js
**  Description: SAT (Smart Autotune Thermostat) Web UI dashboard
**
**  Visualization-only dashboard. All control logic runs on the ESP.
**  This page fetches state via REST API and displays it.
**  Settings changes go through the standard settings API.
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License.
***************************************************************************
*/

var SAT = (function() {
  'use strict';

  var POLL_INTERVAL_MS = 5000;
  var _pollTimer = null;
  var _chartInstance = null;
  var _chartData = { time: [], setpoint: [], flow: [], room: [], outside: [], pid: [] };
  var _maxChartPoints = 720; // 1 hour at 5s intervals

  // --- Mode / Status label maps ---
  var MODE_LABELS  = ['Off', 'Continuous', 'PWM'];
  var CYCLE_LABELS = ['None', 'Good', 'Overshoot', 'Underheat', 'Short', 'Uncertain'];
  var BOILER_LABELS = [
    'Off', 'Idle', 'Preheating', 'At Setpoint', 'Modulating Up', 'Modulating Down',
    'Ignition Surge', 'Stalled Ignition', 'Anti-Cycling', 'Pump Starting',
    'Waiting Flame', 'Overshoot Cooling', 'Post-Cycle', 'Heating', 'Cooling'
  ];

  // --- DOM helpers ---
  function el(id) { return document.getElementById(id); }

  function setText(id, text) {
    var e = el(id);
    if (e) e.textContent = text;
  }

  function setHTML(id, html) {
    var e = el(id);
    if (e) e.innerHTML = html;
  }

  function addClass(id, cls) {
    var e = el(id);
    if (e) e.classList.add(cls);
  }

  function removeClass(id, cls) {
    var e = el(id);
    if (e) e.classList.remove(cls);
  }

  function fmtTemp(v) {
    return (v !== null && v !== undefined) ? v.toFixed(1) + '\u00B0C' : '--';
  }

  // --- Fetch SAT status from REST API ---
  function fetchStatus() {
    fetch(APIGW + 'v2/sat/status')
      .then(function(r) { return r.ok ? r.json() : Promise.reject(r.statusText); })
      .then(function(d) { updateDashboard(d); })
      .catch(function(err) {
        console.warn('SAT fetch error:', err);
        setText('sat-status-badge', 'Error');
        addClass('sat-status-badge', 'sat-badge-error');
      });
  }

  // --- Update the dashboard with fresh data ---
  function updateDashboard(d) {
    // Status badge
    var badge = el('sat-status-badge');
    if (badge) {
      if (!d.enabled) {
        badge.textContent = 'Disabled';
        badge.className = 'sat-badge sat-badge-disabled';
      } else if (d.active) {
        badge.textContent = MODE_LABELS[d.control_mode] || 'Active';
        badge.className = 'sat-badge sat-badge-active';
      } else {
        badge.textContent = 'Idle';
        badge.className = 'sat-badge sat-badge-idle';
      }
    }

    // Temperature cards
    setText('sat-room-temp', fmtTemp(d.room_temp));
    setText('sat-target-temp', fmtTemp(d.target_temp));
    setText('sat-outside-temp', fmtTemp(d.outside_temp));
    setText('sat-flow-temp', fmtTemp(d.final_setpoint));

    // Control details
    setText('sat-heating-curve', d.heating_curve !== undefined ? d.heating_curve.toFixed(1) + '\u00B0C' : '--');
    setText('sat-pid-output', d.pid_output !== undefined ? d.pid_output.toFixed(1) + '\u00B0C' : '--');
    setText('sat-error', d.error !== undefined ? d.error.toFixed(2) + '\u00B0C' : '--');
    setText('sat-control-mode', MODE_LABELS[d.control_mode] || '--');
    setText('sat-boiler-status', BOILER_LABELS[d.boiler_status] || '--');
    setText('sat-coefficient', d.coefficient !== undefined ? d.coefficient.toFixed(1) : '--');
    setText('sat-deadband', d.deadband !== undefined ? d.deadband.toFixed(2) + '\u00B0C' : '--');
    setText('sat-heating-system', d.heating_system === 1 ? 'Underfloor' : 'Radiator');

    // PID details
    setText('sat-pid-p', d.pid_p !== undefined ? d.pid_p.toFixed(2) : '--');
    setText('sat-pid-i', d.pid_i !== undefined ? d.pid_i.toFixed(2) : '--');
    setText('sat-pid-d', d.pid_d !== undefined ? d.pid_d.toFixed(2) : '--');
    setText('sat-kp', d.kp !== undefined ? d.kp.toFixed(4) : '--');
    setText('sat-ki', d.ki !== undefined ? d.ki.toFixed(6) : '--');
    setText('sat-kd', d.kd !== undefined ? d.kd.toFixed(2) : '--');

    // PWM info
    setText('sat-pwm-duty', d.pwm_duty !== undefined ? (d.pwm_duty * 100).toFixed(0) + '%' : '--');
    setText('sat-pwm-flame', d.pwm_flame_req ? 'Requesting' : 'Off');

    // Cycle info
    setText('sat-cycle-count', d.cycle_count !== undefined ? d.cycle_count : '--');
    setText('sat-last-cycle', CYCLE_LABELS[d.last_cycle_class] || '--');
    setText('sat-cycle-max-flow', d.cycle_max_flow !== undefined ? d.cycle_max_flow.toFixed(1) + '\u00B0C' : '--');
    setText('sat-cycle-overshoot', d.cycle_overshoot_sec !== undefined ? d.cycle_overshoot_sec.toFixed(0) + 's' : '--');

    // External sensor indicators
    var extTempEl = el('sat-ext-temp-indicator');
    if (extTempEl) {
      extTempEl.className = 'sat-indicator ' + (d.external_temp_valid ? 'sat-indicator-on' : 'sat-indicator-off');
      extTempEl.title = d.external_temp_valid ? 'External indoor sensor active' : 'Using OT bus room temp';
    }
    var extOutEl = el('sat-ext-outdoor-indicator');
    if (extOutEl) {
      extOutEl.className = 'sat-indicator ' + (d.external_outdoor_valid ? 'sat-indicator-on' : 'sat-indicator-off');
      extOutEl.title = d.external_outdoor_valid ? 'External outdoor sensor active' : 'Using OT bus outdoor temp';
    }

    // Update chart
    updateChart(d);
  }

  // --- Chart ---
  function initChart() {
    var container = el('sat-chart');
    if (!container || typeof echarts === 'undefined') return;
    _chartInstance = echarts.init(container);
    var option = {
      tooltip: { trigger: 'axis' },
      legend: { data: ['Setpoint', 'Flow', 'Room', 'Outside', 'PID'], bottom: 0 },
      grid: { left: 50, right: 20, top: 20, bottom: 60 },
      xAxis: { type: 'category', data: [], boundaryGap: false },
      yAxis: { type: 'value', name: '\u00B0C', axisLabel: { formatter: '{value}\u00B0' } },
      series: [
        { name: 'Setpoint', type: 'line', smooth: true, data: [], lineStyle: { width: 2 }, symbol: 'none' },
        { name: 'Flow',     type: 'line', smooth: true, data: [], lineStyle: { width: 2 }, symbol: 'none' },
        { name: 'Room',     type: 'line', smooth: true, data: [], lineStyle: { width: 2 }, symbol: 'none' },
        { name: 'Outside',  type: 'line', smooth: true, data: [], lineStyle: { width: 1, type: 'dashed' }, symbol: 'none' },
        { name: 'PID',      type: 'line', smooth: true, data: [], lineStyle: { width: 1, type: 'dotted' }, symbol: 'none' }
      ]
    };
    _chartInstance.setOption(option);
  }

  function updateChart(d) {
    if (!_chartInstance) return;
    var now = new Date();
    var timeLabel = ('0' + now.getHours()).slice(-2) + ':' + ('0' + now.getMinutes()).slice(-2) + ':' + ('0' + now.getSeconds()).slice(-2);
    _chartData.time.push(timeLabel);
    _chartData.setpoint.push(d.final_setpoint);
    _chartData.flow.push(d.heating_curve);
    _chartData.room.push(d.room_temp);
    _chartData.outside.push(d.outside_temp);
    _chartData.pid.push(d.pid_output);
    // Trim to max points
    if (_chartData.time.length > _maxChartPoints) {
      _chartData.time.shift();
      _chartData.setpoint.shift();
      _chartData.flow.shift();
      _chartData.room.shift();
      _chartData.outside.shift();
      _chartData.pid.shift();
    }
    _chartInstance.setOption({
      xAxis: { data: _chartData.time },
      series: [
        { data: _chartData.setpoint },
        { data: _chartData.flow },
        { data: _chartData.room },
        { data: _chartData.outside },
        { data: _chartData.pid }
      ]
    });
  }

  function resizeChart() {
    if (_chartInstance) _chartInstance.resize();
  }

  // --- Public API ---
  function start() {
    initChart();
    fetchStatus(); // immediate first fetch
    _pollTimer = setInterval(fetchStatus, POLL_INTERVAL_MS);
    window.addEventListener('resize', resizeChart);
  }

  function stop() {
    if (_pollTimer) { clearInterval(_pollTimer); _pollTimer = null; }
    window.removeEventListener('resize', resizeChart);
  }

  function setTheme(theme) {
    if (_chartInstance) {
      var container = el('sat-chart');
      _chartInstance.dispose();
      _chartInstance = echarts.init(container, theme === 'dark' ? 'dark' : null);
      // Re-apply chart data
      var option = {
        tooltip: { trigger: 'axis' },
        legend: { data: ['Setpoint', 'Flow', 'Room', 'Outside', 'PID'], bottom: 0 },
        grid: { left: 50, right: 20, top: 20, bottom: 60 },
        xAxis: { type: 'category', data: _chartData.time, boundaryGap: false },
        yAxis: { type: 'value', name: '\u00B0C', axisLabel: { formatter: '{value}\u00B0' } },
        series: [
          { name: 'Setpoint', type: 'line', smooth: true, data: _chartData.setpoint, lineStyle: { width: 2 }, symbol: 'none' },
          { name: 'Flow',     type: 'line', smooth: true, data: _chartData.flow, lineStyle: { width: 2 }, symbol: 'none' },
          { name: 'Room',     type: 'line', smooth: true, data: _chartData.room, lineStyle: { width: 2 }, symbol: 'none' },
          { name: 'Outside',  type: 'line', smooth: true, data: _chartData.outside, lineStyle: { width: 1, type: 'dashed' }, symbol: 'none' },
          { name: 'PID',      type: 'line', smooth: true, data: _chartData.pid, lineStyle: { width: 1, type: 'dotted' }, symbol: 'none' }
        ]
      };
      _chartInstance.setOption(option);
    }
  }

  return {
    start: start,
    stop: stop,
    setTheme: setTheme
  };
})();
