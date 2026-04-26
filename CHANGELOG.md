# Changelog

All notable changes to OTGW-firmware (the ESP8266 firmware for the NodoShop OpenTherm Gateway) are documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html).

For full release notes per version, see the matching `RELEASE_NOTES_<version>.md` file. Current release notes live at the repository root; previous release notes are archived in [`docs/releases/`](docs/releases/).

The separate ESP32 / SAT v2.0.0 exploration on `feature-dev-2.0.0-otgw32-esp32-sat-support` is tracked outside this changelog; it targets a different platform and lifecycle.

## [Unreleased]

_No unreleased changes yet. New work on `dev` since v1.5.0-beta lands here._

## [1.5.0-beta] - 2026-04-26

LTS line on Arduino Core 2.7.4. Carries forward the v1.4.x feature set on the proven, conservative Core version. Status: beta, in active development on `dev`.

### Added
- LTS line `1.5.x` on Arduino Core 2.7.4 as a parallel track to the v1.4.x Core 3.1.2 line
- Deferred-reboot machinery with lifecycle heap snapshots at four points around an OTA-triggered reboot
- `logBootSignature()` boot telemetry (reset reason, SDK version, sketch size, free heap at earliest `setup()`)
- `BGTRACE` per-phase timing instrumentation in `doBackgroundTasks` and the main loop (off by default in stability builds)
- `processOT` sub-trace with per-phase heap and time deltas (off by default in stability builds)
- HA auto-discovery for `otgw-firmware/stats/*` metrics so heap and discovery state appear as proper HA sensors
- `sendMQTTDataPic()` helper centralising `otgw-pic/` publish sites
- Self-hosted Inter and JetBrains Mono fonts in the WebUI (no external CDN dependency)
- Design system tokens for centralised colours, spacing, and typography across light and dark themes

### Changed
- Arduino Core baseline reverted to 2.7.4 from 3.1.2 for field-tested stability
- LittleFS partition returns to 1 MB (the v1.4.x 2 MB layout is Core 3.1.2-specific)
- lwIP returns to the 2.1.x branch shipped with Core 2.7.4
- MQTT msgId 0 Status fan-out gate decoupled from `iInterval` with independent 60 s heartbeat
- MQTT msgId 5/6/100 bit-and-byte fan-out gating with 60 s heartbeat (Scope C-min)
- MQTT publish gating tightened to 1 s minimum spacing between gated publishes (250 ms in latest beta)
- Nightly restart routes through the unified `doRestart()` path so it benefits from the same cleanup and snapshot machinery
- `BGTRACE` and `OTTRACE` instrumentation disabled by default in stability builds
- SimpleTelnet submodule bumped to `25a0250` (printf stack raised to 256 bytes)

### Removed
- Legacy `mqttha.cfg` template archive pipeline (streaming HA discovery from v1.4.x supersedes it)
- `WiFi.disconnect()` call from the reboot path (it wiped NVRAM credentials on Core 3.1.x)

### Fixed
- HA discovery for pseudo-ID 247 stats and related publish gates hardened
- `IS_PIC_ENTRY` flag honoured in HA discovery `stat_t` generators
- OTA reboot reliability: explicit service cleanup before `ESP.restart()` (WebSocket, telnet, HTTP, MQTT torn down in defined order)
- OTA reboot reliability: `ESP.reset()` fallback path when `ESP.restart()` returns to caller (Core 3.1.x failure mode)
- OTA safety-tail delay restored after `ESP.restart()` so auto-reset window fires reliably
- WebUI dark theme `.input-changed` was unreadable (black text on dark grey)
- WebUI dark theme `color-scheme` declaration, placeholder colour, scrollbar styling
- WebUI light theme input contrast and mobile header toggle overlap
- WebUI cross-browser dark/light theme rendering (Chrome, Firefox, Safari)
- WebUI log render hotpath stalls; restore buffer capped at 10 000 entries
- Per-message WebSocket console logs silenced behind the `otgwDebug.verbose` gate

## [1.4.1] - 2026-04-21

First public release in the 1.4.x series on Arduino Core 3.1.2. v1.4.0 was tracked internally but not published as a standalone release; v1.4.1 ships the complete 1.4.x body of work.

