# Release Notes — v1.1.0

**Release date:** 2026-02-25
**Branch:** `dev` → `main`
**Base:** v1.0.0

---

## Overview

Version 1.1.0 builds on the stable v1.0.0 foundation and delivers significant improvements across every layer of the firmware: new Dallas temperature sensor features with graph visualization, a complete RESTful API v2 with full frontend migration, PS mode compatibility for Domoticz, WebUI data persistence, improved serial handling, enhanced diagnostic logging, and 20 resolved bugs from a comprehensive codebase review.

---

## ⚠️ Breaking Changes

There are **no breaking changes to REST API endpoints, MQTT topics, or on-device settings** in v1.1.0 relative to v1.0.0.
However, there is a **minor breaking change for custom REST API consumers** due to a response field rename (see the Migration Notes section below).

- All existing REST API endpoints (`/api/v0/`, `/api/v1/`, `/api/v2/`) remain functional (no paths removed or primary semantics changed).
- All MQTT topics are unchanged.
- All settings are preserved on upgrade.

**API Deprecation Notice (not yet removed):** The following endpoints are deprecated and scheduled for removal in v1.3.0:

| Deprecated endpoint | v2 Replacement |
|---------------------|----------------|
| `GET /api/v0/devinfo` | `GET /api/v2/device/info` |
| `GET /api/v0/devtime` | `GET /api/v2/device/time` |
| `GET/POST /api/v0/settings` | `GET/POST /api/v2/settings` |
| `GET /api/v0/otgw/{msgid}` | `GET /api/v2/otgw/messages/{msgid}` |
| `GET /api/firmwarefilelist` | `GET /api/v2/firmware/files` |
| `GET /api/listfiles` | `GET /api/v2/filesystem/files` |

See [ADR-035](docs/adr/ADR-035-restful-api-compliance-strategy.md) for the full migration guide.

---

## New Features

### Dallas Sensor Custom Labels

- Inline non-blocking sensor label editor in Web UI — click a sensor name to edit it in-place
- Labels stored in `/dallas_labels.ini` on LittleFS with zero backend RAM usage
- Maximum 16 characters per label
- Automatic label backup to browser during filesystem flash and auto-restore after reboot
- See: [docs/features/dallas-temperature-sensors.md](docs/features/dallas-temperature-sensors.md)

### Dallas Sensor Graph Visualization

- DS18x20 sensors automatically appear in the real-time temperature graph with a 16-color palette
- Full support for both light and dark themes
- Sensor labels (when set) displayed in graph legend instead of raw hardware addresses
- See: [docs/TEMPERATURE_SENSOR_GRAPH_IMPLEMENTATION.md](docs/TEMPERATURE_SENSOR_GRAPH_IMPLEMENTATION.md)

### Dallas Sensor REST API

- `GET /api/v2/sensors/labels` — retrieve all sensor labels as a JSON map
- `POST /api/v2/sensors/labels` — update sensor labels in bulk (read-modify-write pattern)
- Aliases available at `/api/v1/sensors/labels` for backward compatibility
- See: [docs/DALLAS_SENSOR_LABELS_API.md](docs/DALLAS_SENSOR_LABELS_API.md)

### WebUI Data Persistence

- Automatic log data persistence to `localStorage` with debounced 2-second saves
- Dynamic memory management — calculates optimal buffer size based on browser resources
- **Normal mode**: regular operation with rolling log buffer
- **Capture mode**: maximizes data collection for diagnostic sessions
- Auto-restoration of log buffer and user preferences on page load
- Graceful error handling for storage quota exceeded, corrupted data, or missing localStorage
- Log buffer automatically cleared after a firmware/filesystem flash to ensure a clean post-flash view
- See: [docs/features/data-persistence.md](docs/features/data-persistence.md)

### Browser Debug Console (`otgwDebug`)

