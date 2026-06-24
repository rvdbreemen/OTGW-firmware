# C4 Code Level: SAT (Smart Autotune Thermostat) Subsystem

## Overview

- **Name**: SAT Subsystem
- **Description**: Embedded Smart Autotune Thermostat implementation for OpenTherm heating control, ported from the SAT Python custom component with support for multiple heating systems, PID auto-tuning, cycle classification, pressure monitoring, weather integration, and BLE sensor support.
- **Location**: `/src/OTGW-firmware/SAT*.ino` (SATcontrol.ino, SATcycles.ino, SATpid.ino, SATpressure.ino, SATweather.ino, SATble.ino)
- **Language**: Arduino C++ (ESP8266/ESP32)
- **Purpose**: Autonomous heating control via heating curves, PID feedback, cycle detection, and multi-source temperature inputs; optional fallback when external control is lost.

## Code Elements

### Module: SATcontrol.ino

Central control loop, heating curve calculation, setpoint management, MQTT command dispatch, OPV calibration, and boiler status tracking.

#### Key Functions

- `satControlLoop(): void` (line 3354+)
  - Main control loop entry point (called from doBackgroundTasks)
  - Handles simulation, thermal learning, fallback detection, flame edge detection, cycle sampling, PID update, setpoint calculation

- `satCalcHeatingCurve(float targetTemp, float outsideTemp): float` (line 371-386)
  - Calculates setpoint via heating curve formula
  - Formula: baseOffset + (coeff / 4.0) × [4×(target - 20) + 0.03×(outside - 20)² - 0.4×(outside - 20)]
  - Stores result in state.sat.fHeatingCurveValue

