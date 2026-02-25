# Release Notes — v1.1.0-beta

**Release date:** 2026-02-16  
**Branch:** `dev`  
**Base:** v1.0.0

---

## Overview

Version 1.1.0-beta builds on the stable v1.0.0 foundation with new Dallas temperature sensor features, PS mode detection, improved memory safety, WebUI data persistence, a complete RESTful API v2, and 20 bug fixes from a comprehensive codebase review.

---

## New Features

### Dallas Sensor Custom Labels
- Inline non-blocking sensor label editor in Web UI (click sensor name to edit in-place)
- Labels stored in `/dallas_labels.ini` on LittleFS with zero backend RAM usage
- Maximum 16 characters per label
- Automatic label backup to browser during filesystem flash, auto-restore after reboot
- See: [docs/features/dallas-temperature-sensors.md](docs/features/dallas-temperature-sensors.md)

### Dallas Sensor Graph Visualization
- Sensors auto-appear in real-time graph with 16-color palette
- Full support for both light and dark themes
- See: [docs/TEMPERATURE_SENSOR_GRAPH_IMPLEMENTATION.md](docs/TEMPERATURE_SENSOR_GRAPH_IMPLEMENTATION.md)

### Dallas Sensor REST API
- New bulk endpoint: `GET /api/v1/sensors/labels` — retrieve all sensor labels
- New bulk endpoint: `POST /api/v1/sensors/labels` — update sensor labels
- See: [docs/DALLAS_SENSOR_LABELS_API.md](docs/DALLAS_SENSOR_LABELS_API.md)

### WebUI Data Persistence
- Automatic log data persistence to `localStorage` with debounced 2-second saves
- Dynamic memory management — calculates optimal buffer size based on browser resources
- Normal mode vs Capture mode (maximizes data collection for diagnostics)
- Auto-restoration of log buffer and preferences on page load
- Graceful error handling (quota exceeded, corrupted data, no localStorage)
- See: [docs/features/data-persistence.md](docs/features/data-persistence.md)

### Browser Debug Console (`otgwDebug`)
- Full diagnostic toolkit accessible from browser's JavaScript console
- Commands: `status()`, `info()`, `settings()`, `wsStatus()`, `wsReconnect()`, `otmonitor()`, `logs()`, `api()`, `health()`, `sendCmd()`, `exportLogs()`, `exportData()`, `persistence()`
- See: [docs/guides/browser-debug-console.md](docs/guides/browser-debug-console.md)

### Non-Blocking Modal Dialogs
- Custom HTML/CSS modal dialogs replace blocking `prompt()` / `alert()`
- Maintains real-time WebSocket data flow during user input
- Used for Dallas sensor label editing

### PS Mode (Print Summary) Detection
- Automatic detection of `PS=1` mode from the OTGW PIC controller
- When PS=1 is active: hides OT log section, disables WebSocket streaming, suppresses time-sync commands
- Improves compatibility with legacy integrations (e.g. Domoticz) that require PS=1 mode
- UI shows notification when PS=1 mode is active
- Clean exit: re-enables OT monitor and WebSocket when PS=0 is detected

### Gateway Mode Polling Throttle
- Gateway mode query (`PR=M`) now limited to once per minute maximum
- Prevents excessive serial traffic to the PIC controller
- Enforced in both firmware and UI

