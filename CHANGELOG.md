# Changelog

All notable changes to OTGW-firmware are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

### Added

- **OTDirect TT=/TC= PIC-style remote-override semantics** (TASK-466). TT (temporary) and TC (constant) commands now write both MsgID 16 (TrSet) and MsgID 100 (RemoteOverrideFunction) flag bits, distinguishing program-priority from manual-priority overrides. TT carries an auto-clear honour-state machine that releases the override once the thermostat program resumes; TC persists indefinitely. Behaviour matches the legacy PIC firmware. User-visible: TT and TC are no longer functionally identical. Fixture rows added in `tests/otdirect_pic_parity_fixture.md`.
- **BLE temperature-sensor support on ESP32-S3 OTGW32** (TASK-487 NimBLE port + TASK-494 continuous-scan + TASK-488 HA discovery). Replaces the classic Bluedroid stack with NimBLE-Arduino 2.x (~400 KB flash freed). Async continuous scan matches the OT-Thing reference: sensors are discovered within seconds of boot. Per-MAC MQTT topics under `<TopTopic>/sat/ble/<mac>/{temp,rh,bat,rssi}` plus retained Home Assistant auto-discovery configs. Supports ATC/pvvx custom firmware (UUID `0x181A`) and BTHome v2 (UUID `0xFCD2`). ADR-092 covers the dependency adoption.
- **OTDirect command-queue coalesce-by-MsgID** (TASK-494). Multi-channel fan-in (MQTT/REST/WebUI/serial) now collapses repeated WRITE_DATA commands targeting the same MsgID into the latest value, matching PIC's "latest WRITE_DATA per MsgID wins" semantics on the OT bus. High-water mark counter exposed via telnet debug for diagnostic visibility. ADR-068 amended.

### Changed

- **Project relicensed from MIT to GNU GPLv3.** Applies to the project's own code (Robert van den Breemen, sole copyright holder), including the root LICENSE and every source file that previously carried an MIT header/footer. Third-party vendored libraries (`src/libraries/OpenTherm` — (c) Ihor Melnyk, `src/libraries/OTGWSerial` — (c) Schelte Bron) and the LGPL-2.1-licensed portions of `FSexplorer.ino` (c) Jens Fleischer keep their original licenses and copyright notices unchanged.
- **MQTT on-change publishing is now the default** (TASK-791, ADR-116). New setting `MQTTonChangePublishing` defaults to `true`, and the publish interval defaults to `60` seconds: changed OpenTherm values publish immediately, unchanged values refresh once per minute. On upgrade, a config that still has `MQTTinterval=0` is migrated once to `60` (persisted via the deferred settings write, no boot-time rewrite). Untick "MQTT Publish On-Change" (or set `MQTTonChangePublishing=false`) to restore legacy publish-every-message behaviour. The TASK-400 status-bit heartbeat is independent and unchanged.
- **`settings.sat.iBleInterval` semantics** (TASK-494). Repurposed from "BLE scan rate" to "publish/state-update cadence". The BLE radio scans continuously on ESP32 since this release; existing user configs continue to load and round-trip cleanly. WebUI tooltip updated.
- **MQTT `resetgateway` command** now requires payload `"1"` (matching the HA-discovery `payload_press` value already shipped) and is rate-limited to one PIC reset per 5 seconds. Non-matching payloads are logged and ignored; rapid retries inside the cooldown window are silently dropped with a log line. Closes the unauthenticated-LAN reset-storm path raised by the dev review (TASK-668, port of dev TASK-661).

### Fixed

