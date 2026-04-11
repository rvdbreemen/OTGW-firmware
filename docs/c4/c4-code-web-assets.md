# C4 Code Level: Web Assets Module

## Overview

- **Name**: OTGW Web Assets & UI Layer
- **Description**: Complete web user interface for the OTGW-firmware ESP8266 gateway. Serves HTML, CSS, JavaScript, and PIC firmware binaries from LittleFS flash filesystem. Multi-page SPA with real-time WebSocket monitoring, REST API integration, settings management, and SAT (Smart Autotune Thermostat) control dashboard.
- **Location**: `/src/OTGW-firmware/data/`
- **Language**: HTML5 / CSS3 / JavaScript (ES5+), with inline SVG graphics
- **Purpose**: Provides the complete web UI for configuring, monitoring, and controlling the OpenTherm Gateway. Enables real-time viewing of OT messages, WiFi/Ethernet management, MQTT settings, system updates, firmware flashing, and SAT thermostat tuning.

---

## File Inventory

### Core HTML & Entrypoint

| File | Size | Purpose |
|------|------|---------|
| `index.html` | ~11 KB | Main SPA entry point; loads CSS (with theme detection), JS modules, and ECharts library. Template-driven layout with page sections for Home, SAT, Settings, Advanced modes. Includes OT-Direct panel (OTGW32 only). |

### Stylesheets

| File | Size | Purpose |
|------|------|---------|
| `index.css` | Light theme styles; header bar, nav tabs, tables, settings fields, OT log viewer, graph container |
| `index_dark.css` | Dark theme override; same structure as `index.css` but dark color palette |
| `index_common.css` | Shared responsive rules for both themes; mobile breakpoints (≤768px), layout flex utilities, button styles, form spacing |

### JavaScript Modules

| File | Size | Purpose |
|------|------|---------|
| `index.js` | ~3000 lines | **Main UI controller**. Handles: page navigation, WebSocket OT log streaming, REST API polling, local storage caching, theme switching, settings persistence, firmware flashing, device info display. Core logic for all pages except SAT. |
| `sat.js` | ~600 lines | **SAT (Smart Autotune Thermostat) dashboard**. Fetches `/api/v2/sat/status`, renders heating curve, PID controls, preset buttons, temperature charts, simulation toggle, boiler state labels. |
| `graph.js` | ~800 lines | **Real-time temperature/OT graph visualization**. ECharts-based multi-grid chart with 5 separate y-axes (Flame, DHW, CH, Modulation, Temperatures). Handles live data streaming, theme synchronization, memory-aware buffering (24h), Dallas sensor auto-discovery and color mapping. |

### Configuration & Data Files

| File | Size | Purpose |
|------|------|---------|
| `settings.ini` | JSON | Default device settings template (hostname, MQTT broker/port, NTP timezone, OTGWcommands startup). Seed data for first-time setup. |
| `mqttha.cfg` | ~100 KB | **Home Assistant MQTT Auto-Config definitions**. Defines 200+ MQTT discovery payloads (climate, binary_sensor, sensor) for Home Assistant integration. Includes device metadata (manufacturer, model, version). Templated with `%mqtt_pub_topic%`, `%node_id%`, `%version%` substitutions. |
| `otgw_simulation.log` | Sample data | Example OT message log for testing/demo purposes. |

### File System Explorer (LittleFS Web UI)

| File | Size | Purpose |
|------|------|---------|
| `FSexplorer.html` | ~4 KB | Standalone file browser for managing LittleFS files (upload, delete, rename, download). Embedded in "Advanced" menu. |
| `FSexplorer.css` | Light theme for file explorer |
| `FSexplorer_dark.css` | Dark theme for file explorer |
| `FSexplorer.png` | Icon/graphic for file explorer UI |

### Assets & Media

| File | Size | Purpose |
|------|------|---------|
| `favicon.ico` | Favicon for browser tab |
| `settings.png` | Icon for Settings page button |
| `system_update.png` | Icon for firmware update UI |
| `update.png` | Icon for firmware update button |
| `refresh-page-option.png` | Icon for refresh/reload button |
| `download-to-storage-drive.png` | Icon for download/export log feature |

### PIC Firmware Binaries (OTA Firmware Flashing)

#### PIC16F88 (Original OTGW hardware, Schelte Bron design)

| File | Purpose |
|------|---------|
| `pic16f88/gateway.hex` | PIC16F88 OpenTherm Gateway firmware (latest) |
| `pic16f88/gateway.ver` | Version string for gateway.hex |
| `pic16f88/gateway-4.3.hex` | PIC16F88 Gateway v4.3 (legacy) |
| `pic16f88/gateway-4.3.ver` | Version string for gateway-4.3.hex |
| `pic16f88/interface.hex` | Interface-only firmware (no gateway logic) |
| `pic16f88/interface.ver` | Version string for interface.hex |
| `pic16f88/diagnose.hex` | Diagnostic firmware for troubleshooting |
| `pic16f88/diagnose.ver` | Version string for diagnose.hex |

#### PIC16F1847 (Modern OTGW-PCB variant)

