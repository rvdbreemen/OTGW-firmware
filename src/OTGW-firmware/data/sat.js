/*
***************************************************************************
**  Module   : sat.js
**  Description: SAT (Smart Autotune Thermostat) Web UI dashboard
**
**  Part of SAT (Smart Autotune Thermostat) firmware port.
**  Original SAT component by Alex Wijnholds (https://github.com/Alexwijn/SAT)
**  SAT concept and algorithm design by George Dellas
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
  var _weatherTimer = null;
  var _chartInstance = null;
  var _curveChartInstance = null;
  var _curveVisible = true;
  var _lastCurveCoeff = -1;
  var _lastCurveSystem = -1;
  var _lastCurveTarget = -1;
  var _chartData = { time: [], setpoint: [], flow: [], room: [], outside: [], pid: [] };
  var _maxChartPoints = 720; // 1 hour at 5s intervals

  // --- Heating curve constants (must match SATcontrol.ino) ---
  var HC_BASE_OFFSET_FLOOR = 20.0;
  var HC_BASE_OFFSET_RAD   = 27.2;
  var HC_REF_TEMP          = 20.0;

  // --- Mode / Status label maps ---
  var MODE_LABELS  = ['Off', 'Continuous', 'PWM'];
  var CYCLE_LABELS = ['None', 'Good', 'Overshoot', 'Underheat', 'Short', 'Uncertain'];
  var BOILER_LABELS = {
    'off': 'Off', 'idle': 'Idle', 'preheating': 'Preheating',
    'at_setpoint': 'At Setpoint', 'modulating_up': 'Modulating Up',
    'modulating_down': 'Modulating Down', 'ignition_surge': 'Ignition Surge',
    'stalled_ignition': 'Stalled Ignition', 'anti_cycling': 'Anti-Cycling',
    'pump_starting': 'Pump Starting', 'waiting_flame': 'Waiting Flame',
    'overshoot_cooling': 'Overshoot Cooling', 'post_cycle': 'Post-Cycle',
    'heating': 'Heating', 'cooling': 'Cooling'
  };

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
      .then(function(r) {
        if (!r.ok) return Promise.reject(r.statusText);
        var ct = r.headers.get('content-type') || '';
        if (ct.indexOf('application/json') === -1) return Promise.reject('Not JSON');
        return r.json();
      })
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
      } else if (d.safety_tripped) {
        badge.textContent = 'Safety Tripped';
        badge.className = 'sat-badge sat-badge-error';
      } else if (d.active) {
        badge.textContent = MODE_LABELS[d.control_mode] || 'Active';
        badge.className = 'sat-badge sat-badge-active';
      } else {
        badge.textContent = 'Idle';
        badge.className = 'sat-badge sat-badge-idle';
      }
    }

    // Simulation badge
    var simBadge = el('sat-sim-badge');
    if (simBadge) {
      if (d.simulation) {
        simBadge.textContent = 'SIMULATION';
        simBadge.className = 'sat-badge sat-badge-sim';
        simBadge.style.display = '';
      } else {
        simBadge.style.display = 'none';
      }
    }
    // Simulation toggle button
    var simBtn = el('sat-btn-simulation');
    if (simBtn) {
      _lastSimEnabled = !!d.simulation;
      simBtn.textContent = _lastSimEnabled ? 'Stop Sim' : 'Start Sim';
      simBtn.className = 'sat-btn sat-btn-toggle' + (_lastSimEnabled ? ' active' : '');
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
    setText('sat-overshoot-margin', d.overshoot_margin !== undefined ? d.overshoot_margin.toFixed(1) + '\u00B0C' : '--');
    // Preset
    var presetNames = ['None', 'Away', 'Eco', 'Comfort', 'Sleep', 'Activity'];
    setText('sat-preset', d.active_preset !== undefined ? (presetNames[d.active_preset] || '--') : '--');

    // Update control button states
    _lastSATEnabled = !!d.enabled;
    _lastPresetIdx = d.active_preset || 0;
    _lastModeIdx = d.control_mode || 0;

    // Highlight active preset button
    var presetBtns = ['away', 'eco', 'comfort', 'sleep'];
    var presetMap = { 1: 'away', 2: 'eco', 3: 'comfort', 4: 'sleep' };
    for (var pi = 0; pi < presetBtns.length; pi++) {
      var btn = el('sat-btn-' + presetBtns[pi]);
      if (btn) btn.className = 'sat-btn sat-btn-preset' + (presetMap[_lastPresetIdx] === presetBtns[pi] ? ' active' : '');
    }

    // Highlight active mode button
    var contBtn = el('sat-btn-continuous');
    var pwmBtn = el('sat-btn-pwm');
    if (contBtn) contBtn.className = 'sat-btn sat-btn-mode' + (_lastModeIdx === 1 ? ' active' : '');
    if (pwmBtn) pwmBtn.className = 'sat-btn sat-btn-mode' + (_lastModeIdx === 2 ? ' active' : '');

    // Enable/disable toggle button state
    var enBtn = el('sat-btn-enable');
    if (enBtn) {
      enBtn.textContent = _lastSATEnabled ? 'Disable SAT' : 'Enable SAT';
      enBtn.className = 'sat-btn sat-btn-toggle' + (_lastSATEnabled ? ' active' : '');
    }

    var hsNames = ['Auto', 'Radiators', 'Heat Pump', 'Underfloor'];
    var hsIdx = d.heating_system !== undefined ? d.heating_system : 0;
    var hsDetected = d.heating_system_detected !== undefined ? d.heating_system_detected : 1;
    var hsLabel = hsNames[hsIdx] || 'Unknown';
    if (hsIdx === 0) hsLabel = 'Auto (' + (hsNames[hsDetected] || 'Radiators') + ')';
    setText('sat-heating-system', hsLabel);

    // Manufacturer
    var mfrName = d.manufacturer || '--';
    setText('sat-manufacturer', mfrName);

    // PID details
    setText('sat-pid-p', d.pid_p !== undefined ? d.pid_p.toFixed(2) : '--');
    setText('sat-pid-i', d.pid_i !== undefined ? d.pid_i.toFixed(2) : '--');
    setText('sat-pid-d', d.pid_d !== undefined ? d.pid_d.toFixed(2) : '--');
    setText('sat-kp', d.kp !== undefined ? d.kp.toFixed(4) : '--');
    setText('sat-ki', d.ki !== undefined ? d.ki.toFixed(6) : '--');
    setText('sat-kd', d.kd !== undefined ? d.kd.toFixed(2) : '--');
    setText('sat-raw-derivative', d.raw_derivative !== undefined ? d.raw_derivative.toFixed(4) : '--');

    // PWM info
    setText('sat-pwm-duty', d.pwm_duty !== undefined ? (d.pwm_duty * 100).toFixed(0) + '%' : '--');
    setText('sat-pwm-flame', d.pwm_flame_req ? 'Requesting' : 'Off');

    // Modulation
    setText('sat-modulation', d.current_modulation !== undefined ? d.current_modulation + '%' : '--');
    setText('sat-max-modulation', d.max_rel_modulation !== undefined ? d.max_rel_modulation + '%' : '--');

    // OPV
    var ovpVal = d.ovp_value !== undefined && d.ovp_value > 0 ? d.ovp_value.toFixed(1) + '\u00B0C' : 'Not calibrated';
    setText('sat-ovp-value', ovpVal);
    var calibPhases = ['Idle', 'Starting', 'Warming', 'Measuring', 'Cooldown', 'Done', 'Failed'];
    setText('sat-ovp-phase', d.ovp_calib_phase !== undefined ? (calibPhases[d.ovp_calib_phase] || '--') : '--');

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

    // Simple view status summary
    var simpleStatus = el('sat-simple-status');
    if (simpleStatus) {
      var statusText = 'Idle';
      var statusClass = 'sat-simple-status';
      if (!d.enabled) {
        statusText = 'Disabled';
      } else if (d.safety_tripped) {
        statusText = 'Safety tripped!';
        statusClass += ' error';
      } else if (d.window_open) {
        statusText = 'Window open';
      } else if (d.summer_active) {
        statusText = 'Summer mode';
      } else if (d.active) {
        statusText = 'Heating to ' + (d.target_temp !== undefined ? d.target_temp.toFixed(1) : '--') + '\u00B0C';
        statusClass += ' heating';
      }
      simpleStatus.textContent = statusText;
      simpleStatus.className = statusClass;
    }

    // Smart Features (expert/diag)
    setText('sat-solar-gain', d.solar_gain_active ? 'Active' : 'Inactive');

    if (d.summer_active) {
      var summerHrs = d.summer_hours_above !== undefined ? d.summer_hours_above.toFixed(0) + 'h above' : '';
      setText('sat-summer-mode', 'Active' + (summerHrs ? ' (' + summerHrs + ')' : ''));
    } else {
      setText('sat-summer-mode', 'Inactive');
    }

    if (d.thermal_coefficient !== undefined && d.thermal_coefficient > 0) {
      setText('sat-thermal-learning', d.thermal_coefficient.toFixed(3));
    } else {
      setText('sat-thermal-learning', 'Learning...');
    }

    if (d.comfort_offset !== undefined && d.comfort_offset !== 0) {
      setText('sat-comfort-status', d.comfort_offset.toFixed(2) + '\u00B0C');
    } else {
      setText('sat-comfort-status', 'Disabled');
    }

    setText('sat-simmer-index', d.simmer_index !== undefined ? d.simmer_index.toFixed(1) : '--');

    if (d.autotune_score !== undefined && d.autotune_score > 0) {
      setText('sat-autotune-status', d.autotune_score.toFixed(2));
    } else {
      setText('sat-autotune-status', 'Disabled');
    }

    // Update charts
    updateChart(d);
    updateCurveChart(d);
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

  // --- Heating Curve (Stooklijn) ---
  // JS port of satCalcHeatingCurve() from SATcontrol.ino
  function calcHeatingCurve(outsideTemp, targetTemp, coefficient, heatingSystem) {
    var baseOffset = (heatingSystem === 1) ? HC_BASE_OFFSET_FLOOR : HC_BASE_OFFSET_RAD;
    var diff = outsideTemp - HC_REF_TEMP;
    var curveValue = 4.0 * (targetTemp - HC_REF_TEMP)
                   + 0.03 * diff * diff
                   - 0.4 * diff;
    return baseOffset + (coefficient / 4.0) * curveValue;
  }

  function buildCurveOption(coeff, system, target, outsideTemp, currentSetpoint, theme) {
    // X axis: outside temperatures from -15 to 25
    var xData = [];
    for (var t = -15; t <= 25; t++) xData.push(t);

    var series = [];
    // Reference curves: coefficient 0.5 to 5.0, step 0.5
    var refCoeffs = [0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0];
    var isLight = (theme !== 'dark');
    for (var ci = 0; ci < refCoeffs.length; ci++) {
      var c = refCoeffs[ci];
      var isActive = (Math.abs(c - coeff) < 0.05);
      var data = [];
      for (var xi = 0; xi < xData.length; xi++) {
        data.push(Math.round(calcHeatingCurve(xData[xi], target, c, system) * 10) / 10);
      }
      series.push({
        name: 'c=' + c.toFixed(1),
        type: 'line',
        smooth: true,
        data: data,
        symbol: 'none',
        lineStyle: {
          width: isActive ? 3 : 1,
          color: isActive ? '#ff6600' : (isLight ? '#bbb' : '#555')
        },
        emphasis: { disabled: true },
        silent: true,
        z: isActive ? 5 : 1
      });
    }

    // If coefficient doesn't match any reference, draw it explicitly
    var matchesRef = false;
    for (var ri = 0; ri < refCoeffs.length; ri++) {
      if (Math.abs(refCoeffs[ri] - coeff) < 0.05) { matchesRef = true; break; }
    }
    if (!matchesRef && coeff > 0) {
      var activeData = [];
      for (var ai = 0; ai < xData.length; ai++) {
        activeData.push(Math.round(calcHeatingCurve(xData[ai], target, coeff, system) * 10) / 10);
      }
      series.push({
        name: 'c=' + coeff.toFixed(1),
        type: 'line',
        smooth: true,
        data: activeData,
        symbol: 'none',
        lineStyle: { width: 3, color: '#ff6600' },
        emphasis: { disabled: true },
        silent: true,
        z: 5
      });
    }

    // Current operating point
    if (outsideTemp !== null && outsideTemp !== undefined &&
        currentSetpoint !== null && currentSetpoint !== undefined) {
      series.push({
        name: 'Current',
        type: 'scatter',
        data: [[outsideTemp, Math.round(currentSetpoint * 10) / 10]],
        symbolSize: 14,
        itemStyle: { color: '#ff3300', borderColor: '#fff', borderWidth: 2 },
        z: 10
      });
    }

    return {
      tooltip: {
        trigger: 'axis',
        formatter: function(params) {
          if (!params || !params.length) return '';
          var tip = 'Outside: ' + params[0].axisValue + '\u00B0C<br/>';
          for (var pi = 0; pi < params.length; pi++) {
            if (params[pi].seriesType === 'scatter') continue;
            var val = params[pi].data;
            if (val !== null && val !== undefined) {
              tip += '<span style="color:' + params[pi].color + '">\u25CF</span> '
                   + params[pi].seriesName + ': ' + val + '\u00B0C<br/>';
            }
          }
          return tip;
        }
      },
      grid: { left: 55, right: 25, top: 30, bottom: 40 },
      xAxis: {
        type: 'value',
        name: 'Outside \u00B0C',
        nameLocation: 'center',
        nameGap: 25,
        min: -15,
        max: 25,
        interval: 5,
        axisLabel: { formatter: '{value}\u00B0' }
      },
      yAxis: {
        type: 'value',
        name: 'Flow \u00B0C',
        min: 10,
        max: 80,
        axisLabel: { formatter: '{value}\u00B0' }
      },
      series: series
    };
  }

  function initCurveChart() {
    var container = el('sat-curve-chart');
    if (!container || typeof echarts === 'undefined') return;
    _curveChartInstance = echarts.init(container);
    // Initial empty state — filled on first data fetch
    _curveChartInstance.setOption(buildCurveOption(1.5, 0, 20.0, null, null, 'light'));
  }

  function updateCurveChart(d) {
    if (!_curveChartInstance) return;
    var coeff = d.coefficient;
    var system = d.heating_system;
    var target = d.target_temp;
    var outside = d.outside_temp;
    var setpoint = d.heating_curve;

    // Only rebuild full curve family when parameters change
    var needsRebuild = (coeff !== _lastCurveCoeff ||
                        system !== _lastCurveSystem ||
                        Math.abs(target - _lastCurveTarget) > 0.05);

    if (needsRebuild) {
      _lastCurveCoeff = coeff;
      _lastCurveSystem = system;
      _lastCurveTarget = target;
      var theme = document.body.classList.contains('dark') ? 'dark' : 'light';
      _curveChartInstance.setOption(buildCurveOption(coeff, system, target, outside, setpoint, theme), true);
    } else {
      // Just update operating point (last series)
      var seriesCount = _curveChartInstance.getOption().series.length;
      var lastSeries = _curveChartInstance.getOption().series[seriesCount - 1];
      if (lastSeries && lastSeries.type === 'scatter') {
        var update = [];
        for (var si = 0; si < seriesCount - 1; si++) update.push({});
        update.push({
          data: (outside !== null && outside !== undefined && setpoint !== null && setpoint !== undefined)
            ? [[outside, Math.round(setpoint * 10) / 10]]
            : []
        });
        _curveChartInstance.setOption({ series: update });
      }
    }

    // Update info label
    var label = el('sat-curve-label');
    if (label) {
      label.textContent = (system === 1 ? 'Underfloor' : 'Radiator')
                        + ' | Coefficient: ' + (coeff !== undefined ? coeff.toFixed(1) : '--')
                        + ' | Target: ' + (target !== undefined ? target.toFixed(1) + '\u00B0C' : '--');
    }
  }

  function toggleCurve() {
    _curveVisible = !_curveVisible;
    var section = el('sat-curve-section');
    var arrow = el('sat-curve-arrow');
    if (section) section.style.display = _curveVisible ? '' : 'none';
    if (arrow) arrow.innerHTML = _curveVisible ? '&#9660;' : '&#9654;';
    if (_curveVisible && _curveChartInstance) _curveChartInstance.resize();
  }

  function resizeChart() {
    if (_chartInstance) _chartInstance.resize();
    if (_curveChartInstance) _curveChartInstance.resize();
  }

  // --- View Selector ---
  function switchView(view) {
    var valid = ['simple', 'expert', 'diag'];
    if (valid.indexOf(view) === -1) view = 'simple';
    try { localStorage.setItem('sat-dashboard-view', view); } catch (e) {}
    var dash = document.querySelector('.sat-dashboard');
    if (dash) {
      dash.classList.remove('sat-view-simple', 'sat-view-expert', 'sat-view-diag');
      dash.classList.add('sat-view-' + view);
    }
    var sel = el('sat-view-selector');
    if (sel) sel.value = view;
    // Resize charts since visibility may have changed
    setTimeout(resizeChart, 50);
  }

  function initView() {
    var view = 'simple';
    try { view = localStorage.getItem('sat-dashboard-view') || 'simple'; } catch (e) {}
    switchView(view);
  }

  // --- Public API ---
  function start() {
    initView();
    initChart();
    initCurveChart();
    fetchStatus(); // immediate first fetch
    fetchWeather(); // immediate weather fetch
    _pollTimer = setInterval(fetchStatus, POLL_INTERVAL_MS);
    _weatherTimer = setInterval(fetchWeather, 30000); // weather every 30s
    window.addEventListener('resize', resizeChart);
  }

  function stop() {
    if (_pollTimer) { clearInterval(_pollTimer); _pollTimer = null; }
    if (_weatherTimer) { clearInterval(_weatherTimer); _weatherTimer = null; }
    window.removeEventListener('resize', resizeChart);
  }

  function setTheme(theme) {
    var themeArg = (theme === 'dark') ? 'dark' : null;
    if (_chartInstance) {
      var container = el('sat-chart');
      _chartInstance.dispose();
      _chartInstance = echarts.init(container, themeArg);
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
    if (_curveChartInstance) {
      var curveContainer = el('sat-curve-chart');
      _curveChartInstance.dispose();
      _curveChartInstance = echarts.init(curveContainer, themeArg);
      // Force rebuild on next data update
      _lastCurveCoeff = -1;
    }
  }

  // --- Interactive Controls ---
  var _lastSATEnabled = false;
  var _lastSimEnabled = false;
  var _lastPresetIdx = 0;
  var _lastModeIdx = 0;

  function satPost(endpoint, value) {
    return fetch('/api/v2/sat/' + endpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'text/plain' },
      body: String(value)
    }).then(function(r) {
      if (!r.ok) throw new Error('HTTP ' + r.status);
      showFeedback('OK', false);
      return r;
    }).catch(function(e) {
      showFeedback('Error: ' + e.message, true);
    });
  }

  function showFeedback(msg, isError) {
    var fb = el('sat-feedback');
    if (!fb) return;
    fb.textContent = msg;
    fb.className = 'sat-feedback ' + (isError ? 'sat-feedback-error' : 'sat-feedback-ok');
    setTimeout(function() { fb.textContent = ''; fb.className = 'sat-feedback'; }, 3000);
  }

  function adjustTarget(delta) {
    var curEl = el('sat-target-temp');
    if (!curEl) return;
    var cur = parseFloat(curEl.textContent);
    if (isNaN(cur)) return;
    var next = Math.round((cur + delta) * 10) / 10;
    if (next < 5 || next > 30) return;
    curEl.textContent = next.toFixed(1) + '\u00B0C';
    satPost('target', next.toFixed(1));
  }

  function setPreset(name) {
    satPost('preset', name);
  }

  function setMode(mode) {
    satPost('mode', mode);
  }

  function toggleEnable() {
    satPost('enable', _lastSATEnabled ? '0' : '1');
  }

  function toggleSimulation() {
    var newVal = _lastSimEnabled ? '0' : '1';
    fetch(APIGW + 'v1/settings', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: '{"name":"satsimulation","value":"' + newVal + '"}'
    }).then(function(r) {
      if (!r.ok) throw new Error('HTTP ' + r.status);
      showFeedback('Simulation ' + (newVal === '1' ? 'enabled' : 'disabled'), false);
    }).catch(function(e) {
      showFeedback('Error: ' + e.message, true);
    });
  }

  // --- Weather data ---
  function fetchWeather() {
    fetch(APIGW + 'v2/sat/weather')
      .then(function(r) {
        if (!r.ok) return Promise.reject(r.statusText);
        var ct = r.headers.get('content-type') || '';
        if (ct.indexOf('application/json') === -1) return Promise.reject('Not JSON');
        return r.json();
      })
      .then(function(w) { updateWeather(w); })
      .catch(function(err) {
        console.warn('Weather fetch error:', err);
      });
  }

  function updateWeather(w) {
    var section = el('sat-weather-section');
    if (!section) return;

    if (w.enabled) {
      section.style.display = '';
      setText('sat-weather-temp', w.valid ? parseFloat(w.temperature).toFixed(1) + '\u00B0C' : '--');
      setText('sat-weather-humidity', w.valid ? parseInt(w.humidity) + '%' : '--');
      setText('sat-weather-wind', w.valid ? parseFloat(w.wind_speed).toFixed(1) + ' km/h' : '--');
      setText('sat-weather-errors', String(w.fetch_errors || 0));

      if (w.valid && w.age_seconds >= 0) {
        var mins = Math.floor(w.age_seconds / 60);
        setText('sat-weather-updated', mins < 1 ? 'Just now' : mins + ' min ago');
      } else {
        setText('sat-weather-updated', 'Never');
      }

      // Show coordinates
      var coordEl = el('sat-weather-coords');
      if (coordEl) {
        var lat = parseFloat(w.latitude);
        var lon = parseFloat(w.longitude);
        if (lat !== 0 || lon !== 0) {
          coordEl.textContent = lat.toFixed(4) + ', ' + lon.toFixed(4);
        } else {
          coordEl.textContent = 'No location set';
        }
      }
    } else {
      section.style.display = 'none';
    }
  }

  function detectLocation() {
    if (!navigator.geolocation) {
      showFeedback('Geolocation not supported by browser', true);
      return;
    }
    showFeedback('Detecting location...', false);
    navigator.geolocation.getCurrentPosition(function(pos) {
      var lat = pos.coords.latitude.toFixed(4);
      var lon = pos.coords.longitude.toFixed(4);
      // Send coordinates and enable weather
      var settings = [
        { name: 'SATweatherlat', value: lat },
        { name: 'SATweatherlon', value: lon },
        { name: 'SATweatherenable', value: '1' }
      ];
      var promises = [];
      for (var i = 0; i < settings.length; i++) {
        promises.push(
          fetch(APIGW + 'v1/settings', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: '{"name":"' + settings[i].name + '","value":"' + settings[i].value + '"}'
          })
        );
      }
      Promise.all(promises).then(function() {
        showFeedback('Location set: ' + lat + ', ' + lon, false);
        // Refresh weather data after a short delay
        setTimeout(fetchWeather, 2000);
      }).catch(function(e) {
        showFeedback('Error saving location: ' + e.message, true);
      });
    }, function(err) {
      showFeedback('Location error: ' + err.message, true);
    }, { timeout: 10000 });
  }

  return {
    start: start,
    stop: stop,
    setTheme: setTheme,
    switchView: switchView,
    toggleCurve: toggleCurve,
    adjustTarget: adjustTarget,
    setPreset: setPreset,
    setMode: setMode,
    toggleEnable: toggleEnable,
    toggleSimulation: toggleSimulation,
    detectLocation: detectLocation
  };
})();
