# Release Notes — v2.0.0

**Last updated:** 2026-04-25<br>
**Release branch:** `dev`<br>
**Comparison target:** `main` (current stable `v1.3.5`)<br>

---

## Headline: Dual-Platform Support, SAT Smart Autotune Thermostat, OTDirect Native OpenTherm, Ethernet, OLED Display, and BLE Temperature Sensors

v2.0.0 is the largest release in the project's history. It adds a full dual-platform build for both ESP8266 and ESP32 (OTGW32), introduces SAT: an embedded PID heating controller with weather compensation and Home Assistant auto-discovery, adds OTDirect for native OpenTherm bus mastering on ESP32 without a PIC, brings wired Ethernet support for ESP32, adds an OLED display, and extends the API surface with 30+ new SAT and OTDirect MQTT topics and REST endpoints.

REST endpoints and `settings.ini` format are unchanged from v1.x; one small MQTT breaking change applies to three OT-bus presence topics (see Breaking Changes below). The new features are otherwise purely additive. SAT is disabled by default.

---

## Overview

**User-visible additions:**

- Unified ESP8266 and ESP32 codebase with PlatformIO build system and platform abstraction layer (boards.h, platform.h).
- SAT: embedded heating controller with heating curve, PID v3, weather compensation, OPV calibration, multi-zone temperature, pressure monitoring, solar gain, presets, and 250+ Home Assistant auto-discovery entities.
- SAT WebUI: four-view dashboard (Thermostat, Expert, Diagnostics, Settings) built into the Web UI.
- BLE temperature sensor support (ESP32 only) via passive Bluetooth scanning for Xiaomi LYWSD03MMC using BTHome v2 protocol.
- OTDirect: native OpenTherm bus master on ESP32 using hardware timer Manchester encoding, no PIC required. Five operating modes.
- W5500 SPI Ethernet on ESP32 with automatic DHCP and WiFi/Ethernet auto-failover.
- 128x64 SSD1306 OLED display on I2C (both platforms), four pages, button page cycling, 30s auto-off.
- WiFi signal quality bars in the Web UI header (live RSSI).
- WiFi SSID display and Reset WiFi button in the Settings page.
- Beta: AP fallback mode when saved WiFi credentials are unavailable (pre-release builds only).
- Triple-reset WiFi recovery (carried forward and confirmed working on both platforms).
- NTP bogus SDK time guard: rejects the 0xFFFFFFFF sentinel the ESP8266 SDK emits before the first sync.
- OTGW Answer Thermostat messages now published to the correct boiler MQTT source topic.

**Bug fixes:**

- OTGW Answer Thermostat messages routed to wrong MQTT topic: corrected to `boiler/` source topic.
- NTP last-sync timestamp corrupted by bogus SDK 0xFFFFFFFF: guarded and rejected.
- ESP8266 and ESP32 build failures introduced during the platform merge: resolved.
- SAT PROGMEM stack overflow in `weatherSendStatusJSON`: stacked 556-byte local buffers replaced with static allocation.
- SAT PID deadband default set to 0.1C, manual gains mode restored.
- SAT continuous mode setpoint clamping: flame-off clamp bypass corrected.
- SAT weather HTTP client crash on timeout: null-check added before use.
- SAT solar gain threshold fixed; DHW setpoint command corrected.
- SAT PROGMEM pointer passed to LittleFS in `satMigrateFile`: fixed to RAM buffer.
- ESP32 compilation errors in OTDirect and SATble: resolved.
- Missing `.sat-grid` and `.sat-row` CSS classes: added; diagnostics grid now renders correctly.
- `collectHeaders` variadic function: `#ifdef` guard restored for ESP8266 Core 3.x compatibility.

**Internal improvements:**

- Platform abstraction layer: `boards.h` (GPIO pin map), `platform.h` (serial, filesystem, timing), and `HAS_PIC` / `HAS_DIRECT_OT` / `HAS_ETH_CAPABLE` feature flags.
- PlatformIO `platformio.ini` replaces Arduino IDE as the primary build tool.
- `build.py` updated to prefer PlatformIO's `penv/pio` executable over the system PATH.
- SAT heap and stack pressure reduced: ring buffer reduced to 30 slots on ESP8266 (saves 720 bytes SRAM), static buffers replace local allocations in weather and status functions.
- SAT LittleFS reorganized to `/sat/` subdirectory with timestamps and stale detection.
- ELF file included in build artifacts for crash debugging.
- Multi-area room support uses TASK-25 weighted temperature algorithm.
- OTGW32 OTDirect: PI room compensation and flame ratio tracking added.
- SAT cycle classifier uses per-hour PWM cycle limiter and p90 flow temperature.