- Full diagnostic toolkit accessible from the browser's JavaScript console
- Commands: `status()`, `info()`, `settings()`, `wsStatus()`, `wsReconnect()`, `otmonitor()`, `logs()`, `api()`, `health()`, `sendCmd()`, `exportLogs()`, `exportData()`, `persistence()`
- See: [docs/guides/browser-debug-console.md](docs/guides/browser-debug-console.md)

### Non-Blocking Modal Dialogs

- Custom HTML/CSS modal dialogs replace blocking browser `prompt()` and `alert()` calls
- WebSocket data flow continues uninterrupted while a modal is open
- Used for Dallas sensor label editing (and available for future UI interactions)

### PS Mode (Print Summary) Detection

- Automatic detection of `PS=1` mode from the OTGW PIC controller
- When `PS=1` is active:
  - Hides the OT log section in the Web UI
  - Disables WebSocket OT message streaming
  - Suppresses automatic time-sync commands (to avoid interfering with PS output)
  - Shows a notification banner in the UI
- Clean exit: re-enables OT monitor and WebSocket streaming when `PS=0` is detected
- WebSocket events are now emitted when PS mode changes, so connected clients update immediately
- Improves compatibility with legacy integrations such as Domoticz that require `PS=1` mode

### Gateway Mode Overhaul

- Complete refactor of gateway mode detection and display logic
- REST API field renamed from `mode` to `otgwmode` for clarity (the old field name was ambiguous)
- Frontend migrated to the new field name throughout `index.js` and `restAPI.ino`
- Gateway mode status text improved for clarity (e.g., "Monitor" / "Gateway" / "Unknown")
- Polling limited to once per minute (enforced in both firmware and UI) to prevent excessive serial traffic to the PIC

### RESTful API v2 — Complete Implementation

