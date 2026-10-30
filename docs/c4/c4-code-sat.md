# C4 Code Level: SAT (Smart Autotune Thermostat) Subsystem

## Overview

- **Name**: SAT Subsystem
- **Description**: Embedded Smart Autotune Thermostat implementation for OpenTherm heating control, ported from the SAT Python custom component with support for multiple heating systems and sources, PID auto-tuning, cycle classification, pressure monitoring, weather integration, multi-area and multi-zone inputs, PV-surplus boost, a boiler-less simulation mode and BLE sensor support.
- **Location**: `/src/OTGW-firmware/SAT*.ino`, `SATtypes.h`, `SATmqttPublish.{h,cpp}` (SATcontrol.ino, SATcycles.ino, SATpid.ino, SATpressure.ino, SATweather.ino, SATble.ino)
- **Language**: Arduino C++ (ESP32-S3; the 2.0.0 line has no ESP8266 target)
- **Purpose**: Autonomous heating control via heating curves, PID feedback, cycle detection, and multi-source temperature inputs; optional fallback when external control is lost. While SAT is enabled it owns the OpenTherm control setpoint (MsgID 1, ADR-179).

## Code Elements

### Module: SATcontrol.ino

Central control loop, heating curve calculation, setpoint management, SAT-owned OT command emission, MQTT command handlers, simulation, and boiler status tracking. Entry points are called from `OTGW-firmware.ino`: `initSAT()` in setup, `satControlLoop()`, `satBLELoop()` and `weatherLoop()` in the main loop.

#### Key Functions

- `satControlLoop(): void`
  - Main control loop entry point. Order of work: deferred PID restore (needs NTP), boiler-detected teardown for simulation, command-cache reset on an OT-bus offline-to-online edge, fallback detection (MQTT lost >5 min), early bail when SAT is disabled or a flash is running, `satUpdateSimulation()`, `satUpdateThermalLearning()`, flame edge detection, `satCycleSample()`, once-a-minute 4 h and 24 h window stats, `satPicCSKeepalive()`, `satPwmSuppressionRefresh()` (3 s, TASK-1268), `satCmdResend()` (5 s repeat of the last CS/MM/CH, TASK-1370, ADR-203), then (on the `timerSATControl` tick) DHW detection, power/energy update and save (also during DHW, TASK-1258), the DHW branch, room/outside inputs, 45 s room-temperature EMA (TASK-894), pressure, solar gain, summer simmer, TRV valves, auto-tune, curve recommendation, boiler status, comfort, PV boost, heating curve, PID, optional multi-zone P75 aggregation, clamping, PWM or continuous mode, modulation value, COLD_SETPOINT cutoff, and finally the CS/MM/CH sends (every tick, TASK-1370), the TC enqueue (on change only) and `satPublishMQTT()`
  - Safety trips (`bSafetyTripped`, then `satDisable()`) after `SAT_MAX_SKIP_COUNT` (10) consecutive invalid room readings or `SAT_MAX_PIC_FAILS` (5) control ticks without an OT command interface

- `initSAT(): void`
  - Restores PID, energy, estimated-energy and heating-curve-recommendation state, sends boot-safety `CS=0` (deferred to the first loop when no command interface exists yet), calls `satBLEInit()`, and sends `PM=3`, `PM=15`, `PM=48` (plus `MI=500` for the Ideal/Intergas/Nefit quirk) when SAT is enabled

- `satCalcHeatingCurve(float targetTemp, float outsideTemp): float` (static)
  - Formula: baseOffset + (coeff / 4.0) x [4x(target - 20) + 0.03x(outside - 20)^2 - 0.4x(outside - 20)]
  - Base offset 20.0 (underfloor) or 27.2 (radiators) from `satGetBaseOffset()`; stores the result in `state.sat.fHeatingCurveValue`

- `satGetRoomTemp(): float` (static)
  - Room temperature source priority: simulation, then BLE (`bBleEnable` and fresh), then multi-area weighted average (`satGetWeightedRoomTemp()`), then MQTT/REST external temperature (`iSensorMaxAgeS` staleness), then OT MsgID 24 (`Tr`, stale after `SAT_STALE_TEMP_MS` of 5 min when it came from a thermostat, TASK-1157). Returns NAN when no source is valid; thermal comfort substitutes the Summer Simmer Index when humidity is fresh

- `satOwnsControlSetpoint(): bool` (ADR-179, TASK-1150)
  - True while `settings.sat.bEnabled`; deliberately not keyed on `bActive`, so a safety trip or the boot window does not hand TSet back to the OT-Direct heating curve. Used by OTDirect.ino (heating curve, thermostat MsgID 1 writes, CS heartbeat expiry) and by the CS= refusal in both command paths

