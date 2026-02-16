# ADR-039: Real-Time OTGraph Charting Architecture

**Status:** Accepted  
**Date:** 2026-02-16  
**Updated:** 2026-02-16 (Initial documentation of existing pattern)

## Context

The OTGW-firmware Web UI provides a real-time graph that visualizes OpenTherm system data as it flows through the gateway. Users rely on this graph to:
- Monitor boiler behavior (flame cycles, modulation, temperatures)
- Diagnose heating issues (short-cycling, overshoot, sensor failures)
- Verify setpoint changes take effect
- Track Dallas external temperature sensors

**Requirements:**
- Real-time updates from WebSocket data stream (2-second refresh)
- Multiple data series across different value ranges (0/1 digital, 0-100% analog, -40°C to 100°C temperatures)
- Dynamic addition of Dallas temperature sensors as they appear in the data
- Light/dark theme support matching the Web UI theme
- Export capability (screenshot PNG, CSV data)
- Time window selection (1h, 3h, 6h, 12h, 24h)
- Minimal memory footprint in the browser (long-running sessions)

**Constraints:**
- Must work in Chrome, Firefox, and Safari (browser compatibility per ADR-025)
- WebSocket delivers all OT messages as text lines — the graph must parse and filter relevant data
- Dallas sensor count is dynamic (0-16 sensors, discovered at runtime)
- ESP8266 serves the JavaScript file — must be reasonably sized for LittleFS storage
- No server-side data aggregation — all charting logic runs in the browser

## Decision

**Implement a 5-grid ECharts-based charting module (`OTGraph`) with dynamic Dallas sensor registration, dual-theme palettes, and data sampling for long sessions.**

### Grid Layout (5 Grids)

The graph is divided into 5 vertically stacked grids, each optimized for its data type:

| Grid | Index | Data Type | Y-Axis Range | Series |
|------|-------|-----------|-------------|---------|
| Flame | 0 | Digital (0/1) | 0-1 | Flame on/off |
| DHW Mode | 1 | Digital (0/1) | 0-1 | Domestic hot water active |
| CH Mode | 2 | Digital (0/1) | 0-1 | Central heating active |
| Modulation | 3 | Analog (%) | 0-100 | Boiler modulation level |
| Temperatures | 4 | Analog (°C) | Auto-scale | 6 OT temps + dynamic Dallas sensors |

### Base Series (10 Fixed)

```javascript
seriesConfig: [
  { id: 'flame',   label: 'Flame',             gridIndex: 0, step: 'start' },
  { id: 'dhwMode', label: 'DHW Mode',          gridIndex: 1, step: 'start' },
  { id: 'chMode',  label: 'CH Mode',           gridIndex: 2, step: 'start' },
  { id: 'mod',     label: 'Modulation (%)',     gridIndex: 3 },
  { id: 'ctrlSp',  label: 'Control SetPoint',  gridIndex: 4 },
  { id: 'boiler',  label: 'Boiler Temp',       gridIndex: 4 },
  { id: 'return',  label: 'Return Temp',       gridIndex: 4 },
  { id: 'roomSp',  label: 'Room SetPoint',     gridIndex: 4 },
  { id: 'room',    label: 'Room Temp',         gridIndex: 4 },
  { id: 'outside', label: 'Outside Temp',      gridIndex: 4 }
]
```

### Dynamic Dallas Sensor Registration

Dallas sensors are not known at chart initialization. They are discovered dynamically from the WebSocket data stream:

```javascript
// Detection: API entries with type === 'dallas' are temperature sensors
function isDallasAddress(entry) {
  return entry != null && entry.type === 'dallas';
}
```

When a new Dallas sensor appears:
1. A new series is added to grid 4 (Temperatures)
2. Color is assigned from a 16-color palette (index = sensor discovery order)
3. The ECharts option is dynamically updated with `setOption()`
4. Custom labels from `/api/v1/sensors/labels` are used if available

### Dual Theme Palettes

Two complete color sets for light and dark themes:

```javascript
palettes: {
  light: { flame: 'red', dhwMode: 'blue', chMode: 'green', mod: 'black', ... },
  dark:  { flame: '#ff4d4f', dhwMode: '#40a9ff', chMode: '#73d13d', mod: '#ffffff', ... }
},
sensorColorPalettes: {
  light: ['#FF6B6B', '#4ECDC4', '#45B7D1', ...],  // 16 colors
  dark:  ['#FF8787', '#5FE3D9', '#5BC8E8', ...]   // 16 colors
}
```

Theme switching calls `OTGraph.setTheme()` which rebuilds the chart colors without losing data.

### Performance Optimizations