### RESTful API v2 — Complete Implementation
- **13 new v2 endpoints** with full RESTful compliance (score improved from 5.4/10 → 8.5/10)
- Consistent JSON error responses: `{"error":{"status":N,"message":"..."}}`
- Proper HTTP status codes: 202 Accepted for async operations (`commands`, `discovery`), 400/404/405/413 for errors
- RFC 7231 §6.5.5 `Allow` header on all 405 responses (v1 and v2)
- CORS support: `Access-Control-Allow-Origin: *` on all v2 responses + OPTIONS preflight (204 No Content)
- RESTful resource naming: `messages/{id}`, `commands` (body-based), `discovery`, `device/info`, `device/time`
- New `POST /api/v2/otgw/commands` accepts JSON body (`{"command":"TT=20.5"}`) or plain text
- New `GET /api/v2/device/info` returns map-format device information (fixes frontend bug where `v1/devinfo` didn't exist)
- Versioned replacements for unversioned endpoints: `GET /api/v2/firmware/files`, `GET /api/v2/filesystem/files`
- Backward-compatible aliases for smooth migration: `/otgw/id/`, `/otgw/label/`, `/otgw/command/`
- JSON 404 responses for all `/api/*` routes (HTML 404 for non-API routes)
- Full OpenAPI specification updated for all v2 endpoints
- See: [ADR-035](docs/adr/ADR-035-restful-api-compliance-strategy.md), [API Documentation](docs/api/README.md)

### Frontend Migration to v2 API
- **All frontend API calls migrated from v0/v1/unversioned to v2** — zero legacy calls remain in `index.js`
- Response parsing updated from array-based to map-based format (cleaner, more efficient)
- OTmonitor refresh interval improved from 5s to 1s for more responsive UI
- Temperature graph processing simplified (removed unnecessary visibility check)
- Gateway mode detection improved (handles boolean values)

### API Deprecation Notice
- **v0 endpoints** (`/api/v0/*`) deprecated — removal planned for v1.3.0
- **Unversioned endpoints** (`/api/firmwarefilelist`, `/api/listfiles`) deprecated — use v2 equivalents
- Migration table provided in README

---

## Bug Fixes

### MQTT Whitespace Authentication Fix
- **Problem:** Authentication failures after upgrading from v0.10.3 to v1.0.0
- **Root cause:** `strlcpy()` preserves whitespace that Arduino `String` class may have auto-trimmed
- **Fix:** Added automatic `trimwhitespace()` on MQTT username/password in `readSettings()` and `updateSetting()`
- Commit: `eba5d51` (2026-02-10)
- See: [docs/fixes/mqtt-whitespace-auth-fix.md](docs/fixes/mqtt-whitespace-auth-fix.md)

### Streaming File Serving (Memory Management Fix)
- **Problem:** Loading entire `index.html` (11KB+) into RAM with `readString()` caused memory exhaustion and slow UI
- **Fix:** Replaced with streaming file serving using chunked transfer encoding
- Unified handler using lambda (eliminates code duplication across 3 routes)
- Version-aware caching with proper `Cache-Control` headers
- Static caching of filesystem hash
- **Result:** 95% memory reduction for file serving; UI is fast and responsive
- Commit: `2e93554` (2026-02-01)
- See: [docs/reviews/2026-02-01_memory-management-bug-fix/](docs/reviews/2026-02-01_memory-management-bug-fix/)

### Settings Persistence Fix
- **Problem:** Settings appeared editable in the Web UI but reverted to default values after saving
- **Root cause:** Manual string-split parsing broke on special characters; deferred timer meant flash write could be lost on reboot
- **Fix:** Replaced manual parsing with proper `ArduinoJson` deserialization; added synchronous `flushSettings()` before sending HTTP 200 response
- Case-insensitive field matching via `strcasecmp_P()` ensures frontend field names map correctly to backend variables

### Dark Mode PIC Firmware Icons
- **Problem:** Black PNG icons (update.png, system_update.png) invisible against dark background in dark mode
- **Fix:** Added `filter: invert(1)` to `.firmware-icon` class in dark mode CSS, turning icons white
- Consistent with existing dark mode treatment of navigation icons (`.nav-img`)

### UI Refinements
- Refined editor styles for settings fields
- Fixed log auto-scroll behavior
- OTmonitor refresh interval improved from 5s to 1s for more responsive UI
- Temperature graph processing simplified (removed unnecessary visibility check)
- Gateway mode detection improved (handles boolean values)

### Codebase Review Fixes (20 findings resolved)

A comprehensive review of all `.ino`, `.h`, and `.cpp` files identified and resolved 20 bugs across multiple categories.
Full details: [docs/reviews/2026-02-13_codebase-review/CODEBASE_REVIEW.md](docs/reviews/2026-02-13_codebase-review/CODEBASE_REVIEW.md)

**Critical & High Priority (13 findings):**
- **Out-of-bounds array write** (`OTGW-Core.h`): `msglastupdated[255]` only indexed 0–254; message ID 255 caused memory corruption — fixed to `[256]`
- **Wrong MQTT hour bitmask** (`OTGW-Core.ino`): Mask `0x0F` truncated hours 16–23 — fixed to `0x1F`
- **Global vs parameter reference** (`OTGW-Core.ino`): `is_value_valid()` used global `OTdata` instead of parameter — fixed
- **PIC version off-by-one** (`OTGW-Core.ino`): `sizeof()` included null — fixed with `sizeof()-1`
- **Stack buffer overflow** (`versionStuff.ino`): Hex parser could write beyond 256-byte buffer — added bounds check
- **ISR race conditions** (`s0PulseCount.ino`): Pulse counter had TOCTOU races, missing volatile, and `uint8_t` overflow — fixed with critical sections + `uint16_t`
- **Reflected XSS** (`restAPI.ino`): URI injected into HTML without escaping — added HTML entity escaping
- **GPIO outputs broken** (`outputs_ext.ino`): Feature gated by debug flag — restructured to always run
- **Null pointer crash** (`MQTTstuff.ino`): Missing `strtok()` null checks in callback — added null guards
- **File descriptor leak** (`settingStuff.ino`): File opened before existence check — reordered
- **Year overflow** (`helperStuff.ino`): Year 2026 in `int8_t` — changed to `int16_t`
- **Blocking sensor read** (`sensors_ext.ino`): 750ms blocking call — switched to async mode
- Finding #16 retracted: OTGW protocol correctly uses non-standard `ETX=0x04`

**Medium Priority (7 findings):**
- **Settings flash wear** (`settingStuff.ino`): 20 flash writes per save — deferred to 1 write with 2s debounce + bitmask side effects (commit `86fc6d0`)
- **HTTP client leak** (`OTGW-Core.ino`): `http.end()` only on success — made unconditional
- **MQTT port default** (`settingStuff.ino`): Missing fallback for port setting — added `| default`
- **GPIO conflict detection** (`settingStuff.ino`): No validation — added `checkGPIOConflict()` warn-on-conflict
- **Macro safety** (`versionStuff.ino`): `byteswap` lacked parameter parentheses — added
- **Disconnected sensor** (`sensors_ext.ino`): -127°C published to MQTT — added `DEVICE_DISCONNECTED_C` filter
- **Dead admin password** (`settingStuff.ino`): Never persisted or checked — removed entirely
- **Manual JSON parsing** (`restAPI.ino`): String-split parsing — replaced with `ArduinoJson`

---

## Improvements

### Heap Memory Monitoring and Emergency Recovery
- 4-level health system: CRITICAL (<3KB), WARNING (3-5KB), LOW (5-8KB), HEALTHY (>8KB)
- Adaptive throttling to prevent crashes under memory pressure
- Active WebSocket backpressure control
- See: ADR-030

### Build System
- `version.hash` is now always generated during build (previously required pre-existing file)
- Centralized build configuration via `config.py` module
- Reusable GitHub Actions composite actions for setup and build steps
- Automated release workflow publishing `.elf`, `.bin`, `.littlefs.bin` artifacts

### CI/CD
- New ADR Compliance workflow (`.github/workflows/adr-compliance.yml`) — checks PRs for architectural decision compliance
- Validates ADR references in changed files
- Detects architectural file changes and suggests ADRs

### Documentation
- 6 new Architecture Decision Records (ADR-030 through ADR-035)
- Comprehensive codebase review archive: [docs/reviews/2026-02-13_codebase-review/](docs/reviews/2026-02-13_codebase-review/)
- REST API evaluation and improvement plan: [docs/reviews/2026-02-16_restful-api-evaluation/](docs/reviews/2026-02-16_restful-api-evaluation/)
- New feature docs: Dallas sensors, data persistence
- New guides: browser debug console, release workflow
- 7 code review archives in `docs/reviews/`
- Full OpenAPI specification updated for all v2 endpoints: [docs/api/openapi.yaml](docs/api/openapi.yaml)
- Updated API documentation: [docs/api/README.md](docs/api/README.md)

---

## Migration Notes

When upgrading from v1.0.0:

1. **Filesystem flash recommended** alongside firmware flash — new Web UI files and Dallas sensor label support require updated LittleFS partition
2. **Hard browser refresh (Ctrl+F5)** recommended to pick up new JavaScript (WebUI persistence, debug console, PS mode)
3. **MQTT credentials** — whitespace trimming is now automatic on boot; no user action needed
4. **Settings** — settings persistence is now reliable; saved settings are written to flash before HTTP confirmation
5. **No breaking API changes** — all existing REST API endpoints (`/api/v0/`, `/api/v1/`, `/api/v2/`) remain unchanged
6. **v0 and unversioned endpoints deprecated** — still functional but scheduled for removal in v1.3.0; migrate to v2 endpoints (see migration table in README)
7. **No breaking MQTT changes** — all topics remain the same

---

## Architecture Decision Records (New)

| ADR | Title | Status |
| --- | --- | --- |
| ADR-030 | Heap Memory Monitoring and Emergency Recovery | Accepted |
| ADR-031 | Two-Microcontroller Coordination Architecture | Accepted |
| ADR-032 | No Authentication Pattern / Local Network Security Model | Accepted |
| ADR-033 | Dallas Sensor Custom Labels and Graph Visualization | Accepted |
| ADR-034 | Non-Blocking Modal Dialogs for User Input | Accepted |
| ADR-035 | RESTful API Compliance Strategy | Accepted |
