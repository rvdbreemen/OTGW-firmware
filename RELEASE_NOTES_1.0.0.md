# Release Notes - v1.0.0

**The "Finally 1.0" Release**

Version 1.0.0 marks a major milestone for the OTGW-firmware, bringing enterprise-grade stability, a completely overhauled user interface, and robust integration features that have been in development and testing for months. This release focuses on reliability, memory safety, and user experience.

## 🚀 Major Features

### 📊 Real-Time Graphing & Statistics
- **Interactive Graphs**: Visualize OpenTherm data in real-time directly in the Web UI. Monitor boiler temperatures, setpoints, and pressure with adjustable time windows.
- **Statistics Dashboard**: New dedicated tab showing session and long-term statistics for your heating system.
- **Responsive Design**: Graphs and charts adapt seamlessly to mobile and desktop screens.

### 🎨 Modern Web Interface
- **Dark Mode**: Fully integrated dark theme that respects your system preferences or can be toggled manually (saves state).
- **Live Log Viewer**: Re-engineered using WebSockets for true real-time streaming (no more polling!). Includes better filtering, pausing, and raw message decoding.
- **File System Explorer**: Completely redesigned file manager with better upload/download/delete capabilities.
- **Responsive Layouts**: Improved settings pages and controls that work better on smartphones.

### 🔌 Connectivity & Integration
- **WebSocket Architecture**: Moved from HTTP polling to WebSockets for live data (logs, status, updates), significantly reducing network overhead and latency.
- **MQTT Auto Discovery**: Enhanced Home Assistant integration with improved stability, reliable reconnections, and cleaner entity naming.
- **Stream Logging**: Ability to stream OpenTherm logs directly to files on the file system for troubleshooting.

### 🛠️ Firmware & Flashing
- **Interactive Flashing Tool**: Improved `flash_esp.py` script for easy firmware updates (download release or build from source).
- **PIC Firmware Upgrade**: Safer and more reliable flashing of the PIC controller directly from the Web UI, with binary data validation preventing crashes.
- **Live Update Progress**: Real-time progress bars and status messages during firmware updates via WebSocket.
- **Settings Preservation**: Smart preservation of configuration during firmware upgrades.

### 🛡️ Stability & Security
- **Memory Safety**: Extensive refactoring to use `PROGMEM` for strings, saving significant RAM (heap) and preventing fragmentation.
- **Heap Protection**: Active monitoring of available memory with adaptive throttling to prevent crashes under load.
- **Watchdog Improvements**: More reliable hardware watchdog integration to recover from hangs.
- **Binary Data Handling**: Fixes for buffer manipulation ensuring safe handling of binary firmware files.

## 📋 Full Changelog

### Added
- **Graphs**: Added ECharts-based real-time graphing feature (`dev-feature-graphs`).
- **Statistics**: Added comprehensive statistics page (`dev-stats-in-browser`).
- **WebSockets**: Implemented WebSocket server for real-time logs and status (`dev-webui-live-log-lines`).
- **Dark Theme**: Integrated dark mode CSS and toggle (`dev-integrated-dark-theme`).
- **Flash Tool**: Added `flash_esp.py` for automated flashing and building.
- **Evaluation**: Added `evaluate.py` for code quality checks (`fc63a60`).
- **ADRs**: Added Architecture Decision Records to `docs/adr/`.
- **Auto Config**: Added auto-configuration capabilities (`13826d7`).
- **Legacy Support**: Added setting for legacy Dallas sensor ID format.

### Changed
- **Dependencies**: Removed dependency on `make` for building; fully integrated `arduino-cli`.
- **Logging**: Switched log viewer to use WebSockets.
- **Memory**: Aggressive optimization of string literals using `F()` and `PSTR()`.
- **Formatting**: Improved log line formatting and decoding.
- **Docs**: Comprehensive updates to README and documentation.

### Fixed
- **PIC Flashing**: Fixed critical crashes during PIC firmware updates due to binary data handling (`835d897`, `2634804`).
- **MQTT**: Fixed buffer fragmentation improvements and reconnection logic.
- **Timezone**: Fixed timezone initialization issues.
- **Security**: Added CSRF protection and better input sanitization.
- **Crashes**: Fixed multiple Exception (2) and Exception (28) causes related to memory access.

### Removed
- **Polling**: Removed legacy HTTP polling for logs in favor of WebSockets.
- **Unused Code**: Cleanup of legacy commented-out code and unused libraries.

## ⚠️ Breaking Changes
- **GPIO Defaults**: Default GPIO for Dallas sensors changed to GPIO 10 to match standard hardware behavior.
- **Configuration**: Some settings might need re-verification due to the new preservation logic (though auto-migration is attempted).

## Acknowledgements
Thank you to all contributors, testers, and the OpenTherm Gateway community for their feedback and support during the release candidate phase.
