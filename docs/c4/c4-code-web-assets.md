# C4 Code Level: Web Assets Module

## Overview

- **Name**: OTGW Web Assets & UI Layer
- **Description**: Complete web user interface for the OTGW-firmware ESP32-S3 gateway (2.0.0 line). Serves HTML, CSS, JavaScript, and PIC firmware binaries from LittleFS flash filesystem. Multi-page SPA with real-time WebSocket monitoring, REST API integration, settings management, and SAT (Smart Autotune Thermostat) control dashboard.
- **Location**: `/src/OTGW-firmware/data/`
- **Language**: HTML5 / CSS3 / JavaScript (ES5+), with inline SVG graphics
- **Purpose**: Provides the complete web UI for configuring, monitoring, and controlling the OpenTherm Gateway. Enables real-time viewing of OT messages, WiFi/Ethernet management, MQTT settings, system updates, firmware flashing, and SAT thermostat tuning.

---

## File Inventory

### One UI bundle (v2)

The 2.0.0 line ships one web UI: the v2 bundle (`v2.html`, `v2.js`, `v2-bundle.css`). The root path (`/`, plus `/index` and `/index.html` so old bookmarks keep working) always serves `v2.html`. If `/v2.html` is missing from the filesystem, those paths serve `FSexplorer.html` as a recovery page (`FSexplorer.ino`, `startWebserver`).

**The classic UI was removed on 2026-10-15** (maintainer decision, TASK-1223.12, ADR-192). Only v2 fits the 768K LittleFS of the safeboot OTA layout (ADR-191). Removed with it: `index.html`, `index.js`, `graph.js`, `sat.js`, `sat-slider.js`, `theme-toggle.js`, `echarts-theme.js`, `components.css`, `design.html`, `update.png`, `system_update.png`, `download-to-storage-drive.png`, `refresh-page-option.png`, the device-wide UI switch (`settings.ui.bUseV2`, key `ui_usev2`) and the seven classic display preferences (`ui_autoscroll`, `ui_timestamps`, `ui_capture`, `ui_autoscreenshot`, `ui_autodownloadlog`, `ui_autoexport`, `ui_graphtimewindow`). An old `settings.ini` that still holds those keys loads without error; `updateSetting()` ignores unknown keys and the next settings write drops them. The 19 classic features v2 lacked are being ported (TASK-1223.14 to TASK-1223.18, plus follow-ups TASK-1226 to TASK-1232). The 12 features that were dropped, and how each was built, are documented in [`docs/history/2026-10-15-retired-classic-web-ui-features.md`](../history/2026-10-15-retired-classic-web-ui-features.md); the last commit with the classic UI is `3aa1ef927`. The full comparison is in [`docs/research/2026-10-15-classic-vs-v2-gap-audit.md`](../research/2026-10-15-classic-vs-v2-gap-audit.md).

Confirm the current asset list with `git ls-files src/OTGW-firmware/data`.

### Core HTML & Entrypoint

| File | Purpose |
|------|---------|
| `v2.html` | SPA entry point. Pre-paints the theme, then loads `v2-bundle.css`, `v2.js` and the three fonts strictly one after another, each with retry on `503` (TASK-978/989), so a transient refusal from the file-serve gate does not leave the page half-initialised. |
| `FSexplorer.html` | Standalone LittleFS file browser and recovery page (see below). |

### V2 UI Bundle (tabbed SPA)

`v2.html` / `v2.js` / `v2-bundle.css` form a self-contained single-page app. The top-level pages (`data-page` in `v2.html`) are **Home**, **Monitor**, **SAT**, **Settings**, **Advanced** and **Diagnose** (the last is shown only when the PIC runs the diagnose firmware). Monitor and Advanced have sub-tabs (Monitor: log, values, statistics, OT support, graph, connection; Advanced: PIC, debug, file system, system).

The live OT stream comes from the `/ws` WebSocket; all reads and mutations go through the `/api/v2/*` REST API. v2 has no external script dependency: the graph is drawn as inline SVG, not with a chart library.

### Stylesheets

