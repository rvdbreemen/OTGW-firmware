/*
***************************************************************************
**  Program  : SATtypes.h
**  Version  : v2.0.0-alpha.245
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for the SAT (Smart Autotune Thermostat)
**  subsystem. Extracted from OTGW-firmware.h per ADR-079. Bundles every
**  SAT type declaration in one place so contributors see settings and
**  state together (they are tightly coupled: SATSection default values
**  reference SAT_HSYS_AUTO and SAT_MFR_AUTO enums declared here):
**
**    - SAT runtime enums (SATHeatingSystem, SATControlMode, SATPreset,
**      SATCycleClass, SATManufacturer, SATFlameStatus, SATBoilerStatus,
**      ...)
**    - SAT manufacturer quirk-flag defines (buffer sizes live in boards.h)
**    - SATWindowRecord, SATZoneState (helper structs)
**    - SATRuntimeSection    (state.sat — transient runtime state)
**    - SATSection           (settings.sat — persisted configuration)
**
**  Naming and Hungarian-prefix conventions come from ADR-051 (unchanged).
**  Accessors still use state.sat.<field> / settings.sat.<field>; this
**  file only controls where the types are declared.
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

//====================================================================
//=== SAT runtime enums ===
//====================================================================

// Heating SYSTEM = heat distribution (drives base offset + COLD_SETPOINT). Orthogonal to the
// heating source. (TASK-891.8)
enum SATHeatingSystem : uint8_t {
  SAT_HSYS_AUTO       = 0,  // No OT detection for distribution; defaults to radiators
  SAT_HSYS_RADIATORS  = 1,  // Radiators or mixed system
  SAT_HSYS_UNDERFLOOR = 2   // Underfloor heating
};
// Heating SOURCE = energy device (drives min-on / cycles / max-setpoint timing + MM=100% for
// heat pumps). Orthogonal to the heating system. (TASK-891.8, George+Robert 2026-06-20)
enum SATHeatingSource : uint8_t {
  SAT_SRC_AUTO       = 0,  // Auto-detect from OT MsgID 3 (cooling-enabled => heat pump)
  SAT_SRC_GAS_BOILER = 1,  // Gas/oil boiler
  SAT_SRC_HEAT_PUMP  = 2,  // Heat pump
  SAT_SRC_HYBRID     = 3   // Heat pump + separate gas boiler (manual; coordination = TASK-892)
};
enum SATControlMode : uint8_t { SAT_MODE_OFF = 0, SAT_MODE_CONTINUOUS, SAT_MODE_PWM };
enum SATPreset : uint8_t {
  SAT_PRESET_NONE = 0, SAT_PRESET_AWAY, SAT_PRESET_ECO,
  SAT_PRESET_COMFORT, SAT_PRESET_SLEEP, SAT_PRESET_ACTIVITY,
  SAT_PRESET_HOME
};
enum SATFallbackReason : uint8_t {
  SAT_FB_NONE = 0, SAT_FB_THERMOSTAT_LOST, SAT_FB_MQTT_LOST
};
enum SATCycleClass  : uint8_t {
  SAT_CYCLE_NONE = 0, SAT_CYCLE_GOOD, SAT_CYCLE_OVERSHOOT,
  SAT_CYCLE_UNDERHEAT, SAT_CYCLE_SHORT, SAT_CYCLE_UNCERTAIN
};
enum SATCycleKind : uint8_t {
  SAT_CK_UNKNOWN = 0, SAT_CK_CH, SAT_CK_DHW, SAT_CK_MIXED
};
enum SATCyclePhase : uint8_t {
  SAT_CP_IDLE = 0, SAT_CP_STARTUP, SAT_CP_STEADY, SAT_CP_COOLDOWN
};
enum SATCurveRecommendation : uint8_t {
  SAT_CR_INSUFFICIENT = 0, SAT_CR_INCREASE, SAT_CR_DECREASE, SAT_CR_HOLD
};
// Manufacturer enum — indices must match satManufacturerTable[] in SATcontrol.ino
enum SATManufacturer : uint8_t {
  SAT_MFR_AUTO = 0,   // Auto-detect from OT MemberID (may be ambiguous)
  SAT_MFR_ATAG,       SAT_MFR_BAXI,      SAT_MFR_BROTGE,
  SAT_MFR_DEDIETRICH, SAT_MFR_FERROLI,   SAT_MFR_GEMINOX,
  SAT_MFR_IDEAL,      SAT_MFR_IMMERGAS,  SAT_MFR_INTERGAS,
  SAT_MFR_ITHO,       SAT_MFR_NEFIT,     SAT_MFR_RADIANT,
  SAT_MFR_REMEHA,     SAT_MFR_SIME,      SAT_MFR_VAILLANT,
  SAT_MFR_VIESSMANN,  SAT_MFR_WORCESTER, SAT_MFR_OTHER,
  SAT_MFR_COUNT
};
// Manufacturer quirk flags
#define SAT_QUIRK_MIN_MOD_10     0x01  // Geminox: minimum modulation 10%
#define SAT_QUIRK_IMMERGAS_TP    0x02  // Immergas: extra TP=11:12 command, cap 80%
#define SAT_QUIRK_NO_REL_MOD     0x04  // Ideal/Intergas/Geminox/Nefit: no relative modulation support
#define SAT_QUIRK_MI_500_BOOT    0x08  // Ideal/Intergas/Nefit: send MI=500 on boot
enum SATFlameStatus : uint8_t {
  SAT_FS_INSUFFICIENT_DATA = 0, SAT_FS_HEALTHY, SAT_FS_IDLE_OK,
  SAT_FS_STUCK_ON, SAT_FS_STUCK_OFF, SAT_FS_PWM_SHORT, SAT_FS_SHORT_CYCLING
};
enum SATBoilerStatus : uint8_t {
  SAT_BS_OFF = 0, SAT_BS_IDLE, SAT_BS_PREHEATING, SAT_BS_AT_SETPOINT,
  SAT_BS_MODULATING_UP, SAT_BS_MODULATING_DOWN, SAT_BS_IGNITION_SURGE,
  SAT_BS_STALLED_IGNITION, SAT_BS_ANTI_CYCLING, SAT_BS_PUMP_STARTING,
  SAT_BS_WAITING_FLAME, SAT_BS_OVERSHOOT_COOLING, SAT_BS_POST_CYCLE,
  SAT_BS_HEATING, SAT_BS_COOLING, SAT_BS_HEATING_HOT_WATER
};

// SAT rolling 4-hour window buffer size (Task #227/#236) is defined per board in
// boards.h (ESP-abstraction Tier 3, TASK-743): 30 slots on ESP8266, 360 on ESP32.

//====================================================================
//=== SAT helper structs ===
//====================================================================

// Record for each completed flame cycle stored in the rolling 4-hour window
struct SATWindowRecord {
  uint32_t endMs;           // millis() when flame went OFF (cycle end)
  uint32_t onDurationMs;    // flame-ON duration for this cycle
  uint32_t offDurationMs;   // flame-OFF gap that preceded this cycle
  float    p90FlowTemp;     // 90th percentile flow temp during this cycle (°C)
  float    avgFlowRetDelta; // average (Tboiler - Tret) during this cycle (°C); <0 if no data
  uint8_t  eClass;          // SATCycleClass cast to uint8_t
};

// Per-zone state for multi-zone PID heating control (Task #233)
// Kept compact: 4 zones × ~21 bytes ≈ 84 bytes total on BSS
struct SATZoneState {
  float    fRoomTemp     = 0.0f;   // Last received room temperature (°C)
  float    fSetpoint     = 0.0f;   // Last received zone setpoint (°C)
  float    fPidOutput    = 0.0f;   // Zone PID output (CH setpoint requested by this zone)
  float    fPidIntegral  = 0.0f;   // Zone PID integral accumulator
  float    fPrevError    = 0.0f;   // Previous PID error (for derivative)
  uint32_t iLastUpdateMs = 0;      // millis() of last room_temp or setpoint update
  bool     bRoomValid    = false;  // Room temp has been received at least once
  bool     bSpValid      = false;  // Setpoint has been received at least once
  bool     bOff          = false;  // TASK-593: zone thermostat is in HVACMode.OFF.
                                   // Excluded from PID + P75 aggregation while true.
                                   // Mirrors SAT Python PR #172 (area.py returns None
                                   // for state/error/weight when HVACMode.OFF).
};

//====================================================================
//=== state.sat — transient SAT runtime state ===
//====================================================================

struct SATRuntimeSection {         // state.sat — SAT thermostat controller state
  bool            bActive        = false;
  SATControlMode  eControlMode   = SAT_MODE_OFF;
  SATBoilerStatus eBoilerStatus  = SAT_BS_OFF;
  // Heating curve + PID
  float fHeatingCurveValue       = 0.0f;
  float fPidOutput               = 0.0f;  // = curve + P + I + D
  float fPidP                    = 0.0f;
  float fPidI                    = 0.0f;
  float fPidD                    = 0.0f;
  float fFinalSetpoint           = 0.0f;
  float fError                   = 0.0f;  // target - room temp
  float fKp                      = 0.0f;
  float fKi                      = 0.0f;
  float fKd                      = 0.0f;
  float fRawDerivative           = 0.0f;  // filtered derivative (for diagnostics)
  // Cycle tracking
  SATCycleClass eLastCycleClass  = SAT_CYCLE_NONE;
  uint32_t iCycleCount           = 0;
  uint8_t  iCyclesThisHour       = 0;      // Flame-on events in the rolling 60-min window
  float    fCycleMaxFlow         = 0.0f;
  float    fCycleOvershootSec    = 0.0f;
  float    fLastCycleDuration     = 0.0f;   // Duration of last completed cycle (sec)
  SATCycleKind eLastCycleKind    = SAT_CK_UNKNOWN; // Kind of last completed cycle
  float    fLastCycleFractionCH  = 0.0f;   // Fraction of last cycle that was CH (0-1)
  float    fLastCycleFractionDHW = 0.0f;   // Fraction of last cycle that was DHW (0-1)
  float    fDutyRatio            = 0.0f;   // EMA flame-on fraction
  float    fOvershootFraction    = 0.0f;   // EMA overshoot cycle fraction
  float    fUnderheatFraction    = 0.0f;   // EMA underheat cycle fraction
  // Rolling 4-hour cycle window statistics (Task #227)
  uint8_t  i4hCycles             = 0;      // Number of complete cycles in last 4h
  float    f4hAvgOnSec           = 0.0f;   // Average flame-on duration in last 4h (seconds)
  float    f4hAvgOffSec          = 0.0f;   // Average flame-off duration in last 4h (seconds)
  float    f4hAvgFlow            = 0.0f;   // Average p90 flow temp in last 4h (°C)
  float    f4hDutyRatio          = 0.0f;   // Windowed duty ratio in last 4h
  float    f4hOvershootFraction  = 0.0f;   // Fraction of overshoot cycles in last 4h
  float    f4hUnderheatFraction  = 0.0f;   // Fraction of underheat cycles in last 4h
  float    f4hFlowRetDeltaP50    = 0.0f;   // Median flow-return delta across last 4h cycles (°C)
  float    f4hFlowRetDeltaP90    = 0.0f;   // p90 flow-return delta across last 4h cycles (°C)
  // PWM state
  float fPwmDutyCycle            = 0.0f;
  bool  bPwmFlameRequested       = false;
  // Preset
  SATPreset eActivePreset        = SAT_PRESET_NONE;
  // Modulation control
  uint8_t iCurrentModulation     = 100;   // Last MM= value sent to boiler (0-100)
  // DHW state
  bool     bDhwActive               = false;  // DHW currently active (from OT status)
  // Modulation suppression
  bool     bModSuppressed           = false;
  uint32_t iModSuppressionSinceMs   = 0;
  // External inputs (MQTT/REST overrides)
  float fExternalTemp            = 0.0f;
  float fExternalOutdoor         = 0.0f;
  bool  bExternalTempValid       = false;
  bool  bExternalOutdoorValid    = false;
  uint32_t iLastControlMs        = 0;
  // PV-surplus boost runtime state (TASK-640)
  float    fExternalPvSurplusW      = 0.0f;   // Last received PV surplus power (W)
  bool     bExternalPvSurplusValid  = false;  // PV surplus value is fresh and valid
  uint32_t iExternalPvSurplusLastMs = 0;      // millis() of last PV surplus update
  bool     bPvBoostActive           = false;  // Boost currently applied
  uint32_t iPvBoostStartedMs        = 0;      // millis() when current hold/active period started
  float    fPvBoostAppliedC         = 0.0f;   // Current boost applied (0 when inactive)
  uint32_t iPvBoostCooldownMs       = 0;      // millis() when cooldown expires (0 = no cooldown)
  // Safety / fallback
  uint32_t iExternalTempLastMs   = 0;   // millis() when external indoor temp was last received
  uint32_t iExternalOutdoorLastMs = 0;  // millis() when external outdoor temp was last received
  bool     bSafetyTripped        = false; // true if safety forced satDisable()
  // Fallback mode
  bool     bFallbackActive       = false;
  SATFallbackReason eFallbackReason = SAT_FB_NONE;
  // Heating system detection
  uint8_t  iDetectedHeatingSource  = SAT_SRC_GAS_BOILER; // auto-detected heating source from OT MsgID 3 (cooling-capable => heat pump)
  // Manufacturer detection
  uint8_t  iDetectedManufacturer  = SAT_MFR_OTHER;      // auto-detected from OT MsgID 3 valueLB
  uint8_t  iSlaveMemberID        = 0;                   // raw slave MemberID code from MsgID 3
  // TRV valve detection (Task #29)
  bool     bValvesOpen            = true;   // default true: assume heat demand when no TRV data
  // Window detection
  bool     bWindowOpen            = false;
  uint32_t iWindowOpenSinceMs     = 0;
  float    fPreWindowTarget       = 0.0f;
  uint8_t  iPreWindowPreset       = 0;   // SATPreset before window opened
  // Pre-temperature tracking (Task #67)
  float    fPreCustomTemp         = 0.0f;  // target temp before last preset change (0 = not set)
  float    fPreActivityTemp       = 0.0f;  // target temp before last window-open event (0 = not set, mirrors fPreWindowTarget)
  // Pressure monitoring
  float    fSmoothedPressure      = 0.0f;
  float    fPressureDropRate      = 0.0f; // bar/hour (linear regression), negative when suspended
  bool     bPressureAlarm         = false;
  uint32_t iPressureAlarmSinceMs  = 0;
  bool     bPressureHealthy       = true;  // binary sensor: true=healthy
  float    fLastPressure          = 0.0f;  // last raw pressure reading
  uint32_t iLastPressureMs        = 0;     // millis() of last pressure reading
  uint32_t iLastSeenPressureMs    = 0;     // millis() of last non-zero pressure seen
  // CH pressure health (Task #226): named fields for sat/ch_pressure + sat/ch_pressure_status
  float    fBoilerPressure        = 0.0f;  // raw OT MsgID 18 reading (bar)
  char     sPressureStatus[8]     = "ok";  // "ok", "low", or "high"
  // Modulation reliability
  bool     bModulationReliable    = true;
  uint8_t  iModChangeCount        = 0;   // changes observed in window
  // Heating curve recommendation + error statistics
  SATCurveRecommendation eCurveRecommendation = SAT_CR_INSUFFICIENT;
  float    fMeanError             = 0.0f;
  float    fErrorStdDev           = 0.0f;
  uint8_t  iErrorSampleCount      = 0;     // Number of samples in error ring buffer
  // Daily median recommendation (Task #228): "INCREASE", "DECREASE", "HOLD", or "insufficient"
  char     sHeatCurveRec[13]      = "insufficient";  // result string for MQTT / status JSON
  // Flame health (Task #70/#71)
  SATFlameStatus eFlameStatus     = SAT_FS_INSUFFICIENT_DATA;
  // OT setpoint sync
  bool     bSetpointMismatch      = false;
  uint32_t iMismatchSinceMs       = 0;
  // Weather data (Open-Meteo API) — expanded field set for SAT thermal calculations
  struct {
    // --- Current conditions ---
    float    fTemperature    = 0.0f;   // temperature_2m: outdoor temperature (°C)
    float    fApparentTemp   = 0.0f;   // apparent_temperature: "feels like" (°C) — wind chill / heat index
    float    fHumidity       = 0.0f;   // relative_humidity_2m (%)
    float    fWindSpeed      = 0.0f;   // wind_speed_10m (km/h)
    float    fWindDirection  = 0.0f;   // wind_direction_10m (°) — wind chill direction
    float    fWindGusts      = 0.0f;   // wind_gusts_10m (km/h)
    float    fCloudCover     = 0.0f;   // cloud_cover (%) — for solar gain decisions
    float    fPressureMsl    = 0.0f;   // pressure_msl (hPa) — mean sea level pressure
    float    fPrecipitation  = 0.0f;   // precipitation (mm/h) — total
    float    fRain           = 0.0f;   // rain (mm/h)
    float    fSnowfall       = 0.0f;   // snowfall (cm/h)
    uint16_t iWeatherCode    = 0;      // weather_code (WMO code) — for UI display
    bool     bIsDay          = true;   // is_day flag (1=day, 0=night) — solar gain
    // --- State ---
    bool     bValid          = false;  // true after first successful fetch
    uint32_t iLastUpdateMs   = 0;      // millis() of last successful fetch
    uint16_t iFetchErrors    = 0;      // consecutive or total fetch error count
  } weather;
  // Power and energy (Task #45)
  float    fCurrentPower          = 0.0f;   // Current power in kW (modulation * capacity)
  float    fEnergyTotal           = 0.0f;   // Cumulative energy in kWh
  uint32_t iEnergyLastMs          = 0;      // Last energy integration timestamp
  // Gas consumption estimation (Task #232)
  float    fEnergyEstimatedKWh    = 0.0f;   // Cumulative estimated gas energy in kWh
  float    fEstEnergyLastSavedKWh = 0.0f;   // Value at last LittleFS save (for 0.1 kWh threshold)
  uint32_t iEstEnergyLastMs       = 0;      // Last integration timestamp for estimated energy
  // Simulation (Task #37)
  float    fSimRoomTemp           = 20.0f;
  float    fSimFlowTemp           = 20.0f;
  float    fSimOutdoorTemp        = 5.0f;
  uint32_t iSimLastUpdateMs       = 0;
  bool     bSimWarmupDone         = false;
  // Synthetic boiler-side model (TASK-795, plan §6). Declared inert in commit 1
  // so the simulation wrappers compile; commit 2 drives them via the boiler
  // model. The simulation-contract ADR is authored in commit 3 (plan §15).
  bool     bSimFlameOn            = false;
  uint8_t  iSimModulation         = 0;       // 0-100 %
  float    fSimReturnTemp         = 20.0f;   // °C
  uint32_t iSimFlameOnSinceMs     = 0;       // millis() of last synthetic flame-ON edge
  uint32_t iSimFlameOffSinceMs    = 0;       // millis() of last synthetic flame-OFF edge
  // Command trace (TASK-795 plan §4.3): last would-be boiler-side command the
  // SAT loop tried to emit while simulation blocked the bus. sLastBlockedCmd
  // remains the newest-entry alias for the MQTT single-slot + back-compat.
  char     sLastBlockedCmd[24]    = {0};
  uint32_t iLastBlockedCmdMs      = 0;
  // Command-trace ring (TASK-801 plan §12 F6): last SAT_SIM_TRACE_RING blocked
  // commands, newest-first when read out. ~16*(24+4)=448 B BSS. Head points at
  // the slot the NEXT push will write; iSimTraceCount caps at the ring size.
  char     sSimTraceCmd[16][24]   = {{0}};
  uint32_t iSimTraceMs[16]        = {0};
  uint8_t  iSimTraceHead          = 0;
  uint8_t  iSimTraceCount         = 0;
  // Availability gate (TASK-795 plan §4.2): set by the slave-frame edge hook
  // (interrupt-adjacent), consumed once in the SAT main loop so the heavy
  // writeSettings() teardown runs in cooperative context, not in the hook.
  bool     bBoilerDetectedFlag    = false;
  // Scenario injection (TASK-797 / plan §12 F2). Transient perturbations applied
  // to the synthetic room model via POST /api/v2/sat/sim/event; each expires at
  // its *ExpiryMs (0 = inactive). Only honoured while bSimulation is true.
  float    fSimWindowLossMult     = 1.0f;   // window_open: heat-loss multiplier (>1 = colder faster)
  uint32_t iSimWindowExpiryMs     = 0;      // millis() when window_open ends (0 = inactive)
  float    fSimSolarGainC         = 0.0f;   // solar_gain: additive room-warming °C/min
  uint32_t iSimSolarExpiryMs      = 0;      // millis() when solar_gain ends (0 = inactive)
  // Sensor noise + dropouts (TASK-799 / plan §12 F4). Opt-in via the F2
  // sensor_noise event. Noise is applied at the wrapper read (transient, never
  // written back into the model state, so it cannot accumulate/diverge).
  float    fSimNoiseAmplitudeC    = 0.0f;   // ± noise band on flow/return/room (°C); 0 = off
  uint32_t iSimNoiseExpiryMs      = 0;      // millis() when noise ends (0 = inactive)
  uint32_t iSimNoiseLcg           = 0x1234567U;  // LCG state for cheap deterministic noise
  // Sensor dropout (TASK-799 / plan §12 F4, 2b). During the window the room-temp
  // wrapper returns NAN — driving the real TASK-521/522 ghost-Tr / fallback path
  // — so stale-detection and thermal-estimate fallbacks exercise under sim.
  uint32_t iSimDropoutExpiryMs    = 0;      // millis() when sensor_dropout ends (0 = inactive)
  // DHW demand (TASK-800 / plan §12 F5). While active the synthetic boiler is in
  // DHW mode: bDhwActive is forced true (flame-steal — CH suppressed) so the
  // cycle classifier tags cycles SAT_CK_DHW / SAT_CK_MIXED. Triggered by the F2
  // dhw_demand event or a light periodic schedule.
  uint32_t iSimDhwExpiryMs        = 0;      // millis() when the DHW draw ends (0 = inactive)
  uint32_t iSimDhwNextSchedMs     = 0;      // millis() of the next scheduled light draw (0 = unseeded)
  // Thermal drop learning (Task #21)
  float    fEstimatedRoom         = 0.0f;   // Estimated room temp during fallback
  float    fLastKnownRoom         = 0.0f;   // Last valid room temp before fallback
  uint32_t iLastKnownRoomMs       = 0;      // When last valid room temp was recorded
  bool     bThermalModelValid     = false;   // True after sufficient learning data
  float    fThermalDropRate       = 0.0f;    // Current drop rate sample (C/hr per C delta)
  // Solar gain (Task #23)
  bool     bSolarGainActive       = false;
  float    fIndoorRiseRate        = 0.0f;  // Current indoor temp rise rate (C/hr)
  // Sun elevation (Task #68)
  float    fSunElevation          = 0.0f;  // Current sun elevation from HA (degrees)
  bool     bSunElevationValid     = false; // Whether we have valid sun elevation data
  uint32_t iSunElevLastMs         = 0;     // Timestamp of last sun elevation update
  // Summer simmer (Task #24)
  bool     bSummerActive          = false;  // Summer mode currently suppressing heating
  float    fSummerHoursAbove      = 0.0f;   // Hours outdoor temp has been above threshold
  // Humidity and comfort (Task #28/#47)
  float    fHumidity              = 0.0f;   // Current indoor humidity %
  bool     bHumidityValid         = false;  // Humidity reading available
  uint32_t iHumidityLastMs        = 0;      // Last humidity update timestamp
  float    fComfortOffset         = 0.0f;   // Current comfort adjustment (C)
  // Multi-area room temperature (Task #25)
  float    fAreaTemp[4]           = {0};    // Temperature per area
  bool     bAreaValid[4]          = {false};// Area has valid reading
  uint32_t iAreaLastMs[4]         = {0};    // Last update per area
  // PID auto-tuning (Task #27)
  bool     bAutoTuneActive        = false;  // Currently running auto-tune analysis
  uint16_t iAutoTuneCycles        = 0;      // Cycles analyzed since last adjustment
  float    fAutoTuneScore         = 0.0f;   // Current performance score (-1 to +1)
  // Last-sent OT command cache (TASK-565). Write-on-change for the
  // CS/MM/CH/TC commands the SAT loop emits every control cycle, so an
  // unchanged value is not re-enqueued every 30 s. Refreshed every
  // SAT_CMD_REFRESH_MS even when unchanged so a rebooted PIC / boiler
  // re-syncs to current SAT state. Transient runtime state per ADR-051.
  bool     bLastSentValid         = false;  // false until first emit, or after offline->online reset
  float    fLastSentCS            = 0.0f;   // CS=<float> last emitted (rounded to .1)
  uint8_t  iLastSentMM            = 0;      // MM=<uint> last emitted
  uint8_t  iLastSentCH            = 0;      // CH=<0|1> last emitted
  float    fLastSentTC            = 0.0f;   // TC=<float> last emitted (rounded to .1)
  uint32_t iLastSentCSMs          = 0;
  uint32_t iLastSentMMMs          = 0;
  uint32_t iLastSentCHMs          = 0;
  uint32_t iLastSentTCMs          = 0;
  // BLE temperature sensor (Task #20). ESP-abstraction Tier 2 (TASK-742):
  // fields are unconditional so settings/state round-trip across platforms and
  // downstream code reads them without #ifdef. On ESP8266 they stay zero/false
  // (no BLE radio; SATble.ino is gated by HAS_SAT_BLE), which keeps every
  // BLE-dependent code path inert via the bBleTempValid / iBleSensorCount gates.
  float    fBleTemp               = 0.0f;   // BLE sensor temperature (0.01C precision)
  float    fBleHumidity           = 0.0f;   // BLE sensor humidity %
  bool     bBleTempValid          = false;  // BLE reading available and non-stale
  uint32_t iBleTempLastMs         = 0;      // Last BLE temperature update timestamp
  uint8_t  iBleSensorCount        = 0;      // Number of active BLE sensors seen
  uint8_t  iBleBattery            = 0;      // Battery level of primary BLE sensor
  int8_t   iBleRssi               = 0;      // RSSI of primary BLE sensor
};

//====================================================================
//=== settings.sat — persisted SAT configuration ===
//====================================================================

// TASK-508: BLE roster size — persistent storage for self-discovery + labels.
// 8 slots: typical setup is 1 active sensor + ~7 visible neighbours. See
// SATSection sBleMac/sBleLabel below and SATble.ino _bleRuntime[].
#define SAT_BLE_MAX_ROSTER 8

struct SATSection {
  bool     bEnabled           = false;
  uint8_t  iHeatingSystem     = SAT_HSYS_AUTO; // SATHeatingSystem: auto/radiators/underfloor (heat_pump=legacy, migrated to iHeatingSource)
  uint8_t  iHeatingSource     = SAT_SRC_AUTO;  // SATHeatingSource: auto/gas_boiler/heat_pump/hybrid (TASK-891.8)
  uint16_t iHpCycleSeconds    = 1800;          // Heat-pump min-on / cycle rate: 1800s=2/hr (default) or 2400s=1.5/hr (TASK-891.8)
  float    fTargetTemp        = 20.0f;  // Default room target °C
  float    fHeatingCurveCoeff = 1.5f;   // Heating curve coefficient
  float    fDeadband          = 0.1f;   // PID deadband °C (matches Python DEADBAND=0.1)
  uint16_t iControlInterval   = 30;     // Control loop interval (seconds)
  bool     bUseExternalTemp   = false;  // Prefer MQTT-pushed indoor temp over OT msg 24
  float    fPresetComfort     = 21.0f;  // Preset: Comfort
  float    fPresetEco         = 18.0f;  // Preset: Eco
  float    fPresetAway        = 15.0f;  // Preset: Away
  float    fPresetSleep       = 16.0f;  // Preset: Sleep
  float    fPresetActivity    = 10.0f;  // Preset: Activity (used by window detection)
  float    fPresetHome        = 18.0f;  // Preset: Home (standard at-home temperature)
  bool     bPwmAutoSwitch     = true;   // Auto-switch between PWM and continuous mode
  uint8_t  iMaxRelModulation  = 100;    // Max relative modulation 0-100% (MM= command)
  float    fOvershootMargin   = 3.0f;   // Overshoot margin °C (Python OVERSHOOT_MARGIN_CELSIUS=3.0; cycle classification + auto-switch)
  float    fModSupDelay       = 20.0f;  // Modulation suppression delay (seconds)
  float    fModSupOffset      = 1.0f;   // Modulation suppression offset (°C below setpoint)
  float    fDhwSetpoint       = 0.0f;   // DHW setpoint (0=inactive, 30-60°C)
  bool     bDhwEnabled        = false;  // Enable DHW control in standalone/fallback mode
  bool     bDhwEnable         = true;   // TASK-516: master DHW enable (HW= command). Only acted on when boiler reports MsgID 3 HB3=1 (storage tank). Default true keeps boiler-default DHW enabled.
  bool     bPushSetpoint      = false;  // Push SAT target to thermostat display (TC= command)
  float    fFlameOffOffset    = 18.0f;  // Flame-off hold offset above return temp during PWM (Python flame_off_setpoint_offset_celsius=18.0): primes the boiler for fast, clean re-ignition
  bool     bWindowDetection   = false;  // Enable window open detection via MQTT
  uint16_t iWindowMinOpenSec  = 60;     // Minimum seconds window must stay open before action
  bool     bForcePWM          = false;  // Force PWM mode regardless of boiler modulation
  float    fFlowOffset        = 2.0f;   // Continuous mode: max setpoint drop from boiler temp
  float    fTargetTempStep    = 0.5f;   // Target temperature UI step size (0.1-1.0)
  float    fMinPressure       = 0.8f;   // Pressure alarm: minimum bar
  float    fMaxPressure       = 2.5f;   // Pressure alarm: maximum bar
  float    fMaxPressureDrop   = 0.3f;   // Pressure alarm: max bar/hour drop rate
  uint8_t  iManufacturer      = SAT_MFR_AUTO; // User-confirmed manufacturer (0=auto-detect)
  // Weather data (Open-Meteo API, Task #50)
  bool     bWeatherEnable     = false;  // Enable weather data fetching
  float    fWeatherLat        = 0.0f;   // Latitude (from browser geolocation or manual)
  float    fWeatherLon        = 0.0f;   // Longitude
  uint16_t iWeatherInterval   = 900;    // Poll interval in seconds (default 15 min, min 5 min)
  char     sWeatherApiKey[65] = "";     // OpenWeatherMap API key (empty = Open-Meteo only)
  // Power/energy (Task #45)
  float    fBoilerCapacity    = 24.0f;  // Boiler capacity in kW (for power calculation)
  // Gas consumption estimation (Task #232)
  float    fBoilerRatedKW     = 0.0f;   // Rated boiler input power in kW (0=disabled)
  float    fBoilerEfficiency  = 0.92f;  // Boiler efficiency (0.0-1.0, default 0.92)
  // Preset sync (Task #46) — broadcast preset changes to secondary entities
  bool     bPresetSync        = false;         // Sync preset to secondary entities via MQTT
  char     sPresetSyncTopic[65] = "";          // MQTT topic for preset sync
  // Simulation mode (Task #37) — test SAT without a real boiler
  bool     bSimulation        = false;  // Enable simulation mode
  float    fSimHeatRate       = 0.5f;   // Room heating rate C/min
  float    fSimCoolRate       = 0.1f;   // Room cooling rate C/min
  // Thermal drop learning (Task #21) — learned building thermal decay coefficient
  float    fThermalCoeff      = 0.05f;  // Learned thermal drop coefficient (C/hr per C delta)
  // Solar gain compensation (Task #23)
  bool     bSolarGainEnable   = false;  // Enable solar gain compensation
  float    fSolarMinRiseRate  = 0.6f;   // Minimum indoor rise rate (C/hr) to trigger (Python CONF_SOLAR_GAIN_MIN_RISE_PER_HOUR=0.6)
  float    fSolarSetpointOffset = 2.0f; // Setpoint reduction during solar gain (C)
  float    fSolarMinElevation = 12.0f;  // Minimum sun elevation for solar gain activation (degrees, Task #68)
  // Summer simmer (Task #24) — auto-disable heating when outdoor temp stays warm
  bool     bSummerSimmer      = false;  // Enable summer simmer auto-disable
  float    fSummerThreshold   = 18.0f;  // Outdoor temp threshold for summer mode (C)
  uint8_t  iSummerMinHours    = 6;      // Consecutive hours above threshold to trigger
  // Thermal comfort adjustment (Task #28/#47) — humidity-based setpoint correction
  bool     bComfortAdjust     = false;  // Enable thermal comfort (humidity) adjustment
  float    fComfortHumidity   = 50.0f;  // Reference humidity % (no adjustment at this level)
  float    fComfortMaxOffset  = 1.0f;   // Max target temp adjustment from humidity (C)
  // Multi-area room temperature (Task #25)
  bool     bMultiArea         = false;  // Enable multi-area weighted temperature
  uint8_t  iMultiAreaCount    = 0;      // Number of configured areas (0-4)
  float    fAreaWeight[4]     = {1.0f, 1.0f, 1.0f, 1.0f};  // Weight per area
  // PID auto-tuning (Task #27)
  bool     bAutoTune          = false;  // Enable automatic PID gains tuning
  float    fAutoTuneRate      = 0.02f;  // Adjustment rate per tuning cycle (2%)
  // SAT Python parity settings (Task #82)
  float    fMaxSetpoint        = 65.0f; // Global safety ceiling for all heating systems (Python MAXIMUM_SETPOINT)
  uint32_t iSensorMaxAgeS     = 21600; // Max age of sensor reading before considered stale (seconds, 6h default)
  bool     bErrorMonitoring   = false; // Enable detailed error stats tracking
  float    fAutoGainsValue    = 2.0f;  // Multiplier for auto-tune PID gain calculation
  bool     bAutoGains         = true;  // true=automatic gain formula; false=use fKpManual/fKiManual/fKdManual
  float    fKpManual          = 5.0f;  // Manual proportional gain (used when bAutoGains=false)
  float    fKiManual          = 0.0005f; // Manual integral gain (used when bAutoGains=false)
  float    fKdManual          = 0.0f;  // Manual derivative gain (used when bAutoGains=false)
  bool     bThermalComfort    = false; // true=use SummerSimmer index as PID room temp input
  uint16_t iHumidityTimeoutS  = 1800; // Seconds before humidity reading is considered stale (default 30 min)
  uint8_t  iHeatingMode       = 0;     // 0=COMFORT, 1=ECO
  uint8_t  iCyclesPerHour     = 3;     // Target cycles per hour (2-6)
  float    fValveOffset       = 0.0f;  // Offset for TRV valve position detection (-1 to 1)
  bool     bSolarFreezeIntegral = true; // Freeze PID integral during solar gain compensation
  // LittleFS persistence (Task #237)
  uint16_t iSatFlushThresholdH = 24;   // Hours offline before short-lived SAT data is auto-flushed on next enable
  // Multi-zone PID (Task #233)
  uint8_t  iZoneCount          = 1;    // Number of active heating zones (1-4, default 1 = single-zone)
  uint16_t iZoneTimeoutS       = 300;  // Seconds without update before zone is considered inactive (default 5 min)
  float    fZoneAggregationHeadroom = 5.0f; // Headroom added to P75 zone aggregate (°C, default 5.0)
  // TASK-587: DS18B20 sensor-to-SAT-area mapping (area 0..3)
  char sSensorArea[4][17] = {{0}};  // Dallas address (16 hex chars + null) per area; empty = unmapped
  // PV-surplus setpoint boost (TASK-640)
  bool     bPvBoostEnabled        = false;   // Enable PV-surplus boost
  uint16_t iPvBoostThresholdW     = 1500;    // Surplus threshold in W (100-10000)
  uint16_t iPvBoostHoldS          = 120;     // Hold-time before boost activates (30-600 s)
  float    fPvBoostDeltaC         = 1.5f;    // Temperature boost delta °C (0.5-5.0)
  float    fPvBoostMaxIndoorC     = 23.0f;   // Indoor max during boost (18.0-28.0 °C)
  uint16_t iPvBoostMaxDurationMin = 240;     // Max continuous boost (30-1440 min)
  // BLE temperature sensor (Task #20). ESP-abstraction Tier 2 (TASK-742):
  // persisted unconditionally so settings.json round-trips across platforms
  // (zero/empty on ESP8266, which has no BLE radio). ~360 B of settings/RAM on
  // ESP8266 — matches the already-unconditional remainder of SATSection.
  bool     bBleEnable         = false;         // Enable BLE temperature sensor scanning
  bool     bBleFailover       = true;          // TASK-762: when the pinned sensor (sBleMAC) goes stale, fall back to another fresh roster sensor (roster order)
  char     sBleMAC[18]        = "";            // Bind to specific sensor MAC (empty = accept all)
  uint16_t iBleInterval       = 30;            // Publish/state-update cadence (sec, 10-300). NOT scan rate: TASK-494 made the BLE scan continuous to match OT-Thing.
  // TASK-895: BLE name picker + name-prefix filter. sBleNamePrefix narrows the
  // roster by advertised BLE name (empty = off, admit all = current behaviour).
  // bBleNameFilterIngest promotes the prefix from a UI-display filter to a
  // roster-ingestion gate. Only CONFIRMED mismatches (name known AND not a
  // prefix match) are rejected/hidden; an empty/unknown name is always
  // admitted/shown. The advertised names themselves are runtime-only
  // (SATble.ino BLERuntime.sName) and never persisted.
  char     sBleNamePrefix[24]   = "";          // Name-prefix filter (case-insensitive; empty = off)
  bool     bBleNameFilterIngest = false;       // false = display filter only; true = also gate roster ingestion
  // TASK-508: BLE sensor self-discovery roster (max SAT_BLE_MAX_ROSTER known
  // sensors). sBleMac/sBleLabel are persistent here; runtime data (temp,
  // rssi, age, discovery flags) lives in SATble.ino's _bleRuntime[] array
  // indexed by the same slot. The 'selected' sensor is sBleMAC above
  // (one of these 8 entries when set; kept for backward compatibility).
  char     sBleMac[SAT_BLE_MAX_ROSTER][18]   = {{0}};   // Known sensor MACs (uppercase AA:BB:..)
  char     sBleLabel[SAT_BLE_MAX_ROSTER][24] = {{0}};   // User-friendly names ("Woonkamer")
  uint8_t  iBleRosterCount                   = 0;       // Count of populated slots
  static_assert(sizeof(sBleLabel[0]) <= 100, "label too long for settingStuff line buffer");
};
