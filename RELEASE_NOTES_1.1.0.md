# Release Notes — v1.1.0-beta

**Release date:** 2026-02-15  
**Branch:** `dev`  
**Base:** v1.0.0

---

## Overview

Version 1.1.0-beta builds on the stable v1.0.0 foundation with new Dallas temperature sensor features, improved memory safety, WebUI data persistence, and enhanced developer tooling.

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

---

## Bug Fixes

### MQTT Whitespace Authentication Fix
- **Problem:** Authentication failures after upgrading from v0.10.3 to v1.0.0
- **Root cause:** `strlcpy()` preserves whitespace that Arduino `String` class may have auto-trimmed
- **Fix:** Added automatic `trimwhitespace()` on MQTT username/password in `readSettings()` and `updateSetting()`
- Commit: `eba5d51` (2026-02-10)
- See: [docs/fixes/mqtt-whitespace-auth-fix.md](docs/fixes/mqtt-whitespace-auth-fix.md)

### Streaming File Serving (Memory Management Fix)
- **Problem:** Loading entire `index.html` (11KB+) into RAM with `readString()` caused memory exhaustion
- **Fix:** Replaced with streaming file serving using chunked transfer encoding
- Unified handler using lambda (eliminates code duplication across 3 routes)
- Static caching of filesystem hash
- **Result:** 95% memory reduction for file serving
- Commit: `2e93554` (2026-02-01)
- See: [docs/reviews/2026-02-01_memory-management-bug-fix/](docs/reviews/2026-02-01_memory-management-bug-fix/)

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
- 5 new Architecture Decision Records (ADR-030 through ADR-034)
- New feature docs: Dallas sensors, data persistence
- New guides: browser debug console, release workflow
- 6 code review archives in `docs/reviews/`
- OpenAPI spec for Dallas sensor labels API

---

## Migration Notes

When upgrading from v1.0.0:

1. **Filesystem flash recommended** alongside firmware flash — new Web UI files and Dallas sensor label support require updated LittleFS partition
2. **Hard browser refresh (Ctrl+F5)** recommended to pick up new JavaScript (WebUI persistence, debug console)
3. **MQTT credentials** — whitespace trimming is now automatic on boot; no user action needed
4. **No breaking API changes** — all existing REST API endpoints (`/api/v0/`, `/api/v1/`, `/api/v2/`) remain unchanged; new endpoint added: `GET/POST /api/v1/sensors/labels`
5. **No breaking MQTT changes** — all topics remain the same

---

## Architecture Decision Records (New)

| ADR | Title | Status |
| --- | --- | --- |
| ADR-030 | Heap Memory Monitoring and Emergency Recovery | Accepted |
| ADR-031 | Two-Microcontroller Coordination Architecture | Accepted |
| ADR-032 | No Authentication Pattern / Local Network Security Model | Accepted |
| ADR-033 | Dallas Sensor Custom Labels and Graph Visualization | Accepted |
| ADR-034 | Non-Blocking Modal Dialogs for User Input | Accepted |