| File | Purpose |
|------|---------|
| `ds-tokens.css` | Design-system tokens: light/dark colour palettes, spacing scale, typography variables. **Build input only**: `build.py` (`generate_v2_css_bundle`) concatenates it with `v2.css` into `v2-bundle.css`. Not in the filesystem image (`STAGING_EXCLUDE`). |
| `v2.css` | Rules for the v2 SPA. **Build input only**, like `ds-tokens.css`. |
| `v2-bundle.css` | Generated: `ds-tokens.css` (with its `@font-face` blocks stripped) + `v2.css`. The only stylesheet `v2.html` loads. |
| `fsexplorer.css` | FSexplorer's own stylesheet; FSexplorer depends on no other sheet (TASK-1223.13). |

Web fonts live under `data/fonts/` (`inter-400.woff2`, `inter-700.woff2`, `jetbrains-mono-400.woff2`). `v2.html` loads them one at a time through the `FontFace()` API, and the firmware serves them with an immutable cache header, so the UI renders without internet access.

### Configuration & Data Files

| File | Size | Purpose |
|------|------|---------|
| `settings.ini` | JSON | Default device settings template (hostname, MQTT broker/port, NTP timezone, OTGWcommands startup). Seed data for first-time setup. |
| `version.hash` | <0.1 KB | Firmware/filesystem version hash for mismatch detection. |
| `otgw_simulation.log` | ~13 KB | OT frames replayed by the simulation mode. |

Note: `mqttha.cfg` and the file-based discovery template approach have been retired. Discovery is now driven by the PROGMEM data tables and streaming functions in `MQTTHaDiscovery.cpp` (ADR-077). `docs/archive/mqttha.cfg` is kept as historical reference only.

### File System Explorer (LittleFS Web UI)

| File | Purpose |
|------|---------|
| `FSexplorer.html` | Standalone file browser for managing LittleFS files (upload, delete, download). Styled by `fsexplorer.css` only. Also served at `/` when `v2.html` is missing. v2 has its own in-page file browser (Advanced > File System). |
| `FSexplorer.png` | Icon/graphic for file explorer UI |

### Assets & Media

| File | Purpose |
|------|---------|
| `favicon.ico` | Favicon for browser tab |
| `settings.png` | Icon for Settings page button |

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
| `transfer.dat` | EEPROM settings-transfer map read during a PIC firmware upgrade (`OTGWUpgrade::eepromSettings`, `src/libraries/OTGWSerial/OTGWSerial.cpp`). Keep it. |
| `version.hash` | Firmware version hash for mismatch detection |

---

## Code Elements

### v2.js structure

`v2.js` is one IIFE. The parts other code or docs depend on:

- **Request queue (ADR-184, TASK-1210)**: the first lines wrap `window.fetch`, so every same-origin `/api/` request waits in one first-in-first-out queue with at most 2 in flight (`REST_MAX_INFLIGHT 2`, ADR-165). Write new calls one at a time anyway; bursts still queue.
- **WebSocket** (`connectWs`): opens `/ws`, reconnects 5 s after a close. Text frames feed the log buffer and the live model; JSON frames are dropped.
- **otmonitor fetcher** (`fetchOtmonitor`, `OTMON_POLL_MS = 2000`): one guarded fetcher for the Home seed, the Monitor > Values table and the PS=1 fallback. A `429` pauses all callers for the `retry_after` the device sends (ADR-172).
- **PS=1 check** (`psCheckTick`, `DEVTIME_POLL_MS = 10000`): asks `device/time` only after 15 s of socket silence, then at most every 10 s while PS=1 is on. `OTMON_POLL_MS` and `DEVTIME_POLL_MS` are read by `evaluate.py::check_poll_window_coupling` against the rate-limit windows in `restAPI.ino`.
- **Graph** (`renderGraph`): inline SVG polylines over a sample buffer (`SAMPLES_MAX = 1800`). The control-setpoint series (`chSet`) takes MsgID 1 only from Write-Data frames, and an `R` frame (the value the gateway sent the boiler) wins over a `T` frame for 30 s (`tsetGatewayMs`), so the graph shows the setpoint the boiler got, not the thermostat's own request (TASK-1271).
- **Settings catalogue** (`SET_CATS`, `SET_META`, enum tables): curated categories and labels; any key the firmware sends that is not curated still shows, under a category picked by prefix. `SET_HIDE_ROW` keeps the onboarding flags (`ui_onboarded`, `sat_onboarded`) off the list.
- **Onboarding wizards** (`maybeShowOnboarding`, `satOnbNeeded`): first-time setup and SAT onboarding, gated on `ui_onboarded` / `sat_onboarded`.
- **File browser** (Advanced > File System): mirrors `FSexplorer.html` against `/api/v2/filesystem/files`; `FS_PROTECTED` withholds Delete on the files the UI needs to load.
- **Debug page** (`fetchDebug`, `renderDebugGroups`): curated device/info groups, then every other field bucketed by prefix.