---

## Breaking Changes

### OT-bus state moved to generic MQTT topics (ADR-084)

Three OT-bus presence values previously published under hardware-specific subtrees (`otgw-pic/*` on PIC-based gateways, `otgw-otdirect/*` on OTGW32) now live under the generic value namespace, regardless of hardware variant. The inconsistent `otgw-otdirect/ot_online` name is retired in favour of `otgw_connected`.

**Removed topics:**

- `OTGW/value/<uniqueId>/otgw-pic/boiler_connected`
- `OTGW/value/<uniqueId>/otgw-pic/thermostat_connected`
- `OTGW/value/<uniqueId>/otgw-pic/otgw_connected`
- `OTGW/value/<uniqueId>/otgw-otdirect/boiler_connected`
- `OTGW/value/<uniqueId>/otgw-otdirect/thermostat_connected`
- `OTGW/value/<uniqueId>/otgw-otdirect/ot_online` (renamed)

**New canonical topics:**

- `OTGW/value/<uniqueId>/boiler_connected`
- `OTGW/value/<uniqueId>/thermostat_connected`
- `OTGW/value/<uniqueId>/otgw_connected`

Payload semantics are unchanged: `"ON"` / `"OFF"`, retained.

**Impact and migration:**

- **Home Assistant users**: nothing to do. Entity `unique_id`s are stable, discovery republishes automatically on reconnect, history is preserved. On OTGW32 builds without a PIC, `Boiler connected` and `Thermostat connected` entities now appear for the first time (they were previously gated behind `MQTT_HA_FLAG_IS_PIC_ENTRY`).
- **Custom MQTT consumers** (Node-RED, openHAB, scripts) subscribed to the old paths: update your topic patterns. The firmware self-heals retained payloads on the deprecated topics at the first MQTT reconnect after upgrade, so no manual broker cleanup is required in the typical case. For manual cleanup, see the migration guide in `docs/api/MQTT.md` (search anchor: "Migration from 1.4.x").

