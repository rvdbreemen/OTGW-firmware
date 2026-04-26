/*
 * sat-slider.js  —  live-fill hook voor alle .ds-slider inputs
 *
 * Idempotent. Drop-in: include na de SAT-init JS.
 *
 * Pairs met de .ds-slider regels in components.css, die `--slider-pct`
 * lezen om een linear-gradient stop op de track te bepalen.
 */
(function () {
  'use strict';

  function pct(el) {
    var min = parseFloat(el.min) || 0;
    var max = parseFloat(el.max) || 100;
    var val = parseFloat(el.value);
    if (max <= min) return 0;
    return ((val - min) / (max - min)) * 100;
  }

  function sync(el) {
    el.style.setProperty('--slider-pct', pct(el).toFixed(2) + '%');

    // Mirror naar readout span: <input id="x-slider"> + <span id="x-readout">
    var readoutId = el.id && el.id.replace(/-slider$/, '-readout');
    if (readoutId) {
      var readout = document.getElementById(readoutId);
      if (readout) {
        var unit = readout.dataset.unit || '°C';
        var dec = parseInt(readout.dataset.decimals || '1', 10);
        readout.textContent = parseFloat(el.value).toFixed(dec) + ' ' + unit;
      }
    }
  }

  function bind() {
    var sliders = document.querySelectorAll('input[type="range"].ds-slider');
    for (var i = 0; i < sliders.length; i++) {
      var el = sliders[i];
      if (el.dataset.dsBound === '1') continue;
      el.dataset.dsBound = '1';
      el.addEventListener('input', function (ev) { sync(ev.currentTarget); });
      sync(el);
    }
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', bind);
  } else {
    bind();
  }

  // Re-bind na re-render — sat.js dispatched 'sat:rendered' op zijn render-pad.
  document.addEventListener('sat:rendered', bind);
})();