- `satHandlePreset(const char* value): void` (line 741-791)
  - Switches between named temperature presets (away, eco, comfort, sleep, activity, home, none)
  - Saves pre-custom temperature for restore when preset is cleared (Task #67)

- `satHandleWindow(bool isOpen): void` (line 796-819)
  - Processes window open/close events for anti-heating response

- `satPublishMQTT(): void` (line 1690+)
  - Publishes all SAT state variables to MQTT topics (sat/target, sat/mode, sat/pid_output, etc.)

- `satDetectManufacturer(uint8_t slaveMemberID): void` (line 129-144)
  - Auto-detects boiler manufacturer from OT MsgID 3 slave MemberID

- `satOvpCalibrate(): void` (line 254-333)
  - OPV calibration state machine: STARTING, WARMING, MEASURING, DONE/FAILED

#### Constants (PROGMEM)

- `satManufacturerTable[]` (line 74-94)
  - PROGMEM lookup: MemberID to manufacturer name and quirks flags (18 manufacturers)

### Module: SATcycles.ino

Cycle tracking, on/off transition detection, cycle classification, rolling statistics aggregation.

#### Key Functions

- `satCycleOnFlameChange(bool flameNowOn): void`
  - Handles flame ON/OFF transition: completes cycle, classifies, updates statistics

- `satCycleSample(): void`
  - Samples flow temp, DHW state, accumulates cycle metrics

- `satGetWindow4hStats(): void`
  - Updates rolling 4-hour cycle statistics (Task #227)

- `satCycleIsHourLimitReached(): bool`
  - Checks rolling 60-min window for max cycles/hour reached (Task #203)

#### Data Structures

- `_hourCycleTs[SAT_MAX_CYCLES_PER_HOUR]` (line 38)
  - Ring buffer of millis() timestamps for flame-on events (rolling 60-min window)

- `_win4h[SAT_WIN4H_SIZE]` (line 46)
  - Rolling 4-hour cycle statistics window (Task #227)

- `_flow_samples[SAT_FLOW_SAMPLE_SIZE]` (line 67)
  - Per-cycle flow temperature samples for p90/p10 classifier (Task #225)

- `_hcr_dailyMedian[HCR_DAYS]` (line 144)
  - Heating curve recommendation: rolling window of daily median errors (Task #228)

### Module: SATpid.ino

PID controller with automatic gain calculation, deadband mode, temperature-based derivative.

#### Key Functions

- `satPidReset(): void` (line 51-71)
  - Full PID state reset

- `satResetIntegral(): void` (line 43-48)
  - Resets PID integral to 0 (debug tool)

- `_pidCalculateGains(float curveValue): void` (static, line 77-97)
  - Auto-calculates Kp/Ki/Kd from heating curve value
  - Respects manual gains if bAutoGains=false

- `_pidUpdateIntegral(float error, float curveValue, bool force): void` (static, line 104-130)
  - Updates integral term only inside deadband
  - Task #23: skips accumulation if bSolarGainActive

- `_pidUpdateDerivative(float roomTemp): void` (static, line 138+)
  - Temperature-based derivative with adaptive low-pass filter
  - Freezes inside deadband, updates outside

### Module: SATpressure.ino

CH water pressure monitoring, health status, drop-rate detection (Task #226).

#### Key Functions

- `satPressureHealthUpdate(): void` (line 33-54)
  - Updates state.sat.fBoilerPressure and state.sat.sPressureStatus

- `satPressureHealthPublish(): void` (line 62-72)
  - Publishes sat/ch_pressure and sat/ch_pressure_status to MQTT

### Module: SATweather.ino

Open-Meteo API integration for outdoor temperature, humidity, wind, and 24-hour forecast (no API key required). Single-pass streaming JSON parse — character-by-character over the response stream, no full buffer materialisation, no ArduinoJson. The Open-Meteo URL template is per-platform: ESP8266 uses a minimal 5-field projection, ESP32 fetches the full thermal data set. A 5-minute startup gate (Phase 0 onboarding, TASK-511) suppresses the first request until WiFi is stable. Telnet shortcut `w` triggers an immediate fetch and dumps the parsed state (TASK-513).

#### Key Functions

- `weatherJsonGetFloat(const char* json, PGM_P key, float* out): bool` (static)
  - Extracts "key": <number> from JSON string

- `weatherJsonGetArray(const char* json, PGM_P key, float* arr, uint8_t maxLen, uint8_t* count): bool` (static)
  - Extracts JSON array from "hourly" section (24-hour temperature forecast)

- `satWeatherUpdate(): void`
  - Fetches current weather + 24-hour forecast from Open-Meteo API

### Module: SATble.ino (ESP32 Only) — NimBLE-Arduino 2.x (ADR-092)

BLE advertisement scanning for temperature/humidity sensors, built on NimBLE-Arduino 2.x (TASK-487/488 replaced the classic Bluedroid stack — smaller RAM/flash footprint, scan-callback API). Uses the magic-zero idiom: scan window/interval and result-list cap configured so NimBLE only fires the callback and never builds an internal result list (keeps heap pressure flat under continuous scanning).

Operates in **continuous-scan** mode (TASK-494): the scan runs uninterrupted on the BLE host task; advertisements flow into a per-MAC roster (`SAT_BLE_MAX_ROSTER = 8`) that is settings-backed (`settings.sat.sBleMac[i]`) and survives reboot. Per-ad debug logging was replaced by aggregated periodic scan-stats (TASK-506) to cut spam.

Auto-discovery: every format-passing MAC enters the roster on first sight. The frontend (`/api/v2/sat/ble/*`) lets the user promote a roster slot to the active sensor, attach a persistent label, or forget it. HA discovery for each roster MAC is published lazily (`bDiscoveryPublished`) and re-published on label change (`bDiscoveryDirty`).

#### Key Functions

- `parseBLEAtcFormat(const uint8_t* data, size_t len, float* temp, float* hum, uint8_t* batt): bool` (static)
  - Parses ATC/pvvx custom firmware format (service data UUID 0x181A)

- `parseBLEBTHomeFormat(const uint8_t* data, size_t len, float* temp, float* hum, uint8_t* batt): bool` (static)
  - Parses BTHome v2 format (service data UUID 0xFCD2)

- `parseBLEMiBeaconFormat(const uint8_t* data, size_t len, float* temp, float* hum, uint8_t* batt): bool` (static)
  - Parses plaintext Xiaomi MiBeacon format (service data UUID 0xFE95) for stock Mijia sensors (TASK-930 Phase 1; object TLV 0x1004/0x1006/0x100A/0x100D). Encrypted MiBeacon (frame-control bit 3) is skipped — deferred to a gated Phase 2 (AES-CCM + per-slot bindkey), see ADR-153.

- `SATBLEScanCallbacks::onResult(const NimBLEAdvertisedDevice*)`
  - NimBLE 2.x scan callback. Runs on the BLE host task; defers `flushSettings()` to the loop task so writes never happen from BLE context.

### Multi-Area Room Temperature Mapping (Task #25)

Up to 4 weighted room-temperature areas. Each area accepts either a DS18B20 sensor address (mapped via `/api/v2/sat/sensor-areas`) or an MQTT/BLE-pushed float. The SAT control loop computes a weighted average when `settings.sat.bMultiArea` is true and at least one valid input is fresh. The frontend persists the address→area mapping; the firmware caches the current float per area in `state.sat.fAreaTemp[4]`.

## Dependencies

### Internal Dependencies

- OTGW-firmware.h (type definitions, enums, state structures)
- MQTTstuff.ino (MQTT dispatch, sendMQTTData)
- OTGW-Core.ino (OTcurrentSystemState, command queue)
- settingStuff.ino (settings persistence)
- networkStuff.h (MQTT client, WiFi state)
- safeTimers.h (DECLARE_TIMER_SEC, DUE macro)
- helperStuff.h (utility functions, debug macros)

### External Dependencies

- Arduino core (millis, delay, Serial, digitalWrite, pinMode)
- esp8266/esp32 HAL (PROGMEM, pgm_read_byte, strncpy_P, snprintf_P)
- NimBLEDevice.h (ESP32 BLE scanning, NimBLE-Arduino 2.x per ADR-092 — Bluedroid replaced TASK-487/488; continuous scan in callback-only mode, TASK-494)
- Open-Meteo API (HTTP GET for weather data; free, no API key)

## Settings (Persistent Configuration)

All SAT settings stored in `settings.sat` (SATSection struct, OTGW-firmware.h:829-928):

- `bEnabled: bool` - Enable/disable SAT controller
- `iHeatingSystem: uint8_t` - SAT_HSYS_AUTO/RADIATORS/UNDERFLOOR/HEAT_PUMP
- `fTargetTemp: float` - Current room target (°C)
- `fHeatingCurveCoeff: float` - Heating curve coefficient
- `iControlInterval: uint16_t` - Control loop interval (seconds)
- `fPresetComfort/Eco/Away/Sleep/Activity/Home: float` - Temperature presets
- `bPwmAutoSwitch: bool` - Auto-switch between PWM and continuous mode
- `fOvpValue: float` - Overshoot Protection Value (calibrated max boiler temp)
- `bWeatherEnable: bool` - Enable weather data fetching
- `bAutoGains: bool` - true=automatic PID gain formula; false=manual Kp/Ki/Kd
- `bBleEnable: bool` (ESP32 only) - Enable BLE sensor scanning

## Runtime State (Transient)

All SAT runtime state stored in `state.sat` (SATRuntimeSection struct, OTGW-firmware.h:452-626):

- `bActive: bool` - SAT currently controlling boiler
- `eControlMode: SATControlMode` - OFF/CONTINUOUS/PWM
- `eBoilerStatus: SATBoilerStatus` - OFF/IGNITION_SURGE/BURNER_ON/etc
- `fHeatingCurveValue: float` - Output of heating curve
- `fPidOutput: float` - Final PID output (curve + P + I + D)
- `fFinalSetpoint: float` - Setpoint sent to boiler
- `eLastCycleClass: SATCycleClass` - Classification of last completed cycle
- `iCycleCount: uint32_t` - Total number of cycles since enable
- `iCyclesThisHour: uint8_t` - Flame-on events in rolling 60-min window
- `f4hAvgOnSec/f4hAvgOffSec: float` - Average cycle durations in last 4h
- `fDutyRatio: float` - EMA flame-on fraction
- `fSmoothedPressure: float` - EMA CH water pressure
- `sPressureStatus: char[8]` - "ok"/"low"/"high"
- `weather.*: struct` - Current outdoor temp, humidity, wind
- `fCurrentPower: float` - Current power in kW
- `fEnergyTotal: float` - Cumulative energy in kWh
- `bSolarGainActive: bool` - Solar gain compensation active (freezes integral)
- `fBleTemp: float` (ESP32) - BLE sensor temperature
- `fAreaTemp[4]: float` - Multi-area room temperatures (Task #25)

## Notes

- **Storage Optimization**: Conditional compilation scales buffers for ESP8266 vs ESP32
- **PROGMEM Safety**: Manufacturer lookup in flash; safe string access with pgm_read_byte
- **Re-entrancy**: Static locals preserve state across doBackgroundTasks re-entry
- **Fallback Mode**: Auto-enables when MQTT lost >5 minutes (Task #19)
- **Thermal Learning**: Learns building thermal decay rate during flame-off (Task #21)
- **Solar Gain Compensation**: Freezes integral when indoor temp rises unexpectedly (Task #23)
- **Weather Integration**: Free Open-Meteo API for outdoor temp and 24-hour forecast
- **Multi-Area Support**: Up to 4 weighted room temperature inputs (Task #25)
- **Pressure Monitoring**: EMA smoothing and drop-rate detection (Task #226)
- **Energy Tracking**: Power calculation from modulation and boiler capacity (Task #45)
- **Simulation Mode**: Test SAT logic without real boiler (Task #37)
- **Heating Curve Recommendation**: Daily median error tracking for coefficient adjustment (Task #228)
- **Window Detection**: Detects rapid indoor temp drop; summer simmer disables heating (Task #24/#29)
- **PID Auto-Tuning**: Analyzes cycle performance, adjusts gains incrementally (Task #27)
- **Modulation Suppression**: Anti-hunting control after flame ignition
- **Preset Sync**: Broadcasts preset changes to secondary entities via MQTT (Task #46)