- **13 new v2 endpoints** with full RESTful compliance (API compliance score: 5.4/10 → 8.5/10)
- Consistent JSON error responses: `{"error":{"status":N,"message":"..."}}`
- Proper HTTP status codes: 202 Accepted for async operations (`commands`, `discovery`), 400/404/405/413 for errors
- RFC 7231 §6.5.5 `Allow` header on all 405 responses (v1 and v2)
- CORS support: `Access-Control-Allow-Origin: *` on all v2 responses + OPTIONS preflight (204 No Content)
- RESTful resource naming: `messages/{id}`, `commands` (body-based), `discovery`, `device/info`, `device/time`
- `POST /api/v2/otgw/commands` accepts JSON body (`{"command":"TT=20.5"}`) or plain text
- `GET /api/v2/device/info` returns map-format device information (fixes a frontend bug where `v1/devinfo` didn't exist)
- Versioned replacements for unversioned endpoints: `GET /api/v2/firmware/files`, `GET /api/v2/filesystem/files`
- Backward-compatible aliases for smooth migration: `/otgw/id/`, `/otgw/label/`, `/otgw/command/`
- JSON 404 responses for all `/api/*` routes (HTML 404 for non-API routes)
- Full OpenAPI 3.0 specification: [docs/api/openapi.yaml](docs/api/openapi.yaml)
- See: [ADR-035](docs/adr/ADR-035-restful-api-compliance-strategy.md), [API Documentation](docs/api/README.md)

### Frontend Migration to v2 API

- All frontend API calls migrated from v0/v1/unversioned to v2 — zero legacy calls remain in `index.js`
- Response parsing updated from array-based to map-based format
- OTmonitor refresh interval improved from 5s to 1s for more responsive UI
- Temperature graph processing simplified (removed unnecessary visibility check)
- Gateway mode detection improved to handle both string and boolean values
- DOM element null checks added before event listener registration
- Safe JSON parsing with error recovery added throughout

---

## Bug Fixes

### MQTT Whitespace Authentication Fix

- **Problem:** Authentication failures after upgrading from v0.10.3 to v1.0.0
- **Root cause:** `strlcpy()` preserves whitespace that the Arduino `String` class previously auto-trimmed; users copying credentials from text editors introduced invisible trailing spaces
- **Fix:** Automatic `trimwhitespace()` now applied to MQTT username and password in both `readSettings()` (on boot) and `updateSetting()` (on change) — no user action needed
- Commit: `eba5d51` (2026-02-10)
- See: [docs/fixes/mqtt-whitespace-auth-fix.md](docs/fixes/mqtt-whitespace-auth-fix.md)

### Streaming File Serving (Memory Management Fix)

- **Problem:** Loading the full `index.html` (11 KB+) into RAM using `readString()` caused heap exhaustion and a slow, unresponsive Web UI on v1.0.0
- **Fix:** Replaced with streaming file serving using chunked transfer encoding; unified handler via lambda eliminates code duplication across 3 routes
- Version-aware caching with proper `Cache-Control` headers; filesystem hash cached statically
- **Result:** 95% reduction in RAM used for file serving; UI is fast and responsive
- Commit: `2e93554` (2026-02-01)
- See: [docs/reviews/2026-02-01_memory-management-bug-fix/](docs/reviews/2026-02-01_memory-management-bug-fix/)

### Settings Persistence Fix

- **Problem:** Settings appeared editable in the Web UI but reverted to default values after saving
- **Root cause:** Manual string-split parsing broke on special characters; the deferred save timer could be lost if the device rebooted before the timer fired
- **Fix:** Replaced manual parsing with proper `ArduinoJson` deserialization; added synchronous `flushSettings()` before HTTP 200 response so settings are guaranteed written to flash before the client receives confirmation
- Case-insensitive field matching via `strcasecmp_P()` ensures frontend field names map correctly to backend variables

### Serial Buffer Expansion and Overflow Handling

- **Problem:** Serial input buffer was too small for some burst scenarios; on overflow the firmware could process corrupt, partial OpenTherm lines
- **Fix:** Increased `MAX_BUFFER_READ` to 512 bytes; overflow handling now discards the incomplete line entirely rather than attempting to process it
- Dropped line events are now tracked and logged for diagnostics
- Commit: `edcc2d5`, `9853fcc`

### Dark Mode PIC Firmware Icons

- **Problem:** Black PNG icons (`update.png`, `system_update.png`) were invisible against a dark background in dark mode
- **Fix:** Added `filter: invert(1)` to `.firmware-icon` class in the dark mode CSS, turning the icons white — consistent with existing dark mode treatment of navigation icons (`.nav-img`)

### Codebase Review Fixes — 20 Findings Resolved

A comprehensive review of all `.ino`, `.h`, and `.cpp` files identified and resolved 20 bugs across multiple categories.
Full details: [docs/reviews/2026-02-13_codebase-review/CODEBASE_REVIEW.md](docs/reviews/2026-02-13_codebase-review/CODEBASE_REVIEW.md)

**Critical & High Priority (13 findings):**

| # | File | Finding | Fix |
|---|------|---------|-----|
| 1 | `OTGW-Core.h` | Out-of-bounds array write: `msglastupdated[255]` only valid for IDs 0–254; message ID 255 caused memory corruption | Changed array size to `[256]` |
| 2 | `OTGW-Core.ino` | Wrong MQTT hour bitmask: mask `0x0F` truncated hours 16–23, corrupting night setpoint schedules | Fixed to `0x1F` |
| 3 | `OTGW-Core.ino` | `is_value_valid()` used global `OTdata` instead of the passed parameter — result always based on wrong data | Fixed to use parameter |
| 4 | `OTGW-Core.ino` | PIC version string: `sizeof()` included null terminator, causing one-byte off-by-one in comparison | Fixed with `sizeof()-1` |
| 5 | `versionStuff.ino` | Stack buffer overflow in hex parser: could write beyond the 256-byte buffer | Added bounds check |
| 6 | `s0PulseCount.ino` | ISR race conditions: TOCTOU races, missing `volatile`, `uint8_t` overflow on high pulse rates | Fixed with critical sections + `uint16_t` counter |
| 7 | `restAPI.ino` | Reflected XSS: request URI injected into HTML error page without escaping | Added HTML entity escaping |
| 8 | `outputs_ext.ino` | GPIO outputs feature gated by debug flag — feature was completely non-functional in production builds | Restructured to always run |
| 9 | `MQTTstuff.ino` | Null pointer crash: missing `strtok()` null checks in MQTT callback with malformed topics | Added null guards throughout |
| 10 | `settingStuff.ino` | File descriptor leak: file opened before existence check, leaked on missing file | Reordered to check existence first |
| 11 | `helperStuff.ino` | Year overflow: year stored in `int8_t`, which overflows at year 2128 (and cannot represent 2026 correctly) | Changed to `int16_t` |
| 12 | `sensors_ext.ino` | Blocking sensor read: 750 ms blocking wait during DS18B20 conversion froze the ESP8266 | Switched to async non-blocking mode |
| —  | `OTGW-Core.ino` | Finding #16 retracted: OTGW protocol intentionally uses `ETX=0x04` (non-standard) — not a bug | No change required |

**Medium Priority (7 findings):**

| # | File | Finding | Fix |
|---|------|---------|-----|
| 23 | `settingStuff.ino` | Settings flash wear: 20 separate flash writes per save operation, accelerating flash wear | Deferred to single write with 2s debounce; bitmask side effects also fixed (commit `86fc6d0`) |
| 24 | `OTGW-Core.ino` | HTTP client leak: `http.end()` only called on success path, leaking on errors | Made unconditional in `finally`-style pattern |
| 27 | `settingStuff.ino` | Missing MQTT port fallback for empty setting | Added `| default` fallback |
| 28 | `settingStuff.ino` | GPIO conflict detection missing — two features could silently share the same GPIO pin | Added `checkGPIOConflict()` with warn-on-conflict |
| 29 | `versionStuff.ino` | Macro safety: `byteswap` macro lacked parentheses around parameter | Added parentheses |
| 40 | `sensors_ext.ino` | Disconnected sensor published: `-127°C` (DS18B20 error sentinel) published to MQTT | Added `DEVICE_DISCONNECTED_C` guard to suppress publishing |
| — | `settingStuff.ino` | Dead code: admin password field stored in settings but never validated or checked | Removed entirely |
| — | `restAPI.ino` | Manual JSON parsing for settings POST was fragile (string-split on `=`) | Replaced with `ArduinoJson` |

---

## Improvements

### Enhanced Diagnostic Logging

- **WebSocket event logging for OTGW commands and responses**: all commands sent to the PIC and their responses are now emitted as WebSocket events, visible in the live log
- **PS mode change events**: when the firmware detects a transition to or from `PS=1`, a WebSocket event is broadcast so all connected browser tabs update immediately
- **Serial buffer overflow events**: if the serial input buffer overflows, a WebSocket event is emitted so the issue is visible in the UI rather than silently dropped
- **Dropped line counter**: `handleOTGW()` now tracks and logs the number of lines discarded due to serial buffer overflows
- **Time command logging**: improved logging of NTP time sync commands with timestamps

### Heap Memory Monitoring and Emergency Recovery

- 4-level health system: CRITICAL (<3 KB), WARNING (3–5 KB), LOW (5–8 KB), HEALTHY (>8 KB)
- Adaptive throttling reduces WebSocket and MQTT traffic under memory pressure
- Active WebSocket backpressure control prevents the ESP8266 from running out of heap under sustained load
- See: [ADR-030](docs/adr/ADR-030-heap-memory-monitoring.md)

### Memory Optimizations

- `getOTGWValue()` refactored to eliminate `String` allocations (uses C-string buffers instead)
- Wi-Fi status and MAC address functions refactored away from `String` concatenation
- `PROGMEM` usage extended to reduce RAM usage for string literals
- Static MQTT buffer (1350 bytes) retained to prevent heap fragmentation

### Build System

- `version.hash` is now always generated during build (previously required a pre-existing file)
- Centralized build configuration via `config.py` module
- Reusable GitHub Actions composite actions for setup and build steps
- Automated release workflow publishing `.elf`, `.bin`, and `.littlefs.bin` artifacts
- Retry logic added to ESP8266 platform install for CI reliability
- Build artifacts now assembled in a temporary directory to avoid polluting the source tree

### CI/CD

- New ADR Compliance workflow (`.github/workflows/adr-compliance.yml`) — checks PRs for architectural decision compliance
- Validates ADR references in changed files
- Detects architectural file changes and suggests ADR creation

### Frontend & UI

- Refined editor styles for settings input fields
- Fixed log auto-scroll behavior (scrolls to bottom only when already near the bottom)
- OTmonitor data refresh interval improved from 5s to 1s
- DOM element null checks added before event listener registration to prevent JS errors on partial page loads
- Safe JSON parsing with error recovery added for all REST API responses

### Documentation

- 6 new Architecture Decision Records (ADR-030 through ADR-035):
  - [ADR-030](docs/adr/ADR-030-heap-memory-monitoring.md): Heap Memory Monitoring and Emergency Recovery
  - [ADR-031](docs/adr/ADR-031-two-microcontroller-coordination.md): Two-Microcontroller Coordination Architecture
  - [ADR-032](docs/adr/ADR-032-no-authentication-pattern.md): No Authentication Pattern / Local Network Security Model
  - [ADR-033](docs/adr/ADR-033-dallas-sensor-labels.md): Dallas Sensor Custom Labels and Graph Visualization
  - [ADR-034](docs/adr/ADR-034-non-blocking-modal-dialogs.md): Non-Blocking Modal Dialogs for User Input
  - [ADR-035](docs/adr/ADR-035-restful-api-compliance-strategy.md): RESTful API Compliance Strategy
- Comprehensive codebase review archive with all 20 findings: [docs/reviews/2026-02-13_codebase-review/](docs/reviews/2026-02-13_codebase-review/)
- REST API evaluation and improvement plan: [docs/reviews/2026-02-16_restful-api-evaluation/](docs/reviews/2026-02-16_restful-api-evaluation/)
- Full OpenAPI 3.0 specification updated for all v2 endpoints: [docs/api/openapi.yaml](docs/api/openapi.yaml)
- Updated API reference documentation: [docs/api/README.md](docs/api/README.md)
- New feature docs: Dallas sensors, data persistence, gateway mode
- OpenTherm v4.2 specification converted to searchable Markdown: [docs/opentherm specification/](docs/opentherm%20specification/))