**Why:** these values describe what the firmware observes on the OpenTherm bus, not properties of the PIC coprocessor or the OT-direct driver. Grouping them under hardware-specific subtrees made them disappear from Home Assistant on OTGW32 builds without a PIC, and forced custom consumers to switch topic prefixes when the underlying hardware changed. ADR-084 amends ADR-065 to narrow the `otgw-pic/` subtree to strictly PIC-coprocessor properties (version, deviceid, firmwaretype, designer, picavailable, settings/*).

**Self-healing cleanup**: a temporary firmware block subscribes briefly on each MQTT reconnect to the six deprecated topics and clears any retained payload it finds, then unsubscribes. Idempotent and free on brokers that never saw pre-2.0.0 firmware. Will be removed in firmware 2.3.0 or later (see in-source TEMPORARY MIGRATION CODE comment).

REST endpoints and the `settings.ini` format are unchanged from v1.x.

---

## New Features

### Dual-Platform Support: ESP8266 and ESP32

v2.0.0 introduces a unified codebase that compiles and runs on both the original ESP8266-based OTGW (Nodoshop v1.x, Wemos D1 mini) and the new ESP32-based OTGW32 (Nodoshop v2.x). The platform layer is clean and transparent: application code references symbolic constants only.

- **boards.h**: GPIO pin map per board variant. `PIN_I2C_SCL`, `PIN_OT_MASTER_IN`, `PIN_SPI_CS`, etc. Application code never uses raw GPIO numbers or `Dx` aliases.
- **platform.h**: Serial abstraction (UART1 on ESP32 for PIC communication vs. the hardware UART on ESP8266), filesystem, and timing helpers.
- **Feature flags**: `HAS_PIC`, `HAS_DIRECT_OT`, `HAS_ETH_CAPABLE` gating for platform-specific code paths.
- **PlatformIO**: `platformio.ini` defines both `esp8266` and `esp32` environments. The existing Arduino IDE build via `build.py` continues to work for ESP8266.

The ESP8266 OTGW feature set is identical to v1.3.5. All new ESP32 features are additive.

---

### SAT: Smart Autotune Thermostat

SAT is the flagship feature of v2.0.0. It turns the OTGW from a transparent OpenTherm bridge into an active, adaptive heating controller running entirely on the ESP microcontroller. No Home Assistant, no cloud, no internet connection is required for operation.

**What SAT does:**

Instead of forwarding whatever the wall thermostat requests, SAT intercepts the OpenTherm control loop. It reads room temperature (from the OT bus, an external MQTT push, or a BLE sensor), calculates the exact boiler flow temperature needed to hold the target room temperature, and injects that setpoint via `CS=` commands to the PIC. The boiler runs at lower temperatures for longer, which is how condensing boilers are designed to operate efficiently.

**Heating curve and weather compensation:**

SAT uses a configurable heating curve as its base setpoint calculation:

```
setpoint = baseOffset + (coeff/4) * [4*(target-20) + 0.03*(outside-20)^2 - 0.4*(outside-20)]
```

The heating curve coefficient and offset are configurable per system type (underfloor, low-temperature radiators, high-temperature radiators). Outdoor temperature can come from:

- OpenTherm MsgID 27 (if the boiler provides it),
- an MQTT push (`set/{nodeId}/sat/outdoor_temp`),
- or the Open-Meteo weather API fetched over HTTP (no API key required, uses browser geolocation or configurable coordinates).

Solar gain compensation subtracts an estimated solar contribution from the flow setpoint based on sun elevation angle. Sun elevation can be pushed via MQTT or computed internally from GPS coordinates.

**PID controller v3:**

SAT includes a full PID controller that applies a correction to the heating curve value. Key design points:

- Gains are auto-calculated from the heating curve value (no manual tuning required by default).
- Manual gain override mode is available for advanced users.
- Deadband-gated integral: the integral only accumulates within ±0.1°C of the target. This prevents windup on large setpoint changes.
- Temperature-based derivative with an adaptive low-pass filter to avoid derivative kick on noisy sensor readings.
- PID state (integral, last error, last derivative) persists across reboots in LittleFS at `/sat/pid_state.json`.

**Control modes:**

SAT operates in two control modes and can switch between them automatically:

- **Continuous mode**: The computed flow setpoint is sent directly to the boiler as a `CS=` command. The boiler modulates its own burner to reach the target flow temperature. Best for heat pumps and underfloor systems.
- **PWM mode**: SAT duty-cycles the `CS=` command (on/off) at a configurable period. Suitable for older boilers that do not modulate. SAT monitors the flame duty ratio and switches from PWM to continuous mode automatically when modulation becomes reliable.

**Anti-cycling:**

SAT enforces configurable minimum OFF time (default 180 seconds) and maximum cycles per hour (heat pump: 2, underfloor: 3, radiators: 4). These limits prevent short-cycling which wears out the burner igniter.

**Overshoot Protection Value (OPV) calibration:**

After each heating cycle, SAT performs a three-phase automated calibration to measure per-boiler overshoot before flame-off (WARMING → MEASURING → DONE/FAILED). Calibration requires a minimum of 40 samples before accepting a result. The OPV is stored in settings and applied to all subsequent cycles to prevent room temperature overshoot after the boiler shuts down.

**Boiler status state machine:**

SAT tracks 13 boiler states: OFF, IDLE, WAITING\_FLAME, ANTI\_CYCLING, STALLED\_IGNITION, PUMP\_STARTING, IGNITION\_SURGE, PREHEATING, HEATING, MODULATING\_UP, AT\_SETPOINT, COOLING, OVERSHOOT\_COOLING. Stalled ignition detection uses an adaptive threshold based on the last successful cycle duration.

**Safety layers:**

SAT has six independent release mechanisms to ensure `CS=0` (boiler release) is always sent when something goes wrong:

1. Room temperature sensor staleness timeout (configurable, default 30 minutes).
2. Outdoor temperature staleness timeout.
3. Consecutive OT read failure threshold.
4. Boiler fault detection via OT MsgID 0 fault flags.
5. SAT disabled flag: immediate release on `sat/enabled = 0`.
6. Firmware reboot: SAT always starts in disabled state until explicitly activated.

**Presets:**

Six named presets available via MQTT or REST: Away (15°C), Eco, Comfort (21°C), Sleep (18°C), Activity, Home. Presets are applied via `set/{nodeId}/sat/preset`. Active preset is published to `sat/preset`. On preset change, SAT reverts to the original target if a custom setpoint was in use.

**Multi-zone room temperature:**

SAT accepts temperature inputs from up to 4 independent zones via `set/{nodeId}/sat/area/<0-3>`. A weighted average is computed from all active zones and used as the effective room temperature. Weights are configurable per zone.

**Thermal drop learning:**

SAT's fallback mode (active when MQTT connectivity is lost) uses a learned home heat loss coefficient to estimate how fast the house cools. The coefficient is updated from observed temperature drops and used to compute a conservative setpoint without closed-loop feedback.

**CH pressure monitoring:**

SAT monitors central heating pressure via OT MsgID 18. The raw sensor value is smoothed with an exponential moving average. A linear regression over a rolling window detects slow pressure drop trends. An alarm is raised (with hysteresis) only after a configurable confirmation delay. Pressure status is published to `sat/ch_pressure` and `sat/ch_pressure_status`.

**Manufacturer detection:**

SAT reads OT MsgID 3 (slave MemberID) at startup to identify the boiler brand (18 manufacturers recognized). Per-manufacturer quirks are applied automatically; for example, `SAT_QUIRK_NO_REL_MOD` disables relative modulation commands for Ideal, Nefit, and Intergas boilers that do not support them correctly.

**Summer Simmer Index:**

When indoor humidity is available (pushed via `set/{nodeId}/sat/humidity`), SAT computes the Summer Simmer Index, a thermal comfort metric that accounts for humidity. When SSI indicates the house is already warm enough, SAT inhibits heating automatically.

**Gas consumption estimation:**

SAT estimates gas consumption from modulation level and thermal power, publishing `sat/gas_consumption_kw` and cumulative energy to MQTT. Useful for monitoring heating efficiency trends.

**Cycle statistics:**

SAT tracks rolling 4-hour cycle statistics including flame-on time, idle time, cycle count, EMA duty ratio, and per-cycle classifier results (startup, steady, cooldown, idle). Statistics are published to MQTT and exposed in the REST API.

**Home Assistant integration:**

SAT publishes 250+ Home Assistant auto-discovery entities covering: climate entity, temperature sensors, PID diagnostics, pressure health, cycle statistics, presets, control mode, gas consumption, energy totals, and all configurable settings as number/switch/select entities. All discovery entities are included in the standard `doAutoConfigure()` flow.

---

### SAT WebUI Dashboard

The SAT control interface is built into the firmware Web UI as a four-view dashboard accessed from the main navigation. The dashboard is implemented in `sat.js` and communicates via WebSocket for live updates.

- **Thermostat view**: Target temperature control with a large temperature display, preset buttons (Away, Eco, Comfort, Sleep, Activity, Home), control mode selector, and current room/boiler/outdoor temperature readouts. DHW setpoint slider and boiler-type-aware hot water switch.
- **Expert view**: PID output (P, I, D components), heating curve value, final setpoint, boiler status state, flame status, modulation, duty ratio, OPV value, cycle advisor (INCREASE/DECREASE/HOLD recommendation).
- **Diagnostics view**: Health indicators, pressure status, sensor staleness, consecutive failure counts, window detection state, SAT enabled/disabled, and raw data table.
- **Settings view**: 12 collapsible configuration categories covering all SAT parameters. All settings are saved via REST API calls to `/api/v2/sat/settings/<name>`.

WebSocket status events are sent for flame transitions, control mode changes, and heating curve updates so the dashboard stays live without polling.

An opt-in browser console logging mode is available by setting `window.SAT_DEBUG = true` in the browser console.

---

### BLE Temperature Sensors (ESP32 only)

On ESP32-based OTGW32, SAT can receive room temperature from Bluetooth Low Energy temperature sensors using the BTHome v2 advertisement protocol. Xiaomi LYWSD03MMC sensors (with custom ATC firmware) are the reference hardware.

- Passive BLE advertisement scanning via `SATble.ino`.
- Up to 4 sensors can be configured (one per zone for multi-zone weighted temperature).
- Sensor MAC addresses are configured in settings.
- Temperature values are forwarded to the SAT multi-zone temperature input with the same staleness tracking as MQTT inputs.
- BLE scanning runs only when SAT is enabled and BLE is configured, minimizing heap and CPU overhead.
- Heap fragmentation mitigation: per-advertisement callback avoids dynamic allocation in the hot path.

---

### OTDirect: Native OpenTherm on ESP32

OTGW32 (ESP32) can act as a direct OpenTherm bus master without the PIC microcontroller. This is implemented in `OTDirect.ino` using hardware timer Manchester encoding on dedicated GPIO pins.

**Operating modes:**

| Mode | Description |
|------|-------------|
| Gateway | Standard mode: PIC handles the OT bus, ESP32 observes and injects commands |
| Monitor | Read-only bus observer, no PIC, no bus drive |
| Bypass | PIC bypassed, ESP32 relays messages transparently between thermostat and boiler |
| Master | ESP32 acts as full OpenTherm master, drives the bus directly without PIC or thermostat |
| Loopback | Internal test mode: master and slave ports looped back for self-test |

**Capabilities:**

- Hardware timer-based Manchester encoding for timing-accurate OpenTherm frame generation.
- PI room compensation: injects a room temperature correction to the boiler's flow setpoint calculation, independent of SAT.
- Flame ratio tracking: monitors modulation level trends and publishes normalized flame ratio to MQTT.
- Step-up converter control: 18V OT bus power via GPIO-controlled boost converter.
- All mode transitions available via REST API (`/api/v2/otdirect/*`) and MQTT.

**REST endpoints:**

- `GET /api/v2/otdirect/status` — current mode, last frame, bus statistics.
- `POST /api/v2/otdirect/mode` — switch operating mode.
- Additional diagnostic endpoints for frame counts and error rates.

OTDirect is only active when `HAS_DIRECT_OT = 1` (ESP32 boards).

---

### Ethernet Support (ESP32)

OTGW32 supports wired Ethernet via a W5500 SPI module. Ethernet is handled alongside WiFi with automatic selection and failover.

- **Hardware**: W5500 SPI Ethernet (Wiznet). GPIO assignments via `PIN_SPI_*` constants in `boards.h`.
- **DHCP**: Automatic address assignment via DHCP. No static IP required.
- **MAC address**: Derived from the ESP32 eFuse MAC to ensure a stable, unique address per device.
- **Auto-failover**: When both WiFi and Ethernet are configured, the firmware prefers Ethernet. If the Ethernet link drops, WiFi is used automatically.
- **Transparent**: All features (MQTT, REST API, WebSocket, OTA, mDNS) work identically over Ethernet and WiFi.

Ethernet support is only compiled when `HAS_ETH_CAPABLE = 1` (ESP32 boards).

---

### OLED Display (both platforms)

A 128x64 SSD1306 I2C OLED display is now supported on both ESP8266 and ESP32. The display is entirely optional; if no display is present, the I2C scan completes silently and no display code runs.

**Display pages:**

| Page | Content |
|------|---------|
| Dashboard | Room temp, boiler flow temp, target setpoint, flame status |
| HC1 | Heating circuit 1: flow temp, return temp, modulation |
| HC2 | Heating circuit 2 (if present): DHW temp, DHW mode |
| System Status | IP address, uptime, WiFi RSSI, free heap |

Navigation: a physical button (connected to the config/reset GPIO) cycles through pages on short press. The display turns off automatically after 30 seconds of inactivity and wakes on button press.

---

### Network and WiFi Improvements

**WiFi signal quality indicator:**

The Web UI header now shows a live WiFi signal quality indicator as four bars (using Unicode block characters), updated from the live RSSI value polled over WebSocket. The indicator is shown in all pages that include the header.

**WiFi SSID and Reset WiFi:**

The Settings page now shows the currently connected WiFi SSID, making it easy to confirm which network the device is associated with. A "Reset WiFi" button clears the saved credentials and reboots into the WiFiManager captive portal. This complements the existing triple-reset hardware recovery.

**AP fallback mode (beta):**

When no saved WiFi credentials are available and the device cannot connect, it falls back to an access point mode. This feature is guarded behind a pre-release build flag and is not active in stable releases. It is included here for testers to evaluate.

**NTP bogus time guard:**

The ESP8266 SDK sets the internal time to 0xFFFFFFFF before NTP has synced. Previous firmware versions could record this value as `NtpLastSync`, producing wildly incorrect timestamps in the Web UI and MQTT. The firmware now detects and rejects this sentinel value before recording any sync time.

---

### API and Integration Updates

**SAT MQTT topics (30+ new topics):**

All SAT control and state topics follow the prefix `{topTopic}/sat/`. Full topic list available in [docs/api/MQTT.md](../api/MQTT.md).

Key published topics include:

```
sat/enabled               sat/control_mode         sat/target
sat/room_temp             sat/outdoor_temp         sat/boiler_temp
sat/heating_curve_value   sat/heating_system       sat/final_setpoint
sat/boiler_status         sat/pid_output           sat/pid_p
sat/pid_i                 sat/pid_d                sat/pid_error
sat/ch_pressure           sat/ch_pressure_status   sat/preset
sat/energy/flame_on_sec   sat/energy/cycles_hour   sat/energy/ema_duty_ratio
sat/gas_consumption_kw    sat/climate_attributes   sat/valves_open
```

Subscribed command topics follow the set path:

```
set/{nodeId}/sat/target        set/{nodeId}/sat/indoor_temp
set/{nodeId}/sat/outdoor_temp  set/{nodeId}/sat/humidity
set/{nodeId}/sat/enabled       set/{nodeId}/sat/control_mode
set/{nodeId}/sat/preset        set/{nodeId}/sat/window
set/{nodeId}/sat/area/<0-3>    set/{nodeId}/sat/sun_elevation
```

**SAT REST endpoints:**

All SAT endpoints are under `/api/v2/sat/`. See [docs/api/README.md](../api/README.md) and `openapi.yaml` for the full schema.

```
GET  /api/v2/sat/status                  GET  /api/v2/sat/status?detail=full
POST /api/v2/sat/target                  POST /api/v2/sat/externaltemp
POST /api/v2/sat/externaloutdoor         POST /api/v2/sat/humidity
POST /api/v2/sat/area/<0-3>              POST /api/v2/sat/window
POST /api/v2/sat/preset                  POST /api/v2/sat/reset_integral
POST /api/v2/sat/flush                   POST /api/v2/sat/settings/<name>
GET  /api/v2/sat/settings/               GET  /api/v2/sat/settings/<name>
```

**OTDirect REST endpoints:**

```
GET  /api/v2/otdirect/status
POST /api/v2/otdirect/mode
```

**OTGW Answer Thermostat messages:**

Messages where the OTGW answers on behalf of the thermostat (Answer Thermostat message type, direction boiler) are now published to the boiler MQTT source topic instead of being dropped. This fixes a long-standing gap in data coverage where boiler-directed WRITE_DATA messages were not visible in MQTT.

---

## Bug Fixes

### OTGW Answer Thermostat messages routed to wrong topic

When the OTGW PIC intercepted a thermostat message and replied with an Answer Thermostat frame, the firmware was publishing the message to the wrong MQTT source topic (or not publishing it at all). The fix corrects the direction check so Answer Thermostat frames are published to `{topTopic}/boiler/{msgId}`.

### NTP last-sync guard against bogus SDK time

The ESP8266 SDK sets `time()` to 0xFFFFFFFF before any NTP sync occurs. If the firmware read this value as the NTP last-sync timestamp, it would display a date of 2106 in the Web UI and publish it via MQTT. The guard now checks for this sentinel and skips the update until a valid time is available.

### ESP8266 and ESP32 build failures after platform merge

The dual-platform merge introduced several compilation errors: missing `collectHeaders` variadic guard for ESP8266 Core 3.x, missing `#ifdef` guards for ESP32-only OTDirect code, and symbol conflicts between platform layers. All resolved.

### SAT PROGMEM stack overflow in weather status JSON

`weatherSendStatusJSON()` allocated three local buffers totalling 556 bytes on the stack. On the ESP8266's 4KB CONT stack, this caused a stack overflow when called during the main loop. All buffers are now static.

### SAT PROGMEM pointer passed to LittleFS

`satMigrateFile()` passed a PROGMEM pointer directly to LittleFS open/rename calls. PROGMEM pointers are in flash address space; passing them to APIs that expect RAM pointers causes Exception 3. Fixed to copy the PROGMEM string to a RAM buffer first.

### SAT PID deadband and manual gains

The PID deadband default was incorrectly initialized to 0.0, disabling the deadband entirely. Reset to 0.1°C as designed. Manual gains mode (user-specified Kp/Ki/Kd) was not being applied when auto-gains were disabled; now correctly respected.

### SAT continuous mode setpoint clamp bypass

During flame-off transitions, SAT was applying the setpoint clamp even in continuous mode, causing the `CS=` command to send a value higher than intended. The clamp bypass path now correctly distinguishes between control modes.

### SAT weather HTTP client crash on timeout

If the Open-Meteo HTTP request timed out before the client object was fully initialized, `weatherFetchOutdoor()` dereferenced a null pointer. Added null check before any client method calls.

### ESP32 build failures in OTDirect and SATble

Several ESP32-specific files had unguarded symbol references that failed to compile when the ESP8266 toolchain was selected. All references are now guarded with `#ifdef ESP32` / `#if HAS_DIRECT_OT` as appropriate.

### Missing SAT diagnostics CSS grid classes

The SAT Diagnostics view referenced `.sat-grid` and `.sat-row` CSS classes that were missing from the stylesheet. The diagnostics panel rendered as an unstyled list. Classes added.

### collectHeaders variadic function guard

`collectHeaders()` uses a variadic function signature that was accepted by the ESP8266 Core 2.x compiler but rejected by Core 3.x. The `#ifdef` guard that was present in an earlier version was accidentally removed during a merge. Restored.

---

## Hardware Requirements

| Feature | ESP8266 (OTGW v1.x) | ESP32 (OTGW32 v2.x) |
|---------|---------------------|----------------------|
| Core OTGW functionality | Yes | Yes |
| SAT heating controller | Yes | Yes |
| SAT Web UI dashboard | Yes | Yes |
| OLED display (SSD1306) | Yes | Yes |
| BLE temperature sensors | No | Yes |
| OTDirect (no PIC) | No | Yes |
| Wired Ethernet (W5500) | No | Yes |
| Multi-zone room temp (MQTT) | Yes | Yes |
| Home Assistant auto-discovery | Yes | Yes |
| PlatformIO build | Yes | Yes |
| Arduino IDE build | Yes | Not recommended |

---

## Upgrade Notes

### From v1.3.5 (ESP8266)

1. Flash both firmware and filesystem. The SAT WebUI and OLED support require the updated filesystem.
2. Hard-refresh the browser after flashing (Ctrl+F5).
3. SAT is disabled by default. To enable, set `sat/enabled = 1` via MQTT or the Settings page.
4. All existing MQTT topics, REST endpoints, and `settings.ini` format are unchanged. No migration required.
5. The triple-reset WiFi recovery behavior is unchanged.

### First-time ESP32 / OTGW32 installation

1. Install PlatformIO (VS Code extension or `pip install platformio`).
2. Build and flash the `esp32` environment: `pio run -e esp32 -t upload`.
3. Flash the filesystem: `pio run -e esp32 -t uploadfs`.
4. On first boot, configure WiFi via the captive portal. Ethernet auto-negotiates if a cable is connected.
5. GPIO pin assignments in `boards.h` are defaults for the Nodoshop OTGW32. Verify against your actual hardware before deploying.

### Breaking changes

There are no breaking changes relative to v1.3.5.

| Area | v1.3.5 to v2.0.0 |
|------|------------------|
| MQTT topics | No renames. New `sat/` and `otdirect/` prefixes are additive. |
| REST API | No removals. New `/api/v2/sat/` and `/api/v2/otdirect/` are additive. |
| Settings format | No migration required. New `settings.sat.*` fields initialize to defaults. |
| Home Assistant discovery | Additive only. New SAT entities appear; existing entities unchanged. |
| ESP8266 feature set | Identical to v1.3.5 plus SAT, OLED, and WiFi improvements. |

---

## Validation Basis

These notes were compiled from the full commit history between the v1.3.5 tag and the v2.0.0-beta HEAD, cross-checked against the source files for `SATcontrol.ino`, `SATpid.ino`, `SATcycles.ino`, `SATweather.ino`, `SATble.ino`, `SATpressure.ino`, `OTDirect.ino`, `boards.h`, `platform.h`, the C4 component documentation, and the SAT feature design document.

SAT was contributed by Robert van den Breemen with design input from George Dellas (SergeantD) and attribution to Alex Wijnholds (Alexwijn) for the original SAT Python concept and heating curve algorithm.