- `satEnqueueOwnCS(const char* cmd): void` (static) and `satCommandInFlight(): bool`
  - Every SAT-originated `CS=` (including the `CS=0` releases in `satDisable()`, boot safety and the keepalive) goes through `satEnqueueOwnCS()`, which sets `_satCmdInFlight` around `addCommandToQueue()`. `handleOTDirectCommand()` (OTDirect.ino) and `addCommandToQueue()` (OTGW-Core.ino, TASK-1236) refuse any `CS=` while `satOwnsControlSetpoint() && !satCommandInFlight()`, so an external CS= is rejected on both transports (OT-Direct answers `NG`). Bytes written straight to the PIC over TCP port 25238 bypass the queue and stay unfiltered (ADR-177)

- `satSendCS/MM/CH(...)`, `satCmdResend()`, `satEnqueueIfChangedCS/TC(...): bool` (static)
  - Command cadence (TASK-1370, ADR-203, Sergeant D entry 44): the four control-tick paths call `satSendCS/MM/CH()`, which send every tick, unchanged values included, and update the last-sent cache. `satCmdResend()` repeats the last sent CS, MM and CH every `SAT_CMD_RESEND_MS` (5 s) after the last CS send; it is gated like the other senders and disarmed by a DHW or boiler-fault tick. `satEnqueueIfChangedCS/TC()` stay change-checked (CS and TC rounded to 0.1, or `SAT_CMD_REFRESH_MS` (300 s) elapsed): CS for the 3 s PWM suppression refresh, TC because it is event-driven (TASK-1272). `satResetCmdCache()` (OT-bus offline-to-online edge, `satDisable()`, boot) stops the resend until the next tick sends. Only CS goes through `satEnqueueOwnCS()`; MM, CH, TC use plain `addCommandToQueue()`

- `satPicCSKeepalive(): void` (static, TASK-1236)
  - PIC boards only (`isPICEnabled()`): re-sends the last CS every `SAT_PIC_CS_KEEPALIVE_MS` (30 s) while the last value is 8 C or more, because the PIC drops a CS 63-126 s after the last one (gateway.asm CommandExpiry) and SAT's own cadence is slower. OT-Direct needs no keepalive: it skips its CS heartbeat expiry while SAT owns MsgID 1

- `satSimulationBlocksBusTx(const char* cmd, const __FlashStringHelper* source): bool`
  - Bus-transmit gate (TASK-795 plan 4.1/4.3). True when `settings.sat.bSimulation` or `state.debug.bOTGWSimulation` is set; records the would-be command in `state.sat.sLastBlockedCmd` and the trace ring (`sSimTraceCmd`/`iSimTraceMs`). Called from `OTDirect.ino` (`otdirect-tx`) and `OTGW-Core.ino` (`pic-serial`). Simulation requires boiler absence: `satBoilerHardwarePresent()` plus the slave-frame hook `satNotifyBoilerFrameSeen()` tear simulation down (`satOnBoilerDetected()`) the moment a real boiler answers

- `satUpdateSimulation(): void` and `satSimInjectEvent(const char* event, float value, int32_t durationS): bool`
  - Synthetic boiler and room model (diurnal outdoor curve, DHW draw schedule, flame/modulation model); `satSimInjectEvent` backs `POST /api/v2/sat/sim/event` (window_open, solar_gain, sensor_noise, sensor_dropout, dhw_demand)