---

## Migration Notes

When upgrading from v1.0.0 to v1.1.0:

1. **Flash both firmware and filesystem** — new Web UI files (Dallas label editor, debug console, PS mode UI, gateway mode improvements) and the Dallas sensor label file require an updated LittleFS partition
2. **Hard browser refresh (Ctrl+F5)** — pick up new JavaScript (WebUI persistence, `otgwDebug` console, PS mode, gateway mode refactor); old cached JS will cause display issues
3. **MQTT credentials** — whitespace trimming is now automatic on boot; no user action required
4. **Settings** — settings persistence is now reliable; saved values are written to flash synchronously before HTTP confirmation
5. **No breaking API changes** — all existing REST API endpoints remain functional
6. **No breaking MQTT changes** — all topics remain unchanged
7. **v0 and unversioned endpoints deprecated** — still functional but scheduled for removal in v1.3.0; migrate to v2 (see deprecation table above)
8. **`otgwmode` field** — the gateway mode field in the REST API response was renamed from `mode` to `otgwmode`; update any custom integrations that read this field directly from the API (the Web UI has been updated automatically)

---

## Architecture Decision Records (New in v1.1.0)

| ADR | Title | Status |
| --- | --- | --- |
| ADR-030 | Heap Memory Monitoring and Emergency Recovery | Accepted |
| ADR-031 | Two-Microcontroller Coordination Architecture | Accepted |
| ADR-032 | No Authentication Pattern / Local Network Security Model | Accepted |
| ADR-033 | Dallas Sensor Custom Labels and Graph Visualization | Accepted |
| ADR-034 | Non-Blocking Modal Dialogs for User Input | Accepted |
| ADR-035 | RESTful API Compliance Strategy | Accepted |