The SAT status pill has a browser regression test: `tests/webui/sat-status-pill.test.mjs`.

---

## Key Behaviors & Workflows

### Home Page (Main Dashboard)

1. **On load**: fetch `/api/v2/device/info` (PIC vs. OT-Direct, board), open the `/ws` WebSocket, and stagger the first REST reads (otmonitor seed, SAT status, weather) so the device never sees a burst.
2. **Live stream**: each text frame on `/ws` updates the live model (flow, return, setpoints, modulation, flame) and the raw-frame ticker; the strip chart samples the model on change.
3. **Fallbacks**: while the socket is closed, the otmonitor seed is fetched every 20 s; in PS=1 mode (socket open but silent) Monitor > Values polls `otgw/otmonitor` every 2 s.
4. **Monitor tabs**: Log (live frames, pause, stream-to-file), Values (every otmonitor entry with age), Statistics, OT support, Graph, Connection.

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

### Theme Switching (Design-System Tokens)

1. **Storage**: `localStorage` key `otgw-theme` (`'dark'` or `'light'`).
2. **Pre-paint**: an inline script at the top of `v2.html` reads the key (falling back to `prefers-color-scheme`) and sets `data-theme` on `<html>` before the first frame.
3. **Token swap**: `v2-bundle.css` carries both palettes from `ds-tokens.css` as CSS variables, so a switch is a single attribute flip.
4. **Button**: the header toggle (`initTheme` in `v2.js`) flips the attribute, updates its own label and stores the choice.

---

## Dependencies

### Internal Dependencies (ESP32-S3 Firmware)

- **REST API v2**: `/api/v2/` endpoints (see below)
- **WebSocket endpoint**: `/ws` (plain HTTP, port 80)
- **LittleFS served files**: All static assets (HTML, CSS, JS, firmware bins)

### External Dependencies

None. v2 loads no script or stylesheet from a CDN; fonts are served from LittleFS.

### Browser APIs Used

- **WebSocket API**: Real-time OT message streaming
- **Fetch API**: REST calls with error handling
- **LocalStorage**: Persist UI settings, OT log buffer, Dallas labels cache
- **File System Access API**: Writable streams for log file persistence (`/logs/otgw_YYYYMMDD.log`)
- **matchMedia**: Detect dark/light OS theme preference

### REST API Endpoints (Web UI calls)

From the `APIGW + 'v2/...'` calls in `v2.js`; the full route table is in [c4-code-rest-api.md](./c4-code-rest-api.md).

