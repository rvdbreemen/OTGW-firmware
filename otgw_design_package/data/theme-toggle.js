/*
 * theme-toggle.js  —  ☀ / ☾ toggle met localStorage persistentie
 *
 * Zet/leest <body class="dark"> én <html data-theme="…">. Beide zodat
 * components.css én legacy index_dark.css regels werken.
 *
 * HTML hook:
 *   <button class="theme-toggle-btn" id="theme-toggle"
 *           aria-label="Toggle theme" aria-pressed="false">☾</button>
 */
(function () {
  'use strict';

  var KEY = 'otgw-theme';

  function apply(mode) {
    var dark = mode === 'dark';
    document.body.classList.toggle('dark', dark);
    document.documentElement.setAttribute('data-theme', dark ? 'dark' : 'light');
    var btn = document.getElementById('theme-toggle');
    if (btn) {
      btn.textContent = dark ? '☀' : '☾';
      btn.setAttribute('aria-pressed', String(dark));
    }
    document.dispatchEvent(new CustomEvent('theme:changed', { detail: { mode: mode } }));
  }

  function init() {
    var saved = null;
    try { saved = localStorage.getItem(KEY); } catch (_) {}
    var prefers = window.matchMedia &&
                  window.matchMedia('(prefers-color-scheme: dark)').matches;
    apply(saved || (prefers ? 'dark' : 'light'));

    var btn = document.getElementById('theme-toggle');
    if (btn) {
      btn.addEventListener('click', function () {
        var next = document.body.classList.contains('dark') ? 'light' : 'dark';
        try { localStorage.setItem(KEY, next); } catch (_) {}
        apply(next);
      });
    }
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
