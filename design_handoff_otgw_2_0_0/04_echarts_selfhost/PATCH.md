# Patch 04 — Self-host ECharts (drop the CDN)

## Why

`data/index.html` line 21 currently loads ECharts from a CDN:

```html
<script src="https://cdn.jsdelivr.net/npm/echarts@5.x/dist/echarts.min.js"></script>
```

Three problems:

1. **Privacy** — every device boot phones home to a third party.
2. **Offline** — the device often runs on isolated networks. The graph
   pages silently break when the CDN is unreachable.
3. **Reproducibility** — `5.x` floats. A jsDelivr-side update can change
   behavior without a firmware change.

## What changes

Self-host a **custom ECharts build** that contains only the chart types
the firmware actually uses. The full bundle is ~1 MB; we ship ~180 KB.

### Step 1 — generate the custom build

`echarts.min.js` in this folder is a placeholder — produce the real one
on the build host:

```sh
git clone --depth 1 -b 5.4.3 https://github.com/apache/echarts.git
cd echarts
npm ci
node build/build.js \
  --min \
  --include line,bar \
  --exclude lines,scatter,pie,radar,map,gauge,funnel,heatmap,boxplot,candlestick,parallel,tree,treemap,sunburst,sankey,graph,themeRiver,custom,pictorialBar
cp dist/echarts.min.js <repo>/src/OTGW-firmware/data/echarts.min.js
```

Pin the version in `data/echarts.VERSION`:

```
5.4.3
```

so the next person knows what to rebuild against.

### Step 2 — swap the script tag

In `data/index.html`:

```diff
-  <script src="https://cdn.jsdelivr.net/npm/echarts@5.x/dist/echarts.min.js"></script>
+  <script src="echarts.min.js"></script>
```

### Step 3 — chart theme tokens

Both `sat.js` and `graph.js` instantiate ECharts with a hard-coded
theme. Replace the inline theme objects with a shared helper:

```js
// data/echarts-theme.js  (new file)
function otgwChartTheme() {
  const css = getComputedStyle(document.body);
  const isDark = document.body.classList.contains('dark');
  return {
    backgroundColor: 'transparent',
    textStyle: {
      color: isDark ? '#e0e0e0' : '#1a1a1a',
      fontFamily: 'Inter, Arial, Helvetica, sans-serif',
    },
    color: [
      css.getPropertyValue('--status-info').trim(),
      css.getPropertyValue('--status-ok').trim(),
      css.getPropertyValue('--status-warn').trim(),
      css.getPropertyValue('--status-error').trim(),
    ],
    grid: { left: 40, right: 16, top: 16, bottom: 28 },
    xAxis: {
      axisLine:  { lineStyle: { color: isDark ? '#3a3a3c' : '#d0d0d0' } },
      splitLine: { lineStyle: { color: isDark ? '#3a3a3c' : '#ececec' } },
      axisLabel: { color: isDark ? '#909090' : '#707070' },
    },
    yAxis: {
      axisLine:  { lineStyle: { color: isDark ? '#3a3a3c' : '#d0d0d0' } },
      splitLine: { lineStyle: { color: isDark ? '#3a3a3c' : '#ececec' } },
      axisLabel: { color: isDark ? '#909090' : '#707070' },
    },
  };
}
```

Link it in `index.html` BEFORE `sat.js` and `graph.js`. In each chart-
init site, merge `otgwChartTheme()` into the option object. On theme
toggle, call `chart.setOption(otgwChartTheme(), true)` to refresh.

This fixes the "chart gridlines barely visible in dark mode" issue from
Patch 02.

## File budget

LittleFS partition is 1.5 MB on the firmware. Current `data/` is around
~700 KB. Adding ~180 KB ECharts brings us to ~880 KB — comfortable.

If a future patch needs more chart types, re-run Step 1 with an
extended `--include` list and update `echarts.VERSION`.

## Verification

```sh
curl -I http://<device>/echarts.min.js
# 200 OK, Content-Length around 180000
```

Pull the device offline (disconnect WAN). Reload `/sat`. The
Temperature History chart should still render. Pre-patch it would have
shown an empty container.

## Rollback

Restore the original `<script src="…cdn…">` line in `index.html`.
Delete `data/echarts.min.js` and `data/echarts.VERSION` and
`data/echarts-theme.js`.
