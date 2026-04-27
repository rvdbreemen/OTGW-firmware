/*
 * echarts-theme.js  —  ECharts thema dat de design-tokens leest
 *
 * Gebruik:
 *   const chart = echarts.init(document.querySelector('#sat-chart'));
 *   chart.setOption(Object.assign({}, otgwChartTheme(), {
 *     series: [...],
 *     xAxis: {...},
 *     yAxis: {...},
 *   }));
 *
 *   // Op theme-toggle:
 *   chart.setOption(otgwChartTheme(), true);
 *
 * Leest CSS-vars op moment van aanroep — geen tweede color-bron, geen
 * harde hex. Pak je een token aan in ds-tokens.css, dan volgt de chart.
 */
(function (root) {
  'use strict';

  function v(name) {
    return getComputedStyle(document.body).getPropertyValue(name).trim();
  }

  function otgwChartTheme() {
    return {
      backgroundColor: 'transparent',
      textStyle: {
        color: v('--fg-2'),
        fontFamily: v('--font-sans').replace(/['"]/g, ''),
      },
      color: [
        v('--status-info'),
        v('--status-ok'),
        v('--status-warn'),
        v('--status-error'),
        v('--accent-primary'),
      ],
      grid: { left: 40, right: 16, top: 16, bottom: 28 },
      xAxis: {
        axisLine:  { lineStyle: { color: v('--chart-axis-line') } },
        splitLine: { lineStyle: { color: v('--chart-grid-line') } },
        axisLabel: { color: v('--chart-axis-label') },
      },
      yAxis: {
        axisLine:  { lineStyle: { color: v('--chart-axis-line') } },
        splitLine: { lineStyle: { color: v('--chart-grid-line') } },
        axisLabel: { color: v('--chart-axis-label') },
      },
      tooltip: {
        backgroundColor: v('--bg-surface'),
        borderColor: v('--border-1'),
        textStyle: { color: v('--fg-2') },
      },
    };
  }

  // TASK-462: register otgw-light / otgw-dark theme NAMES so existing
  // echarts.init(container, 'otgw-dark') call sites in sat.js / graph.js
  // actually resolve the token-driven theme. otgwChartTheme() reads CSS
  // vars at call time, so re-register on theme:changed picks up the
  // new dark/light values before charts re-init.
  function registerOtgwThemes() {
    if (typeof echarts === 'undefined' || typeof echarts.registerTheme !== 'function') return;
    var theme = otgwChartTheme();
    echarts.registerTheme('otgw-light', theme);
    echarts.registerTheme('otgw-dark', theme);
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', registerOtgwThemes);
  } else {
    registerOtgwThemes();
  }

  root.otgwChartTheme = otgwChartTheme;
  root.registerOtgwThemes = registerOtgwThemes;
})(window);
