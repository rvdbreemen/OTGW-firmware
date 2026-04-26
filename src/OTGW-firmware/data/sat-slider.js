/*
 * sat-slider.js  —  live-fill hook for the DHW setpoint slider
 *
 * Drop-in: include AFTER sat.js. Idempotent — safe to re-run.
 * Pairs with `.sat-dhw-slider` rules in sat.css (Patch 02), which read
 * `--sat-slider-pct` to drive a linear-gradient stop on the track.
 *
 * The slider's `value` is degrees Celsius, not 0-100, so we normalize
 * against `min`/`max` before setting the variable.
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
    el.style.setProperty('--sat-slider-pct', pct(el).toFixed(2) + '%');

    // Mirror to the readout span if one exists. Convention:
    //   <input id="dhw-setpoint-slider" …>
    //   <span  id="dhw-setpoint-readout">…</span>
    var readoutId = el.id && el.id.replace(/-slider$/, '-readout');
    if (readoutId) {
      var readout = document.getElementById(readoutId);
      if (readout) readout.textContent = parseFloat(el.value).toFixed(1) + ' °C';
    }
  }

  function bind() {
    var sliders = document.querySelectorAll('.sat-dhw-slider');
    for (var i = 0; i < sliders.length; i++) {
      var el = sliders[i];
      if (el.dataset.satSliderBound === '1') continue;   // idempotent
      el.dataset.satSliderBound = '1';
      el.addEventListener('input', function (ev) { sync(ev.currentTarget); });
      sync(el);                                          // initial paint
    }
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', bind);
  } else {
    bind();
  }

  // Re-bind after sat.js re-renders the panel (e.g. after a websocket
  // message rewrites the DHW container's innerHTML). sat.js currently
  // dispatches a 'sat:rendered' event from its render function — if it
  // doesn't, add `document.dispatchEvent(new Event('sat:rendered'));`
  // at the end of the SAT render path. The hook is a no-op if the
  // event is never fired.
  document.addEventListener('sat:rendered', bind);
})();