This table predates the removal of the classic UI and has not been re-derived from `v2.js`; `docs/api/openapi.yaml` is authoritative.

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/v2/health`, `/api/v2/device/info`, `/api/v2/device/time`, `/api/v2/device/crashlog` | GET | Liveness, device and network state, clock, crash log |
| `/api/v2/settings` | GET / POST | All persistent settings, one `{name, value}` per POST |
| `/api/v2/otgw/otmonitor`, `/api/v2/otgw/messages/{id}` | GET | OT monitor snapshot (polled every 2 s), single message by ID |
| `/api/v2/otgw/commands` | POST | Send a gateway command (`GW=1`, `TT=21.5`, ...) |
| `/api/v2/otgw/overrides`, `/api/v2/otgw/boiler-support`, `/api/v2/otgw/ot-support`, `/api/v2/otgw/diagnose` | GET / POST | Active overrides, supported MsgIDs, PIC diagnose keystrokes |
| `/api/v2/pic/settings`, `/api/v2/pic/flash-status`, `/api/v2/pic/update-check` | GET | PIC settings readout, PIC flash progress, PIC firmware update check |
| `/api/v2/firmware/files` | GET | PIC firmware images available on LittleFS |
| `/api/v2/otdirect/status`, `/api/v2/otdirect/overrides` | GET / POST | OT-Direct panel state and overrides (only with `HAS_DIRECT_OT`) |
| `/api/v2/sat/status`, `/api/v2/sat/target`, `/api/v2/sat/weather`, `/api/v2/sat/markers`, `/api/v2/sat/ble/*` | GET / POST | SAT dashboard (polled every 5 s while visible), target temperature, weather, calibration markers, BLE roster and discovery |
| `/api/v2/sensors`, `/api/v2/sensors/labels` | GET / POST | Sensor status and Dallas sensor labels |
| `/api/v2/filesystem/files`, `/api/v2/filesystem/hash-check` | GET | File list, delete (`?delete=`), filesystem version check |
| `/api/v2/simulate`, `/api/v2/debug` | GET / POST | OT replay simulation, debug state |
| `/api/v2/webhook/test?state=` | POST | Fire a test webhook |

### MQTT Integration

- HA discovery is published by the firmware from PROGMEM tables in `MQTTHaDiscovery.cpp` (ADR-077); the `mqttha.cfg` file in `data/` no longer exists. ADR-100 defaults to JIT discovery (on first OT message arrival); ADR-101 keeps value topics as flat per-value scalars; ADR-106 publishes self-describing names by default.
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

- v2 was designed phone-first and shows the live log at any width.

### Known Limitations

- **Dark theme scrollbar styling**: Uses `-webkit-scrollbar` CSS (Chrome/Safari/Edge only); Firefox renders default scrollbar
- **Password fields**: Browser auto-fill may not work reliably; manual entry recommended for critical passwords

---

## Theme System

### Token Layer (ds-tokens.css)

Light and dark palettes are defined as CSS custom properties on `:root` and on `[data-theme="dark"]`. Tokens cover surface/background colours, text levels, accent (header / focus), borders, table rows, and graph palette. Themes are pure variable swaps: no separate stylesheet per theme. `ds-tokens.css` is a build input: it reaches the browser only inside `v2-bundle.css`.

### Component Layer (v2.css)

All v2 widgets are styled in `v2.css`, which consumes the token variables. It too reaches the browser only inside `v2-bundle.css`.

---

## File Serving & LittleFS

### How Files Are Served

1. The firmware mounts LittleFS at startup.
2. `python build.py` packs `data/` into the LittleFS image from a staging copy in which `.js`/`.css` are minified and `ds-tokens.css` and `v2.css` are left out (ADR-190, `STAGING_EXCLUDE`). The device serves the minified files; the readable source stays in git.
3. HTTP server routes (`FSexplorer.ino`, `startWebserver`):
   - `/`, `/index`, `/index.html` → `v2.html` (or `FSexplorer.html` when `v2.html` is missing)
   - `/v2.html`, `/v2.js`, `/v2-bundle.css`, `/fsexplorer.css` → the UI assets, ETag-validated
   - `/fonts/*.woff2` → immutable-cached fonts
   - `/pic16f88/*`, `/pic16f1847/*` → PIC firmware binaries and version files
   - `/api/*` → REST endpoints (handled by firmware C++ code)
   - `/ws` → WebSocket endpoint (handled by firmware C++ code)
   - anything else → the `onNotFound` catch-all (`handleFile`); a path ending in `/` gets `v2.html` appended

### Cache & Backpressure Policy

- **`Cache-Control: no-cache` + ETag revalidation (ADR-163, amends ADR-139)**: static assets ship with stable URLs (no `?v=` query versioning) and a `Cache-Control: no-cache` header, so the browser may store an asset but must revalidate it every load via `If-None-Match`. The ETag is the filesystem hash combined with the served path, so each asset has its own validator; an unchanged filesystem returns `304`, an OTA/FS upgrade changes the hash and returns `200` with fresh content on the next load.
- **Client-side request queue (ADR-184)**: the first lines of `v2.js` wrap `window.fetch`. Every same-origin `/api/` request waits in one first-in-first-out queue with at most 2 in flight, which matches the device's REST gate (`REST_MAX_INFLIGHT 2`, ADR-165).
- **503 backpressure gate (ADR-147)**: the LittleFS file-serve path is allocation-bounded and refuses a new file serve under heap pressure or excessive concurrency with a cheap `503`, so abusive concurrency cannot drive the ESP32-S3 out of memory. `v2.html` loads its assets sequentially with retry to survive a transient `503`.

### File Size Constraints

- **ESP32-S3 4MB boards**: LittleFS partition 768 KB in the safeboot OTA layout (ADR-191); the v2-only image is what makes it fit.
- **UI assets**: `v2.html`, `v2.js`, `v2-bundle.css` (minified in the image) plus three fonts.
- **PIC binaries**: about 140 KB on disk in `pic16f1847/` and `pic16f88/` (gateway, interface and diagnose images for both PIC types)

### Build Process

```bash
build.bat                    # Windows: firmware + LittleFS image (build.sh on macOS/Linux)
build.bat --clean            # Clean build
```

The LittleFS image is built from a minified staging copy of `data/` (ADR-190). It is flashed over USB (`flash_otgw --update --fs`) or uploaded on the update page; a filesystem image resets the settings.

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
- No cross-domain requests

---

## Performance Considerations

### Memory Usage (browser)

- **Log buffer**: `LOG_DEF` = 2000 lines by default, `LOG_CAPTURE` = 200000 lines in capture mode, kept in browser memory (`logBuf`); the device stores no OT log file
- **Rendered window**: `LOG_SHOW` = 2000 lines in the DOM
- **Graph**: `SAMPLES_MAX` = 1800 samples

### CPU & Bandwidth

- **WebSocket**: Continuous stream (1-10 msg/sec typical, up to 100+ msg/sec during busy periods)
- **REST polling**: OT monitor every 2 s (`OTMON_POLL_MS`), device time every 10 s (`DEVTIME_POLL_MS`), SAT status every 5 s while the SAT page is visible; all through the 2-in-flight fetch queue (ADR-184)
- **Frontend**: Debounced UI updates (200-500ms) to avoid excessive reflows

### Optimization Techniques

- **CSS containment**: No CSS containment used (not supported by all browsers)
- **Image compression**: PNG icons are minimal size
- **Streaming**: Files served straight from flash (AsyncFileResponse), never buffered whole
- **Caching**: ETag and Cache-Control headers set by firmware

---

## SAT-Specific Architecture

The SAT module is a complete subsystem for thermostat autotune:

- **Visualization-only dashboard**: All logic runs on the device (`SATcontrol.ino`); the web UI shows state and edits settings
- **Real-time feedback**: Dashboard updates every 5 seconds via `/api/v2/sat/status`, only while the SAT page is visible (TASK-986)
- **Heating curve**: computed on the device from `SAT_HC_BASE_OFFSET_RAD` (27.2) or `SAT_HC_BASE_OFFSET_FLOOR` (20.0), `SAT_HC_REF_TEMP` (20.0) and the curve coefficient (see c4-code-sat.md)
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

### PS=1 Mode (Passthrough Serialization)

- When PIC firmware reports PS=1, raw OT frames are not available
- UI displays decoded field summaries instead
- Search/filter features work on summaries, not raw frames
- Banner displayed: "PS=1 mode active: showing decoded field summaries"

### localStorage Quota

- Typical limit: 5-10 MB per domain
- OT log buffer + SAT history + Dallas labels must fit within quota
- Oldest entries auto-purged if quota exceeded
- Clear stored data option available in device info

---

## References & Related Files

### Firmware Components

- **index.ino**: HTTP server setup, REST endpoints, WebSocket handler
- **MQTTstuff.ino** / **MQTTHaDiscovery.cpp**: MQTT HA auto-discovery streaming from PROGMEM tables (replaces former runtime `mqttha.cfg` parsing)
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

The Web Assets module provides a complete, responsive, real-time web UI for the OTGW-firmware ESP32-S3 gateway. It combines:

1. **Real-time monitoring**: WebSocket live OT message stream with graph visualization
2. **System control**: Settings, WiFi, MQTT, NTP configuration
3. **Firmware management**: OTA updates for ESP and PIC microcontrollers
4. **SAT integration**: Smart autotune thermostat dashboard and tuning UI
5. **Home Assistant integration**: MQTT auto-discovery configuration for seamless HA integration
6. **Responsive design**: Desktop, tablet, and mobile support with light/dark theme toggle
7. **Offline resilience**: no CDN; fonts and assets come from LittleFS

The module is served entirely from LittleFS flash storage, with no external dependencies. Code follows modern JavaScript patterns (ES5+, Fetch API, async/await) with safety checks for JSON parsing, DOM access, and error handling throughout.