- `satHandleTargetTemp / ExternalTemp / ExternalOutdoor / Humidity / SunElevation / PvSurplus / PvBoostEnabled(const char*): bool`, `satHandleEnabled`, `satHandleControlMode`, `satHandleHeatingMode`
  - MQTT/REST input handlers. `satHandleTargetTemp` auto-activates a preset when the value equals a preset temperature (Task #218). `satClearSafetyTrip()` is the single place a safety trip is cleared (TASK-1159)

- `satHandlePreset(const char* value): void`
  - Switches between named temperature presets (away, eco, comfort, sleep, activity, home, none); saves the pre-custom temperature for restore when the preset is cleared (Task #67)

- `satHandleWindow(bool isOpen): void` and `_satCheckWindowTimer()`
  - Window open/close events for anti-heating response

- Area and zone inputs: `satSetAreaTemp(area, temp)` (sensor polling path, TASK-587), `satHandleAreaTemp(area, value)` (MQTT/REST push), `satGetWeightedRoomTemp()` (static, weighted average of up to `SAT_MAX_AREAS` = 4 areas, each stale after `SAT_AREA_STALE_MS` of 5 min), `satHandleZoneRoomTemp`, `satHandleZoneSetpoint`, `satZonePidAdvance()` / `satZonePidAdvanceAll()` (per-zone PID integral step on the 60 s PID timer) and `satZonePidOutput()` (static, per-zone output on every control tick), `satPublishZoneDiagnostics()`
  - Multi-zone (Task #233, ported from thermo-nova area.py in TASK-1440): the zone block runs for every zone count (1 to 4) and `satAggregateZoneSetpoint()` folds the zones into the requested setpoint. A zone counts when it is usable (`satZoneUsable()`) and its PID output is above the cold setpoint (28.2 radiators, 21.0 underfloor, ADR-199). The zones output is `min(highest, outputs[int(N * 0.75)] + fZoneAggregationHeadroom)`, rounded to 1 decimal, or 10 with no counted zone. The requested setpoint is `max(main PID output, zones output)`; every counted zone more than 0.3 C above its target gives a cap `cold + 10 - 4 x degrees over`, floored at the cold setpoint, applied as a minimum. ECO (`iHeatingMode` 1) follows the main PID only. `fZoneAggregationHeadroom` is the most the zones may ask above their P75 output; with at most 4 zones it does not bind. Zone diagnostics on MQTT are published only when `iZoneCount > 1`

- `satUpdatePvBoost(): void` (static, TASK-640, ADR-110)
  - Adds `fPvBoostAppliedC` to the effective target while PV surplus exceeds the threshold for the hold time, bounded by indoor maximum and maximum duration; never mutates `settings.sat.fTargetTemp`

- `satPublishMQTT(): void`
  - Publishes all SAT state variables under `sat/` (e.g. `sat/target`, `sat/mode`, `sat/setpoint`, `sat/requested_setpoint`, `sat/heating_curve`, `sat/pid_output`, `sat/boiler_status`, `sat/area/0..3`, `sat/pv_*`). Uses the on-change plus jittered-heartbeat helpers from `SATmqttPublish.h` (ADR-111)

- `satSendStatusJSON(): void`
  - Chunked `GET /api/v2/sat/status` emitter built on a per-response `SatStatusSnap` (TASK-883) so the closure is deterministic across TCP windows

- `satDetectManufacturer(uint8_t slaveMemberID): void`
  - Auto-detects boiler manufacturer from OT MsgID 3 slave MemberID

- `satDisable(bool persistPid = true): void`, `satFlushShortLivedData(): void`
  - `satDisable()` releases control (`CS=0` via `satEnqueueOwnCS`), resets and persists PID/energy state, resets the command cache. `persistPid=false` skips only the PID file write; the OpenTherm-loss hand-back (`satOtLinkStep()`) passes `satPidRoomSeen() && isNTPtimeSet()`, so a loss before the first room reading or before NTP time leaves the saved PID state on flash (TASK-1365 AC#4, entry 115). The loss rule is decided: 30 s without a boiler reply, Data-Invalid counts as a reply (Q33 A, Robert 2026-10-26), automatic resume on the first boiler reply (Q34 A, entry 79). `satFlushShortLivedData()` clears PID integral and the cycle window

- `satUpdateBoilerStatus()` (static), `satUpdateFlameStatus()`, `satUpdatePressure()`, `satUpdatePowerEnergy()`, `satUpdateSolarGain()`, `satUpdateSummerSimmer()`, `satUpdateCurveRecommendation()`, `satUpdateModulationReliability()`, `satCheckSetpointSync()`, `satUpdateThermalLearning()`, `satAutoTuneUpdate()` (all static)
  - Per-tick analysis steps called from `satControlLoop()`

- `satApplyPWM(float)` and `satApplyContinuous(float)` (static)
  - Control-mode output stages. `satPwmSchedule()` only moves the PWM ON/OFF state; `satApplyPWM()` derives CS from it (TASK-1268). With `bOvershootProtection` on, no DHW and a requested setpoint above the cold setpoint: OFF phase CS=10 (CH off); ON phase with the flame off: `satPwmFlameOffHold()` = return + `fFlameOffOffset` (kept for `fModSupDelay` after ignition); flame on without a recorded hold and inside the delay since the real flame-on time: previous control setpoint; after the delay: `satPwmSuppressionSetpoint()` = flow - `fModSupOffset`. Otherwise no override: CS keeps `_sat_lastControlCS` (the previous control setpoint before the solar adjustment). The flame-off offset is used nowhere else
- `satPwmSuppressionRefresh()` (static, 3 s `timerSATSuppress`)
  - Recomputes the suppression CS between control ticks (OpenTherm text 2.1.1.2.5) through `satEnqueueIfChangedCS()` and updates `fFinalSetpoint`. Idle unless SAT is active, PWM ON phase, past the hold, protection on, no DHW/fault, valves open and the thermal safe fallback (`_sat_thermalSafeActive`) not active
- `satPwmReset()`, `satSetControlMode()`
  - Entering or leaving PWM (and `satDisable()`) clears the PWM state so a new stint starts idle
- `satComputeMM(...)` (static, pure)
  - MM per mode: 0 in PWM, `iMaxRelModulation` otherwise; heat pump and hybrid never 0 (heat pump 100 outside PWM); Geminox floor 10 (0 included); Immergas cap 80

#### Constants and tables

- `satManufacturerTable[]` (PROGMEM): MemberID to name and quirk flags (18 entries, index matches `SATManufacturer`, AUTO=0 is skipped). Quirks: `SAT_QUIRK_MIN_MOD_10`, `SAT_QUIRK_IMMERGAS_TP`, `SAT_QUIRK_NO_REL_MOD`, `SAT_QUIRK_MI_500_BOOT`
- `SAT_CMD_RESEND_MS` (5 s), `SAT_CMD_REFRESH_MS` (300 s), `SAT_PIC_CS_KEEPALIVE_MS` (30 s), `SAT_MIN_SETPOINT` (10 C), `SAT_GLOBAL_MAX_SETPOINT` (65 C), `SAT_HARD_MAX_FLOOR` (50 C), `SAT_HARD_MAX_RAD` (80 C), `SAT_COLD_SETPOINT_RAD` (28.2 C) and `SAT_COLD_SETPOINT_FLOOR` (21 C, below which the boiler is commanded off, TASK-891.2)
- Heating system (`SAT_HSYS_AUTO/RADIATORS/UNDERFLOOR`) and heating source (`SAT_SRC_AUTO/GAS_BOILER/HEAT_PUMP/HYBRID`, TASK-891.8) are separate settings. Heat-pump source forces MM=100 outside PWM (the configured maximum in PWM), no hourly PWM start limit (entry 81, TASK-1419) and a user-selectable PWM cycle length (`iHpCycleSeconds`, 1800 or 2400 s; the minimum on-time is 180 s for every source, TASK-1366)

#### State machines

- Boiler status: `SATBoilerStatus` has 16 values (`SAT_BS_OFF` through `SAT_BS_HEATING_HOT_WATER`), evaluated by `satUpdateBoilerStatus()` with the `BS_*` timing constants (180 s anti-cycle minimum off, 600 s stalled-ignition floor)

The OPV (overshoot protection value) calibration state machine and `settings.sat.fOvpValue` no longer exist in the code; only ADR-076 and a stale comment in the simulation section still mention OPV.

### Module: SATcycles.ino

Cycle tracking, on/off transition detection, cycle classification, rolling statistics aggregation, heating-curve recommendation.

#### Key Functions

- `satCycleInit(): void`
- `satCycleOnFlameChange(bool flameOn): void`
  - Handles flame ON/OFF transition: completes cycle, classifies (`_cycleClassify`), updates statistics
- `satCycleSample(): void`
  - Samples flow temp, DHW state, accumulates cycle metrics
- `satGetWindow4hStats(): void` and `satGetWindow24hStats(): void`
  - Rolling 4-hour (Task #227) and 24-hour (TASK-891.4) cycle statistics, refreshed once per minute
- `satCycleIsHourLimitReached(): bool`, `satCycleGetCyclesThisHour(): uint8_t`
  - Rolling 60-min window for max cycles per hour (Task #203)
- `satCycleCheckOvershootSwitch(float requestedSetpoint, uint32_t tickMs): bool` (continuous -> PWM, 60 s, before the mode is applied)
- `satCycleCheckPwmExits(float requestedSetpoint, uint32_t tickMs): bool` (PWM -> continuous: underheat 180 s, saturation 300 s, after `satApplyPWM`)
- `satCycleNoteDhwTick(uint32_t tickMs): void` (DHW tick: refreshes the 300 s guard end, clears the sustained starts)
  - Continuous/PWM auto-switch (Tasks #42/#43)
- `satHCRAddSample()`, `satHCRSaveState()`, `satHCRLoadState()`, `satHCRReset()`, `satHeatingCurveRecommendation()`
  - Heating curve recommendation (Task #228), persisted to `/sat/sat_hcr.json`
- `satFlushCycleWindow()`: clears the cycle window in memory and on LittleFS (`/sat/sat_cycles.json`)

#### Data Structures

- `_hourCycleTs[SAT_MAX_CYCLES_PER_HOUR]` (SAT_MAX_CYCLES_PER_HOUR = 6)
  - Ring buffer of millis() timestamps of PWM ON-phase starts (rolling 60-min window; hot-water ignitions and relights are not recorded, TASK-1401)
- `_win4h[SAT_WIN4H_SIZE]` (360 slots, `SATWindowRecord`, size defined in boards.h)
  - Rolling cycle statistics window (Task #227)
- `_flow_samples[SAT_FLOW_SAMPLE_SIZE]` (board-defined in boards.h)
  - Per-cycle flow temperature samples for p90/p10 classifier (Task #225)
- `_hcr_dailyMedian[HCR_DAYS]`
  - Heating curve recommendation: rolling window of daily median errors (Task #228)

### Module: SATpid.ino

PID controller with automatic gain calculation, deadband mode, temperature-based derivative.

#### Key Functions

- `satPidError(float target, float room): float`
  - target - room rounded to 3 decimals, like pid.py's `TemperatureState.error`, so an error of exactly 0.1 is inside the 0.1 deadband (TASK-1195)
  - Used by `satPidStep()` and by the zone PID (`satZonePidAdvance()` in SATcontrol.ino)

- `satPidReset(): void`
  - Full PID state reset. Leaves the room-temperature change stamp alone, as pid.py's `reset()` keeps its timers

- `satResetIntegral(): void`
  - Resets PID integral to 0 (debug tool)

- `satPidRestoreState(float integral, float rawDerivative): void`
  - Applies the state loaded from LittleFS

- `_pidCalculateGains(float curveValue): void` (static)
  - Auto-calculates Kp/Ki/Kd from heating curve value

- `_pidUpdateIntegral(float error, float curveValue, bool force): void` (static)
  - Updates integral term only inside deadband
  - Task #23: skips accumulation if `bSolarGainActive` and `bSolarFreezeIntegral`

- `_pidUpdateDerivative(float roomTemp, float error): void` (static)
  - Temperature-based derivative with adaptive low-pass filter, alpha = dt/(60+dt), +-5 cap
  - Freezes inside deadband, updates outside
  - Timed by when the room temperature changed (`_pid_roomTempChangedMs`, stamped by `satPidNoteRoom()`, called by the control tick and by `satPidStep()`), not by when the PID ran: pid.py times it by the sensor's `last_changed` (TASK-1195)
  - The reference temperature (`_pid_derivRefTemp`) moves only together with the timer, so a step that the TASK-894 room EMA spreads over several control ticks counts in full; pid.py overwrites its `last_temperature` on every update and comes out low on such input
  - Proof: `python test/host/test_sat_pid_derivative.py --old-rev <rev>`

- `satPidService(uint32_t nowMs): void` (TASK-1368, entries 42, 28, 111)
  - Called every loop pass of `satControlLoopRun()` while SAT is enabled, not tripped, not flashing and with a live OpenTherm link, before the control tick's early returns
  - Takes one start-up step after `satPidReset()`, then one step per expiry of `timerSATPid` (`DECLARE_TIMER_SEC(timerSATPid, 60, SKIP_MISSED_TICKS)`, thermo-nova `climate.py:712-716`); a loop stall gives one step
  - Gets its inputs from `satPidGatherInputs()` (SATcontrol.ino: filtered room temperature, effective target, heating curve, thermal deadband widening); no room temperature: `satPidCurveOnly()`; then `satZonePidAdvanceAll()`
- `satPidStep(float roomTemp, float targetTemp, float heatingCurveValue, uint32_t nowMs): void`
  - Moves the held state once (gains, integral, derivative, error); no time gate and no unchanged-error gate (`pid.py:159-171`)
- `satPidOutput(float heatingCurveValue): float`
  - Called on every control tick: curve + kp(curve) * held error + I + kd(curve) * held raw derivative (`pid.py:60-132`); NAN held error gives the curve alone
- `satPidNoteRoom(float roomTemp, uint32_t nowMs): void`
  - Stamps the moment a new room temperature shows up, before the derivative timer reads it

### Module: SATpressure.ino

CH water pressure monitoring, health status, drop-rate detection (Task #226). The smoothing and alarm state itself is computed by `satUpdatePressure()` in SATcontrol.ino.

#### Key Functions

- `satPressureHealthUpdate(): void`
  - Updates `state.sat.fBoilerPressure` and `state.sat.sPressureStatus` ("ok"/"low"/"high", non-ok only once the alarm is confirmed)

- `satPressureHealthPublish(): void`
  - Publishes `sat/ch_pressure` and `sat/ch_pressure_status` via the ADR-111 helpers

### Module: SATweather.ino

Outdoor temperature, humidity, wind and (when `HAS_WEATHER_FORECAST`, set on every ESP32 board in boards.h) a 24-hour forecast. Open-Meteo is the only source (no API key; TASK-1280 removed the OpenWeatherMap option). Single-pass streaming JSON parse of the response stream (`wstreamGet`, `weatherParseStream`), no full buffer materialisation, no ArduinoJson. The request is HTTP/1.0 (`http.useHTTP10(true)`: no keep-alive, no chunked encoding), and `weatherParseStream` returns as soon as the top-level object closes, so a fetch does not wait for the 5 s idle timeout of `wstreamGet` (TASK-1478). A 5-minute startup gate (`WEATHER_POLL_MIN_SEC`, TASK-511) delays the first request, and `weatherLoop()` skips fetching while the OT bus already reports a non-zero `Toutside`. Telnet shortcut `w` triggers an immediate fetch and dumps the parsed state (`weatherDumpStateToTelnet()`, TASK-513).

Source comments still mention an ESP8266 minimal URL; the `#if !HAS_WEATHER_FORECAST` branch is the only remnant and no dev board takes it.

#### Key Functions

- `weatherLoop(): void`
  - Timer-guarded poll, called from the main loop
- `weatherFetch(): void`
  - Calls `weatherFetchOpenMeteo()`
- `weatherFetchOpenMeteoNow(): void`
  - Public wrapper for the telnet `w` shortcut
- `weatherSendStatusJSON(): void`
  - `GET /api/v2/sat/weather`
- `weatherPublishMQTT(): void`

### Module: SATble.ino (`HAS_SAT_BLE`) - NimBLE-Arduino 2.x (ADR-092)

BLE advertisement scanning for temperature/humidity sensors, built on NimBLE-Arduino 2.x (TASK-487/488 replaced the classic Bluedroid stack). Uses the magic-zero idiom: scan window/interval and result-list cap configured so NimBLE only fires the callback and never builds an internal result list.

Operates in **continuous-scan** mode (TASK-494): the scan runs uninterrupted on the BLE host task; advertisements flow into a per-MAC roster (`SAT_BLE_MAX_ROSTER = 8`) that is settings-backed (`settings.sat.sBleMac[i]`, `sBleLabel[i]`, `sBleBindkey[i]`) and survives reboot. Aggregated periodic scan-stats (TASK-506) replace per-ad logging.

**Independent of SAT**: `satBLELoop()` is called from the main loop outside the SAT-enabled gate and `bleActive()` checks only `bBleEnable` plus PSRAM or `bBleRiskAck` (TASK-995), so BLE sensors keep publishing (`sat/ble/<mac>/{temp,rh,bat,rssi}`) with SAT off. `bBleEnable` defaults to true. SAT consumes the pinned sensor (`sBleMAC`) as its highest-priority room temperature, with `bBleFailover` (TASK-762) falling back to another fresh roster slot.

**Area sensors (ADR-178, TASK-1153)**: `settings.sat.sSensorArea[area]` holds either a 16-hex Dallas address or a 17-character BLE MAC. `satBLEUpdateState()` routes the temperature of a roster MAC that matches an area entry into `satSetAreaTemp()`; `sensors_ext.ino` does the same for Dallas addresses. The value's shape decides the owner, so an area has exactly one writer.

Auto-discovery: every format-passing MAC enters the roster on first sight (optionally gated by the name-prefix filter `sBleNamePrefix` / `bBleNameFilterIngest`, TASK-895/1231). HA discovery for each roster MAC is published lazily and re-published on label change.

#### Key Functions

- `parseBLEAtcFormat(const uint8_t* data, size_t len, float* temp, float* hum, uint8_t* batt): bool` (static)
  - ATC/pvvx custom firmware format (service data UUID 0x181A)
- `parseBLEBTHomeFormat(...)` (static)
  - BTHome v2 format (service data UUID 0xFCD2)
- `parseBLEMiBeaconFormat(...)` (static)
  - Plaintext Xiaomi MiBeacon (UUID 0xFE95); object TLV 0x1004/0x1006/0x100A/0x100D. See ADR-153
- `mibeaconDecryptInPlace(uint8_t* raw, size_t len, const char* bindkeyHex, const uint8_t addr6[6], size_t* plaintextLen): bool` (static)
  - Decrypts an encrypted MiBeacon v4/v5 frame via mbedtls AES-128-CCM with the slot's bindkey. See ADR-154
- `satBLERosterSetBindkey(const char* mac, const char* key): bool`
  - Provisions a slot's bindkey (backs `POST /api/v2/sat/ble/bindkey`); never logged or echoed, discovery exposes only `has_bindkey`
- `satBLEInit()`, `satBLELoop()`, `satBLEUpdateState()`, `satBLEPublishMQTT()`, `satBLERescanRequest()`, `satBLEFailoverActive()`, `satBLESendStatusJSON(JsonEmit&, ...)`
- `satBLERosterSendJSON()`, `satBLERosterSelect()`, `satBLERosterSetLabel()`, `satBLERosterForget()`
  - Roster management behind `/api/v2/sat/ble/*`
- `SATBLEScanCallbacks::onResult(const NimBLEAdvertisedDevice*)`
  - NimBLE 2.x scan callback on the BLE host task; defers `flushSettings()` to the loop task

### Module: SATmqttPublish.{h,cpp} (ADR-111)

On-change plus jittered-heartbeat (7-11 min) publish helpers (`publishIfChangedF/I/B/S` with `SATShadow*` shadows) used by `satPublishMQTT()`, `satPressureHealthPublish()`, the BLE state publisher and `weatherPublishMQTT()`. Shadows are not reset on MQTT reconnect.

### Multi-Area Room Temperature Mapping (Task #25, TASK-587, ADR-178)

Up to 4 weighted areas (`fAreaWeight[4]`, `iMultiAreaCount`). Each area is fed by a mapped Dallas or BLE sensor (`/api/v2/sat/sensor-areas`) or by an MQTT/REST push (`sat/area/<0-3>`). `satGetRoomTemp()` uses the weighted average when `bMultiArea` is true and at least one area is fresh; otherwise it falls through to the single-sensor logic. The firmware caches the current float per area in `state.sat.fAreaTemp[4]`.

## Dependencies

### Internal Dependencies

- OTGW-firmware.h and SATtypes.h (type definitions, enums, settings/state structures)
- MQTTstuff.ino (`kSatMqttCmds[]` dispatch table per ADR-078, `sendMQTTData`)
- OTGW-Core.ino (`OTcurrentSystemState`, `addCommandToQueue()`, CS= refusal, `otRealBoilerSeenRecently()`)
- OTDirect.ino (`otDirectBoilerPresent()`, CS= refusal, heating-curve gate on `satOwnsControlSetpoint()`)
- restAPI.ino (`/api/v2/sat/*` handler)
- sensors_ext.ino (Dallas to area routing via `satSetAreaTemp()`)
- settingStuff.ino (settings persistence, `updateSetting()`)
- safeTimers.h (DECLARE_TIMER_SEC, DUE macro)
- helperStuff.h (utility functions, debug macros)

### External Dependencies

- Arduino-ESP32 core (millis, PROGMEM helpers, mbedtls)
- NimBLEDevice.h (NimBLE-Arduino 2.x per ADR-092, continuous scan in callback-only mode, TASK-494)
- Open-Meteo API (HTTP GET, free, no API key)

## Settings (Persistent Configuration)

All SAT settings are stored in `settings.sat` (`SATSection` in SATtypes.h, 100+ fields). Main groups:

- Core: `bEnabled`, `iHeatingSystem` (auto/radiators/underfloor), `iHeatingSource` (auto/gas_boiler/heat_pump/hybrid), `iHpCycleSeconds`, `fTargetTemp`, `fHeatingCurveCoeff`, `fDeadband`, `iControlInterval`, `bUseExternalTemp`, `iSensorMaxAgeS`, `fMaxSetpoint`
- Presets: `fPresetComfort/Eco/Away/Sleep/Activity/Home`, `bPresetSync`, `sPresetSyncTopic`
- PID: `bAutoTune`, `fAutoTuneRate`
- Output: `bOvershootProtection` (default on), `fOvershootMargin` (default 2.0), `iMaxRelModulation`, `fFlameOffOffset`, `fFlowOffset`, `fModSupDelay/Offset`, `bPushSetpoint` (TC=), `bFollowThermostat` (TASK-1259: `satOnThermostatSetpoint()` is called by `processOT()` for each thermostat MsgID 16 Write-Data T frame; `satFollowThermostatDecide()` is the pure edge/echo test; `satAdoptThermostatSetpoint()` sets the target and clears the preset without the none-restore; default = push-setpoint when no stored key), `iCyclesPerHour`, DHW (`bDhwEnabled`, `bDhwEnable`, `fDhwSetpoint`)
- Environment: weather (`bWeatherEnable`, `fWeatherLat/Lon`, `iWeatherInterval`), solar gain, summer simmer, window detection, pressure limits
- Inputs: `bMultiArea`, `iMultiAreaCount`, `fAreaWeight[4]`, `sSensorArea[4][18]` (Dallas address or BLE MAC), `iZoneCount`, `iZoneTimeoutS`, `fZoneAggregationHeadroom`, PV boost (`bPvBoostEnabled`, `iPvBoostThresholdW`, `iPvBoostHoldS`, `fPvBoostDeltaC`, `fPvBoostMaxIndoorC`, `iPvBoostMaxDurationMin`)
- Simulation: `bSimulation`, `fSimHeatRate`, `fSimCoolRate`
- BLE: `bBleEnable`, `bBleRiskAck`, `bBleFailover`, `sBleMAC`, `iBleInterval`, `sBleNamePrefix`, `bBleNameFilterIngest`, roster `sBleMac[8]/sBleLabel[8]/sBleBindkey[8]`

## Runtime State (Transient)

All SAT runtime state is stored in `state.sat` (`SATRuntimeSection` in SATtypes.h). Notable fields:

- `bActive`, `eControlMode` (OFF/CONTINUOUS/PWM), `eBoilerStatus`, `bSafetyTripped`, `bFallbackActive`/`eFallbackReason`
- `fHeatingCurveValue`, `fPidOutput`, `fFinalSetpoint`, `iCurrentModulation`
- Last-sent cache (read by the 5 s resend, the PIC keepalive and the change-checked CS/TC helpers): `bLastSentValid`, `fLastSentCS`, `iLastSentMM`, `iLastSentCH`, `fLastSentTC` and the `iLastSent*Ms` stamps
- Cycles: `eLastCycleClass`, `iCycleCount`, `iCyclesThisHour`, 4 h averages, `fDutyRatio`
- Pressure: `fBoilerPressure`, `fSmoothedPressure`, `sPressureStatus`
- `weather.*`, `fCurrentPower`, `fEnergyTotal`, `bSolarGainActive`, `bSummerActive`, `bValvesOpen`
- BLE and areas: `fBleTemp`/`bBleTempValid`, `fAreaTemp[4]`/`bAreaValid[4]`
- PV boost: `bPvBoostActive`, `fPvBoostAppliedC`
- Simulation: `fSimRoomTemp`, `bSimFlameOn`, `iSimModulation`, `sLastBlockedCmd`, trace ring (`sSimTraceCmd`, `iSimTraceMs`, `iSimTraceHead`, `iSimTraceCount`)

## Notes

- **Command ownership**: SAT owns MsgID 1 while enabled. External `CS=` is refused on OT-Direct and on the PIC command queue; SAT's own CS= is the only one let through (marker `satCommandInFlight()`). The thermostat-timeout setback in OTDirect.ino is the one documented exception (fail-safe)
- **Command cadence**: CS/MM/CH are sent every control tick and repeated every 5 s between ticks (TASK-1370, ADR-203); TC= is sent on change only; the 3 s PWM suppression CS refresh stays change-checked; on PIC boards the 30 s CS keepalive stays silent while the resend runs
- **Simulation mode**: blocks all bus TX (`satSimulationBlocksBusTx`), requires boiler absence and switches itself off for good when a real boiler answers (Task #37, TASK-795)
- **PROGMEM Safety**: Manufacturer lookup in flash; safe string access with pgm_read_byte
- **Re-entrancy**: Static locals preserve state across doBackgroundTasks re-entry
- **Fallback Mode**: Auto-enables when MQTT lost >5 minutes (Task #19); after 2 h of thermal estimation without real data a fixed 45 C flow is used
- **Thermal Learning**: Learns building thermal decay rate during flame-off (Task #21)
- **Solar Gain Compensation**: Freezes integral when indoor temp rises unexpectedly (Task #23)
- **Pressure Monitoring**: EMA smoothing and drop-rate detection (Task #226)
- **Energy Tracking**: Power = modulation x the active capacity: `fDhwCapacity` while the boiler reports DHW (0 = use `fBoilerCapacity`), `fBoilerCapacity` otherwise (Task #45, TASK-1258). Estimated gas input = power / `fBoilerEfficiency`, switched on and capped by `fBoilerRatedKW` (Task #232, TASK-1258). `satUpdatePowerEnergy()` and `satPersistEnergyIfDue()` run right after DHW detection, before the DHW and summer-simmer early returns, so power and energy keep moving during hot water; persisted to `/sat_energy.json` and `/sat_energy_estimate.json`
- **Persistence files**: `/sat_pid_state.json`, `/sat/sat_preset.json` (active preset, pre-custom and window pre-target, TASK-1487), `/sat/sat_cycles.json`, `/sat/sat_hcr.json`, `/sat_markers.json` (written by restAPI.ino)
- **Heating Curve Recommendation**: Daily median error tracking for coefficient adjustment (Task #228)
- **Window Detection**: Detects rapid indoor temp drop; summer simmer disables heating (Task #24/#29)
- **PID Auto-Tuning**: Analyzes cycle performance, adjusts gains incrementally (Task #27)
- **Narration**: `satNarrate_P` / `satNarratef_P` emit human-readable state-change messages to the live log
- **Preset Sync**: Broadcasts preset changes to secondary entities via MQTT (Task #46)