### Added
- SimpleTelnet debug console with formatted welcome banner and structured debug-key dispatch
- Retained MQTT discovery verification self-heal mechanism with daily auto-verify (ADR-062)
- Hourly heap diagnostic MQTT topic with 17-field JSON payload covering memory and discovery stats
- Nightly restart with configurable hour setting (default 4:00 AM)
- Configurable device manufacturer and model in MQTT device announcements (credit: Schelte Bron)
- NTP telemetry and debug toggle on telnet key 6
- WiFi SSID display and Reset WiFi button on the Settings page
- REST API endpoints for sensor status, discovery state, on-demand verification, and republish
- OTGW simulation mode for testing without physical hardware
- Unified time-boundary dispatcher for periodic tasks (ADR-064)

### Changed
- Arduino Core upgraded from 2.7.4 to 3.1.2 with improved WiFi driver stability
- LittleFS partition size increased from 1 MB to 2 MB (Core 3.1.2 layout)
- MQTT HA discovery rewritten with streaming bitmap-driven drip API (309 configs / 80+ msgIds, no static staging buffer)
- WiFi reconnect hardening: erroneous DHCP calls during active association removed
- OpenTherm v4.2 alignment: IDs 58 to 69 treated as reserved in v4.x mode
- Nightly restart timing now wall-clock aligned through the unified dispatcher
- Heap pressure reduction during HA discovery with configurable drip intervals (2 s normal / 10 s slow-mode)