- **Batch updates:** Data points accumulate in `pendingData` and are flushed to the chart every 2 seconds (reduces ECharts redraws)
- **LTTB sampling:** `sampling: 'lttb'` (Largest-Triangle-Three-Buckets) algorithm downsamples dense data for rendering while preserving visual accuracy
- **Large dataset mode:** `large: true` enables ECharts' WebGL-accelerated rendering for >10K points
- **24-hour buffer:** `maxPoints: 864000` allows 24h of data at 10 messages/second
- **Time window clipping:** Only data within the selected time window is rendered

## Alternatives Considered

### Alternative 1: Server-Side Charting (Pre-Rendered Images)
**Pros:**
- No JavaScript library needed
- Works on any browser
- Consistent rendering

**Cons:**
- ESP8266 cannot render images (insufficient CPU/memory)
- No interactivity (zoom, hover, pan)
- Huge bandwidth per update
- Impractical for real-time data

**Why not chosen:** ESP8266 hardware cannot render charts server-side. Client-side JavaScript is the only viable option.

### Alternative 2: Chart.js (Canvas-Based)
**Pros:**
- Lightweight (~70KB)
- Simple API
- Good documentation
- No dependencies

**Cons:**
- Poor performance with >10K data points
- No built-in data sampling/downsampling
- Limited multi-grid support (requires multiple canvas elements)
- No WebGL acceleration

**Why not chosen:** Performance degrades significantly in long-duration sessions (24h of data). ECharts handles large datasets better with LTTB sampling and WebGL.

### Alternative 3: D3.js (SVG-Based)
**Pros:**
- Maximum flexibility
- Professional visualizations
- Industry standard

**Cons:**
- Very large library (~250KB minified)
- SVG performance degrades with many elements
- Steep learning curve
- Requires custom implementation for everything

**Why not chosen:** Too large for LittleFS storage. SVG rendering cannot handle the data volume of 24h sessions.

### Alternative 4: Plotly.js
**Pros:**
- Interactive charts out of the box
- Good time-series support
- Built-in export

**Cons:**
- Very large (~3MB minified)
- Heavy memory footprint in browser
- Overkill for embedded system UI

**Why not chosen:** Library size is prohibitive for LittleFS storage (2MB partition total).

## Consequences

### Positive
- **Real-time visualization:** 2-second update cycle provides near-live monitoring
- **Scalable:** Handles 24h of data without browser slowdown (LTTB + WebGL)
- **Self-maintaining:** Dallas sensors auto-register without configuration
- **Theme-aware:** Matches Web UI light/dark preference seamlessly
- **Exportable:** Screenshot and CSV export for sharing/debugging
- **No server load:** All processing in the browser — ESP8266 only sends raw data

### Negative
- **ECharts library size:** ~300KB adds to filesystem image
  - Accepted: Fits within 2MB LittleFS partition alongside other UI files
- **Browser memory:** Long sessions accumulate data in memory
  - Mitigation: 24h cap with LTTB sampling prevents unbounded growth
  - Mitigation: `maxPoints` limit enforced per series
- **No persistence:** Graph data is lost on page refresh
  - Accepted: Historical data not needed — MQTT/InfluxDB provides long-term storage
- **Dynamic series complexity:** Adding sensors at runtime requires ECharts option rebuild
  - Mitigation: Handled transparently in `addSensorSeries()` method

### Risks & Mitigation
- **Browser tab crash:** Very long sessions with many sensors
  - **Mitigation:** `maxPoints` cap prevents unbounded memory growth
  - **Mitigation:** LTTB sampling reduces visual data points
- **WebSocket disconnect:** Graph stops updating
  - **Mitigation:** Disconnect markers drawn on chart for visual indication
  - **Mitigation:** Auto-reconnect in WebSocket client restores data flow
- **Theme mismatch:** Chart colors wrong after theme switch
  - **Mitigation:** `setTheme()` rebuilds entire color configuration

## Related Decisions
- ADR-005: WebSocket for Real-Time Streaming (data transport for graph)
- ADR-020: Dallas DS18B20 Temperature Sensor Integration (dynamic sensor discovery)
- ADR-026: Conditional JavaScript Cache-Busting (graph.js cache management)
- ADR-033: Dallas Sensor Custom Labels and Graph Visualization (sensor labels integration)
- ADR-034: Non-Blocking Modal Dialogs (graph export dialogs)
- ADR-038: OpenTherm Message Data Flow Pipeline (data source for graph)

## References
- Implementation: `data/graph.js` (OTGraph module, ~1091 lines)
- ECharts library: https://echarts.apache.org/
- LTTB algorithm: Sveinn Steinarsson, "Downsampling Time Series for Visual Representation"
- WebSocket data source: `webSocketStuff.ino` sendLogToWebSocket()
- Dallas sensor labels: `/api/v1/sensors/labels` endpoint
- Theme switching: `data/index.js` applyTheme()
