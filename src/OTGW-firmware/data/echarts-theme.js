/*
 * echarts-theme.js  —  shared chart theme helper for OTGW-firmware
 *
 * Pulls colour values from the live --status-* tokens (ds-tokens.css,
 * Patch 01) so chart series follow the same palette the rest of the
 * web UI uses. Two themes are auto-registered on script load:
 *
 *   otgw-light  — for body without .dark
 *   otgw-dark   — for body.dark
 *
 * sat.js and graph.js reference them in their echarts.init() calls.
 *
 * Loaded AFTER echarts.min.js so the `echarts` global is in scope when
 * registerTheme runs. If echarts is not loaded (e.g. dev page that
 * skips the chart bundle), the registration is a no-op and the helper
 * stays callable for ad-hoc setOption merges.
 */
function otgwChartTheme(isDark) {
  var css = getComputedStyle(document.body);
  var token = function (name, fallback) {
    var v = (css.getPropertyValue(name) || '').trim();
    return v || fallback;
  };
  return {
    backgroundColor: 'transparent',
    textStyle: {
      color: isDark ? '#e0e0e0' : '#1a1a1a',
      fontFamily: 'Inter, Arial, Helvetica, sans-serif'
    },
    color: [
      token('--status-info',    '#1976d2'),
      token('--status-ok',      '#2e7d32'),
      token('--status-warn',    '#ef6c00'),
      token('--status-error',   '#c62828'),
      token('--status-neutral', '#607d8b')
    ],
    grid: { left: 40, right: 16, top: 16, bottom: 28 },
    categoryAxis: {
      axisLine:  { lineStyle: { color: isDark ? '#3a3a3c' : '#d0d0d0' } },
      splitLine: { lineStyle: { color: isDark ? '#3a3a3c' : '#ececec' } },
      axisLabel: { color: isDark ? '#909090' : '#707070' }
    },
    valueAxis: {
      axisLine:  { lineStyle: { color: isDark ? '#3a3a3c' : '#d0d0d0' } },
      splitLine: { lineStyle: { color: isDark ? '#3a3a3c' : '#ececec' } },
      axisLabel: { color: isDark ? '#909090' : '#707070' }
    },
    timeAxis: {
      axisLine:  { lineStyle: { color: isDark ? '#3a3a3c' : '#d0d0d0' } },
      splitLine: { lineStyle: { color: isDark ? '#3a3a3c' : '#ececec' } },
      axisLabel: { color: isDark ? '#909090' : '#707070' }
    },
    logAxis: {
      axisLine:  { lineStyle: { color: isDark ? '#3a3a3c' : '#d0d0d0' } },
      splitLine: { lineStyle: { color: isDark ? '#3a3a3c' : '#ececec' } },
      axisLabel: { color: isDark ? '#909090' : '#707070' }
    }
  };
}

(function registerOtgwThemes() {
  if (typeof echarts === 'undefined' || typeof echarts.registerTheme !== 'function') {
    return;
  }
  // Build both palettes up front so a runtime dark-mode toggle picks the
  // right pre-registered name without recomputing tokens on every poll.
  echarts.registerTheme('otgw-light', otgwChartTheme(false));
  echarts.registerTheme('otgw-dark',  otgwChartTheme(true));
})();