| File | Purpose |
|------|---------|
| `pic16f1847/gateway.hex` | PIC16F1847 OpenTherm Gateway firmware (latest) |
| `pic16f1847/gateway.ver` | Version string for gateway.hex |
| `pic16f1847/interface.hex` | Interface-only firmware |
| `pic16f1847/interface.ver` | Version string for interface.hex |
| `pic16f1847/diagnose.hex` | Diagnostic firmware |
| `pic16f1847/diagnose.ver` | Version string for diagnose.hex |

### Auxiliary Files

| File | Purpose |
|------|---------|
| `transfer.dat` | Binary data file (internal use, possibly for OTA staging) |
| `version.hash` | Firmware version hash for mismatch detection (fs-mismatch-banner) |

---

## Code Elements

### Key JavaScript Functions (index.js)

#### Page Navigation & State

- `initMainPage()` — Entry point; loads device info, listens for theme/visibility changes, initializes all pollers
- `setActivePageSection(activeId: string): void` — Switch between page sections (home, settings, sat, advanced)
- `renderSharedPageNavShell(): void` — Renders tab navigation bar from template
- `updateThemeToggle(): void` — Updates theme toggle button appearance (sun/moon icon)

#### WebSocket & Real-Time OT Monitoring

- `initOTLogWebSocket(force: boolean): void` — Connect to `/ws` (port 80, plain HTTP), stream live OT messages
- `disconnectOTLogWebSocket(): void` — Close WebSocket connection and clean up listeners
- `updateWSStatus(connected: boolean): void` — Update connection indicator (● color/text)
- `resetWSWatchdog(): void` — Reset watchdog timer to detect stale connections
- `parseLogLine(line: string): object` — Parse raw OT message format into structured object (id, dir, type, label, raw, unit)
- `formatLogLine(logLine: object): string` — Format parsed OT message for display (with or without timestamp)
- `addLogLine(logLine: object): void` — Add line to circular buffer, trigger filtered view update
- `renderLogDisplay(): void` — Render buffered log lines to DOM (#otLogContent), respecting search filter & scroll position
- `updateLogDisplay(): void` — Debounced update (calls `renderLogDisplay()`)

#### OT Log Controls & Filtering

- `setupOTLogControls(): void` — Attach listeners to log controls (clear, download, search, auto-scroll, auto-download, capture mode)
- `clearLogBuffer(): void` — Clear in-memory log buffer
- `downloadLog(isAuto?: boolean): void` — Trigger file download of current log
- `toggleAutoDownloadLog(enabled: boolean): void` — Enable/disable 15-minute auto-save to device storage
- `setOTLogCommandsOnly(enabled: boolean): void` — Filter log to show only command lines (PS=1 mode)
- `updateOTLogModeNotice(displayState: object): void` — Display PS=1 mode warning banner

#### File Streaming & Log Persistence

- `startFileStreaming(): Promise<void>` — Open writable stream to `/logs/otgw_YYYYMMDD.log` on device storage
- `processLogQueue(): Promise<void>` — Async worker to flush queued log lines to file
- `writeToStream(entry: object): Promise<void>` — Write single log entry to open file stream
- `checkFileRotation(): Promise<void>` — Check if date changed, rotate to new file if needed
- `stopFileStreaming(): void` — Close stream and cleanup

#### REST API Polling

- `refreshFirmware(): void` — Fetch `/api/v2/device/fw` (firmware version, build info, PS=1 mode indicator)
- `refreshDevTime(): void` — Fetch `/api/v2/device/time` (ESP system time, NTP sync status)
- `refreshPICsettings(): void` — Fetch `/api/v2/pic/settings` (GPIO, LED, temp sensor, thermostat, etc. from PIC)
- `refreshDevInfo(): void` — Fetch `/api/v2/device/info` (hostname, IP, WiFi SSID, Ethernet state, uptime, heap, crash info)
- `refreshOTmonitor(): void` — Fetch `/api/v2/otgw/monitor` (current OT state snapshot)
- `refreshOTDStatus(): void` — Fetch `/api/v2/ot-direct/status` (OTGW32 OT-Direct panel state)
- `refreshGatewayMode(force?: boolean): void` — Fetch `/api/v2/otgw/mode` (gateway vs. monitor mode)
- `applyTheme(): void` — Apply light/dark theme CSS, sync with localStorage

#### Settings & Form Handling

- `settingsPage(): void` — Render settings form from `/api/v2/device/settings` (WiFi, MQTT, NTP, OTGWcommands, etc.)
- `refreshSettings(): void` — Fetch latest settings, populate form fields
- `saveSettings(): void` — POST all form fields to `/api/v2/device/settings`
- `sendPostSetting(field: string, value: any): void` — POST single setting change to `/api/v2/device/settings`
- `webhookPage(): void` — Render webhook test form and event log
- `testWebhookUI(stateOn: boolean): void` — POST test event to configured webhook URL
- `saveWebhookSettings(): void` — POST webhook URL to server

#### Firmware Flashing

- `startFlash(filename: string): void` — Initiate OTA firmware update from device storage or URL
- `performFlash(filename: string): void` — Upload firmware binary to `/api/v2/device/fw/upload`
- `startFlashPolling(): void` — Begin polling `/api/v2/device/fw/status` during flash
- `pollFlashStatus(): void` — Check flash progress (%) and handle completion/error
- `handleFlashMessage(data: object): void` — Process flash status response, update UI progress bar
- `handleFlashCompletion(filename: string, error?: string): void` — Clean up on success/failure

#### PIC Firmware Update

- `firmwarePage(): void` — Render PIC firmware selection/flashing UI for PIC16F88 and PIC16F1847 variants
- `parseFirmwareInfo(filename: string): object` — Extract PIC type (16f88 or 16f1847) and variant (gateway, interface, diagnose) from filename

#### Device Information

- `deviceinfoPage(): void` — Render system info page: hostname, IP, WiFi SSID, uptime, heap usage, crash log
- `refreshDeviceInfo(): void` — Fetch and display device info
- `renderCrashLogInfo(crashlog: object): void` — Parse and display ESP crash log if available
- `estimateMemoryUsage(): number` — Estimate memory used by in-memory OT log buffer
- `getActualMemoryUsage(): number` — Fetch actual heap from server

#### Local Storage & Persistence

- `loadUISettings(): void` — Restore UI state from localStorage (auto-scroll, timestamp, search, etc.)
- `saveUISetting(field: string, value: any): void` — Save single UI setting to localStorage
- `saveDataToLocalStorage(): void` — Persist OT log buffer and OT stats to localStorage (time-delayed)
- `restoreDataFromLocalStorage(): void` — Restore OT log buffer from localStorage on page load
- `clearStoredData(): void` — Clear all localStorage
- `startPersistenceTimer(): void` — Start debounced saver (saves ~1x per 30 seconds)
- `stopPersistenceTimer(): void` — Stop persistence timer

#### PIC Settings Caching

- `getPICSettingsCache(): object | null` — Retrieve cached PIC settings from localStorage with age check
- `savePICSettingToCache(key: string, value: any, timestampMs: number): void` — Cache individual PIC setting
- `getPICSettingFromCache(key: string): any` — Retrieve single cached setting

#### PIC Mode Detection (PIC vs. OTGW32)

- `applyPICAvailability(available: boolean, otCommandAvailable: boolean): void` — Show/hide PIC-specific UI elements based on device type
- `applyOTDirectAvailability(available: boolean): void` — Show/hide OTGW32 OT-Direct panel
- `detectStorageQuota(): void` — Estimate available storage for log downloads

#### OT-Direct Overrides (OTGW32 only)

- `startOTDStatusPolling(): void` — Begin polling OT-Direct status every 5 seconds
- `stopOTDStatusPolling(): void` — Stop polling
- `refreshOTDStatus(): void` — Fetch `/api/v2/ot-direct/status` (mode, bus state, thermostat/boiler connected, overrides)
- `refreshOTDOverrides(): void` — Fetch `/api/v2/ot-direct/overrides` (list of active overrides)
- `renderOTDOverrides(ov: array): void` — Render override list with delete buttons
- `sendOTDOverride(): void` — POST new override to `/api/v2/ot-direct/override` (SR, CR, RM, CM, UI, KI actions)
- `clearOTDOverride(action: string, msgid: number): void` — DELETE specific override

#### Network Indicator

- `updateNetworkIndicator(mode: string): void` — Update WiFi/Ethernet icon and status text based on network mode (WiFi, Ethernet, offline)
- `parseGatewayModeValue(modeValue: string): object` — Parse gateway mode string (gateway, monitor, etc.)
- `updateGatewayModeIndicator(value: string): void` — Display current gateway mode (● color indicator + text)
- `formatGatewayModeDisplayValue(modeValue: string): string` — Localized mode label

#### Utility Functions

- `safeJSONParse(text: string): object | null` — Parse JSON with error handling
- `safeGetElementById(id: string, warnIfMissing?: boolean): Element | null` — Safely query DOM
- `isDallasAddress(entry: object): boolean` — Detect if entry is a Dallas temperature sensor (by type field or hex address format)
- `fetchDallasLabels(): Promise<object>` — Fetch `/api/v2/sensors/labels` (user-defined Dallas sensor names)
- `normalizeOTGWcommand(cmd: string): string` — Validate and normalize OTGW command format
- `sendOTGWcommand(cmd: string): void` — Send OTGW command via `/api/v1/otgw/command`
- `toggleInteraction(enabled: boolean): void` — Enable/disable form inputs during async operations
- `strToBool(s: string): boolean` — Parse boolean from string ("1", "true", "on")
- `round(value: number, precision: number): number` — Round to N decimal places
- `translateToHuman(longName: string): string` — Convert camelCase setting name to human-readable label
- `translateTooltip(longName: string): string` — Generate tooltip help text for setting
- `formatDeviceInfoLabel(key: string): string` — Format device info keys for display
- `formatDeviceInfoValue(key: string, value: any): string` — Format device info values (uptime, heap, etc.)

#### SAT Integration

- `satPage(): void` — Render SAT dashboard from template
- `satSettingsPage(): void` — Render SAT advanced settings form
- `toggleSATSettingsGroup(headerId: string): void` — Collapse/expand SAT settings group
- `refreshSATSettings(): void` — Fetch `/api/v2/sat/settings` and populate form
- `buildSATSettingsGroups(page: Element, data: object): void` — Build settings UI groups from API response
- `populateSATSettingsValues(data: object): void` — Populate form fields with current values
- `saveSATSettingsGroup(groupId: string): void` — POST SAT settings group to `/api/v2/sat/settings`
- `simulationToggle(): void` — Toggle OT message simulation mode

#### Statistics & Logging

- `processStatsLine(line: string): void` — Parse OT message stats from log format
- `updateStatisticsDisplay(): void` — Render statistics table (message count, success rate, average interval)
- `sortStats(col: string): void` — Sort stats by column (message ID, count, success, interval)

#### Search & Filtering

- `openInlineSensorLabelEditor(address: string, targetNode: Element, evt: Event): void` — Open inline editor for Dallas sensor name
- `closeInlineSensorLabelEditor(cancelOnly?: boolean): void` — Close editor and save or discard changes
- `saveInlineSensorLabel(): void` — POST updated Dallas sensor label to `/api/v2/sensors/labels`

#### Utility UI Helpers

- `toggleHidden(className: string, hideOnly?: boolean): void` — Toggle hidden class on elements
- `setVisible(className: string, visible: boolean): void` — Show/hide elements by class
- `setBackGround(field: string, newColor: string): void` — Set background color for validation feedback
- `getBackGround(field: string): string` — Get current background color
- `renderBottomMessage(): void` — Render footer status message (heap, uptime, etc.)
- `updateHeapDisplay(): void` — Update heap usage display
- `ensureWebkitScrollbarStyles(): void` — Inject -webkit-scrollbar CSS for dark mode log viewer

---

### SAT Module (sat.js)

Encapsulated object `SAT` with internal state and methods:

#### Status & Configuration

- `SAT.fetchStatus(): void` — Fetch `/api/v2/sat/status` (enabled, active, mode, control setpoints, temperatures, PID tuning)
- `SAT.updateDashboard(data: object): void` — Render status badges, temperature cards, PID details, preset/mode buttons
- `SAT.startPolling(): void` — Begin 5-second poll loop
- `SAT.stopPolling(): void` — Stop polling, clear timers

#### Heating Curve

- `SAT.calculateHeatingCurve(outsideTemp: number): number` — Compute setpoint using heating curve formula (constants defined as HC_BASE_OFFSET_FLOOR, HC_REF_TEMP, etc.)

#### Weather Data Integration

- `SAT.fetchWeatherData(): Promise<object>` — Fetch `/api/v2/sat/weather` (outside temperature from weather service)
- `SAT.updateWeatherDisplay(data: object): void` — Render weather info

#### Presets & Control

- `SAT.activatePreset(presetIdx: number): void` — POST preset activation to `/api/v2/sat/preset`
- `SAT.setControlMode(modeIdx: number): void` — POST control mode (Off/Continuous/PWM) to `/api/v2/sat/mode`
- `SAT.enableSAT(): void` — POST enable command
- `SAT.disableSAT(): void` — POST disable command
- `SAT.toggleSimulation(): void` — Toggle OT message simulation

#### Charts (Historical Data)

- `SAT._pollTimer` — Interval handle for status polling (5s)
- `SAT._weatherTimer` — Interval handle for weather polling
- `SAT._maxChartPoints = 720` — Max data points for 1-hour window
- `SAT._chartData = { time: [], setpoint: [], flow: [], room: [], outside: [], pid: [] }` — Time-series buffers
- Chart updates coordinated with graph.js if present

#### Label Maps (Internal)

- `MODE_LABELS[]` — ['Off', 'Continuous', 'PWM']
- `CYCLE_LABELS[]` — ['None', 'Good', 'Overshoot', 'Underheat', 'Short', 'Uncertain']
- `BOILER_LABELS{}` — Maps boiler state codes to readable labels (off, idle, heating, cooling, etc.)

---

### Graph Module (graph.js)

Encapsulated object `OTGraph` managing ECharts-based real-time visualization:

#### Initialization & Cleanup

- `OTGraph.init(): void` — Initialize ECharts container; detect theme; set up event listeners for time window, export, screenshot
- `OTGraph.initDone(): boolean` — Track initialization state (prevent duplicate listeners)
- `OTGraph.destroy(): void` — Clean up chart instance and event listeners on page unload

#### Data Management

- `OTGraph.data = {}` — Dictionary of time series: `{ flame: [...], dhwMode: [...], boiler: [...], room: [...], ...}`
- `OTGraph.pendingData = {}` — Accumulate new data points between chart updates
- `OTGraph.maxPoints = 864000` — Buffer capacity for 24 hours at 10 msg/sec
- `OTGraph.timeWindow = 3600 * 1000` — Default 1-hour window in milliseconds
- `OTGraph.disconnectMarkers[]` — Track WebSocket disconnects/reconnects for display

#### Live Data Streaming

- `OTGraph.addDataPoint(seriesId: string, timestamp: number, value: number): void` — Add new value to time series
- `OTGraph.addDataPoints(dataObj: object, timestamp: number): void` — Batch add multiple series in one update
- `OTGraph.updateChart(): void` — Refresh ECharts with pending data (throttled every 2 seconds)
- `OTGraph.clearData(): void` — Reset all buffers

#### Chart Configuration

- `OTGraph.seriesConfig[]` — Array of series definitions: `{ id, label, gridIndex, type, step, sampling: 'lttb', ... }`
- **Grid indices (y-axes):**
  - 0: Flame (binary, 0-1)
  - 1: DHW Mode (binary, 0-1)
  - 2: CH Mode (binary, 0-1)
  - 3: Modulation (0-100%)
  - 4: Temperature (analog, °C)
- `OTGraph.palettes = { light: {...}, dark: {...} }` — Color schemes for main sensors
- `OTGraph.sensorColorPalettes = { light: [...16 colors...], dark: [...16 colors...] }` — Colors for Dallas temperature sensors

#### Dallas Sensor Auto-Discovery

- `OTGraph.detectedSensors[]` — List of discovered Dallas sensors
- `OTGraph.sensorAddressToId = {}` — Map sensor hex address → internal ID
- `OTGraph.refreshSensorLabels(labels: object): void` — Update sensor label map from `/api/v2/sensors/labels`
- `isDallasAddress(entry: object): boolean` — Detect Dallas sensor by `type: 'dallas'` field

#### Theme Management

- `OTGraph.currentTheme = 'light'` — Current theme (auto-detect from localStorage or system preference)
- `OTGraph.onThemeChange(newTheme: string): void` — Re-render chart with new color palette
- Listens to `theme-change` custom event from index.js

#### User Interactions

- `OTGraph.setTimeWindow(windowMs: number): void` — Change visible time range (30m, 1h, 3h, 6h, 12h, 24h)
- `OTGraph.toggleSeries(seriesId: string): void` — Show/hide series
- `OTGraph.exportData(format: 'csv' | 'json'): void` — Download data in selected format
- `OTGraph.screenshot(): void` — Download chart as PNG
- `OTGraph.toggleAutoScreenshot(enabled: boolean): void` — Auto-capture every N minutes

#### Resize Handling

- `OTGraph.resizeHandler` — Event listener reference for cleanup
- Chart automatically resizes when window changes

#### Performance Optimization

- `OTGraph.updateInterval = 2000` — Throttle chart updates to every 2 seconds
- `OTGraph.sampling = 'lttb'` — ECharts line simplification algorithm (Largest-Triangle-Three-Buckets)
- `OTGraph.large = true` — Enable ECharts large-mode rendering for performance

---

## Key Behaviors & Workflows

### Home Page (Main Dashboard)

1. **On Load**: 
   - Restore UI settings from localStorage (auto-scroll, timestamp visibility, etc.)
   - Fetch device info (`/api/v2/device/info`) to detect PIC vs. OTGW32, platform
   - Start WebSocket connection to `/ws` for live OT messages
   - Start REST polling (3s interval): device time, firmware, gateway mode, network indicator
   - If SAT enabled, start SAT polling (5s interval)
   - If OT-Direct available (OTGW32), start OT-Direct polling

2. **WebSocket Live Stream**:
   - Messages arrive as raw text lines: `{"time":"12:34:56.123456","id":0,"type":"T","raw":"Pxxxx....","label":"Status","unit":"","dir":"in/out"}`
   - Each line parsed via `parseLogLine()`, added to circular buffer (1M lines max in capture mode)
   - Buffer rendered to DOM every `updateInterval` (debounced, ~200ms)
   - Respects filters: search text, timestamps checkbox, auto-scroll
   - Memory usage monitored; if exceeded, oldest entries purged

3. **OT Monitor Tabs**:
   - **Log**: Live streaming OT messages with optional timestamps
   - **Statistics**: Aggregated message counts, success rates, average intervals per message ID
   - **Graph**: Real-time ECharts visualization of temperatures, flame, modulation, modes

4. **Controls**:
   - Clear log, Download log, Auto-download every 15 min
   - Search/filter messages
   - Toggle timestamp display
   - Capture mode (up to 1M lines)
   - OTGW command input (e.g., `GW=1`, `TT=21.5`)

### Settings Page

1. **On Load**: Fetch `/api/v2/device/settings` (all persistent settings)
2. **Form Fields**: WiFi SSID/password, MQTT broker/port/user, NTP timezone, LED blink, OTGW commands, etc.
3. **On Save**: POST all fields as JSON to `/api/v2/device/settings`
4. **Password Handling**: Passwords shown as asterisks; if unchanged, original value sent (via placeholder detection)
5. **Validation**: Background color feedback (green = valid, red = error)

### SAT (Smart Autotune Thermostat) Page

1. **Dashboard**: Shows heating curve, PID loop state, room temp, outside temp, flow setpoint, boiler status
2. **Presets**: Buttons for Away, Eco, Comfort, Sleep, Activity, Home
3. **Control Mode**: Off, Continuous, PWM
4. **Advanced Settings**: PID coefficients (Kp, Ki, Kd), heating system type (radiators, underfloor, heat pump), simulation
5. **Charts**: Historical temperature and PID output trends (last 1 hour)

### Firmware Update (Advanced > PIC firmware)

1. **PIC Type Detection**: Fetch `/api/v2/device/info`, check PIC model (PIC16F88 or PIC16F1847)
2. **Firmware Selection**: List available firmwares from `/pic16f88/` or `/pic16f1847/` directories (gateway, interface, diagnose)
3. **Start Flash**: POST firmware binary to `/api/v2/device/fw/upload`
4. **Progress Polling**: Poll `/api/v2/device/fw/status` every 1 second; display progress bar (0-100%)
5. **Completion**: Verify success or show error message

### ESP Firmware Update (Advanced > System Update)

1. **OTA Upload**: Select `.bin` file, POST to `/api/v2/device/fw/upload`
2. **Flash Progress**: Poll `/api/v2/device/fw/status`, update progress indicator
3. **Reboot**: Device reboots after successful flash; page shows "Waiting for device..."

### WebSocket OT Message Format

Raw message (example):
```json
{
  "time": "12:34:56.123456",
  "id": 0,
  "type": "T",
  "raw": "Pxxxx.....",
  "label": "Status: Master and Boiler",
  "unit": "",
  "dir": "in",
  "source": "T (Master Status)"
}
```

Parsed log entry (internal):
```javascript
{
  time: "12:34:56.123456",
  id: 0,
  dir: "in",
  type: "T",
  label: "Status: Master and Boiler",
  source: "T (Master Status)",
  raw: "Pxxxx.....",
  unit: "",
  valid: " " or "X" (invalid)
}
```

Formatted display (example):
```
[12:34:56.123] T 0 → P5A80 | Status: Master and Boiler (T)
```

### Theme Switching

1. **Storage**: `localStorage.setItem('theme', 'dark' | 'light')`
2. **Detection**: On page load, check localStorage; if not set, check `window.matchMedia('(prefers-color-scheme: dark)')`
3. **CSS Injection**: Script in `<head>` of `index.html` dynamically selects `index.css` or `index_dark.css` before page renders
4. **Sync**: Graph and all charts re-render with new palette when theme changes
5. **Button**: Theme toggle button updates icon (☀/🌙) and text

---

## Dependencies

### Internal Dependencies (ESP8266 Firmware)

- **REST API v2**: `/api/v2/` endpoints (see below)
- **WebSocket endpoint**: `/ws` (plain HTTP, port 80)
- **LittleFS served files**: All static assets (HTML, CSS, JS, firmware bins)

### External Dependencies (CDN)

- **ECharts 5.4.3**: Chart library (real-time graph visualization) from JSDelivr CDN
  - URL: `https://cdn.jsdelivr.net/npm/echarts@5.4.3/dist/echarts.min.js`
  - Fallback: Graph feature displays error message if CDN is unreachable

### Browser APIs Used

- **WebSocket API**: Real-time OT message streaming
- **Fetch API**: REST calls with error handling
- **LocalStorage**: Persist UI settings, OT log buffer, Dallas labels cache
- **File System Access API**: Writable streams for log file persistence (`/logs/otgw_YYYYMMDD.log`)
- **matchMedia**: Detect dark/light OS theme preference

### REST API Endpoints (from index.js calls)

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/v2/device/info` | GET | Hostname, IP, WiFi SSID, Ethernet state, uptime, heap, crash info, PIC presence, SAT enabled |
| `/api/v2/device/fw` | GET | Firmware version, build date, PS=1 mode, OTA update availability |
| `/api/v2/device/time` | GET | ESP system time, NTP sync status |
| `/api/v2/device/settings` | GET / POST | Persistent settings (WiFi, MQTT, NTP, LED, OTGW commands) |
| `/api/v2/device/fw/upload` | POST | Upload firmware binary (ESP or PIC) |
| `/api/v2/device/fw/status` | GET | Flash progress (0-100%) and status |
| `/api/v1/otgw/command` | POST | Send OTGW command (GW=1, TT=21.5, etc.) |
| `/api/v2/otgw/monitor` | GET | Current OT state snapshot (temperatures, flame, mode, etc.) |
| `/api/v2/otgw/mode` | GET | Gateway vs. Monitor mode |
| `/api/v2/pic/settings` | GET | PIC settings (GPIO, LED, temp sensor, Smart Power, Thermostat, etc.) |
| `/api/v2/ot-direct/status` | GET | OT-Direct panel state (mode, bus, thermostat, boiler, overrides count) — OTGW32 only |
| `/api/v2/ot-direct/overrides` | GET | List of active OT-Direct overrides |
| `/api/v2/ot-direct/override` | POST / DELETE | Add or clear OT-Direct override (SR, CR, RM, CM, UI, KI) |
| `/api/v2/sat/status` | GET | SAT dashboard state (enabled, active, temps, PID, setpoint, mode, etc.) |
| `/api/v2/sat/settings` | GET / POST | SAT advanced tuning parameters |
| `/api/v2/sat/preset` | POST | Activate SAT preset (Away, Eco, Comfort, Sleep, Activity, Home) |
| `/api/v2/sat/mode` | POST | Set SAT control mode (Off, Continuous, PWM) |
| `/api/v2/sensors/labels` | GET / POST | Dallas temperature sensor user-defined names |
| `/webhook` | POST | Test webhook endpoint (user-configured URL) |

### MQTT Integration

- Uses `mqttha.cfg` for Home Assistant MQTT discovery auto-configuration
- Topics: `OTGW/*` (default), `homeassistant/climate/*/config`, etc.
- Publishes: OT state, temperatures, flame status, DHW, CH, mode statuses
- Subscribes: Thermostat setpoint commands, DHW control commands

---

## Browser Compatibility

### Minimum Requirements

- **Chrome**: Latest + 2 versions (ES5+, Fetch, WebSocket, LocalStorage, File API)
- **Firefox**: Latest + 2 versions
- **Safari**: Latest + 2 versions (CSS flexbox, CSS grid, ES5)
- **Edge**: Latest (Chromium-based)

### Features Requiring Modern Browser

- **Fetch API**: All REST calls (fallback: XMLHttpRequest not available)
- **WebSocket**: Live OT log streaming (fallback: HTTP polling if WS unavailable, but not implemented)
- **LocalStorage**: Settings and log persistence (disabled silently if unavailable)
- **CSS Grid / Flexbox**: Responsive layout (falls back to basic layout on older browsers)
- **File Streams**: Log file writing requires File System Access API (modern feature, log download still works via blob)

### Device Support

- **Desktop**: Full support
- **Tablet** (≤768px): Optimized responsive layout (OT log hidden on small screens, settings stack vertically)
- **Mobile** (phone): Functional but limited (OT log hidden, touch-friendly buttons, simplified navigation)

### Known Limitations

- **Dark theme scrollbar styling**: Uses `-webkit-scrollbar` CSS (Chrome/Safari/Edge only); Firefox renders default scrollbar
- **ECharts CDN**: Graph feature requires internet connectivity; offline mode shows error message
- **localStorage quota**: Typical limit 5-10 MB per domain; OT log buffer limited to avoid quota exceeded
- **Password fields**: Browser auto-fill may not work reliably; manual entry recommended for critical passwords

---

## Theme System

### Light Theme (index.css)

- **Background**: Cyan/light blue (`#e6ffff`)
- **Header**: Sky blue (`#00bffe`)
- **Text**: Black on light backgrounds
- **Accent**: Light blue tables, white form inputs

### Dark Theme (index_dark.css)

- **Background**: Dark gray/charcoal
- **Header**: Dark blue
- **Text**: White/light gray on dark backgrounds
- **Accent**: Dark tables, darker form inputs

### Common Styles (index_common.css)

- Mobile responsiveness: `@media (max-width: 768px)` — hide OT log, stack nav vertically, adjust button sizes
- Navigation bar: Flexbox row layout with wrapping
- Settings form: 320px field width, left-floated columns
- Tab system: Active/inactive tab styling
- Accessibility: ARIA labels on interactive elements, semantic button roles

---

## File Serving & LittleFS

### How Files Are Served

1. ESP8266 firmware (`OTGW-firmware.ino`) initializes LittleFS at startup
2. Files in `data/` directory are embedded during build: `python build.py` packs all files into SPIFFS/LittleFS image
3. HTTP server (AsyncWebServer) routes:
   - `/` → `/index.html`
   - `/index.css`, `/index_dark.css`, `/index_common.css` → CSS files
   - `/index.js`, `/graph.js`, `/sat.js` → JavaScript modules
   - `/pic16f88/*`, `/pic16f1847/*` → PIC firmware binaries and version files
   - `/api/*` → REST endpoints (handled by firmware C++ code)
   - `/ws` → WebSocket endpoint (handled by firmware C++ code)

### File Size Constraints

- **ESP8266 Flash**: Total 4 MB; firmware + LittleFS split typically 2 MB / 2 MB
- **index.html**: ~11 KB (relatively large; streamed by default to avoid RAM overhead)
- **JavaScript**: ~3 KB (index.js) + 600 B (sat.js) + 800 B (graph.js) = ~4.4 KB total
- **CSS**: ~10 KB total (index.css + dark + common)
- **PIC binaries**: ~100 KB total (multiple versions for 2 PIC types)

### Build Process

```bash
python build.py              # Packs data/ into LittleFS image
python build.py --clean      # Clean build
```

Resulting files uploaded to ESP8266 via OTA or serial.

---

## Security & Access Control

### HTTP-Only (No HTTPS)

- Firmware uses plain HTTP only (no HTTPS/WSS)
- Assumption: Device is on trusted local network only
- Remote access should use VPN + reverse HTTPS proxy

### Authentication (if configured)

- Optional HTTP Basic Auth (password configurable in settings)
- Credentials sent in Authorization header with each request
- Stored in settings.json as plaintext (risk: filesystem access)

### Input Validation

- All form inputs validated on client side (HTML5 validation)
- Server validation on firmware side (checked by backend code)
- OTGW commands validated against regex pattern
- MQTT settings sanitized (no special chars in topic names)

### CORS & Same-Origin

- All API calls made to same origin (`window.location.protocol + '//' + window.location.host`)
- No cross-domain requests (except ECharts CDN which is read-only)

---

## Performance Considerations

### Memory Usage (ESP8266)

- **Circular log buffer**: Default 100K lines, max 1M lines in capture mode
- **Estimated RAM**: ~20 MB for 100K lines (too large for ESP8266 RAM, must use file storage)
- **Mitigation**: OT log persisted to `/logs/otgw_YYYYMMDD.log` on LittleFS
- **In-Memory**: Only active window (last 1000 lines) rendered to DOM at once

### CPU & Bandwidth

- **WebSocket**: Continuous stream (1-10 msg/sec typical, up to 100+ msg/sec during busy periods)
- **Graph updates**: Throttled to 2-second intervals (ECharts large-mode rendering)
- **REST polling**: 3-5 second intervals (device time, settings, gateway mode)
- **Frontend**: Debounced UI updates (200-500ms) to avoid excessive reflows

### Optimization Techniques

- **ECharts sampling**: LTTB (Largest-Triangle-Three-Buckets) reduces visible points
- **CSS containment**: No CSS containment used (not supported by all browsers)
- **Image compression**: PNG icons are minimal size
- **Streaming**: Large responses streamed (index.html, firmware binaries)
- **Caching**: ETag and Cache-Control headers set by firmware

---

## SAT-Specific Architecture

The SAT module is a complete subsystem for thermostat autotune:

- **Visualization-only dashboard**: All logic runs on ESP8266 (SATcontrol.ino), web UI is read-only except for settings
- **Real-time feedback**: Dashboard updates every 5 seconds via `/api/v2/sat/status`
- **Heating curve formula**: `setpoint = HC_BASE_OFFSET + (HC_REF_TEMP - outsideTemp) * curve_coeff`
- **PID tuning**: Interactive adjustment of Kp, Ki, Kd coefficients via settings form
- **Presets**: Quick activation of predefined temperature profiles (Away=18°C, Sleep=17°C, Comfort=21°C, etc.)

---

## Testing & Debug

### Simulation Mode

- Toggle with SAT "Start Sim" button or via API
- Sends pre-recorded OT messages instead of real messages
- Uses `otgw_simulation.log` as source data
- Useful for testing UI without active boiler system

### Web Console (F12)

- All API calls logged to browser console
- JSON parse errors caught and logged
- WebSocket connection events logged
- Unhandled promise rejections caught and logged

### Debug Information

- Device info page shows: crash log (if available), heap usage, uptime, WiFi/Ethernet signal
- Status message bar at bottom shows current heap and time
- Network tab in browser DevTools shows all API calls and responses

---

## Notes & Known Issues

### ECharts CDN Dependency

- **Issue**: Graph fails to load if device has no internet access
- **Workaround**: Host ECharts locally or use CDN proxy
- **Future**: Consider minified inline charting library for offline support

### PS=1 Mode (Passthrough Serialization)

- When PIC firmware reports PS=1, raw OT frames are not available
- UI displays decoded field summaries instead
- Search/filter features work on summaries, not raw frames
- Banner displayed: "PS=1 mode active: showing decoded field summaries"

### Mobile Responsiveness

- OT log viewer hidden on tablets/phones (≤768px)
- Settings form stacks vertically on mobile
- Recommended: Access from desktop for full features

### localStorage Quota

- Typical limit: 5-10 MB per domain
- OT log buffer + SAT history + Dallas labels must fit within quota
- Oldest entries auto-purged if quota exceeded
- Clear stored data option available in device info

---

## References & Related Files

### Firmware Components

- **index.ino**: HTTP server setup, REST endpoints, WebSocket handler
- **MQTTstuff.ino**: MQTT auto-config generation (mqttha.cfg processing)
- **SATcontrol.ino**: SAT algorithm and real-time control (backend)
- **restAPI.ino**: REST endpoint implementation

### Architecture Documentation

- See `ADR-051` (Settings & State Architecture) for persistent/transient state model
- See `ADR-040` (ADR-040 Bug Fix) for stack overflow fixes in MQTT auto-config

### File Paths

- Web assets: `/src/OTGW-firmware/data/`
- Build output: `/build/` (LittleFS image)
- Firmware binaries: `src/OTGW-firmware/data/pic16f*/`

---

## Summary

The Web Assets module provides a complete, responsive, real-time web UI for the OTGW-firmware ESP8266 gateway. It combines:

1. **Real-time monitoring**: WebSocket live OT message stream with graph visualization
2. **System control**: Settings, WiFi, MQTT, NTP configuration
3. **Firmware management**: OTA updates for ESP and PIC microcontrollers
4. **SAT integration**: Smart autotune thermostat dashboard and tuning UI
5. **Home Assistant integration**: MQTT auto-discovery configuration for seamless HA integration
6. **Responsive design**: Desktop, tablet, and mobile support with light/dark theme toggle
7. **Offline resilience**: LocalStorage persistence of logs and settings

The module is served entirely from LittleFS flash storage, with minimal external dependencies (ECharts CDN for graphing). Code follows modern JavaScript patterns (ES5+, Fetch API, async/await) with safety checks for JSON parsing, DOM access, and error handling throughout.