### Fixed
- WiFi reconnect regression causing repeated cancelled associations before completion (#525)
- `MaxTSet` and `TdhwSet` showing 0 °C in Home Assistant (WRITE_ACK now accepted for OT_WRITE)
- PROGMEM-as-RAM `Exception (3)` crashes after Core 3.1.2 alignment enforcement (byte-safe helpers added)
- Retained MQTT discovery state can now be verified against broker; missing configs re-announced
- OpenTherm Answer Thermostat messages published to boiler MQTT source topic
- NTP last-sync field no longer poisoned by SDK boot value `0xFFFFFFFF`

## [1.3.5] - 2026-04-05

Stability follow-up to v1.3.4.

### Added
- MQTT uptime and firmware version publishing on connect

### Fixed
- WiFi reconnection regression introduced in v1.3.0 with too-aggressive 5-second per-attempt timeout

## [1.3.4] - 2026-04-01

### Added
- Thermostat-only MQTT support (OTGW stays online without boiler connected)

### Changed
- Renamed "OTGW Connected" to "OpenTherm Active" for clarity on the Device Info page

### Fixed
- MQTT throttle slot permanent suppression of stable sensor values after transient publish failures
- Debug Information page tooltips not wired up to device info labels

## [1.3.3] - 2026-03-31

### Added
- PIC-less OTGW support with automatic PIC availability detection and re-detection
- Central `isPICEnabled()` guard protecting all PIC-dependent code paths

### Fixed
- Dashboard no longer shows unsupported OpenTherm message IDs with empty or zero values
- Gateway mode detection for non-gateway PIC firmware returns "N/A" correctly

## [1.3.2] - 2026-03-29

### Fixed
- File deletion failures caused by global buffer conflicts during LittleFS operations
- File explorer "Error loading file list" by switching to streaming implementation

## [1.3.1] - 2026-03-28

### Changed
- Ser2net awareness in command queue to avoid conflicting PIC commands (ADR-059)
- All commands now route through the unified command queue

### Fixed
- Command queue matching by full register letter instead of just 2-character prefix
- `PR=A` banner response never dequeued from command queue
- CS override interference from PIC settings readout triggers
- Time-sync `SC=` command bypassed queue; now routes through proper queue
- Startup queue pause lasting 2 seconds without ser2net activity
- WebUI footer overlapping log window in Firefox / LibreWolf

## [1.3.0] - 2026-03-26

Major feature release: PIC settings visibility, safer upgrades, optional admin protection, fuller `PS=1` integration, lower RAM pressure.

### Added
- PIC Gateway Settings panel exposing all 15 configuration registers via REST API and MQTT
- Single-click GitHub release OTA with version comparison and rollback support
- Optional HTTP Basic Authentication for admin endpoints (disabled by default)
- Configurable MQTT publish gating for OpenTherm and `PS=1` summary data
- Full `PS=1` summary translation with MQTT publishing and HA discovery
- Monitor-page command bar for one-shot OTGW PIC commands
- Light/dark theme toggle button with persistent preference
- Triple-reset WiFi recovery to reopen captive portal
- OTGW simulation mode for testing
- Crash log endpoint for ESP8266 diagnostics
- OTGW event reporting (PIC restart, serial errors) via MQTT and WebSocket
- Heap memory info in device status and Web UI footer
- Gateway mode and WebSocket connection status indicators with tooltips

### Changed
- Global variables reorganised into `OTGWSettings` and `OTGWState` structs
- ArduinoJson dependency completely removed in favour of bounded manual JSON handling
- MQTT autodiscovery memory reduced via streaming template rendering
- Non-blocking WiFi reconnect state machine replaces the blocking 30-second loop
- REST API migration completed with dispatch table routing
- WiFiManager upgraded to stable 2.0.17
- Adaptive throttling based on a 4-level heap health system (ADR-030)

### Fixed
- ESP hostname reverting to `ESP-XXXXXX` after settings save
- Settings page blank on iOS Safari
- Boot-time spurious service restarts
- Hostname normalisation writing to wrong buffer
- File Explorer delete handling
- Webhook payload truncation after reboot
- Unsafe LittleFS OTA flashing without WiFi suppression
- IP validation incorrectly rejecting valid addresses with `255` octet
- NTP hostname not applied in all code paths
- Numeric settings accepting out-of-range values
- MQTT subscription topic truncation
- WiFi portal triggered by stale RTC data after USB flash
- PIC settings buffer truncation for longer-than-expected text responses

### Security
- Centralised auth enforcement in API dispatcher prevents individual handler oversights
- CORS wildcard removed; dynamic origin echoing instead
- CSRF validation hardened using static buffers instead of Arduino `String` class
- Webhook SSRF prevention with DNS resolution and RFC1918 validation
- XSS fix in statistics table with HTML entity escaping
- Boot command validation with alphabetic prefix check
- MQTT payload truncation guard rejects oversized payloads

## [1.2.0] - 2026-03-03

Protocol-alignment and discovery release.

### Added
- Comprehensive Home Assistant MQTT auto-discovery for 309 OpenTherm configs across 80+ message IDs
- Configurable source-separated MQTT publishing with nested topic paths (disabled by default)
- Webhook feature with configurable URL, payload, and content type for OpenTherm status bit changes
- OpenTherm v4.2 alignment with new message IDs 39, 93 to 97

### Changed
- OpenTherm direction flags corrected for IDs 4, 27, 37, 38, 98, 99, 109, 110, 112, 124, 126
- OpenTherm type / byte semantics updated for IDs 38, 71, 77, 78, 87, 98, 99
- `FanSpeed` handling as Hz instead of RPM
- `RelativeHumidity` handling as f8.8 instead of split-byte legacy format
- Legacy pre-v4.2 IDs 50 to 55 and 58 to 63 suppressed in AUTO mode (v4.x systems)
- Gateway mode parsing handles actual `PR=M` response format
- Serial read line buffer increased from 256 to 512 bytes for `PS=1` summary support
- Improved mobile responsiveness with stacked layouts and better touch targets

### Removed
- v0 and v1 REST API endpoints (return 410 Gone)

### Fixed
- MQTT topic spelling: `eletric` to `electric`, `incidator` to `indicator`, `ventlation` to `ventilation`
- MQTT HA discovery mismatches for `FanSpeed`, `Hcratio`, and `vh_configuration`
- `MQTTseparatesources` setting not persisted across reboots
- Gateway mode detection now properly tracks known / unknown state
- Serial robustness for overflow handling and line corruption

## [1.1.0] - 2026-02-25

Dallas sensors, RESTful API v2, and a 20-bug codebase overhaul.

### Added
- Dallas sensor custom labels with inline Web UI editor and LittleFS storage
- Dallas sensor graph visualisation with 16-colour palette and theme support
- Dallas sensor REST API endpoints for bulk label management
- WebUI data persistence to `localStorage` with auto-restoration and capture mode
- Browser debug console (`otgwDebug`) with diagnostic toolkit
- Non-blocking modal dialogs replacing blocking `prompt` / `alert` calls
- `PS=1` mode auto-detection with UI handling and WebSocket events
- Gateway mode display improvements and one-minute polling limit
- RESTful API v2 with 13 new endpoints, consistent JSON errors, and CORS support
- Full OpenAPI 3.0 specification documentation
- Architecture Decision Records ADR-030 through ADR-035

### Changed
- Frontend API migration from v0 / v1 to v2 endpoints
- OTmonitor refresh interval improved from 5 s to 1 s

### Fixed
- MQTT whitespace authentication issue with automatic trimming on boot and change
- Streaming file serving reducing RAM usage by 95 % (fixes slow Web UI)
- Settings persistence with synchronous flush before HTTP confirmation
- Serial buffer expansion to 512 bytes with proper overflow handling
- Dark mode PIC firmware icon visibility with CSS invert filter
- Out-of-bounds array write on OT message ID 255
- Wrong MQTT hour bitmask corrupting night setpoint schedules
- `is_value_valid()` using wrong data parameter
- PIC version string one-byte off-by-one comparison error
- Stack buffer overflow in hex parser
- ISR race conditions in S0 pulse counter (missing `volatile`, `uint16_t` counter)
- GPIO outputs feature gated by debug flag (non-functional in production)
- Null pointer crash from missing `strtok` checks in MQTT callback
- File descriptor leak in settings path
- Year overflow in date handling (`int8_t` to `int16_t`)
- Blocking 750 ms DS18B20 sensor read replaced with async non-blocking mode
- HTTP client resource leak with unconditional `end()`
- Settings flash wear reduced from 20 writes to 1 with 2-second debounce
- Disconnected sensor (-127 °C) published to MQTT suppressed
- GPIO conflict detection

### Security
- CSRF protection added to settings and admin endpoints
- Reflected XSS in error page fixed with HTML entity escaping
- Input sanitisation improvements across the API surface

## [1.0.0] - 2026-02-04

Major milestone: improved stability, modern UI, robust integration.

### Added
- Real-time graphs with ECharts for boiler temperatures, setpoints, pressure, and modulation
- Statistics dashboard with session and long-term heating system data
- Dark mode fully integrated with system preference detection
- Live log viewer using WebSockets for real-time streaming
- File System Explorer redesigned with better upload / download / delete
- WebSocket architecture for live data, reducing network overhead and latency
- MQTT auto-discovery with Home Assistant integration and stable reconnections
- Stream logging for OpenTherm logs to filesystem
- Interactive firmware flashing tool (`flash_esp.py`)
- PIC firmware upgrade from Web UI with binary validation
- Live update progress via WebSocket
- Settings preservation during firmware upgrades
- Memory safety via PROGMEM string optimisation
- Heap protection with active memory monitoring and adaptive throttling
- Watchdog improvements for recovery from hangs

### Changed
- Build pipeline migrated from `make` to fully integrated `arduino-cli`
- Log viewer switched to WebSocket transport
- Aggressive string-literal optimisation using `F()` and `PSTR()`
- Log line formatting and decoding improvements

### Removed
- HTTP polling for logs in favour of WebSockets
- Legacy commented-out code and unused libraries

### Fixed
- PIC firmware update crashes from binary data handling (`strncmp_P` to `memcmp_P`)
- MQTT buffer fragmentation and reconnection logic
- Timezone initialisation issues
- Multiple `Exception (2)` and `Exception (28)` causes related to memory access

## Pre-1.0 history

The pre-1.0 versions predate the Keep a Changelog format adopted in this project. Brief summaries are preserved here for completeness; full detail lives in the [GitHub releases page](https://github.com/rvdbreemen/OTGW-firmware/releases) and in commit history.

### [0.10.3]
- Changed: MQTT password masking on settings page
- Changed: HA discovery template improvements
- Fixed: status function regressions

### [0.10.2]
- Fixed: PIC firmware update path
- Changed: filesystem image bundles latest PIC firmware

### [0.10.1]
- Changed: build process improvements
- Fixed: VH status parsing
- Added: WiFi quality indicator

### [0.10.0]
- Added: PIC16F1847 (6.x firmware) support
- Added: DHCP NTP override
- Added: S0 pulse counter
- Added: Dallas sensor auto-configure

### [0.9.x]
- Added: JIT Home Assistant auto-discovery
- Added: climate entity
- Added: MQTT `set` commands
- Added: time setup and NTP improvements

### [0.8.x]
- Changed: MQTT topic convention
- Added: HA device grouping
- Added: climate entity (early form)
- Added: PIC firmware integration
- Added: Dallas temperature sensors
- Added: command queue

### [0.7.x]
- Changed: filesystem migrated to LittleFS
- Added: ser2net on TCP port 25238
- Added: ventilation / heat-recovery message IDs
- Added: PIC reset on boot

### [0.6.x]
- Added: standalone Web UI
- Added: OTA support

### [0.5.x]
- Added: REST API v1
- Added: settings UI

### [0.4.x]
- Added: ser2net
- Added: REST API v0

### [0.2.x and 0.3.x]
- Added: MQTT integration
- Added: serial stream output

### [0.0.1]
- Added: initial OpenTherm protocol parsing
