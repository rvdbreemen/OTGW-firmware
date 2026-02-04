# Release Notes - v1.0.0

**The "Finally 1.0" Release**

Version 1.0.0 marks a major milestone for the OTGW-firmware, bringing a modern web experience, major stability work, and a long list of integration improvements. This release focuses on reliability, memory safety, and a smooth day-to-day user experience.

## 🚀 Major Features

### 📊 Real-Time Graphing & Statistics
- **Interactive graphs**: Real-time ECharts visualizations with time windows, extended history buffers, and auto-screenshot support.
- **Statistics dashboard**: Dedicated tab for session and long-term statistics.
- **Responsive layout**: Graphs and controls scale for both desktop and mobile.

### 🎨 Modern Web Interface
- **Dark Mode**: Fully integrated dark theme with persistence and improved accessibility.
- **Live OT Monitor**: Real-time OpenTherm log streaming via WebSockets (port 81), including OTmonitor-compatible export.
- **File System Explorer**: Major UI refresh with better upload, download, directory navigation, and error handling.
- **Settings UX**: Cleaner layout, responsive controls, and improved tab navigation.

### 🧾 Stream Logging & Export
- **Stream to File**: Direct-to-disk logging using the File System Access API (Chrome/Edge/Opera desktop).
- **Auto-download**: Timed log downloads for browsers without filesystem access.
- **CSV export reliability**: Fixes for empty exports and formatting alignment.

### 🛠️ Firmware & Flashing
- **XHR-based OTA**: Simplified firmware/filesystem flashing with `/api/v1/health` reboot verification (no WebSocket progress).
- **Filesystem flashing robustness**: Improved safeguards and clearer status handling.
- **PIC firmware flashing**: Safer binary parsing and crash prevention during upgrades.
- **flash_esp.py**: Interactive flashing tool for download/build/flash workflows.

### 🔌 Connectivity & Integration
- **REST API v2**: Optimized `/api/v2/otgw/otmonitor` endpoint (map-based JSON format).
- **Health endpoint**: `/api/v1/health` for uptime/heap/status checks; used for OTA verification.
- **Device time endpoint**: `/api/v0/devtime` for timezone-aware time reporting.
- **MQTT improvements**: Streaming auto-discovery, outside temperature override, hot water command clarifications, and event topics.
- **Gateway mode detection**: Reliable `PR=M` polling for explicit gateway mode.

## 🛡️ Performance, Reliability & Safety

- **PROGMEM enforcement**: Extensive migration to `F()` and `PSTR()` to reduce heap pressure.
- **Static buffers**: Reduced heap fragmentation and crash risk under load.
- **Heap backpressure**: Adaptive throttling for WebSocket/MQTT to prevent memory exhaustion.
- **WebSocket robustness**: Queue limits, backpressure, Safari fixes, and improved reconnection logic.
- **Binary safety**: `memcmp_P()` for hex/banner parsing to prevent Exception (2) crashes.
- **Watchdog & Wi-Fi**: Improved watchdog feeding and clearer Wi-Fi connection diagnostics.

## 🧰 Tooling & CI/CD

- **build.py**: Local build workflow using `arduino-cli` (no `make` requirement).
- **evaluate.py**: Automated code-quality analysis and memory checks.
- **autoinc-semver.py**: Version bump automation, hash updates, and CI integration.
- **Release workflow**: Streamlined asset publishing and verification steps.

## 📚 Documentation

- **ADR library**: 29 ADRs documenting architectural decisions.
- **Guides**: Build, flash, WebSocket, Safari, and stream logging documentation.
- **API references**: v1/v2 API change summaries and MQTT command examples.

## ⚠️ Breaking Changes & Behavioral Changes

- **Dallas sensors**: Default GPIO changed from GPIO 13 (D7) to **GPIO 10 (SD3)** and sensor IDs now use the full 16-char format (legacy mode available).
- **OTA flow**: WebSocket flash progress removed in favor of XHR + health-check verification.
- **PIC firmware checks**: Automatic periodic checks disabled; manual checks only.
- **FS Explorer**: LittleFS format endpoint removed from the UI.

## Acknowledgements

Thank you to all contributors, testers, and the OpenTherm Gateway community for the feedback, bug reports, and release candidate testing that made 1.0.0 possible.
