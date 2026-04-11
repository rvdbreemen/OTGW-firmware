# C4 Code Level: Sensors & I/O Module

## Overview

- **Name**: Sensors & I/O Module (OTGW-firmware)
- **Description**: Hardware input/output abstraction layer for the OpenTherm Gateway. Manages Dallas DS18B20 temperature sensors via 1-Wire bus, S0 pulse counter for energy metering, GPIO relay outputs, and optional OLED display. Publishes sensor data via MQTT with Home Assistant auto-discovery.
- **Location**: [src/OTGW-firmware/](../../src/OTGW-firmware/)
- **Language**: Arduino C/C++ (single translation unit `.ino` files)
- **Purpose**: Provide unified sensor and I/O control, abstract hardware platform differences (ESP8266 vs ESP32), enable real-time monitoring and control of heating system auxiliary hardware

## Code Elements

### Dallas DS18B20 Temperature Sensors (`sensors_ext.ino`)

**Module Purpose**: Manages 1-Wire temperature sensor bus, auto-discovery, label persistence, and MQTT publishing.

#### Functions

- `void initSensors(void)`
  - **Location**: `sensors_ext.ino:134`
  - **Signature**: Initializes OneWire bus and Dallas Temperature library
  - **Description**: Sets up OneWire communication on configured GPIO pin; probes for connected DS18B20 sensors; stores device addresses; enforces max 16 devices; calls `ensureSensorDefaultLabels()` to populate LittleFS labels file on first discovery or new sensor addition. Guards detection state in `bSensorsDetected` runtime flag (separate from `settings.sensors.bEnabled` to track actual boot-time detection). Returns early if sensors disabled AND not in simulation mode.
  - **Acceptance Criteria Met**: 
    - Configures OneWire.begin() only after settings loaded (ADR-020)
    - Non-blocking async DS18B20 conversion via `setWaitForConversion(false)`
    - Enforces `MAXDALLASDEVICES` limit gracefully
  - **Dependencies**: 
    - `OneWire oneWire` (module-level OneWire instance)
    - `DallasTemperature sensors(&oneWire)` (DS18B20 library instance)
    - `ensureSensorDefaultLabels()` (internal label file init)
    - `settings.sensors.bEnabled`, `settings.sensors.iPin` (persistent config)
    - `state.debug.bSensorSim` (runtime simulation flag)

- `void ensureSensorDefaultLabels(void)`
  - **Location**: `sensors_ext.ino:32`
  - **Signature**: Ensures all discovered sensors have default labels in `/dallas_labels.ini`
  - **Description**: Two-pass algorithm: Pass 1 reads existing address→label pairs from LittleFS JSON file via `readJsonStringPair()`; Pass 2 rebuilds file, preserving existing labels and appending auto-generated defaults ("Sensor N") for any newly discovered sensors. Checks both real and simulated sensors. Updates file only if changes detected. Uses static struct array to buffer file content in RAM (max 16 devices).
  - **Acceptance Criteria Met**:
    - Reads and preserves user labels across reboots
    - Generates sensible defaults for undiscovered sensors
    - Minimal file writes (compare-before-write pattern)
  - **Dependencies**:
    - `DallasrealDeviceCount`, `DallasrealDevice[]` (detected sensors)
    - `getDallasAddress()` (address-to-string conversion)
    - `readJsonStringPair()`, `writeJsonStringPair()` (JSON file helpers)
    - `state.debug.bSensorSim` (determines label prefix)
    - LittleFS `/dallas_labels.ini` file

- `void initSimulatedDallasSensors(void)`
  - **Location**: `sensors_ext.ino:105`
  - **Signature**: Initializes virtual sensor array for testing/demo without physical 1-Wire bus
  - **Description**: Creates `SIM_SENSOR_COUNT` (3) simulated DS18B20 devices with pre-defined fake ROM addresses (`DallasSimDeviceAddresses[]`). Each sensor initialized with starting temp (30.0 + i*5.0 °C). Zeroes simulation timer for immediate first update. Sets `numberOfDevices` and `DallasrealDeviceCount`.
  - **Dependencies**:
    - `DallasSimDeviceAddresses[3][8]` (fake ROM addresses for simulants)
    - `simLastUpdateTime` (simulation timer)

- `void configSensors(void)`
  - **Location**: `sensors_ext.ino:205`
  - **Signature**: Triggers Home Assistant MQTT auto-discovery for all detected sensors
  - **Description**: Loops through all `DallasrealDeviceCount` sensors and calls `sensorAutoConfigure(OTGWdallasdataid, false, strDeviceAddress)` for each. After last sensor, calls `setMQTTConfigDone(OTGWdallasdataid)` to mark configuration complete. Safe to call repeatedly (idempotent). Early return if MQTT disabled.
  - **Acceptance Criteria Met**:
    - Publishes discovery message per sensor for HA auto-discovery
    - Defers to MQTT handler for actual publishing
  - **Dependencies**:
    - `settings.mqtt.bEnable` (MQTT guard)
    - `DallasrealDevice[]`, `DallasrealDeviceCount`
    - `sensorAutoConfigure()`, `setMQTTConfigDone()` (MQTT autoconfigure API)
    - `OTGWdallasdataid = 246` (pseudo-dataid for HA discovery)