- **HA `DHW Control` / `Thermostat` / SAT / sensor entities flapping `unavailable`** (TASK-608, ADR-102; 2.0.0 port of dev TASK-607/ADR-074, PR #583). HA entity availability (`avty_t` = the base `<toptopic>/value/<nodeid>` topic) now reflects only the ESP↔MQTT link (birth/LWT) instead of OpenTherm-bus liveness: `publishOTGWConnectedState()` no longer overwrites the base topic with the `state.otBus.bOnline` 30 s wall-clock window, which flapped every HA entity when OT traffic was bursty or the clock unstable. OT-bus liveness remains on the dedicated `otgw_connected` sensor (unchanged). ESP32-S3's faster NTP makes the trigger rarer than on ESP8266, but the defect/fix are platform-independent. **Contract change:** consumers that read the base topic as OT-bus liveness must migrate to the `otgw_connected` sensor.

- **`bleSensorPublishHaDiscovery` sticky on transient failure** (TASK-489 + TASK-493). The per-MAC `bDiscoveryPublished` flag was set to `true` even when MQTT was not yet connected at first BLE scan, permanently silencing HA discovery for that sensor. Helper now returns `bool`; caller gates the flag on success and retries on the next iBleInterval cycle. Slot-recycling reset added so a recycled slot for a new MAC re-publishes discovery.
- **`setRemoteOverride()` float-narrowing UB on out-of-range setpoints** (TASK-491 sign-extend + TASK-495 clamp). Cast through `(int16_t)` to preserve f8.8 sign; range-clamp `celsius` to `-40..127` before the cast.
- **`feedWatchDog()` only on success path in BLE HA-discovery** (TASK-496). The 16-publish first-scan burst could starve the watchdog when broker timeouts piled up. Watchdog now fed before every `return false` after a network attempt.
- **NimBLE `_bleSensors[]` cross-task data race on ESP32-S3** (TASK-497). BLE scan callback (BLE host task on core 0) and Arduino loop task (core 1) accessed shared slot data without synchronisation. Added `portMUX_TYPE` snapshot pattern: scan callback locks during slot updates, loop task takes a snapshot under the lock and processes outside. Critical sections stay short (one struct copy ~ 40 bytes). ADR-090 amended for the FreeRTOS cross-task case.
- **`platformio.ini` auto-format scope creep** (TASK-494 / 1B-H1). The build-rationale prose-rich layout was restored after an inadvertent auto-formatter pass erased ~90 lines of comments and silently bumped `tool-esptoolpy` from `~1.30000.0` to `^2.41100.0`. Original pin restored.
- **`evaluate.py` PROGMEM gate false positives** (TASK-482). The gate now skips lines following backslash-continued `#define` macros and excludes the auto-generated `*.ino.cpp` (Arduino preprocessor concatenation). Baseline drops from 15 to 14 violations and is now stable across builds.
- **`bleMacToCompact` dead duplicate consolidated** (TASK-492). The parallel-agent execution left two implementations of the same MAC-compact helper; the canonical one in MQTTstuff.ino is now exported via `OTGW-firmware.h` and the duplicate in SATble.ino is removed.
- **WiFi association without DHCP/IP after reboot** (TASK-432, ported from `dev`). Removed three `platformRestartDHCP()` call sites in `networkStuff.ino` (startWiFi catch-all, WIFI_RECONNECTED, startNTP) and the function itself from `platform_esp8266.h` and `platform_esp32.h`. Calling `wifi_station_dhcpc_stop/start` while the station is connected takes DHCP ownership away from the SDK and breaks subsequent `setAutoReconnect()`-driven DHCP. Returns to v1.2.0 baseline pattern: SDK manages DHCP autonomously.
- **MQTT publish gating per source topic** (TASK-478, ported from `dev`). Implements ADR-066: the base topic `{prefix}/value/{id}/{label}` publishes only thermostat-side intent (Read-Ack and Write-Data); slave Write-Ack values no longer flow to the base topic. The `/boiler` subtopic (when `bSeparateSources=true`) is gated by a per-MsgID `bSlaveEchoesValue` flag for the six OpenTherm v4.2 messages whose slave Write-Ack data byte is per-spec undefined (`Tr` 24, `TrSet` 16, `MaxRelModLevelSetting` 14, `TrSetCH2` 23, `TRoomCH2` 37, `RFstrengthbatterylevel` 98). Spec-audit covering all OT v4.2 MsgIDs lives in `docs/api/MQTT-message-id-echo-audit.md`.

### Added

- ADR-066 documenting the source-aware MQTT publish gating decision (ported verbatim from `dev`)
- `docs/api/MQTT-message-id-echo-audit.md` spec-audit reference per OpenTherm v4.2 (ported from `dev`)
- `bSlaveEchoesValue` field on `OTlookup_t` populated for every MsgID
- Added .github/workflows/dependency-scan.yml weekly outdated-deps visibility job (TASK-501 4B-M3)
- Added tools/generate_release_sha256sums.py + docs/MANUAL.md release-verification section (TASK-501 4B-M4)
- Added docs/templates/hardware-verification-log.md template for AC evidence capture (TASK-501 4B-M5)
- Polished 8 comment / `static_cast` / named-constant Lows from Phase 1-4 review (TASK-502): BLE_STALE_MS static_assert tripwire, malformed-MAC debug log in satBLEMacToCompact, NimBLE 0.625 ms tick conversion comment, OT_OVERRIDE_*_DELTA_F88 float-truncation foot-gun note, satBLEPublishHaDiscovery / OTGW-firmware.h "publish cycle vs scan" wording refresh, named BLE_SCAN_INTERVAL_TICKS / BLE_SCAN_WINDOW_TICKS / BLE_SCAN_MAX_RESULTS constants, static_cast<int8_t> for getRSSI(), shared BLE_MAC_COMPACT_SIZE constant.

## [2.0.0-beta] - 2026-04-11

### Added

- **Dual-platform support**: Single codebase targeting both ESP8266 and ESP32 with a unified platform abstraction layer (`platform.h`, `platform_esp8266.h`, `platform_esp32.h`).
- **OTDirect (OTGW32)**: Direct GPIO OpenTherm bus communication on ESP32 using the Phunkafizer OT library, bypassing the PIC controller. Full OT-Thing parity including protocol handshake, `CC=` command support, self-disabling poll, and command ring buffer.
- **W5500 Ethernet support (OTGW32)**: Hardware Ethernet with automatic WiFi/Ethernet failover. Unified network access via `isNetworkUp()`, `getActiveIP()`, `getActiveMAC()` helpers.
- **SAT (Smart Autotune Thermostat)**: Embedded heating controller with PID control, heating curve advisor (INCREASE/DECREASE/HOLD), adaptive stalled-ignition thresholds, multi-zone PID with MQTT room sensor inputs, DHW setpoint slider, boiler gas consumption estimation, Summer Simmer Index thermal comfort calculation, and MQTT humidity input.
- **OLED display module**: SSD1306 OLED support with hardware platform info display in UI and MQTT.
- **BLE (Bluetooth Low Energy)**: SAT BLE interface for wireless configuration and status.
- **AP fallback mode** (beta builds): When no configured WiFi is available, the device opens a fallback access point (`OTGW-<MAC>`, IP `192.168.4.1`, password `otgw123`) rather than going offline.
- **WiFi signal quality bars**: Visual signal strength indicator in the Web UI header using quadratic RSSI-to-percentage mapping.
- **WiFi SSID display and Reset WiFi button**: Settings page now shows the connected SSID and provides a one-click Reset WiFi button.
- **SAT WebSocket status events**: WebSocket events for flame, mode, and curve changes.
- **SAT opt-in browser console logging**: `window.SAT_DEBUG` flag enables verbose SAT diagnostics in the browser console.
- **Full OpenTherm MsgID coverage**: Extended message ID handling and protocol handshake support.
- **OTGW32 REST/MQTT parity**: OTGW32-specific endpoints matching PIC-based API surface.
- **ELF file in build artifacts**: `.elf` output included in CI artifacts for crash debugging.
- **PlatformIO support**: `platformio.ini` added for ESP32 builds alongside Arduino CLI builds.

### Fixed

- **OTGW Answer Thermostat MQTT routing**: Messages were published to the wrong source topic; now correctly routed to the boiler source topic.
- **NTP last-sync guard**: `NtpLastSync` now ignores bogus SDK initial time value on first boot.
- **ESP8266 and ESP32 compilation**: Multiple compilation errors resolved; ESP32 compatibility fixes in OTDirect and SATble.
- **SAT LittleFS migration**: `satMigrateFile()` was passing PROGMEM pointers to LittleFS, causing Exception 3 crash.
- **SAT stack pressure**: Stacked 556-byte local buffers in weather JSON function flattened to reduce stack pressure.
- **collectHeaders compile error**: `#ifdef` guard restored for `collectHeaders` on ESP8266 Core 3.x.

### Changed

- **Settings/state architecture**: 62+ flat globals replaced with `OTGWSettings` and `OTGWState` structs (ADR-051).
- **Network abstraction**: All code uses `isNetworkUp()`/`getActiveIP()` rather than direct `WiFi.*` calls.
- **SAT subsystem**: Reorganized LittleFS layout under `/sat/`, added timestamps and stale detection.
- **SAT window ring buffer**: Reduced to 30 slots on ESP8266 to save 720 bytes SRAM; expanded on ESP32.
- **Version**: Bumped to `2.0.0-beta`.

---

## [1.3.5] - 2026-04-06

### Added

- **MQTT uptime and version publishing**: Firmware publishes uptime and version info to MQTT on connect for better device visibility.

### Fixed

- **WiFi reconnection regression** (#530): The WiFi state machine timeout (5 s) was too short for ESP8266 association (5-10 s+), causing repeated connection cancellations. Timeout increased to 30 s, restoring the behavior of v1.2.0. Devices that went offline periodically since v1.3.0 should now stay connected.

---

## [1.3.4] - 2026-04-01

### Added

- **Thermostat-only MQTT support**: OTGW now stays online via MQTT when only a thermostat is connected; a boiler is no longer required.

### Fixed

- **MQTT throttle slot suppression**: Stable values such as Room Temperature could become permanently suppressed after a transient publish failure. The throttle slot is now only updated after a successful publish.
- **Debug Information page tooltips**: Tooltips were defined but never wired up to the device info labels; now visible on hover.

### Changed

- **Renamed "OTGW Connected" to "OpenTherm Active"**: Clearer label with updated tooltip.

---

## [1.3.3] - 2026-03-31

### Added

- **PIC-less OTGW support**: All PIC functions are automatically disabled when no PIC is detected at boot, with auto-recovery if the PIC appears later. REST API returns 503 and the UI hides PIC-specific elements.

### Fixed

- **Dashboard showing unsupported OT values**: Empty or zero values for message IDs the boiler does not support are no longer displayed; only valid responses are tracked and shown.
- **Gateway mode detection**: Non-gateway PIC firmware now correctly shows "N/A" instead of continuously polling.
- **"Home Assistant Integration" label**: Renamed to "OTGW Connected" with an accurate tooltip describing OTGW online status.

---

## [1.3.2] - 2026-03-29

### Fixed

- **File delete reliability**: The delete handler used the global `cMsg` buffer shared with MQTT, webhooks, and settings. Background tasks could overwrite it mid-operation, causing "Failed to delete file" errors and unresponsive behavior. Now uses a dedicated local buffer with proper HTTP status codes.
- **File listing**: Replaced the RAM-heavy `dirMap[]` array and bubble sort with direct LittleFS streaming. Sorting and size formatting moved to the frontend. Hidden files (`.` prefix) are now filtered out.

### Changed

- MQTT LWT developer documentation added.

---

## [1.3.1] - 2026-03-28

### Fixed

- **CS override interference**: Setpoint commands (`CS=`) from Home Assistant/MQTT are no longer intermittently overridden by the thermostat's lower setpoint. PIC settings readout triggers are now whitelisted to specific commands only.
- **PR command queue matching**: Responses like `PR: S=16.00` now correctly remove `PR=S` from the queue instead of the first `PR=` entry found, preventing unnecessary retries and queue slot waste.
- **PR=A banner dequeue**: The PIC banner response is now properly detected and removes `PR=A` from the command queue, preventing 5 unnecessary retries.
- **`strstr_P` crash**: Inverted PROGMEM/RAM arguments in the PIC settings trigger check caused an Exception (2) crash.
- **SC= time-sync bypassed queue**: The time synchronization command now goes through the command queue like all other PIC commands, preventing serial bus collisions.
- **UI footer overlap**: The log window no longer overlaps the status bar in Firefox/LibreWolf when stretched to the bottom of the screen.

### Added

- **Ser2net awareness**: The command queue detects traffic from port 25238 (OTmonitor) and pauses queue processing for 2 seconds to avoid conflicting commands. Conflicting queue entries are automatically removed.

### Changed

- **Non-blocking PIC queries**: `PR=` settings and gateway mode queries are now fully asynchronous, no longer blocking the HTTP server.
- **Queue code**: Deduplicated queue removal logic into a single `removeFromCmdQueue()` helper.

---

## [1.3.0] - 2026-03-26

### Added

- **PIC gateway settings panel**: All 15 PIC configuration registers (setpoint override, GPIO, LEDs, tweaks, smart power, thermostat detection, voltage reference, etc.) exposed via REST API (`/api/v2/pic/settings`), MQTT (`otgw-pic/settings/*`), and a new "Gateway Settings" section in the Web UI. Human-readable formatting, color-coded live/cached indicators, and browser localStorage caching per hostname for up to 7 days.
- **Single-click GitHub release OTA**: The update page lists GitHub releases with Installed/Update/Rollback badges. One-click download and flash with semver-aware version comparison including pre-release tags and Intel HEX integrity validation for PIC firmware.
- **Optional HTTP Basic Auth**: Settings, maintenance, file-management, reboot, and OTA routes can be protected with an admin password via the Protected Endpoints Password setting.
- **Configurable MQTT publish gating**: OpenTherm and `PS=1` summary data can be rate-limited to reduce MQTT broker load and WiFi chatter, with automatic status republish after boot and reconnect.
- **Full `PS=1` summary integration**: `PS=1` output is now parsed into the normal data pipeline, published to MQTT, and exposed through Home Assistant discovery.
- **Monitor-page command bar**: Send one-shot OTGW PIC commands (`TT=20.5`, `GW=R`, etc.) directly from the Web UI with command/response/error feedback in the log.
- **Light/dark theme toggle button**: Switch between themes with a single click; preference persists across sessions.
- **Triple-reset WiFi recovery**: Three quick hardware resets reopen the captive portal and clear stale WiFi credentials without requiring a reflash.
- **Safer OTA/LittleFS flashing**: Reboot verification via `/api/v2/health`, browser backups of `settings.ini` and `dallas_labels.ini`, WiFi reconnect suppressed during flash writes, full partition erase before write.
- **OTGW simulation mode**: Test firmware and Web UI without physical hardware. SIMULATION badge shown in the monitor header.
- **Crash log endpoint**: `/api/v2/device/crashlog` exposes ESP8266 crash information for diagnostics.
- **OTGW event reporting**: PIC restart, serial overrun, and RX errors forwarded via MQTT and WebSocket.
- **Heap memory in status**: Free heap and fragmentation shown in the Web UI footer and device info API.
- **GPIO conflict detection**: Conflicting GPIO pin assignments detected and warned about at boot.

### Fixed

- **ESP hostname reverting to ESP-XXXXXX**: Deep audit and fix of all hostname code paths; hostname now set before `WiFi.begin()`, before/after `configTime()`, and after SDK auto-connect.
- **OTA filesystem corruption**: WiFi reconnect suppressed during flash writes; full LittleFS partition erased before writing.
- **IP validation**: Only `255.255.255.255` is now rejected; valid addresses containing a `255` octet are no longer blocked.
- **Boot-time restart cleanup**: Prevents spurious service restarts immediately after startup.
- **Webhook payload truncation**: Buffer widened to accommodate full webhook payloads after reboot.
- **File Explorer delete**: File deletion works consistently again.
- **NTP hostname**: `startNTP()` moved after `startWiFi()` so the configured hostname is active.
- **MQTT subscription truncation**: Buffer size increased for long topic strings.
- **WiFi portal false trigger**: Stale RTC data after USB flash no longer triggers triple-reset detection.

### Changed

- **ArduinoJson removed**: All JSON paths use bounded manual handling, reducing flash and RAM usage.
- **Settings/state reorganized**: 62+ flat globals replaced with `OTGWSettings` and `OTGWState` structs.
- **String class eliminated from hot paths**: Protocol, settings, and HTTP handlers use `char[]` buffers, reducing heap fragmentation.
- **MQTT autodiscovery optimized**: Streaming template rendering and in-place line parsing replace bulk buffer allocation.
- **Non-blocking WiFi reconnect**: State machine replaces the blocking 30-second reconnect loop.
- **REST API v2 completed**: Dispatch table routing; all remaining v1 calls migrated.
- **Security hardening**: Centralized auth for all POST/PUT in API dispatcher; CORS wildcard replaced with dynamic origin; webhook SSRF prevention via DNS resolution; CSRF validation rewritten without String class; XSS fix in statistics table.
- **Dead code removal**: ~450 lines of legacy v1 JSON functions, unused helpers, dead enums, and stale CSS removed.
- **Stack pressure**: ~1,400 bytes freed via centralized `otTopic` buffer and static local buffers in hot-path functions.
- **WiFiManager**: Upgraded from 2.0.15-rc.1 to stable 2.0.17.

---

## [1.2.0] - 2026-03-02

### Added

- **Comprehensive Home Assistant auto-discovery**: All OpenTherm message types automatically exposed to Home Assistant — 309 discovery configurations across 80+ message IDs covering heating, cooling, solar thermal, DHW, ventilation/heat recovery, CH2, relative humidity, operational counters, and system status. No manual YAML required.
- **Webhook support**: Configurable outbound HTTP call triggered on any OpenTherm status bit change (e.g. flame on/off). Separate URL, payload, and content type for on/off events. Restricted to local network URLs; disabled by default.
- **Source-separated MQTT topics**: Optional `mqttseparatesources` setting publishes per-source topics (`<metric>/thermostat`, `<metric>/boiler`, `<metric>/gateway`) alongside the existing unsuffixed topics for backward compatibility.
- **OpenTherm v4.2 protocol alignment**: Added missing message IDs 39 and 93-97; corrected direction flags, type semantics, and units for several IDs; added legacy ID compatibility profile (`AUTO` / `V4X_STRICT` / `PRE_V42_LEGACY` modes) for IDs 50-55 and 58-63.

### Fixed

- **`MQTTseparatesources` not persisted**: Setting was written to `settings.ini` but never read back on boot, silently resetting to `false` after every reboot.
- **Gateway mode parsing**: Fixed `PR=M` response parsing (`M=G`/`M=M`); added explicit detecting/unknown state.
- **MQTT/HA topic typos**: `eletric_production` → `electric_production`, `solar_storage_slave_fault_incidator` → `solar_storage_slave_fault_indicator`, and several `vh_*_ventilation_*` spelling fixes.
- **HA discovery mismatches**: `vh_configuration_*` now keyed to correct message ID 74; `Hcratio` discovery `stat_t` corrected; `FanSpeed` split into `FanSpeed_setpoint_hz` and `FanSpeed_actual_hz` (Hz).
- **MQTT autoconfig re-entry**: Shared static buffer workspace and scoped re-entry lock prevent buffer clobbering during concurrent auto-configuration runs.

### Changed

- **REST API v0 and v1 removed**: All `/api/v0/` and `/api/v1/` endpoints now return **410 Gone**. Only `/api/v2/` remains.
- **ArduinoJson dependency removed**: Settings I/O replaced with lightweight streaming helpers (`wStrF`, `wBoolF`, `wIntF`, `parseSettingsLine()`), reducing flash and RAM.
- **Device info API key renames**: `gatewaymode`/`mode` → `otgwmode`; `wifiqualitytldr` → `wifiquality_text`.
- **Runtime safety hardening**: OT map bounds checks in parser and REST paths; safe fallback metadata for unknown IDs; `sendOTGWvalue()` validates message ID range before map access.
- **Serial line buffer**: Increased to 512 bytes (PS=1 summary lines can exceed 256 bytes).

### Removed

- **REST API v0 and v1**: Removed (see above). Requests to these paths return 410 Gone.

### Breaking Changes

- REST API v0 and v1 endpoints removed — migrate to `/api/v2/`.
- MQTT topic renames (OpenTherm v4.2 alignment): delete orphaned Home Assistant entities after upgrading. See [docs/fixes/opentherm-v42-mqtt-breaking-changes.md](docs/fixes/opentherm-v42-mqtt-breaking-changes.md).
- `FanSpeed` (rpm) split into `FanSpeed_setpoint_hz` + `FanSpeed_actual_hz` (Hz).
- Source-specific MQTT topics changed from underscore-separated (`TSet_thermostat`) to path-separated (`TSet/thermostat`).

---

## [1.1.0] - 2026-02-22

### Added

- **Dallas sensor custom labels**: Inline non-blocking label editor in Web UI. Labels stored in `/dallas_labels.ini` on LittleFS with zero backend RAM usage (max 16 characters per label). Automatic backup/restore during filesystem flash.
- **Dallas sensor graph visualization**: DS18x20 sensors automatically appear in the real-time temperature graph with a 16-color palette. Sensor labels displayed in graph legend.
- **Dallas sensor REST API**: `GET /api/v2/sensors/labels` and `POST /api/v2/sensors/labels` for bulk label management.
- **RESTful API v2**: 13 new `/api/v2/` endpoints with consistent JSON error responses (`{"error":{"status":N,"message":"..."}}`), proper HTTP status codes (202 Accepted for async operations), CORS support, OPTIONS preflight, and RESTful resource naming. Full OpenAPI 3.0 specification in `docs/api/openapi.yaml`. API compliance score improved from 5.4 to 8.5/10.
- **WebUI data persistence**: Automatic log data persistence to browser `localStorage` with debounced 2-second saves, dynamic buffer sizing, and auto-restore on page load. Log buffer cleared automatically after firmware/filesystem flash.
- **Browser debug console (`otgwDebug`)**: Full diagnostic toolkit in the browser JavaScript console: `status()`, `info()`, `settings()`, `wsStatus()`, `wsReconnect()`, `otmonitor()`, `logs()`, `api()`, `health()`, `sendCmd()`, `exportLogs()`, `exportData()`, `persistence()`.
- **Non-blocking modal dialogs**: Custom HTML/CSS modals replace blocking `prompt()`/`alert()` calls; WebSocket data flow continues uninterrupted while a modal is open.
- **PS mode detection**: Automatic detection of `PS=1` from the OTGW PIC. When active, hides the OT log section, disables WebSocket OT streaming, and suppresses time-sync commands (improves Domoticz compatibility). WebSocket events broadcast on PS mode transitions.
- **Gateway mode overhaul**: Complete refactor of detection and display logic. REST API field renamed from `mode` to `otgwmode`. Gateway mode polling limited to once per minute.
- **Heap memory health system**: 4-level monitoring (CRITICAL <3 KB, WARNING 3-5 KB, LOW 5-8 KB, HEALTHY >8 KB) with adaptive throttling and WebSocket backpressure control.
- **ADR-030 through ADR-035**: Six new Architecture Decision Records covering heap memory monitoring, two-microcontroller coordination, no-auth LAN security model, Dallas sensor labels, non-blocking modals, and RESTful API compliance.

### Fixed

- **MQTT auth after upgrade from v0.10.x**: MQTT credentials are now automatically whitespace-trimmed on boot and on change — invisible trailing spaces from text editor copy/paste no longer cause authentication failures.
- **Slow Web UI** (index.html loaded into RAM): Replaced full-file-to-RAM loading (`readString()`) with streaming chunked transfer encoding. 95% reduction in RAM used for file serving.
- **Settings reverting to defaults**: Settings now written to flash synchronously before HTTP confirmation. ArduinoJson replaces fragile string-split parsing.
- **Serial buffer**: `MAX_BUFFER_READ` increased to 512 bytes; overflow now discards the corrupt partial line entirely rather than processing it.
- **Out-of-bounds write on message ID 255**: `msglastupdated` array enlarged from `[255]` to `[256]`.
- **Wrong MQTT hour bitmask**: Mask `0x0F` truncated hours 16-23, corrupting night setpoint schedules. Fixed to `0x1F`.
- **`is_value_valid()` wrong data source**: Function used global `OTdata` instead of the passed parameter.
- **ISR race conditions in S0 pulse counter**: TOCTOU races, missing `volatile`, and `uint8_t` overflow on high pulse rates fixed.
- **Reflected XSS**: Request URI injected into HTML error page without escaping. Added HTML entity escaping.
- **GPIO outputs non-functional**: Feature was gated by a debug flag, making it completely non-functional in production builds.
- **750 ms blocking Dallas sensor read**: Replaced with async non-blocking mode.
- **Disconnected sensor publishing -127°C**: Added `DEVICE_DISCONNECTED_C` guard to suppress publishing.
- **File descriptor leak**: File opened before existence check; reordered to check existence first.
- **Null pointer crash on malformed MQTT topics**: Added `strtok()` null guards throughout MQTT callback.
- **Settings flash wear**: 20 separate flash writes per save operation reduced to 1 via 2-second debounce.
- **Year overflow**: Year stored in `int8_t` (overflows at year 2128 and cannot represent 2026). Changed to `int16_t`.
- **Dark mode PIC firmware icons**: Black icons invisible in dark mode; fixed with `filter: invert(1)` in dark mode CSS.
- **Settings persistence**: Deferred save timer could be lost before firing on reboot; now synchronous write before HTTP 200.

### Changed

- **Frontend fully migrated to v2 API**: Zero legacy API calls remain in `index.js`.
- **OTmonitor refresh interval**: Improved from 5 s to 1 s for more responsive UI.
- **`getOTGWValue()` refactored**: Eliminates `String` allocations; uses C-string buffers.
- **Build system**: `version.hash` always generated; centralized `config.py`; reusable GitHub Actions composite actions; automated release workflow publishing `.elf`, `.bin`, and `.littlefs.bin` artifacts.
- **CI/CD**: New ADR Compliance workflow to detect architectural file changes and validate ADR references.
- **Documentation**: OpenTherm v4.2 specification converted to searchable Markdown.

### Deprecated

- REST API v0 and v1 endpoints deprecated; scheduled for removal in v1.3.0 (removed in v1.2.0). Migrate to `/api/v2/`.

### Breaking Changes

- Gateway mode field in REST API response renamed from `mode` to `otgwmode`. Update any custom integrations that read this field directly.

---

## [1.0.0] - 2026-02-08

### Added

- **Real-time graphing**: ECharts-based interactive graphs for OpenTherm data (temperatures, setpoints, pressure) with adjustable time windows directly in the Web UI.
- **Statistics dashboard**: Dedicated tab with session and long-term heating system statistics.
- **WebSocket architecture**: Moved from HTTP polling to WebSockets for live data (logs, status, updates). Significantly reduces network overhead and latency.
- **Dark mode**: Fully integrated dark theme that respects system preferences or can be toggled manually; state saved across sessions.
- **Live log viewer**: Re-engineered with WebSockets for true real-time streaming. Includes filtering, pausing, and raw message decoding.
- **File System Explorer**: Redesigned file manager with upload/download/delete capabilities.
- **MQTT Auto Discovery**: Enhanced Home Assistant integration with improved stability, reliable reconnections, and cleaner entity naming.
- **Stream logging**: Ability to stream OpenTherm logs to files on the file system for troubleshooting.
- **Interactive flash tool**: Improved `flash_esp.py` script for firmware updates.
- **PIC firmware upgrade**: Safer PIC controller flashing from the Web UI with binary data validation.
- **Live OTA progress**: Real-time progress bars and status messages during firmware updates via WebSocket.
- **Settings preservation**: Smart preservation of configuration during firmware upgrades.
- **Evaluation tool**: `evaluate.py` for code quality checks (PROGMEM usage, unsafe patterns).
- **Architecture Decision Records (ADRs)**: ADR system established in `docs/adr/`.
- **Legacy Dallas sensor format**: Added setting for legacy Dallas sensor ID format.

### Fixed

- **PIC flashing crashes**: Fixed critical crashes during PIC firmware updates due to binary data handling.
- **MQTT buffer fragmentation**: Improved reconnection logic and buffer management.
- **Timezone initialization**: Fixed timezone initialization issues.
- **CSRF protection**: Added CSRF protection and better input sanitization.
- **Multiple Exception (2) and Exception (28) crashes**: Related to memory access patterns.

### Changed

- **Build system**: Removed `make` dependency; fully integrated `arduino-cli`.
- **Log viewer**: Switched from HTTP polling to WebSockets.
- **Memory**: Aggressive optimization of string literals using `F()` and `PSTR()`.
- **Responsive design**: Improved settings pages and controls for mobile/desktop.

### Removed

- **HTTP polling for logs**: Replaced by WebSocket streaming.
- **Legacy commented-out code and unused libraries**.

### Breaking Changes

- Default GPIO for Dallas sensors changed to GPIO 10 to match standard hardware.
- Some settings may need re-verification due to new preservation logic (auto-migration attempted).

---

## [0.10.3] - 2024-04-17

Legacy release. See git history for details.

## [0.10.x] and earlier

Legacy releases. See git history for details.

[Unreleased]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.5...HEAD
[2.0.0-beta]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.5...HEAD
[1.3.5]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.4...v1.3.5
[1.3.4]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.3...v1.3.4
[1.3.3]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.2...v1.3.3
[1.3.2]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.1-fix...v1.3.2
[1.3.1]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.0...v1.3.1-fix
[1.3.0]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.2.0...v1.3.0
[1.2.0]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/rvdbreemen/OTGW-firmware/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.0.0