- `void pollSensors(void)`
  - **Location**: `sensors_ext.ino:222`
  - **Signature**: `void pollSensors(void)` — Main sensor polling loop, called from `doBackgroundTasks()`
  - **Description**: Non-blocking sensor read cycle. Requests async temperature conversion on all devices (non-blocking thanks to `setWaitForConversion(false)` in init). Stores latest °C values in `DallasrealDevice[i].tempC` and `lasttime` timestamp. Publishes via `sendMQTTData(strDeviceAddress, _msg)` if MQTT enabled. Handles simulated sensors with `SIM_SENSOR_UPDATE_INTERVAL_SECONDS` throttle (separate from physical sensor read rate). Logs disconnected sensors (gracefully skips, keeps previous value per Finding #29). Calls `configSensors()` if auto-discovery pending.
  - **Acceptance Criteria Met**:
    - Non-blocking (async conversion)
    - Graceful disconnect handling (skips stale reads, preserves last known value)
    - Simulation with configurable update interval
    - Logs all reads (when debug enabled)
  - **Sensor Disconnection Behavior**: If `getTempC()` returns `DEVICE_DISCONNECTED_C`, sensor is skipped (value not updated, no error thrown)
  - **Dependencies**:
    - `sensors.requestTemperatures()` (OneWire request)
    - `sensors.getTempC(DeviceAddress)` (read converted value)
    - `getDallasAddress()` (address string for logging/MQTT)
    - `sendMQTTData()` (publish to MQTT)
    - `configSensors()` (auto-discovery if pending)
    - `DallasrealDevice[]`, `DallasrealDeviceCount`, `bSensorsDetected`
    - Simulation: `SIM_SENSOR_MIN/MAX`, `SIM_SENSOR_UPDATE_INTERVAL_SECONDS`, `simLastUpdateTime`

- `char* getDallasAddress(DeviceAddress deviceAddress)`
  - **Location**: `sensors_ext.ino:304`
  - **Signature**: `char* getDallasAddress(DeviceAddress deviceAddress)` — Converts 8-byte ROM address to hex string
  - **Description**: Returns static 17-char buffer containing hex representation of sensor ROM address. Two modes: (1) **Standard (default)**: 16 hex chars (8 bytes × 2), e.g., `"28D0000000000001"`; (2) **Legacy format** (`settings.sensors.bLegacyFormat = true`): Replicates buggy behavior of ~v0.10.x (compressed ~9-10 char output) for backward compatibility with existing Home Assistant automations. Legacy mode emulates original `sprintf(dest+i, "%X")` which overwrote chars sequentially rather than appending.
  - **Acceptance Criteria Met**:
    - Static buffer safe for immediate use in telemetry/MQTT
    - Legacy flag preserves HA automation compatibility
  - **Dependencies**:
    - `settings.sensors.bLegacyFormat` (format selection)

#### Global State

- `OneWire oneWire` (module scope)
  - **Type**: `OneWire` instance
  - **Description**: 1-Wire bus abstraction; default constructor (no pin) per ADR-020; pin binding deferred to `initSensors()` after settings loaded
  - **Location**: `sensors_ext.ino:128`

- `DallasTemperature sensors(&oneWire)` (module scope)
  - **Type**: `DallasTemperature` (Miles Burton library)
  - **Description**: DS18B20-specific abstraction over OneWire; manages async conversion requests and temp reading
  - **Location**: `sensors_ext.ino:131`

- `int numberOfDevices` (module scope)
  - **Type**: `int`
  - **Description**: Count of 1-Wire devices found during last `initSensors()` call (includes both real and simulated)
  - **Location**: `sensors_ext.ino:15`

#### Global Simulation State

- `time_t simLastUpdateTime` (module scope)
  - **Type**: `time_t` (static)
  - **Description**: Timestamp of last simulated sensor update; used to throttle simulation to `SIM_SENSOR_UPDATE_INTERVAL_SECONDS` (10s)
  - **Location**: `sensors_ext.ino:22`

- `const uint32_t SIM_SENSOR_UPDATE_INTERVAL_SECONDS = 10`
  - **Description**: Update interval for simulated sensors (separate from physical read throttle)

- `const float SIM_SENSOR_MIN = 20.0f, SIM_SENSOR_MAX = 60.0f`
  - **Description**: Temperature bounds for simulated sensor walk (clamps random delta)

---

### S0 Pulse Counter (`s0PulseCount.ino`)

**Module Purpose**: Counts energy meter S0 pulses via GPIO interrupt, calculates instantaneous power, publishes via MQTT.

#### Functions

- `void initS0Count(void)`
  - **Location**: `s0PulseCount.ino:39`
  - **Signature**: Initializes S0 pulse counter interrupt and pin
  - **Description**: Configures GPIO pin as `INPUT_PULLUP` (meter contact pulls low on pulse). Attaches ISR on `FALLING` edge (meter resets contact when pulse complete). Resets global `pulseCount = 0`. Early return if `settings.s0.bEnabled = false`. Logs on debug.
  - **Acceptance Criteria Met**:
    - Interrupt-driven (non-blocking)
    - Debounce enforced in ISR (not in init)
  - **Dependencies**:
    - `settings.s0.bEnabled`, `settings.s0.iPin`
    - `IRQcounter()` ISR function (FALLING edge handler)

- `void IRAM_ATTR IRQcounter(void)`
  - **Location**: `s0PulseCount.ino:24`
  - **Signature**: ISR for S0 pulse detection (FALLING edge)
  - **Description**: Interrupt service routine called on meter contact closure (FALLING edge). Increments `pulseCount` only if interrupt time delta > `settings.s0.iDebounceTime` (default 80ms), rejecting bounces. Tracks `last_pulse_duration` (time since previous valid pulse) for power calculation. Uses `IRAM_ATTR` (must-be-in-SRAM on ESP8266). Static locals `last_interrupt_time` safe because ISR cannot re-enter (no nested interrupts).
  - **Acceptance Criteria Met**:
    - Hardware debounce via timer check
    - Volatile state safe for ISR
    - Calculation of pulse duration for power formula
  - **Dependencies**:
    - `settings.s0.iDebounceTime` (configurable bounce rejection)
    - `pulseCount`, `last_pulse_duration` (volatile counters)

- `void sendS0Counters(void)`
  - **Location**: `s0PulseCount.ino:50`
  - **Signature**: Publishes S0 counter metrics to MQTT and updates global state
  - **Description**: Atomic read of volatile pulse counters (inside `noInterrupts()/interrupts()` block to prevent ISR race). Updates global accumulators: `OTGWs0pulseCount` (current interval), `OTGWs0pulseCountTot` (lifetime total), `OTGWs0powerkw` (calculated from pulse duration), `OTGWs0lasttime` (timestamp). Resets ISR `pulseCount` to 0 after read. Only publishes (calls `s0sendMQ()`) if `localPulseCount > 0` (reduces chatter). Calls `sensorAutoConfigure(OTGWs0dataid, true, "")` for HA auto-discovery on each publish (idempotent). Early return if S0 disabled.
  - **Acceptance Criteria Met**:
    - Atomic read of ISR state (critical section)
    - Power calculation: `(3600000 / iPulsekw / lastPulseDuration)` → kW
    - Lifetime accumulator protected
    - Silent when no pulses (no spam)
  - **Power Formula**: Converts pulse duration (ms) to instantaneous power in kW
    - `OTGWs0powerkw = 3600000 / settings.s0.iPulsekw / last_pulse_duration`
    - Example: iPulsekw=1000 (1000 pulses/kW), last_pulse_duration=1000ms → 3.6 kW
  - **Dependencies**:
    - `pulseCount`, `last_pulse_duration` (volatile ISR state)
    - `OTGWs0pulseCount`, `OTGWs0pulseCountTot`, `OTGWs0powerkw`, `OTGWs0lasttime` (persistent globals)
    - `settings.s0.bEnabled`, `settings.s0.iPulsekw`, `settings.s0.iInterval`
    - `s0sendMQ()` (MQTT publisher)
    - `sensorAutoConfigure()`, `OTGWs0dataid = 245` (HA auto-discovery)

- `void s0sendMQ(void)`
  - **Location**: `s0PulseCount.ino:77`
  - **Signature**: Publishes four S0 metrics to MQTT
  - **Description**: Formats and publishes: `s0pulsecount` (pulses this interval), `s0pulsecounttot` (lifetime total), `s0pulsetime` (duration of last pulse in ms), `s0powerkw` (instantaneous power). Uses `snprintf_P()` with PROGMEM format strings. Calls `sendMQTTData(otTopic, _msg)` for each metric. Shared scratch buffer `otTopic[OT_TOPIC_LEN=50]` and `_msg[15]` (stack local).
  - **Dependencies**:
    - `OTGWs0pulseCount`, `OTGWs0pulseCountTot`, `last_pulse_duration`, `OTGWs0powerkw`
    - `sendMQTTData()` (MQTT publisher)
    - `otTopic[]` (shared MQTT topic scratch buffer)

#### Global State

- `volatile uint16_t pulseCount` (module scope)
  - **Type**: `volatile uint16_t`
  - **Description**: Pulse counter updated by ISR; read atomically by `sendS0Counters()` and zeroed
  - **Location**: `s0PulseCount.ino:20`

- `volatile uint32_t last_pulse_duration` (module scope)
  - **Type**: `volatile uint32_t`
  - **Description**: Duration (ms) between last two valid pulses; updated by ISR; used for power calculation
  - **Location**: `s0PulseCount.ino:21`

---

### OLED Display Support (`OLED.ino`)

**Module Purpose**: SSD1306 I2C OLED display driver for real-time status visualization (page-cycled). Text-only, no framebuffer (saves ~1KB RAM vs Adafruit_SSD1306).

#### Functions

- `void initOLED(void)` (conditional: `#if HAS_OLED_CAPABLE`)
  - **Location**: `OLED.ino:249`
  - **Signature**: Initializes SSD1306 I2C OLED display at 0x3C
  - **Description**: Probes I2C address 0x3C for SSD1306 response. If found, initializes SSD1306Ascii library with System5x7 font (6px wide × 8px tall). Sets display mode to 128×64 pixels. Shows boot splash ("OTGW" or "OTGW32", firmware version). Attaches button ISR on `PIN_BUTTON` (FALLING edge) for page cycling. Sets initial state: `oledPresent = true`, `oledPage = 1` (dashboard), `oledLastActivity = millis()`. Early return (silently disables OLED) if probe fails.
  - **Acceptance Criteria Met**:
    - Runtime detection (no hard error if display absent)
    - Compact boot splash
    - Button ISR for page navigation
  - **Dependencies**:
    - `Wire.begin()` (I2C bus, must be called before this function)
    - `SSD1306Ascii`, `SSD1306AsciiWire` libraries
    - `PIN_BUTTON` (GPIO for page-cycle button)
    - `probeOLED()` (I2C probe helper)

- `void loopOLED(void)` (conditional)
  - **Location**: `OLED.ino:292`
  - **Signature**: Non-blocking OLED main loop, called once per `doBackgroundTasks()`
  - **Description**: Handles button debounce, page cycling, auto-off, and 1 Hz display refresh. Debounce: checks `oledButtonPressed` flag set by ISR, enforces `OLED_DEBOUNCE_MS` (300ms) gap. On valid press: increments `oledPage` (wraps 0→1, 1→2, 2→3, 3→4, 4→0). When `oledPage=0`, sends `DISPLAYOFF` command. When transitioning to page 1-4, sends `DISPLAYON`. Auto-off: if `oledPage != 0` and inactivity timeout (30s) exceeded, forces page 0 (display off). Refresh: throttles draw to 1 Hz (`OLED_REFRESH_MS = 1000ms`). Clears display and calls appropriate page renderer (`drawPageDashboard()`, `drawPageHC1()`, etc.). Early return if display not present.
  - **Acceptance Criteria Met**:
    - Software debounce (not hardware)
    - Non-blocking (cooperative timing)
    - Auto-off power saving
    - 1 Hz refresh (low CPU cost)
  - **Dependencies**:
    - `oledButtonPressed` flag (ISR sets)
    - `oledPage`, `oledLastActivity`, `oledLastRefresh`, `oledLastDebounce` (state)
    - `drawPageDashboard()`, `drawPageHC1()`, `drawPageHC2()`, `drawPageSystem()` (page renderers)

- `void oledWake(void)` (conditional)
  - **Location**: `OLED.ino:339`
  - **Signature**: Externally callable to wake display (e.g., on network state change)
  - **Description**: If display present and currently off (`oledPage == 0`), turns on and returns to page 1. Updates `oledLastActivity` to reset auto-off timer. Useful for alerting user to status changes without button press.
  - **Dependencies**:
    - `oledPresent`, `oledPage`

- `static void oledButtonISR(void)` (conditional, module scope)
  - **Location**: `OLED.ino:69`
  - **Signature**: ISR for button press (FALLING edge)
  - **Description**: Sets volatile flag `oledButtonPressed = true`. Actual debounce and page cycle logic in `loopOLED()`. Uses `IRAM_ATTR` (for ESP8266 ISR context).

- `static bool probeOLED(void)` (conditional, module scope)
  - **Location**: `OLED.ino:76`
  - **Signature**: Probes I2C for SSD1306 response at 0x3C
  - **Description**: Calls `Wire.beginTransmission(OLED_ADDR)` and checks `endTransmission()` return value. Returns true if device ACKs (endTransmission == 0).

#### Page Renderer Functions (conditional, all module scope)

- `static void drawHeader(const __FlashStringHelper* title)` (conditional)
  - **Location**: `OLED.ino:84`
  - **Signature**: Renders common header with title and page indicator
  - **Description**: Sets cursor to row 0, prints title in PROGMEM. Positions page counter on right (col 108). Fills row 1 with dashes as separator.
  - **Dependencies**: `oledPage`, `OLED_NUM_PAGES`

- `static void drawPageDashboard(void)` (conditional)
  - **Location**: `OLED.ino:102`
  - **Signature**: Page 1 — Dashboard: flame status, modulation, temperatures, system flags
  - **Description**: Displays: flame on/off + modulation %; CH/DHW active indicators; setpoint & boiler temps; return & DHW temps; system pressure & outside temp (from `OTcurrentSystemState` struct). Reads bit flags from `OTcurrentSystemState.SlaveStatus`: bit 3 = flame, bit 2 = DHW, bit 1 = CH.
  - **Dependencies**: `OTcurrentSystemState` (global OT state struct)

- `static void drawPageHC1(void)` (conditional)
  - **Location**: `OLED.ino:145`
  - **Signature**: Page 2 — Heating Circuit 1: room setpoint, room temp, flow & return temps
  - **Description**: Displays HC1-specific temps: room setpoint, room temp, flow temp, return temp.

- `static void drawPageHC2(void)` (conditional)
  - **Location**: `OLED.ino:172`
  - **Signature**: Page 3 — Heating Circuit 2: room setpoint, room temp, flow & DHW temps
  - **Description**: Displays HC2-specific temps: room setpoint CH2, room temp CH2, flow temp CH2, DHW temp.

- `static void drawPageSystem(void)` (conditional)
  - **Location**: `OLED.ino:199`
  - **Signature**: Page 4 — System Status: network, MQTT, OT bus, firmware version
  - **Description**: Displays: network mode (WiFi SSID or "Wired" if Ethernet) and IP; MQTT status; OT bus status; firmware version (major.minor.patch-prerelease).
  - **Dependencies**: `isNetworkUp()`, `networkModeName()`, `getActiveIP()`, `state.net.eMode`, `state.mqtt.bConnected`, `state.otBus.bOnline`, `_VERSION_*` macros

#### Display Characteristics

- **Hardware**: 128×64 pixel SSD1306 I2C display
- **Font**: System5x7 (6px wide, 8px tall) → 21 chars × 8 rows
- **I2C Address**: 0x3C (fixed, probed at init)
- **Refresh**: 1 Hz non-blocking (cooperative)
- **Auto-off**: 30s inactivity timeout
- **Button**: `PIN_BUTTON` (GPIO) with software debounce (300ms)
- **Pages**: 
  - 0 = Display off
  - 1 = Dashboard (flame, temps, CH/DHW status)
  - 2 = Heating Circuit 1
  - 3 = Heating Circuit 2
  - 4 = System Status
  - Cycle via button press

#### Global Display State (module scope, conditional)

- `static SSD1306AsciiWire oledDisplay`
  - **Description**: SSD1306 driver instance; initialized in `initOLED()`

- `static bool oledPresent`
  - **Description**: Display detected and initialized; guards all OLED operations

- `static uint8_t oledPage`
  - **Description**: Current page (0=off, 1-4=content)

- `static uint32_t oledLastActivity, oledLastRefresh, oledLastDebounce`
  - **Description**: Timing state for auto-off, refresh throttle, debounce

- `static volatile bool oledButtonPressed`
  - **Description**: Flag set by ISR, read by main loop

---

### GPIO Relay Outputs (`outputs_ext.ino`)

**Module Purpose**: Controls GPIO digital output pin (relay) based on OpenTherm status bit condition.

#### Functions

- `void initOutputs(void)`
  - **Location**: `outputs_ext.ino:30`
  - **Signature**: Initializes GPIO output pin and validates configuration
  - **Description**: Validates pin and trigger bit ranges. Configures pin as `OUTPUT`. Sets initial state to OFF. Sets `outputsInitialized = true`. Early return if outputs disabled or validation fails.
  - **Acceptance Criteria Met**:
    - Validation guards prevent invalid GPIO/bit use
    - Initial safe state (OFF)
  - **Validation Ranges**:
    - Pin: 0–16 (ESP8266 GPIO range)
    - Trigger bit: 0–15 (valid OT status bit range)
  - **Dependencies**:
    - `settings.outputs.bEnabled`, `settings.outputs.iPin`, `settings.outputs.iTriggerBit`
    - `validateGPIOOutputsConfig()` (static helper)

- `static bool validateGPIOOutputsConfig(void)` (static, module scope)
  - **Location**: `outputs_ext.ino:18`
  - **Signature**: Validates output pin and trigger bit configuration
  - **Description**: Checks pin 0–16, trigger bit 0–15. Returns false on invalid range; logs error. Called by `initOutputs()` and `setOutputState()` to guard I/O operations.

- `void setOutputState(uint8_t status = ON)` (overload 1)
  - **Location**: `outputs_ext.ino:47`
  - **Signature**: `void setOutputState(uint8_t status)` — Sets output HIGH or LOW
  - **Description**: Convenience overload: `status == ON` → calls `setOutputState(true)`, else `setOutputState(false)`. Uses `ON`/`OFF` macros (numeric constants).

- `void setOutputState(bool set_HIGH = true)` (overload 2)
  - **Location**: `outputs_ext.ino:51`
  - **Signature**: `void setOutputState(bool set_HIGH)` — Core output setter
  - **Description**: Writes GPIO output pin HIGH or LOW. Guards with `settings.outputs.bEnabled` and pin validation. Lazy-initializes pin if not yet done. Logs current pin state via `digitalRead()`.
  - **Acceptance Criteria Met**:
    - Guard on enable flag
    - Lazy init (safe for late-enable scenarios)
    - State confirmation via read-back
  - **Dependencies**:
    - `settings.outputs.iPin`
    - `validateGPIOOutputsConfig()`, `outputsInitialized`

- `void evalOutputs(void)`
  - **Location**: `outputs_ext.ino:62`
  - **Signature**: Evaluates OT status bit and updates output accordingly
  - **Description**: Core control logic: reads bit `settings.outputs.iTriggerBit` from `OTcurrentSystemState.Statusflags`; passes result to `setOutputState(bitState)`. Implements relay logic: output follows OT status bit state. Guards with enabled check and validation. Optional debug logging (when `settings.bMyDEBUG` flag is set, prints current pin state and evaluated bit state).
  - **Acceptance Criteria Met**:
    - Direct mapping of OT bit to output
    - Per-call validation
    - Debug logging for troubleshooting
  - **OT Status Bit Reference**:
    - Bit 0 (Master): CH enable
    - Bit 1 (Master): DHW enable
    - Bit 2 (Master): Cooling enable
    - Bit 3 (Slave): Flame status
    - And others (see comment in source, line 65–84)
  - **Dependencies**:
    - `OTcurrentSystemState.Statusflags` (OT protocol status)
    - `settings.outputs.iTriggerBit`, `settings.outputs.bEnabled`
    - `setOutputState()` (GPIO writer)

#### Global Output State (module scope)

- `static bool outputsInitialized`
  - **Description**: Tracks whether GPIO pin has been configured; allows lazy initialization
  - **Location**: `outputs_ext.ino:16`

---

## Dependencies

### Internal Dependencies

| Module | Function/Variable | Purpose |
|--------|-------------------|---------|
| `MQTTstuff.ino` | `sendMQTTData()` | Publish sensor/counter data to MQTT broker |
| `MQTTstuff.ino` | `sensorAutoConfigure()` | Register sensors with Home Assistant MQTT discovery |
| `MQTTstuff.ino` | `setMQTTConfigDone()`, `getMQTTConfigDone()` | Track auto-discovery completion per sensor type |
| `restAPI.ino` | `sendMQTTData()` overloads | JSON-aware MQTT publish |
| `OTGW-Core.ino` | `OTcurrentSystemState` | Global OpenTherm protocol state (temps, status bits) |
| `OTGW-Core.ino` | `doBackgroundTasks()` | Main loop calling `pollSensors()`, `sendS0Counters()`, `loopOLED()` |
| `helperStuff.ino` | `readJsonStringPair()`, `writeJsonStringPair()` | LittleFS label file I/O |
| `networkStuff.ino` | `isNetworkUp()`, `networkModeName()`, `getActiveIP()` | Network status for OLED System page |
| Settings (OTGW-firmware.h) | `settings.sensors.*`, `settings.s0.*`, `settings.outputs.*` | Configuration structs |
| State (OTGW-firmware.h) | `state.debug.bSensors`, `state.debug.bSensorSim` | Runtime flags |
| Global (OTGW-firmware.h) | `otTopic[]`, `cMsg[]` | Shared scratch buffers |

### External Dependencies (Libraries)

| Library | Version/Source | Used In | Purpose |
|---------|-----------------|---------|---------|
| `OneWire` | Arduino standard | `sensors_ext.ino` | 1-Wire protocol (DS18B20 bus) |
| `DallasTemperature` | Miles Burton's library | `sensors_ext.ino` | DS18B20 temperature reading |
| `SSD1306Ascii` | [Greiman/SSD1306Ascii](https://github.com/greiman/SSD1306Ascii) | `OLED.ino` | OLED text-only driver (RAM-efficient) |
| `SSD1306AsciiWire` | Part of SSD1306Ascii | `OLED.ino` | I2C transport for SSD1306 |
| `Wire` (I2C) | Arduino standard | `OLED.ino` | I2C master for OLED (and SAT/BLE) |
| `Arduino.h` | Arduino core | All `.ino` files | GPIO, millis(), interrupts() |

### Home Assistant Integration

| Sensor Type | Data ID | MQTT Topic Namespace | Auto-Discovery |
|-------------|---------|----------------------|-----------------|
| Dallas DS18B20 | 246 | `{topTopic}/value/{deviceAddress}` (from `getDallasAddress()`) | Via `sensorAutoConfigure()` |
| S0 Pulse Counter | 245 | `s0pulsecount`, `s0pulsecounttot`, `s0pulsetime`, `s0powerkw` | Via `sensorAutoConfigure()` |

---

## Hardware Configuration

### Dallas DS18B20 Sensors (1-Wire)

| Parameter | Setting | Notes |
|-----------|---------|-------|
| **GPIO Pin** | `settings.sensors.iPin` (default GPIO 13 = D7) | Configurable; bound after settings load (ADR-020) |
| **Bus Topology** | Parasitic, pull-up to 3.3V | Standard DS18B20 wiring |
| **Max Devices** | 16 (`MAXDALLASDEVICES`) | Enforced in `initSensors()` |
| **Async Conversion** | Enabled (`setWaitForConversion(false)`) | Non-blocking; conversion time ~750ms |
| **Resolution** | Default (12-bit, 0.0625°C) | Per DS18B20 default |
| **Disconnect Handling** | Graceful skip, retain prior value | Finding #29 |

### S0 Pulse Counter

| Parameter | Setting | Notes |
|-----------|---------|-------|
| **GPIO Pin** | `settings.s0.iPin` (default GPIO 12 = D6) | Interrupt-capable GPIO required |
| **Trigger** | FALLING edge (meter contact closes) | Meter pulls pin LOW on pulse |
| **Pull-up** | `INPUT_PULLUP` | Contact closure is NO (normally open) |
| **Debounce** | `settings.s0.iDebounceTime` (default 80ms) | Hardware debounce in ISR |
| **Pulses/kW** | `settings.s0.iPulsekw` (default 1000) | Meter-specific calibration |
| **Report Interval** | `settings.s0.iInterval` (default 60s) | Informational, no hard throttle |

### GPIO Relay Output

| Parameter | Setting | Notes |
|-----------|---------|-------|
| **GPIO Pin** | `settings.outputs.iPin` (default GPIO 16 = D0) | Standard GPIO |
| **Logic** | Active-HIGH (HIGH=on, LOW=off) | Relay usually triggered by HIGH |
| **Trigger Bit** | `settings.outputs.iTriggerBit` (default 0) | OpenTherm status bit index (0–15) |
| **OT Status Bit Map** | See code comments (line 65–84) | Master bits 0–4, Slave bits 0–6 |

### OLED Display (optional, SSD1306 I2C)

| Parameter | Setting | Notes |
|-----------|---------|-------|
| **Hardware** | 128×64 pixel SSD1306 | I2C OLED display |
| **I2C Address** | 0x3C (fixed, probed) | Runtime detection; no error if absent |
| **I2C Bus** | `Wire` (GPIO 4/5 on ESP8266) | Shared with other I2C devices (SAT, BLE, etc.) |
| **Font** | System5x7 (6px × 8px) | 21 chars wide, 8 rows tall |
| **Button** | `PIN_BUTTON` (GPIO 0 = D3) | Page cycling on FALLING edge |
| **Power Saving** | Auto-off after 30s inactivity | User can press button to wake |
| **Conditional Build** | `HAS_OLED_CAPABLE` | No impact if disabled or display absent |

---

## Data Flows

### Temperature Sensor Pipeline

```
Hardware (DS18B20)
    ↓
OneWire bus (GPIO pin)
    ↓
DallasTemperature.requestTemperatures() [non-blocking]
    ↓ (polling loop, every ~20s per settings)
pollSensors()
    ├─ sensors.getTempC(DeviceAddress) → DallasrealDevice[].tempC
    ├─ getDallasAddress() → 16-char hex string (or legacy format)
    ├─ sendMQTTData(strDeviceAddress, "NN.N°C") → MQTT publish
    └─ LittleFS /dallas_labels.ini (label persistence)
    ↓
MQTT Broker (Home Assistant)
    ├─ Auto-discovery (configSensors() → sensorAutoConfigure())
    └─ Real-time updates (s0pulsecount, s0pulsetime, etc.)
```

### S0 Pulse Pipeline

```
Hardware (Energy Meter S0 Contact)
    ↓ (FALLING edge)
GPIO Interrupt
    ↓
IRQcounter() ISR [debounced]
    ├─ pulseCount++ [atomic]
    └─ last_pulse_duration = time_delta [atomic]
    ↓ (periodic read, every 60s per settings)
sendS0Counters()
    ├─ Atomic read (noInterrupts/interrupts)
    ├─ OTGWs0pulseCount = pulseCount
    ├─ OTGWs0pulseCountTot += pulseCount [lifetime accumulator]
    ├─ OTGWs0powerkw = 3600000 / iPulsekw / last_pulse_duration [power calc]
    └─ s0sendMQ() → MQTT publish
    ↓
MQTT Broker
    ├─ s0pulsecount (interval count)
    ├─ s0pulsecounttot (lifetime total)
    ├─ s0pulsetime (last pulse duration ms)
    └─ s0powerkw (instantaneous power kW)
```

### GPIO Output Control Pipeline

```
OpenTherm PIC (serial or direct GPIO)
    ↓
OTcurrentSystemState.Statusflags [updated by OT decode]
    ↓
evalOutputs() [called from main loop]
    ├─ Read bit N from Statusflags
    └─ setOutputState(bitState)
    ↓
GPIO Output Pin [digitalWrite(iPin, HIGH/LOW)]
    ↓
Relay/Switch (e.g., auxiliary heating zone)
```

### OLED Display Refresh Loop

```
millis() timer [non-blocking, 1 Hz]
    ↓
loopOLED() [cooperative main loop]
    ├─ Handle button ISR (debounced page cycle)
    ├─ Check auto-off timeout (30s)
    ├─ Draw page (Dashboard / HC1 / HC2 / System)
    │  ├─ readPageDashboard() → OTcurrentSystemState (flame, temps, flags)
    │  ├─ drawPageHC1() → OTcurrentSystemState (HC1 temps)
    │  ├─ drawPageHC2() → OTcurrentSystemState (HC2 temps)
    │  └─ drawPageSystem() → Network state, MQTT status, OT bus status
    └─ I2C write to SSD1306
```

---

## Settings & Configuration

### Persistent Settings (OTGW-firmware.h / restAPI.ino)

```c
struct SensorsSection {
  bool    bEnabled       = false;      // Enable Dallas sensor monitoring
  bool    bLegacyFormat  = false;      // Backward compat: legacy 1-Wire address format
  int8_t  iPin           = 10;         // GPIO pin for 1-Wire bus
  int16_t iInterval      = 20;         // Poll interval (seconds)
};

struct S0Section {
  bool     bEnabled      = false;      // Enable S0 pulse counter
  uint8_t  iPin          = 12;         // GPIO pin for S0 interrupt
  uint16_t iDebounceTime = 80;         // Debounce window (milliseconds)
  uint16_t iPulsekw      = 1000;       // Pulses per kWh (meter calibration)
  uint16_t iInterval     = 60;         // Reporting interval (seconds, informational)
};

struct OutputsSection {
  bool   bEnabled    = false;          // Enable GPIO relay output
  int8_t iPin        = 16;             // GPIO pin for relay
  int8_t iTriggerBit = 0;              // OT status bit to monitor (0–15)
};
```

### Runtime State (OTGW-firmware.h)

```c
// Sensor detection state (separate from settings.sensors.bEnabled)
bool      bSensorsDetected = false;     // true if sensors initialized this boot
int       DallasrealDeviceCount = 0;    // Count of detected real sensors
DeviceAddress DallasrealDevice[16];     // Detected sensor addresses & temps

// S0 counter state
volatile uint16_t pulseCount = 0;       // Updated by ISR
volatile uint32_t last_pulse_duration = 0; // Time between pulses (ms)
uint16_t  OTGWs0pulseCount = 0;         // Pulses this interval
uint32_t  OTGWs0pulseCountTot = 0;      // Lifetime total
float     OTGWs0powerkw = 0.0f;         // Instantaneous power (kW)
time_t    OTGWs0lasttime = 0;           // Timestamp of last read
```

---

## Important Design Notes

### ADR-020: Deferred OneWire Initialization
The `OneWire` object uses the default constructor (no pin argument), deferring bus initialization to `initSensors()` after `readSettings()` loads the correct GPIO pin. This prevents hardcoded I/O during header inclusion and improves testability.

### Finding #29: Graceful Sensor Disconnect Handling
If a DS18B20 returns `DEVICE_DISCONNECTED_C` from `getTempC()`, the sensor is silently skipped in `pollSensors()` and the previous temperature value is retained. No error is thrown; monitoring continues on remaining sensors. This improves system resilience when a single sensor fails or disconnects mid-operation.

### Power Calculation Formula
S0 pulse duration is converted to instantaneous power via:
```
OTGWs0powerkw = 3600000 / settings.s0.iPulsekw / last_pulse_duration
```
Where:
- 3600000 = 3600 seconds/hour × 1000 ms/second (converts ms interval to hour-based rate)
- `iPulsekw` = pulses per kWh (meter calibration, default 1000)
- `last_pulse_duration` = milliseconds between last two pulses

Example: If iPulsekw=1000 and last pulse was 1000ms apart:
- Power = 3600000 / 1000 / 1000 = 3.6 kW

### MQTT Auto-Discovery
Both Dallas sensors and S0 counter integrate with Home Assistant via MQTT discovery:
- **Dallas**: Uses pseudo-dataid 246; discovery topic derived from device ROM address
- **S0**: Uses pseudo-dataid 245; fixed topics (`s0pulsecount`, `s0pulsecounttot`, `s0pulsetime`, `s0powerkw`)

Discovery is idempotent and repeats on each sensor publish (safe; HA deduplicates).

### Simulation Mode
Dallas sensors can be simulated for testing/demo without physical 1-Wire bus:
- Set `state.debug.bSensorSim = true` (runtime flag, not persisted)
- Three virtual sensors created with starting temps: 30°C, 35°C, 40°C
- Temps drift randomly (±0.5°C per update interval) within 20–60°C bounds
- Updates every 10 seconds (separate from physical read throttle)

---

## Testing & Debugging

### Telnet Debug Output
Enable via `state.debug.bSensors = true` or `state.debug.bSensorSim = true` (telnet port 23):
- DS18B20: `"Sensor device no[N] addr[HEX] TempC: X.X"`
- S0: `"*** S0PulseCount(N) S0PulseCountTot(M) S0LastPulsetime(Nms) S0Pulsekw:(X.XXX)"`

### Configuration Validation
All three modules validate configuration at init time:
- **Sensors**: Pin must be 0–16; guarded early return if invalid
- **S0**: Pin must be interrupt-capable; debounce > 0
- **Outputs**: Pin 0–16, trigger bit 0–15; validation called on every `setOutputState()`
- **OLED**: Probed at I2C 0x3C; silently disabled if absent

### Common Issues

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| No sensor readings | `settings.sensors.bEnabled = false` OR no sensors on bus OR bus pin mismatch | Check settings, verify 1-Wire wiring, probe with separate tool |
| S0 pulses not counted | ISR not firing (pin not interrupt-capable OR incorrect pin) OR debounce too high | Verify pin supports interrupts; check debounce setting; scope the pin |
| Output always OFF | `settings.outputs.bEnabled = false` OR incorrect trigger bit | Enable in web UI; confirm OT status bit mapping |
| OLED blank | Display not at 0x3C OR I2C bus conflict (Wire not initialized) | Probe I2C; check display address; ensure `Wire.begin()` called first |

---

## File Locations Summary

| File | Functions | Purpose |
|------|-----------|---------|
| `src/OTGW-firmware/sensors_ext.ino` | `initSensors()`, `pollSensors()`, `configSensors()`, `ensureSensorDefaultLabels()`, `getDallasAddress()`, `initSimulatedDallasSensors()` | DS18B20 1-Wire sensor management |
| `src/OTGW-firmware/s0PulseCount.ino` | `initS0Count()`, `sendS0Counters()`, `s0sendMQ()`, `IRQcounter()` | S0 energy meter pulse counting |
| `src/OTGW-firmware/OLED.ino` | `initOLED()`, `loopOLED()`, `oledWake()`, `drawPage*()` | SSD1306 I2C OLED display driver |
| `src/OTGW-firmware/outputs_ext.ino` | `initOutputs()`, `setOutputState()`, `evalOutputs()`, `validateGPIOOutputsConfig()` | GPIO relay output control |
| `src/OTGW-firmware/OTGW-firmware.h` | Data structures, global variables, forward declarations | Configuration and state types |

---

## Mermaid Diagram: Sensor & I/O Architecture

```mermaid
---
title: Sensors & I/O Module Architecture
---
graph TB
    subgraph Hardware
        DS18B20["DS18B20<br/>1-Wire Bus"]
        S0["S0 Energy<br/>Meter Contact"]
        Relay["GPIO Relay<br/>Output"]
        OLED["SSD1306<br/>I2C OLED"]
    end

    subgraph "GPIO Interfaces"
        OneWire["OneWire Bus<br/>(GPIO sensor pin)"]
        S0INT["S0 Interrupt<br/>(GPIO 12)"]
        OutGPIO["Output GPIO<br/>(GPIO 16)"]
        I2C["I2C Wire<br/>(GPIO 4/5)"]
    end

    subgraph "Sensor Module (sensors_ext.ino)"
        Init["initSensors()"]
        Poll["pollSensors()"]
        Config["configSensors()"]
        GetAddr["getDallasAddress()"]
        Labels["ensureSensorDefaultLabels()"]
    end

    subgraph "S0 Module (s0PulseCount.ino)"
        S0Init["initS0Count()"]
        S0ISR["IRQcounter() ISR"]
        S0Send["sendS0Counters()"]
        S0MQ["s0sendMQ()"]
    end

    subgraph "Output Module (outputs_ext.ino)"
        OutInit["initOutputs()"]
        OutSet["setOutputState()"]
        OutEval["evalOutputs()"]
        Validate["validateGPIOOutputsConfig()"]
    end

    subgraph "OLED Module (OLED.ino)"
        OLEDInit["initOLED()"]
        OLEDLoop["loopOLED()"]
        Draw["drawPage*()"]
        Probe["probeOLED()"]
    end

    subgraph "State & Configuration"
        Settings["settings.sensors<br/>settings.s0<br/>settings.outputs"]
        State["state.debug<br/>OTcurrentSystemState"]
        Globals["DallasrealDevice[]<br/>pulseCount<br/>OTGWs0*"]
    end

    subgraph "MQTT Integration"
        MQTT["sendMQTTData()"]
        AutoCfg["sensorAutoConfigure()"]
    end

    subgraph "LittleFS Persistence"
        Labels_File["/dallas_labels.ini"]
    end

    DS18B20 -->|1-Wire| OneWire -->|GPIO pin| Init
    Init --> Poll
    Poll --> GetAddr
    GetAddr --> Labels
    Labels -->|read/write| Labels_File
    Poll --> Config
    Config --> AutoCfg
    Poll -->|publish temp| MQTT

    S0 -->|FALLING edge| S0INT -->|interrupt| S0ISR
    S0ISR -->|volatile update| Globals
    S0Init -->|attach ISR| S0INT
    S0Send -->|atomic read| Globals
    S0Send --> S0MQ
    S0MQ -->|publish| MQTT
    S0Send --> AutoCfg

    OutInit -->|configure| OutGPIO
    OutEval -->|read bit| State
    OutEval --> OutSet
    OutSet -->|validate| Validate
    OutSet -->|digitalWrite| OutGPIO
    OutGPIO -->|control| Relay

    OLEDInit -->|probe| I2C
    Probe -->|Wire.beginTransmission| I2C
    OLEDInit --> OLEDLoop
    OLEDLoop --> Draw
    Draw -->|read state| State
    Draw -->|write| I2C
    I2C -->|display| OLED

    Settings -->|configure| Init
    Settings -->|configure| S0Init
    Settings -->|configure| OutInit
    Settings -->|configure| OLEDInit
    State -->|live state| Poll
    State -->|live state| OutEval
    State -->|live state| Draw

    style Hardware fill:#e1f5ff
    style "GPIO Interfaces" fill:#fff3e0
    style "Sensor Module (sensors_ext.ino)" fill:#f3e5f5
    style "S0 Module (s0PulseCount.ino)" fill:#f3e5f5
    style "Output Module (outputs_ext.ino)" fill:#f3e5f5
    style "OLED Module (OLED.ino)" fill:#f3e5f5
    style "State & Configuration" fill:#e8f5e9
    style "MQTT Integration" fill:#fce4ec
    style "LittleFS Persistence" fill:#f1f8e9
```

---

## Summary

This C4 Code-level documentation captures the complete Sensors & I/O module of OTGW-firmware:

1. **Dallas DS18B20 Temperature Sensors** (`sensors_ext.ino`): 1-Wire bus management, async non-blocking reads, automatic label persistence, graceful disconnect handling, Home Assistant auto-discovery
2. **S0 Energy Meter Counter** (`s0PulseCount.ino`): Interrupt-driven pulse counting with debounce, power calculation, atomic state reads, MQTT publishing
3. **GPIO Relay Outputs** (`outputs_ext.ino`): OT status bit monitoring, GPIO control, configuration validation
4. **OLED Display** (`OLED.ino`): Runtime I2C detection, text-only efficient rendering, 4-page dashboard, auto-off power saving, button-driven navigation

All modules integrate with MQTT for Home Assistant, use persistent settings, and provide runtime debug logging via telnet. Hardware detection is graceful (no hard failure if optional hardware absent). Code follows ESP8266 resource constraints: PROGMEM strings, minimal RAM usage (especially OLED with no framebuffer), cooperative non-blocking I/O.
