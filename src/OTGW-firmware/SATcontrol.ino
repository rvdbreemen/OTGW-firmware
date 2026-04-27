/*
***************************************************************************
**  Module   : SATcontrol.ino
**  Description: SAT (Smart Autotune Thermostat) control loop
**
**  Ported from SAT Python custom component (releases/thermo-nova)
**  Original SAT component by Alex Wijnholds (https://github.com/Alexwijn/SAT)
**  SAT concept and algorithm design by George Dellas
**
**  Standalone smart heating controller embedded in OTGW firmware.
**  Uses OpenTherm data directly from OTcurrentSystemState.
**  Optional external sensor input via MQTT/REST.
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License. See bottom of OTGW-firmware.h
***************************************************************************
*/

// Per-module conditional debug — toggle with key '5' in telnet debug menu
#define SATDebugTf(fmt, ...)  do { if (state.debug.bSAT) DebugTf(fmt,  ##__VA_ARGS__); } while(0)
#define SATDebugTln(s)        do { if (state.debug.bSAT) DebugTln(s);                  } while(0)
#define SATDebugf(fmt, ...)   do { if (state.debug.bSAT) Debugf(fmt,   ##__VA_ARGS__); } while(0)

// --- Heating Curve Constants ---
static const float SAT_HC_BASE_OFFSET_FLOOR  = 20.0f;   // Underfloor base offset
static const float SAT_HC_BASE_OFFSET_RAD    = 27.2f;   // Radiator base offset
static const float SAT_HC_REF_TEMP           = 20.0f;   // Reference temperature

// --- Control Constants ---
static const float SAT_MIN_SETPOINT          = 10.0f;   // Minimum boiler setpoint °C
static const float SAT_MAX_SETPOINT_DEFAULT  = 75.0f;   // Default max boiler setpoint °C
static const float SAT_FLOW_OFFSET_CONTINUOUS = 5.0f;   // Flow temp offset for continuous mode smoothing
static const float SAT_PWM_MIN_ON_SEC        = 180.0f;  // Min flame-on time in PWM mode (matches Python HEATER_STARTUP_TIMEFRAME = 180s)

// --- Safety Constants ---
static const float    SAT_HARD_MAX_FLOOR     = 50.0f;   // Absolute ceiling for underfloor heating
static const float    SAT_HARD_MAX_RAD       = 80.0f;   // Absolute ceiling for radiator heating
static const float    SAT_GLOBAL_MAX_SETPOINT = 65.0f;  // Python MAXIMUM_SETPOINT: global safety ceiling for all heating systems
static const uint32_t SAT_STALE_TEMP_BLE_MS  = 300000UL; // 5 min: BLE indoor temp considered stale
static const uint32_t SAT_STALE_TEMP_MS      = 300000UL; // 5 min: legacy alias (kept for any future use)
static const uint32_t SAT_STALE_OUTDOOR_MS   = 600000UL; // 10 min: external outdoor temp considered stale
static const uint8_t  SAT_MAX_SKIP_COUNT     = 10;       // Consecutive invalid-input skips before disable
static const uint8_t  SAT_MAX_PIC_FAILS      = 5;        // Consecutive PIC comm failures before disable

// --- Thermal Drop Learning Constants (Task #21) ---
static const uint32_t SAT_THERMAL_SAMPLE_MS   = 300000UL; // 5 min between learning samples
static const uint32_t SAT_THERMAL_SAVE_MS     = 3600000UL; // 1 hour between settings saves
static const uint32_t SAT_THERMAL_VALID_MS    = 86400000UL; // 24h of data for model validity
static const float    SAT_THERMAL_MIN_DELTA   = 2.0f;    // Min indoor-outdoor delta for learning
static const float    SAT_THERMAL_MIN_DROP    = 0.05f;   // Min room temp drop per sample (C)
static const float    SAT_THERMAL_EMA_ALPHA   = 0.1f;    // EMA smoothing for coefficient
static const float    SAT_THERMAL_COEFF_MIN   = 0.005f;  // Minimum plausible coefficient
static const float    SAT_THERMAL_COEFF_MAX   = 0.3f;    // Maximum plausible coefficient
static const float    SAT_THERMAL_EST_MIN     = 5.0f;    // Minimum estimated room temp
static const float    SAT_THERMAL_EST_MAX     = 35.0f;   // Maximum estimated room temp
static const float    SAT_THERMAL_SAFE_FLOW   = 45.0f;   // Fixed safe setpoint after 2h estimation
static const uint32_t SAT_THERMAL_MAX_EST_MS  = 7200000UL; // 2h: max estimation before safe setpoint

// --- Multi-area Constants (Task #25) ---
static const uint8_t  SAT_MAX_AREAS           = 4;         // Maximum number of temperature areas
static const uint32_t SAT_AREA_STALE_MS       = 300000UL;  // 5 min: area temp considered stale

// --- Simulation Constants (Task #37) ---
static const float    SAT_SIM_FLOW_HEAT_RATE = 2.0f;    // Flow temp rise rate C/s when heating
static const float    SAT_SIM_FLOW_COOL_RATE = 1.0f;    // Flow temp fall rate C/s when off
static const uint32_t SAT_SIM_WARMUP_MS      = 300000UL; // 5 min warmup period

// --- Manufacturer Table (PROGMEM) ---
struct SATMfrEntry {
  uint8_t  memberID;   // OT MemberID code (MsgID 3 valueLB)
  uint8_t  quirks;     // Bitmask of SAT_QUIRK_* flags
  char     name[12];   // Short display name
};
static const SATMfrEntry satManufacturerTable[] PROGMEM = {
  // Index must match SATManufacturer enum (skip AUTO=0)
  /*  1 ATAG       */ {   4, 0,                                              "Atag"       },
  /*  2 BAXI       */ {   4, 0,                                              "Baxi"       },
  /*  3 BROTGE     */ {   4, 0,                                              "Brotge"     },
  /*  4 DEDIETRICH */ {   4, 0,                                              "DeDietrich" },
  /*  5 FERROLI    */ {   9, 0,                                              "Ferroli"    },
  /*  6 GEMINOX    */ {   4, SAT_QUIRK_MIN_MOD_10 | SAT_QUIRK_NO_REL_MOD,   "Geminox"    },
  /*  7 IDEAL      */ {   6, SAT_QUIRK_NO_REL_MOD | SAT_QUIRK_MI_500_BOOT,  "Ideal"      },
  /*  8 IMMERGAS   */ {  27, SAT_QUIRK_IMMERGAS_TP,                          "Immergas"   },
  /*  9 INTERGAS   */ { 173, SAT_QUIRK_NO_REL_MOD | SAT_QUIRK_MI_500_BOOT,  "Intergas"   },
  /* 10 ITHO       */ {  29, 0,                                              "Itho"       },
  /* 11 NEFIT      */ { 131, SAT_QUIRK_NO_REL_MOD | SAT_QUIRK_MI_500_BOOT,  "Nefit"      },
  /* 12 RADIANT    */ {  41, 0,                                              "Radiant"    },
  /* 13 REMEHA     */ {  11, 0,                                              "Remeha"     },
  /* 14 SIME       */ {  27, 0,                                              "Sime"       },
  /* 15 VAILLANT   */ {  24, 0,                                              "Vaillant"   },
  /* 16 VIESSMANN  */ {  33, 0,                                              "Viessmann"  },
  /* 17 WORCESTER  */ {  95, 0,                                              "Worcester"  },
  /* 18 OTHER      */ {   0, 0,                                              "Other"      },
};
static const uint8_t SAT_MFR_TABLE_SIZE = sizeof(satManufacturerTable) / sizeof(satManufacturerTable[0]);

// Returns the effective manufacturer (resolves AUTO to detected)
static uint8_t satGetEffectiveManufacturer()
{
  if (settings.sat.iManufacturer == SAT_MFR_AUTO)
    return state.sat.iDetectedManufacturer;
  return settings.sat.iManufacturer;
}

// Returns the manufacturer name from PROGMEM table
static void satGetManufacturerName(char* buf, size_t bufLen)
{
  uint8_t mfr = satGetEffectiveManufacturer();
  if (mfr >= SAT_MFR_ATAG && mfr <= SAT_MFR_OTHER) {
    strncpy_P(buf, satManufacturerTable[mfr - 1].name, bufLen - 1);
    buf[bufLen - 1] = '\0';
  } else {
    strlcpy(buf, "Unknown", bufLen);
  }
}

// Returns the quirk flags for the effective manufacturer
static uint8_t satGetManufacturerQuirks()
{
  uint8_t mfr = satGetEffectiveManufacturer();
  if (mfr >= SAT_MFR_ATAG && mfr <= SAT_MFR_OTHER)
    return pgm_read_byte(&satManufacturerTable[mfr - 1].quirks);
  return 0;
}

// Auto-detect manufacturer from OT MsgID 3 slave MemberID
// Note: some MemberIDs are shared (e.g. 4 = Atag/Baxi/Brotge/DeDietrich/Geminox)
// Returns first match; user should confirm via dropdown for ambiguous IDs
void satDetectManufacturer(uint8_t slaveMemberID)
{
  state.sat.iSlaveMemberID = slaveMemberID;
  for (uint8_t i = 0; i < SAT_MFR_TABLE_SIZE; i++) {
    if (pgm_read_byte(&satManufacturerTable[i].memberID) == slaveMemberID) {
      state.sat.iDetectedManufacturer = i + 1; // +1 because enum starts at ATAG=1
      char name[12];
      strncpy_P(name, satManufacturerTable[i].name, sizeof(name) - 1);
      name[sizeof(name) - 1] = '\0';
      SATDebugTf(PSTR("SAT: Detected manufacturer: %s (MemberID %d)\r\n"), name, slaveMemberID);
      return;
    }
  }
  state.sat.iDetectedManufacturer = SAT_MFR_OTHER;
  SATDebugTf(PSTR("SAT: Unknown manufacturer (MemberID %d)\r\n"), slaveMemberID);
}

// --- Heating System Helper Functions ---
// Returns the effective heating system (resolves AUTO to detected or fallback)
static uint8_t satGetEffectiveHeatingSystem()
{
  if (settings.sat.iHeatingSystem == SAT_HSYS_AUTO)
    return state.sat.iDetectedHeatingSystem;
  return settings.sat.iHeatingSystem;
}

// Returns max boiler setpoint for the current heating system.
//
// Task #230 -- 62C vs Python 55C for radiators: intentional and correct.
// Python SAT calculate_default_maximum_setpoint() returns 55C as a conservative default
// for modern condensing boilers with weather compensation. The C++ value of 62C is the
// correct ceiling for traditional European high-temperature radiator systems (EN 12831,
// typical design at -10C outdoor temp requires 70/60C or 75/65C flow/return). Modern
// condensing boilers are typically limited by their own MaxTSet OT parameter (read in
// satControlLoop), which caps this value in practice. 62C is therefore the right upper
// bound for legacy radiators; the Python 55C is a conservative out-of-the-box default
// for new installs and does not represent an OT spec requirement.
// Heat pump: C++ 40C matches Python's heat pump cap -- no discrepancy there.
static float satGetMaxSetpoint()
{
  switch (satGetEffectiveHeatingSystem()) {
    case SAT_HSYS_HEAT_PUMP:  return 40.0f;
    case SAT_HSYS_UNDERFLOOR: return 45.0f;
    case SAT_HSYS_RADIATORS:
    default:                  return 62.0f;  // 62C: correct for high-temp radiator systems (see comment above)
  }
}

// Returns heating curve base offset for the current heating system
static float satGetBaseOffset()
{
  switch (satGetEffectiveHeatingSystem()) {
    case SAT_HSYS_UNDERFLOOR: return SAT_HC_BASE_OFFSET_FLOOR;  // 20.0
    case SAT_HSYS_HEAT_PUMP:
    case SAT_HSYS_RADIATORS:
    default:                  return SAT_HC_BASE_OFFSET_RAD;    // 27.2
  }
}

// Returns max PWM cycles per hour for the current heating system
static uint8_t satGetMaxCyclesPerHour()
{
  // Use user-configured value if set (Task #82); otherwise fall back to system defaults
  if (settings.sat.iCyclesPerHour >= 2 && settings.sat.iCyclesPerHour <= 6) {
    return settings.sat.iCyclesPerHour;
  }
  switch (satGetEffectiveHeatingSystem()) {
    case SAT_HSYS_HEAT_PUMP:  return 2;   // Heat pumps: max 2 cycles/hr
    case SAT_HSYS_UNDERFLOOR: return 3;
    case SAT_HSYS_RADIATORS:
    default:                  return 4;
  }
}

// Returns minimum ON time in seconds for the current heating system
static uint32_t satGetMinOnTimeSec()
{
  switch (satGetEffectiveHeatingSystem()) {
    case SAT_HSYS_HEAT_PUMP:  return 1800; // 30 minutes for heat pumps
    case SAT_HSYS_UNDERFLOOR:
    case SAT_HSYS_RADIATORS:
    default:                  return 180;  // 3 minutes for gas boilers
  }
}

// Returns whether MM=100 should always be used (heat pumps)
static bool satAlwaysMaxModulation()
{
  return (satGetEffectiveHeatingSystem() == SAT_HSYS_HEAT_PUMP);
}

// Returns the name string for the current heating system.
// Kept available for future diagnostic endpoint exposing heating-system type.
__attribute__((unused))
static const char* satGetHeatingSystemName()
{
  switch (satGetEffectiveHeatingSystem()) {
    case SAT_HSYS_HEAT_PUMP:  return "heat_pump";
    case SAT_HSYS_UNDERFLOOR: return "underfloor";
    case SAT_HSYS_RADIATORS:  return "radiators";
    default:                  return "auto";
  }
}

static uint8_t _sat_consecutiveSkips  = 0;
static uint8_t _sat_picFailCount      = 0;
static bool    _sat_bootCS0sent       = false;  // One-shot: ensure CS=0 is sent once PIC is available
static bool    _sat_prevDhwActive     = false;  // Track DHW transition to send SW= on entry (TASK-437)

// --- Thermal Drop Learning State (Task #21) ---
static float    _thermal_prevRoom     = 0.0f;
static uint32_t _thermal_prevMs       = 0;
static bool     _thermal_learning     = false;  // true when flame is off and measuring decay
static float    _thermal_coeffEma     = 0.05f;  // Running EMA of thermal coefficient
static uint32_t _thermal_lastSaveMs   = 0;
static uint32_t _thermal_totalLearnMs = 0;       // Accumulated learning time

// --- OPV Calibration Constants ---
static const uint32_t SAT_CALIB_TIMEOUT_MS    = 1800000UL; // 30 min total timeout
static const uint32_t SAT_CALIB_FLAME_WAIT_MS = 180000UL;  // 3 min wait for flame
static const uint32_t SAT_CALIB_MEASURE_MS    = 1200000UL; // 20 min measuring phase
static const uint32_t SAT_CALIB_SAMPLE_MS     = 10000UL;   // Sample every 10s
static const float    SAT_CALIB_WARM_DELTA    = 5.0f;      // Temp must rise 5C above start
static const uint16_t SAT_CALIB_MIN_SAMPLES   = 40;        // Minimum samples before accepting result (Python parity)

// OPV calibration state machine - called from control loop when calibration is active
static void satOvpCalibrate()
{
  float boilerTemp = OTcurrentSystemState.Tboiler;
  bool  flameOn    = (OTcurrentSystemState.Statusflags & 0x08) != 0;
  uint32_t elapsed = millis() - state.sat.iCalibStartMs;

  switch (state.sat.eCalibPhase) {
    case SAT_CALIB_STARTING: {
      // Send high setpoint + MM=0 to find minimum boiler output
      float calibSetpoint = satGetMaxSetpoint();
      char cmdBuf[16];
      snprintf_P(cmdBuf, sizeof(cmdBuf), PSTR("CS=%.1f"), calibSetpoint);
      addCommandToQueue(cmdBuf, strlen(cmdBuf), false, 0);
      addCommandToQueue("MM=0", 4, false, 0);
      state.sat.fCalibStartTemp = boilerTemp;
      state.sat.fCalibMaxTemp   = boilerTemp;
      state.sat.iCalibSamples   = 0;
      SATDebugTf(PSTR("OPV: calibration started, CS=%.1f MM=0, boiler=%.1f\r\n"), calibSetpoint, boilerTemp);
      state.sat.eCalibPhase = SAT_CALIB_WARMING;
      break;
    }
    case SAT_CALIB_WARMING: {
      // Wait for flame and temp to rise
      if (!flameOn && elapsed > SAT_CALIB_FLAME_WAIT_MS) {
        DebugTln(F("OPV: FAILED - no flame after 3 min"));
        state.sat.eCalibPhase = SAT_CALIB_FAILED;
        break;
      }
      if (boilerTemp > state.sat.fCalibStartTemp + SAT_CALIB_WARM_DELTA) {
        SATDebugTf(PSTR("OPV: warming done, boiler=%.1f, starting measurement\r\n"), boilerTemp);
        state.sat.fCalibMaxTemp = boilerTemp;
        state.sat.iCalibStartMs = millis(); // Reset timer for measuring phase
        state.sat.eCalibPhase = SAT_CALIB_MEASURING;
      }
      if (elapsed > SAT_CALIB_TIMEOUT_MS) {
        DebugTln(F("OPV: FAILED - timeout during warming"));
        state.sat.eCalibPhase = SAT_CALIB_FAILED;
      }
      break;
    }
    case SAT_CALIB_MEASURING: {
      // Sample boiler temp, track maximum
      state.sat.iCalibSamples++;
      if (boilerTemp > state.sat.fCalibMaxTemp) {
        state.sat.fCalibMaxTemp = boilerTemp;
      }
      // Keep sending MM=0 to maintain minimum modulation
      addCommandToQueue("MM=0", 4, false, 0);
      elapsed = millis() - state.sat.iCalibStartMs;
      if (elapsed >= SAT_CALIB_MEASURE_MS) {
        // Enforce minimum sample count before accepting result (Python: OVERSHOOT_PROTECTION_REQUIRED_DATASET = 40)
        if (state.sat.iCalibSamples < SAT_CALIB_MIN_SAMPLES) {
          DebugTf(PSTR("OPV: FAILED - only %u/%u samples collected, result rejected\r\n"),
                  (unsigned)state.sat.iCalibSamples, (unsigned)SAT_CALIB_MIN_SAMPLES);
          state.sat.eCalibPhase = SAT_CALIB_FAILED;
        } else {
          // Measurement complete with sufficient samples
          settings.sat.fOvpValue = state.sat.fCalibMaxTemp;
          settings.sat.bOvpEnabled = true;
          SATDebugTf(PSTR("OPV: calibration DONE! OPV=%.1f from %u/%u samples\r\n"),
                  state.sat.fCalibMaxTemp, (unsigned)state.sat.iCalibSamples, (unsigned)SAT_CALIB_MIN_SAMPLES);
          state.sat.eCalibPhase = SAT_CALIB_DONE;
        }
      }
      break;
    }
    case SAT_CALIB_DONE:
    case SAT_CALIB_FAILED: {
      // Recovery: send CS=0 and MM=100 to return to normal
      addCommandToQueue("CS=0", 4, false, 0);
      char cmdBuf[8];
      snprintf_P(cmdBuf, sizeof(cmdBuf), PSTR("MM=%u"), settings.sat.iMaxRelModulation);
      addCommandToQueue(cmdBuf, strlen(cmdBuf), false, 0);
      state.sat.eCalibPhase = SAT_CALIB_IDLE;
      break;
    }
    default:
      break;
  }
}

// Start OPV calibration
static void satOvpStartCalibration()
{
  if (state.sat.eCalibPhase != SAT_CALIB_IDLE) return; // Already running
  state.sat.eCalibPhase  = SAT_CALIB_STARTING;
  state.sat.iCalibStartMs = millis();
  SATDebugTln(F("OPV: calibration requested"));
}

// Cancel OPV calibration
static void satOvpStopCalibration()
{
  if (state.sat.eCalibPhase == SAT_CALIB_IDLE) return;
  SATDebugTln(F("OPV: calibration cancelled"));
  state.sat.eCalibPhase = SAT_CALIB_FAILED; // Will trigger recovery on next call
}

// --- Boiler Status Tracking ---
static bool     _bs_prevFlame          = false;
static float    _bs_prevModulation     = 0.0f;
static float    _bs_prevBoilerTemp     = 0.0f;
static uint32_t _bs_stateEntryMs       = 0;
static uint32_t _bs_flameOnMs          = 0;
static uint32_t _bs_flameOffMs         = 0;

// --- Previous flame state for edge detection ---
static bool     _sat_prevFlameState = false;

// --- Timer for control loop (initial value, updated from settings in initSAT) ---
DECLARE_TIMER_SEC(timerSATControl, settings.sat.iControlInterval, CATCH_UP_MISSED_TICKS);
// --- Timer for 4-hour window stats (Task #227): update once per minute ---
DECLARE_TIMER_SEC(timerSAT4hStats, 60, SKIP_MISSED_TICKS);

//=====================================================================
//=== Heating Curve Calculation ===
//=====================================================================
static float satCalcHeatingCurve(float targetTemp, float outsideTemp)
{
  float baseOffset = satGetBaseOffset();
  float coeff = settings.sat.fHeatingCurveCoeff;
  float diff = outsideTemp - SAT_HC_REF_TEMP;

  // curve = 4*(target - 20) + 0.03*(outside - 20)^2 - 0.4*(outside - 20)
  float curveValue = 4.0f * (targetTemp - SAT_HC_REF_TEMP)
                   + 0.03f * diff * diff
                   - 0.4f * diff;

  float setpoint = baseOffset + (coeff / 4.0f) * curveValue;

  state.sat.fHeatingCurveValue = setpoint;
  return setpoint;
}

//=====================================================================
//=== Boiler Status Evaluator ===
//=====================================================================
// SAT Python status.py timing constants
static const uint32_t BS_ANTI_CYCLE_MIN_OFF_MS    = 180000UL;  // 180s min OFF between cycles
static const uint32_t BS_STALLED_IGNITION_MIN_MS  = 600000UL;  // 600s fallback stalled threshold (no prior cycle)
static const uint32_t BS_STALLED_IGNITION_FLOOR_MS = 120000UL; // 120s adaptive threshold floor
static const uint32_t BS_STALLED_IGNITION_CAP_MS   = 900000UL; // 900s adaptive threshold cap
static const uint32_t BS_POST_CYCLE_SETTLE_MS     = 60000UL;   // 60s post-cycle settling
static const uint32_t BS_IGNITION_SURGE_WINDOW_MS = 30000UL;   // 30s window for ignition surge
static const float    BS_DEMAND_HYSTERESIS        = 0.7f;      // demand = setpoint > flow + 0.7C
static const float    BS_AT_SETPOINT_BAND         = 1.5f;      // +/- 1.5C at setpoint
static const float    BS_PUMP_START_DELTA         = 6.0f;      // pump starting: temp drop > 6C
static const float    BS_IGNITION_SURGE_RATE      = 0.5f;      // 0.5C/s temp rise rate

static void satUpdateBoilerStatus()
{
  bool flame = (OTcurrentSystemState.Statusflags & 0x08) != 0;
  float mod = OTcurrentSystemState.RelModLevel;
  float boilerTemp = OTcurrentSystemState.Tboiler;
  float setpoint = state.sat.fFinalSetpoint;
  uint32_t now = millis();
  SATBoilerStatus prev = state.sat.eBoilerStatus;

  // Track flame transitions for timing
  if (flame && !_bs_prevFlame) {
    _bs_flameOnMs = now;
  }
  if (!flame && _bs_prevFlame) {
    _bs_flameOffMs = now;
  }

  // Demand detection with hysteresis (SAT Python)
  bool hasDemand = (setpoint > boilerTemp + BS_DEMAND_HYSTERESIS);

  // Temperature rate of change (degrees per second)
  float tempRate = 0.0f;
  if (_bs_prevBoilerTemp > 0.001f) {
    // Approximate: we're called each control cycle
    float dt = (float)(now - _bs_stateEntryMs) / 1000.0f;
    if (dt > 0.5f) {
      tempRate = (boilerTemp - _bs_prevBoilerTemp);  // per call, not per second
    }
  }

  SATBoilerStatus newStatus = SAT_BS_OFF;

  if (!flame && !_bs_prevFlame) {
    // --- No flame, was already off ---
    uint32_t offDuration = (now - _bs_flameOffMs);

    if (boilerTemp > setpoint + settings.sat.fOvershootMargin) {
      newStatus = SAT_BS_OVERSHOOT_COOLING;
    }
    else if (_bs_flameOffMs > 0 && offDuration < BS_POST_CYCLE_SETTLE_MS &&
             (prev == SAT_BS_HEATING || prev == SAT_BS_AT_SETPOINT ||
              prev == SAT_BS_COOLING || prev == SAT_BS_POST_CYCLE)) {
      newStatus = SAT_BS_POST_CYCLE;
    }
    else if (hasDemand && _bs_flameOffMs > 0 && offDuration < BS_ANTI_CYCLE_MIN_OFF_MS) {
      newStatus = SAT_BS_ANTI_CYCLING;
    }
    else if (hasDemand && _bs_flameOffMs > 0) {
      // Adaptive stall threshold: max(last_cycle_duration * 1.5, 120s), cap at 900s.
      // Falls back to fixed 600s when no prior cycle duration is available (cold start).
      uint32_t stalledThreshold;
      if (state.sat.fLastCycleDuration > 0.0f) {
        uint32_t adaptive = (uint32_t)(state.sat.fLastCycleDuration * 1500.0f); // * 1000 ms/s * 1.5
        if (adaptive < BS_STALLED_IGNITION_FLOOR_MS) adaptive = BS_STALLED_IGNITION_FLOOR_MS;
        if (adaptive > BS_STALLED_IGNITION_CAP_MS)   adaptive = BS_STALLED_IGNITION_CAP_MS;
        stalledThreshold = adaptive;
      } else {
        stalledThreshold = BS_STALLED_IGNITION_MIN_MS; // 600s fallback
      }
      if (offDuration > stalledThreshold) {
        newStatus = SAT_BS_STALLED_IGNITION;
      } else {
        newStatus = SAT_BS_WAITING_FLAME;
      }
    }
    else {
      newStatus = SAT_BS_IDLE;
    }
  }
  else if (flame && !_bs_prevFlame) {
    // --- Flame just ignited ---
    if (boilerTemp < setpoint - BS_PUMP_START_DELTA) {
      newStatus = SAT_BS_PUMP_STARTING;
    } else {
      newStatus = SAT_BS_IGNITION_SURGE;
    }
  }
  else if (flame) {
    // --- Flame is on ---
    uint32_t flameDuration = (now - _bs_flameOnMs);

    // Check for ignition surge (within 30s, rapid temp rise)
    if (flameDuration < BS_IGNITION_SURGE_WINDOW_MS && tempRate >= BS_IGNITION_SURGE_RATE) {
      newStatus = SAT_BS_IGNITION_SURGE;
    }
    // Pump starting: early flame, temp dropping, large delta
    else if (flameDuration < BS_IGNITION_SURGE_WINDOW_MS &&
             tempRate < 0.0f && (setpoint - boilerTemp) > BS_PUMP_START_DELTA) {
      newStatus = SAT_BS_PUMP_STARTING;
    }
    // At setpoint band (1.5C)
    else if (fabsf(boilerTemp - setpoint) <= BS_AT_SETPOINT_BAND) {
      newStatus = SAT_BS_AT_SETPOINT;
    }
    else if (boilerTemp < setpoint) {
      if (mod > _bs_prevModulation + 1.0f) {
        newStatus = SAT_BS_MODULATING_UP;
      } else {
        newStatus = SAT_BS_PREHEATING;
      }
    }
    else {
      // boilerTemp > setpoint + band
      if (mod < _bs_prevModulation - 1.0f) {
        newStatus = SAT_BS_MODULATING_DOWN;
      } else {
        newStatus = SAT_BS_OVERSHOOT_COOLING;
      }
    }
  }
  else {
    // --- Flame just turned off ---
    newStatus = SAT_BS_COOLING;
  }

  if (newStatus != prev) {
    _bs_stateEntryMs = now;
  }

  state.sat.eBoilerStatus = newStatus;
  _bs_prevFlame = flame;
  _bs_prevModulation = mod;
  _bs_prevBoilerTemp = boilerTemp;
}

// String labels for boiler status (PROGMEM)
static const char _bsOff[]        PROGMEM = "off";
static const char _bsIdle[]       PROGMEM = "idle";
static const char _bsPreheat[]    PROGMEM = "preheating";
static const char _bsAtSetpoint[] PROGMEM = "at_setpoint";
static const char _bsModUp[]      PROGMEM = "modulating_up";
static const char _bsModDown[]    PROGMEM = "modulating_down";
static const char _bsSurge[]      PROGMEM = "ignition_surge";
static const char _bsStalled[]    PROGMEM = "stalled_ignition";
static const char _bsAntiCycle[]  PROGMEM = "anti_cycling";
static const char _bsPumpStart[]  PROGMEM = "pump_starting";
static const char _bsWaiting[]    PROGMEM = "waiting_flame";
static const char _bsOvershoot[]  PROGMEM = "overshoot_cooling";
static const char _bsPostCycle[]  PROGMEM = "post_cycle";
static const char _bsHeating[]    PROGMEM = "heating";
static const char _bsCooling[]    PROGMEM = "cooling";

static PGM_P const _bsNames[] PROGMEM = {
  _bsOff, _bsIdle, _bsPreheat, _bsAtSetpoint,
  _bsModUp, _bsModDown, _bsSurge, _bsStalled,
  _bsAntiCycle, _bsPumpStart, _bsWaiting, _bsOvershoot,
  _bsPostCycle, _bsHeating, _bsCooling
};

void satGetBoilerStatusName(char* buf, size_t bufLen)
{
  int idx = (int)state.sat.eBoilerStatus;
  if (idx < 0 || idx > (int)SAT_BS_COOLING) idx = 0;
  strncpy_P(buf, (PGM_P)pgm_read_ptr(&_bsNames[idx]), bufLen - 1);
  buf[bufLen - 1] = '\0';
}

//=====================================================================
//=== PWM Control Mode ===
//=====================================================================
// PWM state tracking for effective temperature and flame timing
static float    _pwm_effectiveBoilerTemp    = 0.0f;
static uint32_t _pwm_flameOnMs             = 0;
static bool     _pwm_waitingForFlame       = false;
static uint32_t _pwm_waitForFlameStartMs   = 0;    // Timestamp when flame wait began (for 180s timeout)
static float    _pwm_flameOffHoldSetpoint  = 0.0f;
// Python HEATER_STARTUP_TIMEFRAME: max wait for ignition before giving up
static const uint32_t PWM_IGNITION_TIMEOUT_MS = 180000UL; // 180s ignition timeout

static float satApplyPWM(float pidOutput)
{
  // --- Duty cycle calculation per SAT Python: (setpoint - base) / (effective_temp - base) ---
  float baseOffset = satGetBaseOffset();
  float effectiveTemp = _pwm_effectiveBoilerTemp;
  if (effectiveTemp < baseOffset + 5.0f) effectiveTemp = baseOffset + 20.0f; // Fallback

  float duty = (pidOutput - baseOffset) / (effectiveTemp - baseOffset);
  if (duty < 0.0f) duty = 0.0f;
  if (duty > 1.0f) duty = 1.0f;
  state.sat.fPwmDutyCycle = duty;

  // --- Time thresholds from heating system ---
  uint32_t minOnMs   = satGetMinOnTimeSec() * 1000UL;  // 180s gas, 1800s heat pump
  uint8_t  cyclesHr  = satGetMaxCyclesPerHour();
  uint32_t upperMs   = (3600000UL / cyclesHr);          // e.g. 900s at 4 cycles/hr
  uint32_t maxMs     = upperMs * 2;                      // Max cycle time

  // --- Effective temperature tracking (EMA during first 30s of flame-on) ---
  bool flame = (OTcurrentSystemState.Statusflags & 0x08) != 0;
  float boilerTemp = OTcurrentSystemState.Tboiler;
  if (flame && !_pwm_waitingForFlame) {
    uint32_t sinceFlamOn = millis() - _pwm_flameOnMs;
    if (sinceFlamOn < 30000UL) {
      // EMA smoothing during first 30s (alpha=0.3)
      _pwm_effectiveBoilerTemp = 0.3f * boilerTemp + 0.7f * _pwm_effectiveBoilerTemp;
    }
  }

  // --- Flame sync: track flame-on moment ---
  if (flame && _pwm_waitingForFlame) {
    _pwm_flameOnMs = millis();
    _pwm_waitingForFlame = false;
    _pwm_waitForFlameStartMs = 0;  // Clear ignition timeout on successful ignition
  }
  if (!flame && !_pwm_waitingForFlame && state.sat.bPwmFlameRequested) {
    _pwm_waitingForFlame = true;
    _pwm_waitForFlameStartMs = millis();  // Start ignition timeout timer
  }

  // --- Duty cycle thresholds derived from cycles_per_hour (per SAT Python pwm.py) ---
  float dutyLower = (float)minOnMs / (float)upperMs;   // e.g. 180/900 = 0.20 at 4cph
  float dutyUpper = 1.0f - dutyLower;                  // e.g. 0.80
  float dutyMin   = dutyLower / 2.0f;                  // e.g. 0.10
  float dutyMax   = 1.0f - dutyMin;                    // e.g. 0.90

  // --- 5-range duty cycle mapper (SAT Python parity) ---
  uint32_t onTimeMs, offTimeMs;

  float maxSetpoint = satGetMaxSetpoint();

  if (duty >= dutyMax) {
    // Range 5: Over-max - continuous ON (no CS startup sequence needed)
    state.sat.bPwmFlameRequested = true;
    return pidOutput;
  } else if (duty < dutyMin) {
    // Range 1: Ultra-low - keep off or let existing flame finish min-on
    if (flame && !state.sat.bDhwActive) {
      // Flame already on: run minimum on-time, then long off
      onTimeMs  = minOnMs;
      offTimeMs = maxMs - minOnMs;
    } else {
      // No flame: stay off entirely
      onTimeMs  = 0;
      offTimeMs = maxMs;
    }
  } else if (duty <= dutyLower) {
    // Range 2: Low - on=min, off scaled by duty
    onTimeMs  = minOnMs;
    offTimeMs = (uint32_t)((float)minOnMs * (1.0f - duty) / duty);
    if (offTimeMs > maxMs - minOnMs) offTimeMs = maxMs - minOnMs;
  } else if (duty <= dutyUpper) {
    // Range 3: Mid - proportional split
    onTimeMs  = (uint32_t)(duty * (float)upperMs);
    offTimeMs = upperMs - onTimeMs;
    if (onTimeMs < minOnMs) onTimeMs = minOnMs;
  } else {
    // Range 4: High - on=min/(1-d)-min, off=min (SAT Python formula)
    onTimeMs  = (uint32_t)((float)minOnMs / (1.0f - duty)) - minOnMs;
    offTimeMs = minOnMs;
    if (onTimeMs < minOnMs) onTimeMs = minOnMs;
  }

  // --- PWM state machine with 4-step CS startup sequence ---
  uint32_t sinceFlameStart = millis() - satCycleGetFlameOnStartMs();
  if (state.sat.bPwmFlameRequested) {
    // ON phase: 4-step CS sequence (SAT Python _compute_pwm_control_setpoint)
    if (sinceFlameStart >= onTimeMs && sinceFlameStart >= minOnMs) {
      // ON time expired: switch to OFF
      state.sat.bPwmFlameRequested = false;
      _pwm_flameOffHoldSetpoint = 0.0f;
      return SAT_MIN_SETPOINT;
    }
    // Step 2: waiting for flame - send low CS to avoid overshoot on ignition
    if (_pwm_waitingForFlame) {
      // 180s ignition timeout (Python HEATER_STARTUP_TIMEFRAME): if boiler fails to ignite,
      // give up and switch to OFF phase, matching Python pwm.py:194-202 behavior.
      if (_pwm_waitForFlameStartMs > 0 &&
          (millis() - _pwm_waitForFlameStartMs) >= PWM_IGNITION_TIMEOUT_MS) {
        SATDebugTln(F("SAT PWM: ignition timeout (180s), aborting ON phase"));
        _pwm_waitingForFlame = false;
        _pwm_waitForFlameStartMs = 0;
        state.sat.bPwmFlameRequested = false;
        _pwm_flameOffHoldSetpoint = 0.0f;
        return SAT_MIN_SETPOINT;
      }
      float cs = OTcurrentSystemState.Tret + settings.sat.fFlameOffOffset;
      if (cs < SAT_MIN_SETPOINT) cs = SAT_MIN_SETPOINT;
      if (cs > maxSetpoint) cs = maxSetpoint;
      _pwm_flameOffHoldSetpoint = cs;
      return cs;
    }
    // Step 3: flame just lit, within fModSupDelay - hold the ignition setpoint
    uint32_t sinceFlameOn = millis() - _pwm_flameOnMs;
    if (sinceFlameOn < (uint32_t)(settings.sat.fModSupDelay * 1000.0f)) {
      return _pwm_flameOffHoldSetpoint;
    }
    // Step 4: flame stable, after fModSupDelay - apply modulation suppression setpoint
    {
      float cs = OTcurrentSystemState.Tboiler - settings.sat.fModSupOffset;
      if (cs < SAT_MIN_SETPOINT) cs = SAT_MIN_SETPOINT;
      if (cs > maxSetpoint) cs = maxSetpoint;
      _pwm_flameOffHoldSetpoint = 0.0f;
      return cs;
    }
  } else {
    // OFF phase: clear hold setpoint, wait for off duration then restart
    _pwm_flameOffHoldSetpoint = 0.0f;
    uint32_t sinceFlameOff = millis() - satCycleGetFlameOffStartMs();
    if (sinceFlameOff >= offTimeMs) {
      // Per-hour cycle limit (Task #203): suppress new ON cycle if rolling-hour count is reached
      static bool _hourLimitLogged = false;
      if (satCycleIsHourLimitReached()) {
        // Stay in OFF until the oldest timestamp leaves the 60-minute window.
        // Log once per suppression event (guard against log spam via a latch).
        if (!_hourLimitLogged) {
          SATDebugTf(PSTR("SAT PWM: cycle limit %u/hr reached, suppressing new cycle\r\n"),
                  (unsigned)satGetMaxCyclesPerHour());
          _hourLimitLogged = true;
        }
        return SAT_MIN_SETPOINT;
      }
      _hourLimitLogged = false;  // reset latch when limit clears
      state.sat.bPwmFlameRequested = true;
      _pwm_waitingForFlame = true;
      _pwm_waitForFlameStartMs = millis();  // Start ignition timeout timer (180s, Task #208)
      return pidOutput;
    }
    return SAT_MIN_SETPOINT;
  }
}

//=====================================================================
//=== Preset Handling ===
//=====================================================================
static const char* satGetPresetName(SATPreset p)
{
  switch (p) {
    case SAT_PRESET_AWAY:     return "away";
    case SAT_PRESET_ECO:      return "eco";
    case SAT_PRESET_COMFORT:  return "comfort";
    case SAT_PRESET_SLEEP:    return "sleep";
    case SAT_PRESET_ACTIVITY: return "activity";
    case SAT_PRESET_HOME:     return "home";
    default:                  return "none";
  }
}

void satHandlePreset(const char* value)
{
  SATPreset newPreset = SAT_PRESET_NONE;
  float newTarget = settings.sat.fTargetTemp;

  if (strcasecmp_P(value, PSTR("away")) == 0)          { newPreset = SAT_PRESET_AWAY;     newTarget = settings.sat.fPresetAway; }
  else if (strcasecmp_P(value, PSTR("eco")) == 0)      { newPreset = SAT_PRESET_ECO;      newTarget = settings.sat.fPresetEco; }
  else if (strcasecmp_P(value, PSTR("comfort")) == 0)  { newPreset = SAT_PRESET_COMFORT;  newTarget = settings.sat.fPresetComfort; }
  else if (strcasecmp_P(value, PSTR("sleep")) == 0)    { newPreset = SAT_PRESET_SLEEP;    newTarget = settings.sat.fPresetSleep; }
  else if (strcasecmp_P(value, PSTR("activity")) == 0) { newPreset = SAT_PRESET_ACTIVITY;  newTarget = settings.sat.fPresetActivity; }
  else if (strcasecmp_P(value, PSTR("home")) == 0)     { newPreset = SAT_PRESET_HOME;     newTarget = settings.sat.fPresetHome; }
  else if (strcasecmp_P(value, PSTR("none")) == 0)     { newPreset = SAT_PRESET_NONE; }
  else return; // Unknown preset

  state.sat.eActivePreset = newPreset;
  if (newPreset != SAT_PRESET_NONE) {
    // Save pre-custom temperature before changing target (Task #67)
    if (state.sat.fPreCustomTemp == 0.0f) {
      state.sat.fPreCustomTemp = settings.sat.fTargetTemp;
    }
    settings.sat.fTargetTemp = newTarget;
    // Reset PID integral to prevent overshoot on large temp jumps
    state.sat.fPidI = 0.0f;
    SATDebugTf(PSTR("SAT: preset '%s' -> target %.1f, integral reset\r\n"), satGetPresetName(newPreset), newTarget);
  } else {
    // Preset cleared: restore pre-custom temperature if one was saved (Task #219).
    // Mirrors Python climate.py:614-616: preset_mode=PRESET_NONE restores the temperature
    // that was active before the preset was activated, so the user's manual setpoint survives.
    if (state.sat.fPreCustomTemp > 0.0f) {
      settings.sat.fTargetTemp = state.sat.fPreCustomTemp;
      state.sat.fPidI = 0.0f;  // Reset integral to avoid overshoot on temp jump
      SATDebugTf(PSTR("SAT: preset cleared, restored pre-custom target %.1f\r\n"), settings.sat.fTargetTemp);
      // Publish restored target immediately so MQTT stays in sync
      if (state.mqtt.bConnected) {
        char valBuf[12];
        dtostrf(settings.sat.fTargetTemp, 1, 1, valBuf);
        sendMQTTData(F("sat/target"), valBuf, true);
      }
    }
    state.sat.fPreCustomTemp = 0.0f;
  }

  // Sync preset to secondary entities (Task #46)
  if (settings.sat.bPresetSync && settings.sat.sPresetSyncTopic[0] != '\0') {
    const char* presetName = satGetPresetName(newPreset);
    if (presetName && state.mqtt.bConnected) {
      sendMQTTData(settings.sat.sPresetSyncTopic, presetName, true);
      SATDebugTf(PSTR("SAT: Preset synced to %s: %s\r\n"), settings.sat.sPresetSyncTopic, presetName);
    }
  }
}

//=====================================================================
//=== Window Detection Handler ===
//=====================================================================
void satHandleWindow(bool isOpen)
{
  if (!settings.sat.bWindowDetection) return;

  if (isOpen && !state.sat.bWindowOpen) {
    // Window just opened - start timer
    state.sat.bWindowOpen = true;
    state.sat.iWindowOpenSinceMs = millis();
    SATDebugTln(F("SAT: window opened, starting timer"));
  }
  else if (!isOpen && state.sat.bWindowOpen) {
    // Window closed - restore previous state
    state.sat.bWindowOpen = false;
    state.sat.iWindowOpenSinceMs = 0;
    if (state.sat.fPreWindowTarget > 0.0f) {
      settings.sat.fTargetTemp = state.sat.fPreWindowTarget;
      state.sat.eActivePreset = (SATPreset)state.sat.iPreWindowPreset;
      state.sat.fPreWindowTarget = 0.0f;
      state.sat.fPreActivityTemp = 0.0f;  // Task #67: clear MQTT-visible pre-activity temp
      satResetIntegral();
      SATDebugTf(PSTR("SAT: window closed, restored target %.1f\r\n"), settings.sat.fTargetTemp);
    }
  }
}

// Called from satControlLoop to apply window detection action after timer expires
static void _satCheckWindowTimer()
{
  if (!state.sat.bWindowOpen || state.sat.fPreWindowTarget > 0.0f) return;  // not open or already switched
  if (state.sat.iWindowOpenSinceMs == 0) return;

  uint32_t openDuration = (millis() - state.sat.iWindowOpenSinceMs) / 1000;
  if (openDuration >= settings.sat.iWindowMinOpenSec) {
    // Timer expired - switch to Activity preset
    state.sat.fPreWindowTarget = settings.sat.fTargetTemp;
    state.sat.fPreActivityTemp = settings.sat.fTargetTemp;  // Task #67: mirror for MQTT visibility
    state.sat.iPreWindowPreset = (uint8_t)state.sat.eActivePreset;
    state.sat.eActivePreset = SAT_PRESET_ACTIVITY;
    settings.sat.fTargetTemp = settings.sat.fPresetActivity;
    satResetIntegral();
    SATDebugTf(PSTR("SAT: window open > %us, switched to Activity (%.1f)\r\n"),
            settings.sat.iWindowMinOpenSec, settings.sat.fPresetActivity);
  }
}

//=====================================================================
//=== Continuous Control Mode ===
//=====================================================================
static float satApplyContinuous(float pidOutput)
{
  // Asymmetric setpoint clamping (Task #44, SAT Python heating_control.py):
  // minimum_allowed = boiler_temp - flow_offset (configurable, default 2.0C)
  // Prevents setpoint spikes on overheat and ensures smooth ramp-down.
  //
  // Task #194: Mirror Python _compute_continuous_control_setpoint() bypass cases.
  // Python bypasses the clamp in three situations; we must do the same or the clamp
  // incorrectly prevents the PID output from lowering the setpoint when flame is off:
  //   1. Flame is off  -- clamp would wrongly block setpoint from falling with PID
  //   2. boiler_temperature is invalid/unavailable (None in Python, 0 or out-of-range here)
  //   3. boilerTemp <= pidOutput -- setpoint already above boiler temp, no clamping needed

  // Case 1: flame is off -- return pidOutput directly
  bool flame = (OTcurrentSystemState.Statusflags & 0x08) != 0;
  if (!flame) {
    return pidOutput;
  }

  float boilerTemp = OTcurrentSystemState.Tboiler;

  // Case 2: boilerTemp invalid / unavailable (sensor absent or not yet received)
  if (boilerTemp <= 0.0f || boilerTemp > 100.0f) {
    return pidOutput;
  }

  // Case 3: boilerTemp at or below the requested setpoint -- no asymmetric correction needed
  if (boilerTemp <= pidOutput) {
    return pidOutput;
  }

  float flowOffset = settings.sat.fFlowOffset;
  float minAllowed = boilerTemp - flowOffset;

  if (pidOutput < minAllowed) {
    return minAllowed;
  }

  return pidOutput;
}

//=====================================================================
//=== Get Room Temperature (OT bus or external) ===
//=====================================================================
static float satGetRoomTemp()
{
  // Simulation mode overrides all other sources
  if (settings.sat.bSimulation) {
    return state.sat.fSimRoomTemp;
  }
#if defined(ESP32)
  // BLE sensor has highest priority when available (Task #20)
  // Temperature source priority: BLE > MQTT external > OT bus MsgID 24
  if (settings.sat.bBleEnable && state.sat.bBleTempValid) {
    // Check staleness: if no update for 5 min, fall back to next source
    if ((millis() - state.sat.iBleTempLastMs) > SAT_STALE_TEMP_BLE_MS) {
      state.sat.bBleTempValid = false;
      SATDebugTln(F("SAT: BLE temp stale, falling back to next source"));
    } else {
      return state.sat.fBleTemp;  // BLE has 0.01C precision
    }
  }
#endif
  // Multi-area weighted average (Task #25) — takes priority when enabled and valid
  if (settings.sat.bMultiArea && settings.sat.iMultiAreaCount > 0) {
    float weighted = satGetWeightedRoomTemp();
    if (!isnan(weighted)) return weighted;
    // No valid areas: fall through to single-sensor logic
  }
  if (settings.sat.bUseExternalTemp && state.sat.bExternalTempValid) {
    // Check staleness — use settings.sat.iSensorMaxAgeS (default 6h, Python CONF_SENSOR_MAX_VALUE_AGE)
    if ((millis() - state.sat.iExternalTempLastMs) > ((uint32_t)settings.sat.iSensorMaxAgeS * 1000UL)) {
      state.sat.bExternalTempValid = false;
      SATDebugTln(F("SAT: external indoor temp stale, falling back to OT bus"));
    } else {
      return state.sat.fExternalTemp;
    }
  }
  float otRoom = OTcurrentSystemState.Tr;  // OT message ID 24
  // Task #21: If in fallback mode and OT room temp is invalid, use thermal estimation
  if (state.sat.bFallbackActive && (otRoom < -10.0f || otRoom > 50.0f)) {
    if (state.sat.iLastKnownRoomMs > 0) {
      float outside = satGetOutsideTemp();
      return satEstimateRoomTemp(outside);
    }
  }

  // TASK-204: Thermal comfort mode -- substitute Summer Simmer Index as PID room temp input.
  // Matches Python climate.py thermal_comfort: the PID targets heat-index-adjusted perceived
  // temperature rather than raw sensor temp, so e.g. 22C at 70% humidity "feels like" 23C.
  // Only active when bThermalComfort is enabled AND humidity data is fresh (< iHumidityTimeoutS).
  // Falls back silently to raw room temp if humidity is unavailable or stale.
  if (settings.sat.bThermalComfort && state.sat.bHumidityValid) {
    if ((millis() - state.sat.iHumidityLastMs) <= ((uint32_t)settings.sat.iHumidityTimeoutS * 1000UL)) {
      float ssi = satCalcSimmerIndex(otRoom, state.sat.fHumidity);
      SATDebugTf(PSTR("SAT: thermal_comfort: raw=%.1f SSI=%.1f H=%.0f%%\r\n"),
              otRoom, ssi, state.sat.fHumidity);
      return ssi;
    }
    SATDebugTln(F("SAT: thermal_comfort: humidity stale, using raw room temp"));
  }

  return otRoom;
}

//=== Get Outside Temperature (OT bus or external) ===
static float satGetOutsideTemp()
{
  // Simulation mode overrides all other sources
  if (settings.sat.bSimulation) {
    return state.sat.fSimOutdoorTemp;
  }
  if (state.sat.bExternalOutdoorValid) {
    // Check staleness — if no update for 10 min, fall back to OT bus
    if ((millis() - state.sat.iExternalOutdoorLastMs) > SAT_STALE_OUTDOOR_MS) {
      state.sat.bExternalOutdoorValid = false;
      SATDebugTln(F("SAT: external outdoor temp stale, falling back to OT bus"));
    } else {
      return state.sat.fExternalOutdoor;
    }
  }
  // Weather API fallback: use fetched outdoor temp when OT bus has no sensor
  if (state.sat.weather.bValid && OTcurrentSystemState.Toutside == 0.0f) {
    return state.sat.weather.fTemperature;
  }
  return OTcurrentSystemState.Toutside;  // OT message ID 27
}

//=====================================================================
//=== External Input Handlers (called from MQTT/REST) ===
//=====================================================================
bool satHandleExternalTemp(const char* value)
{
  if (!value || !*value) return false;
  char* endp = nullptr;
  float temp = strtof(value, &endp);
  if (endp == value || *endp != '\0') return false;  // non-numeric input
  if (temp > -50.0f && temp < 100.0f) {
    state.sat.fExternalTemp = temp;
    state.sat.bExternalTempValid = true;
    state.sat.iExternalTempLastMs = millis();
    SATDebugTf(PSTR("SAT: external indoor temp set to %.1f°C\r\n"), temp);
    return true;
  }
  return false;
}

bool satHandleExternalOutdoor(const char* value)
{
  if (!value || !*value) return false;
  char* endp = nullptr;
  float temp = strtof(value, &endp);
  if (endp == value || *endp != '\0') return false;  // non-numeric input
  if (temp > -50.0f && temp < 100.0f) {
    state.sat.fExternalOutdoor = temp;
    state.sat.bExternalOutdoorValid = true;
    state.sat.iExternalOutdoorLastMs = millis();
    SATDebugTf(PSTR("SAT: external outdoor temp set to %.1f°C\r\n"), temp);
    return true;
  }
  return false;
}

// Returns true if the value was valid and applied
bool satHandleHumidity(const char* value)
{
  if (!value || !*value) return false;
  char* endp = nullptr;
  float h = strtof(value, &endp);
  if (endp == value || *endp != '\0') return false;
  if (h < 0.0f || h > 100.0f) return false;
  state.sat.fHumidity = h;
  state.sat.bHumidityValid = true;
  state.sat.iHumidityLastMs = millis();
  SATDebugTf(PSTR("SAT: humidity set to %.0f%%\r\n"), h);
  return true;
}

// --- Sun elevation handler (Task #68) ---
bool satHandleSunElevation(const char* value)
{
  if (!value || !*value) return false;
  char* endp = nullptr;
  float elev = strtof(value, &endp);
  if (endp == value || *endp != '\0') return false;
  if (elev < -90.0f || elev > 90.0f) return false;
  state.sat.fSunElevation = elev;
  state.sat.bSunElevationValid = true;
  state.sat.iSunElevLastMs = millis();
  SATDebugTf(PSTR("SAT: sun elevation set to %.1f deg\r\n"), elev);
  return true;
}

// --- Multi-area room temperature handler (Task #25) ---
bool satHandleAreaTemp(uint8_t area, const char* value)
{
  if (area >= SAT_MAX_AREAS) return false;
  if (!value || !*value) return false;
  char* endp = nullptr;
  float temp = strtof(value, &endp);
  if (endp == value || *endp != '\0') return false;  // non-numeric input
  if (temp > -50.0f && temp < 100.0f) {
    state.sat.fAreaTemp[area] = temp;
    state.sat.bAreaValid[area] = true;
    state.sat.iAreaLastMs[area] = millis();
    SATDebugTf(PSTR("SAT: area %u temp set to %.1f\r\n"), area, temp);
    return true;
  }
  return false;
}

// Returns weighted average of valid, non-stale area temperatures.
// Returns NAN if no valid areas are available.
static float satGetWeightedRoomTemp()
{
  float sumWeightedTemp = 0.0f;
  float sumWeight = 0.0f;
  uint32_t now = millis();
  uint8_t count = settings.sat.iMultiAreaCount;
  if (count > SAT_MAX_AREAS) count = SAT_MAX_AREAS;

  for (uint8_t i = 0; i < count; i++) {
    if (!state.sat.bAreaValid[i]) continue;
    // Stale check: mark invalid if no update for 5 min
    if ((now - state.sat.iAreaLastMs[i]) > SAT_AREA_STALE_MS) {
      state.sat.bAreaValid[i] = false;
      SATDebugTf(PSTR("SAT: area %u temp stale\r\n"), i);
      continue;
    }
    float w = settings.sat.fAreaWeight[i];
    if (w <= 0.0f) continue;
    sumWeightedTemp += state.sat.fAreaTemp[i] * w;
    sumWeight += w;
  }
  if (sumWeight <= 0.0f) return NAN;
  return sumWeightedTemp / sumWeight;
}

//=====================================================================
//=== Multi-zone PID support (Task #233) ===
//=====================================================================

// Static zone state array — BSS, not stack. 4 zones × 28 bytes ≈ 112 bytes.
static SATZoneState satZones[4];

// Maximum number of zones supported
static const uint8_t SAT_MAX_ZONES = 4;

// Handler: push room temperature for zone n (1-based)
bool satHandleZoneRoomTemp(uint8_t zone, const char* value)
{
  if (zone < 1 || zone > SAT_MAX_ZONES) return false;
  if (!value || !*value) return false;
  char* endp = nullptr;
  float temp = strtof(value, &endp);
  if (endp == value || *endp != '\0') return false;
  if (temp < -10.0f || temp > 50.0f) return false;
  uint8_t idx = zone - 1;
  satZones[idx].fRoomTemp     = temp;
  satZones[idx].bRoomValid    = true;
  satZones[idx].iLastUpdateMs = millis();
  SATDebugTf(PSTR("SAT zone %u: room_temp=%.1f\r\n"), zone, temp);
  return true;
}

// Handler: push setpoint for zone n (1-based)
bool satHandleZoneSetpoint(uint8_t zone, const char* value)
{
  if (zone < 1 || zone > SAT_MAX_ZONES) return false;
  if (!value || !*value) return false;
  char* endp = nullptr;
  float sp = strtof(value, &endp);
  if (endp == value || *endp != '\0') return false;
  if (sp < 5.0f || sp > 30.0f) return false;
  uint8_t idx = zone - 1;
  satZones[idx].fSetpoint     = sp;
  satZones[idx].bSpValid      = true;
  satZones[idx].iLastUpdateMs = millis();
  SATDebugTf(PSTR("SAT zone %u: setpoint=%.1f\r\n"), zone, sp);
  return true;
}

// Run a simplified PID step for a single zone and return requested CH setpoint.
// Uses zone-local integral + prev_error state. Reuses heating curve from zone setpoint
// and shared gain calculation for consistent behavior.
// Returns SAT_MIN_SETPOINT (10°C) if zone is inactive or inputs are invalid.
static float satZonePidStep(uint8_t idx, float outsideTemp)
{
  SATZoneState& z = satZones[idx];
  uint32_t timeoutMs = (uint32_t)settings.sat.iZoneTimeoutS * 1000UL;
  uint32_t now = millis();

  // Mark zone inactive if stale
  if (!z.bRoomValid || !z.bSpValid) return SAT_MIN_SETPOINT;
  if ((now - z.iLastUpdateMs) > timeoutMs) return SAT_MIN_SETPOINT;

  float roomTemp = z.fRoomTemp;
  float target   = z.fSetpoint;

  // Validate inputs
  if (roomTemp < -10.0f || roomTemp > 50.0f) return SAT_MIN_SETPOINT;
  if (target < 5.0f || target > 30.0f) return SAT_MIN_SETPOINT;

  // Heating curve for this zone's target
  float curveValue = satCalcHeatingCurve(target, outsideTemp);

  // Gains from shared auto-gain formula (zone uses same Kp/Ki as primary).
  // Constants mirror SATpid.ino: KP_DIVISOR_FLOOR=4, KP_DIVISOR_RAD=3, AGGRESSION=8400.
  float coeff   = settings.sat.fHeatingCurveCoeff;
  float divisor = (settings.sat.iHeatingSystem == 1) ? 4.0f : 3.0f;
  float kp      = (coeff * curveValue) / divisor;
  float ki      = kp / 8400.0f;   // SAT_PID_AGGRESSION_V3

  float error = target - roomTemp;
  float deadband = settings.sat.fDeadband;

  // Integral: only inside deadband (matches SAT Python convention)
  if (fabsf(error) <= deadband) {
    z.fPidIntegral += ki * error * 60.0f;   // SAT_PID_UPDATE_INTERVAL = 60s
    if (z.fPidIntegral < 0.0f)        z.fPidIntegral = 0.0f;
    if (z.fPidIntegral > curveValue)  z.fPidIntegral = curveValue;
  } else {
    z.fPidIntegral = 0.0f;
  }

  float P = kp * error;
  float I = z.fPidIntegral;

  float output = curveValue + P + I;
  float sysMax = satGetMaxSetpoint();
  if (output < SAT_MIN_SETPOINT) output = SAT_MIN_SETPOINT;
  if (output > sysMax)           output = sysMax;

  z.fPidOutput = output;
  z.fPrevError = error;

  return output;
}

// Publish per-zone diagnostics to MQTT (retained).
// Topics: sat/zone/<n>/output, sat/zone/<n>/error, sat/zone/<n>/active  (n is 1-based)
static void satPublishZoneDiagnostics()
{
  char topicBuf[48];
  char valBuf[12];
  uint8_t zoneCount = settings.sat.iZoneCount;
  if (zoneCount > SAT_MAX_ZONES) zoneCount = SAT_MAX_ZONES;
  uint32_t timeoutMs = (uint32_t)settings.sat.iZoneTimeoutS * 1000UL;
  uint32_t now = millis();

  for (uint8_t i = 0; i < zoneCount; i++) {
    feedWatchDog();
    SATZoneState& z = satZones[i];
    bool active = z.bRoomValid && z.bSpValid && ((now - z.iLastUpdateMs) <= timeoutMs);

    // sat/zone/<n>/active
    snprintf_P(topicBuf, sizeof(topicBuf), PSTR("sat/zone/%u/active"), i + 1);
    sendMQTTData(topicBuf, active ? "true" : "false", true);

    // sat/zone/<n>/output
    dtostrf(z.fPidOutput, 1, 1, valBuf);
    snprintf_P(topicBuf, sizeof(topicBuf), PSTR("sat/zone/%u/output"), i + 1);
    sendMQTTData(topicBuf, valBuf, true);

    // sat/zone/<n>/error (target - room_temp)
    dtostrf(z.fSetpoint - z.fRoomTemp, 1, 2, valBuf);
    snprintf_P(topicBuf, sizeof(topicBuf), PSTR("sat/zone/%u/error"), i + 1);
    sendMQTTData(topicBuf, valBuf, true);
  }
}

bool satHandleTargetTemp(const char* value)
{
  if (!value || !*value) return false;
  char* endp = nullptr;
  float temp = strtof(value, &endp);
  if (endp == value || *endp != '\0') return false;  // non-numeric input
  if (temp >= 5.0f && temp <= 30.0f) {
    // Auto-map to preset if temperature exactly matches a preset value (Task #218).
    // Mirrors Python climate.py:562-568 behaviour: set_temperature auto-activates the preset
    // so that HA preset display and MQTT preset topic stay in sync.
    struct { const char* name; float val; } presets[] = {
      { "comfort",  settings.sat.fPresetComfort  },
      { "eco",      settings.sat.fPresetEco      },
      { "away",     settings.sat.fPresetAway     },
      { "sleep",    settings.sat.fPresetSleep    },
      { "activity", settings.sat.fPresetActivity },
      { "home",     settings.sat.fPresetHome     },
    };
    for (uint8_t i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
      if (fabsf(temp - presets[i].val) < 0.05f) {
        SATDebugTf(PSTR("SAT: target %.1f matches preset '%s', activating preset\r\n"), temp, presets[i].name);
        satHandlePreset(presets[i].name);
        return true;
      }
    }
    // No preset match: go through updateSetting() to persist via deferred flush
    updateSetting("SATtargettemp", value);
    SATDebugTf(PSTR("SAT: target temp set to %.1f°C\r\n"), temp);
    return true;
  }
  return false;
}

void satHandleEnabled(const char* value)
{
  if (!value || !*value) return;
  bool enabled = EVALBOOLEAN(value);
  // Route through updateSetting() so the change persists to flash
  updateSetting("SATenabled", enabled ? "1" : "0");
  if (enabled) {
    // Clear safety trip so SAT can resume
    state.sat.bSafetyTripped = false;
    _sat_consecutiveSkips = 0;
    _sat_picFailCount = 0;
  }
  SATDebugTf(PSTR("SAT: %s\r\n"), enabled ? "enabled" : "disabled");
}

//=== SAT LittleFS file paths (Task #237) ===
// Defined in SATcontrol.ino so they are visible to SATcycles.ino (alphabetically later).
static const char SAT_CYCLES_FILE[]    PROGMEM = "/sat/sat_cycles.json";

//=== File migration helper (Task #237): move a file from old to new path if old exists ===
// Both paths are PROGMEM pointers (PGM_P). Copy to RAM before passing to LittleFS,
// which internally calls strlen() and expects a RAM pointer. Passing a flash pointer
// directly causes Exception 3 (LoadStoreAlignmentCause).
static void satMigrateFile(PGM_P oldPath, PGM_P newPath)
{
  char oldBuf[32], newBuf[32];
  strncpy_P(oldBuf, oldPath, sizeof(oldBuf) - 1); oldBuf[sizeof(oldBuf) - 1] = '\0';
  strncpy_P(newBuf, newPath, sizeof(newBuf) - 1); newBuf[sizeof(newBuf) - 1] = '\0';
  if (!LittleFS.exists(oldBuf)) return;
  if (LittleFS.exists(newBuf)) { LittleFS.remove(oldBuf); return; } // new already present
  LittleFS.rename(oldBuf, newBuf);
  SATDebugTf(PSTR("SAT: migrated %s -> %s\r\n"), oldBuf, newBuf);
}

//=== PID State Persistence (Tasks #6, #49, #222) ===
static const char* SAT_PID_STATE_FILE PROGMEM = "/sat_pid_state.json";
static uint32_t _pidLastSaveMs = 0;

// Max age (seconds) before a saved PID state is considered stale and discarded on restore.
// 30 minutes: covers brief power cuts and OTA updates; rejects summer/long-downtime state.
static const uint32_t SAT_PID_STALE_SEC = 1800UL;

void satSavePidState()
{
  File f = LittleFS.open(FPSTR(SAT_PID_STATE_FILE), "w");
  if (!f) return;
  char buf[160];
  time_t ts = time(nullptr);
  snprintf_P(buf, sizeof(buf), PSTR("{\"i\":%.4f,\"d\":%.4f,\"err\":%.2f,\"ts\":%lu}"),
             state.sat.fPidI, state.sat.fPidD, state.sat.fError, (unsigned long)ts);
  f.print(buf);
  f.close();
  _pidLastSaveMs = millis();
}

void satLoadPidState()
{
  File f = LittleFS.open(FPSTR(SAT_PID_STATE_FILE), "r");
  if (!f) return;
  char buf[160];
  size_t len = f.readBytes(buf, sizeof(buf) - 1);
  buf[len] = 0;
  f.close();
  // Simple parse: extract values from {"i":X,"d":Y,"err":Z,"ts":N}
  float i = 0, d = 0, err = 0;
  unsigned long savedTs = 0;
  char* p;
  if ((p = strstr(buf, "\"i\":"))   != nullptr) i      = atof(p + 4);
  if ((p = strstr(buf, "\"d\":"))   != nullptr) d      = atof(p + 4);
  if ((p = strstr(buf, "\"err\":")) != nullptr) err    = atof(p + 6);
  if ((p = strstr(buf, "\"ts\":"))  != nullptr) savedTs = strtoul(p + 5, nullptr, 10);

  // Staleness guard (Task #222): discard if NTP not yet synced or saved > 30 min ago.
  time_t nowTs = time(nullptr);
  if (nowTs < 1000000L) {
    // NTP not yet synced at boot — skip restore; PID starts from zero.
    SATDebugTln(F("SAT: PID state skipped (NTP not synced yet)"));
    return;
  }
  if (savedTs == 0 || (uint32_t)(nowTs - (time_t)savedTs) > SAT_PID_STALE_SEC) {
    SATDebugTf(PSTR("SAT: PID state discarded (stale, age=%lus)\r\n"),
            (unsigned long)(nowTs - (time_t)savedTs));
    return;
  }
  state.sat.fPidI = i;
  state.sat.fPidD = d;
  state.sat.fError = err;
  SATDebugTf(PSTR("SAT: PID state restored (I=%.4f D=%.4f err=%.2f age=%lus)\r\n"),
          i, d, err, (unsigned long)(nowTs - (time_t)savedTs));
}

//=== Energy State Persistence (Task #196) ===
static const char* SAT_ENERGY_FILE PROGMEM = "/sat_energy.json";
static uint32_t _energyLastSaveMs = 0;

// Save interval: every hour. Energy changes slowly; hourly saves balance
// data loss vs. LittleFS write wear (rated ~100k cycles per sector).
static const uint32_t SAT_ENERGY_SAVE_INTERVAL_MS = 3600000UL; // 1 hour

void satSaveEnergyState()
{
  File f = LittleFS.open(FPSTR(SAT_ENERGY_FILE), "w");
  if (!f) return;
  char buf[64];
  snprintf_P(buf, sizeof(buf), PSTR("{\"kwh\":%.3f}"), state.sat.fEnergyTotal);
  f.print(buf);
  f.close();
  _energyLastSaveMs = millis();
  SATDebugTf(PSTR("SAT: energy saved (%.3f kWh)\r\n"), state.sat.fEnergyTotal);
}

void satLoadEnergyState()
{
  File f = LittleFS.open(FPSTR(SAT_ENERGY_FILE), "r");
  if (!f) return;
  char buf[64];
  size_t len = f.readBytes(buf, sizeof(buf) - 1);
  buf[len] = 0;
  f.close();
  // Simple parse: extract value from {"kwh":X.XXX}
  char* p;
  float kwh = 0.0f;
  if ((p = strstr(buf, "\"kwh\":")) != nullptr) kwh = atof(p + 6);
  if (kwh >= 0.0f) {
    state.sat.fEnergyTotal = kwh;
    SATDebugTf(PSTR("SAT: energy restored (%.3f kWh)\r\n"), state.sat.fEnergyTotal);
  }
}

//=== Estimated Gas Energy Persistence (Task #232) ===
static const char* SAT_EST_ENERGY_FILE PROGMEM = "/sat_energy_estimate.json";

static void satSaveEstimatedEnergy()
{
  File f = LittleFS.open(FPSTR(SAT_EST_ENERGY_FILE), "w");
  if (!f) return;
  char buf[48];
  snprintf_P(buf, sizeof(buf), PSTR("{\"kwh\":%.3f}"), state.sat.fEnergyEstimatedKWh);
  f.print(buf);
  f.close();
  state.sat.fEstEnergyLastSavedKWh = state.sat.fEnergyEstimatedKWh;
  SATDebugTf(PSTR("SAT: estimated energy saved (%.3f kWh)\r\n"), state.sat.fEnergyEstimatedKWh);
}

static void satLoadEstimatedEnergy()
{
  File f = LittleFS.open(FPSTR(SAT_EST_ENERGY_FILE), "r");
  if (!f) return;
  char buf[48];
  size_t len = f.readBytes(buf, sizeof(buf) - 1);
  buf[len] = 0;
  f.close();
  char* p;
  float kwh = 0.0f;
  if ((p = strstr(buf, "\"kwh\":")) != nullptr) kwh = atof(p + 6);
  if (kwh >= 0.0f) {
    state.sat.fEnergyEstimatedKWh    = kwh;
    state.sat.fEstEnergyLastSavedKWh = kwh;
    SATDebugTf(PSTR("SAT: estimated energy restored (%.3f kWh)\r\n"), kwh);
  }
}

//=== Cleanly disable SAT and release boiler control ===
void satDisable()
{
  SATDebugTf(PSTR("SAT: satDisable called (safety=%d fallback=%d)\r\n"),
             (int)state.sat.bSafetyTripped, (int)state.sat.bFallbackActive);
  state.sat.eControlMode = SAT_MODE_OFF;
  state.sat.bActive = false;
  state.sat.fFinalSetpoint = 0.0f;
  satSavePidState();         // Persist PID state before reset
  satSaveEnergyState();      // Persist energy total before reset (Task #196)
  satSaveEstimatedEnergy();  // Persist estimated gas energy before reset (Task #232)
  satPidReset();
  satHCRReset();        // Reset daily-median recommendation (Task #228)
  // Intentional: release boiler control to the thermostat (CS=0) rather than holding
  // a warm-idle setpoint like Python SAT's COLD_SETPOINT=22C. OTGW is a gateway
  // sitting between the thermostat and the boiler -- when SAT is disabled, the physical
  // thermostat resumes authority. Python SAT uses a warm-idle setpoint because it *is*
  // the thermostat (standalone HA replacement); OTGW firmware is not, so it defers.
  addCommandToQueue("CS=0", 4, false, 0);
  SATDebugTln(F("SAT: disabled, sent CS=0 to release boiler control"));
}

//=== Flush short-lived SAT data (Task #237) ===
// Clears PID integral and cycle window from both memory and LittleFS.
// Called on manual flush (MQTT sat/flush or REST POST /api/v2/sat/flush).
void satFlushShortLivedData()
{
  // Reset PID integral (only the integral — P and D terms don't need clearing)
  state.sat.fPidI = 0.0f;
  satPidReset();
  // Flush cycle window (in-memory and file)
  satFlushCycleWindow();
  SATDebugTln(F("SAT: short-lived data flushed (PID integral + cycle window)"));
}

//=====================================================================
//=== Handle SAT heating_mode payload ===
//=====================================================================
// Map "eco"/"comfort" string aliases to the numeric SATheatingmode setting
// (1=eco, 0=comfort); passthrough any other payload so REST/MQTT callers can
// still send a bare number. Kept here alongside the other satHandle* helpers
// so MQTT/REST transport layers stay free of SAT-specific string tokens
// (ARCH-L3 2026-04-18 review).
void satHandleHeatingMode(const char* value)
{
  if (!value || !*value) return;
  if (strcasecmp_P(value, PSTR("eco")) == 0) {
    updateSetting("SATheatingmode", "1");
  } else if (strcasecmp_P(value, PSTR("comfort")) == 0) {
    updateSetting("SATheatingmode", "0");
  } else {
    updateSetting("SATheatingmode", value);
  }
}

void satHandleControlMode(const char* value)
{
  if (!value || !*value) return;
  // TASK-205: avoid strcmp() with bare string literals -- use atoi() for numeric
  // forms and strcmp_P(..., PSTR(...)) for named forms. No bare string compares.
  int prevMode = (int)state.sat.eControlMode;
  int numericVal = atoi(value);
  if (strcasecmp_P(value, PSTR("continuous")) == 0 || numericVal == 1) {
    state.sat.eControlMode = SAT_MODE_CONTINUOUS;
  } else if (strcasecmp_P(value, PSTR("pwm")) == 0 || numericVal == 2) {
    state.sat.eControlMode = SAT_MODE_PWM;
  } else if (strcasecmp_P(value, PSTR("auto")) == 0 || numericVal == 0) {
    state.sat.eControlMode = SAT_MODE_CONTINUOUS; // Start in continuous, auto-switch
  }
  SATDebugTf(PSTR("SAT: control mode %d -> %d (value='%s')\r\n"),
             prevMode, (int)state.sat.eControlMode, value);
  if ((int)state.sat.eControlMode != prevMode) {
    const char* modeName = (state.sat.eControlMode == SAT_MODE_PWM) ? "pwm"
                         : (state.sat.eControlMode == SAT_MODE_CONTINUOUS) ? "continuous"
                         : "off";
    static char _wsMsg[64];
    snprintf_P(_wsMsg, sizeof(_wsMsg), PSTR("{\"type\":\"status\",\"msg\":\"SAT mode: %s\"}"), modeName);
    sendWebSocketJSON(_wsMsg);
  }
}

//=====================================================================
//=== Send SAT Status as JSON (for REST API) ===
//=====================================================================
// Precision-aware float entry — SAT fields need varying decimal places
// (sendJsonMapEntry(float) defaults to %.3f which doesn't suit all fields)
static void satSendJsonFloat(const __FlashStringHelper* cName, float fValue, uint8_t decimals)
{
  char nameBuf[25];
  strncpy_P(nameBuf, (PGM_P)cName, sizeof(nameBuf));
  nameBuf[sizeof(nameBuf) - 1] = 0;
  char numBuf[16];
  dtostrf(fValue, 1, decimals, numBuf);
  char jsonBuff[60];
  snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": %s"), nameBuf, numBuf);
  sendBeforenext();
  sendIdent();
  httpServer.sendContent(jsonBuff);
}

void satSendStatusJSON()
{
  sendStartJsonMap("");
  sendJsonMapEntry(F("enabled"),              settings.sat.bEnabled);
  sendJsonMapEntry(F("active"),               state.sat.bActive);
  sendJsonMapEntry(F("control_mode"),         (int32_t)state.sat.eControlMode);
  { char bsName[20]; satGetBoilerStatusName(bsName, sizeof(bsName));
    sendJsonMapEntry(F("boiler_status"),        bsName); }
  satSendJsonFloat(F("target_temp"),          settings.sat.fTargetTemp, 1);
  satSendJsonFloat(F("room_temp"),            satGetRoomTemp(), 1);
  satSendJsonFloat(F("outside_temp"),         satGetOutsideTemp(), 1);
  satSendJsonFloat(F("heating_curve"),        state.sat.fHeatingCurveValue, 1);
  satSendJsonFloat(F("pid_output"),           state.sat.fPidOutput, 1);
  satSendJsonFloat(F("final_setpoint"),       state.sat.fFinalSetpoint, 1);
  satSendJsonFloat(F("error"),                state.sat.fError, 2);
  satSendJsonFloat(F("pid_p"),                state.sat.fPidP, 2);
  satSendJsonFloat(F("pid_i"),                state.sat.fPidI, 2);
  satSendJsonFloat(F("pid_d"),                state.sat.fPidD, 2);
  satSendJsonFloat(F("kp"),                   state.sat.fKp, 4);
  satSendJsonFloat(F("ki"),                   state.sat.fKi, 6);
  satSendJsonFloat(F("kd"),                   state.sat.fKd, 2);
  satSendJsonFloat(F("raw_derivative"),        state.sat.fRawDerivative, 4);
  satSendJsonFloat(F("coefficient"),          settings.sat.fHeatingCurveCoeff, 1);
  satSendJsonFloat(F("deadband"),             settings.sat.fDeadband, 2);
  satSendJsonFloat(F("overshoot_margin"),     settings.sat.fOvershootMargin, 1);
  sendJsonMapEntry(F("cycle_count"),          state.sat.iCycleCount);
  sendJsonMapEntry(F("cycles_this_hour"),     (int32_t)satCycleGetCyclesThisHour());
  sendJsonMapEntry(F("last_cycle_class"),     (int32_t)state.sat.eLastCycleClass);
  satSendJsonFloat(F("cycle_max_flow"),       state.sat.fCycleMaxFlow, 1);
  satSendJsonFloat(F("cycle_overshoot_sec"),  state.sat.fCycleOvershootSec, 0);
  satSendJsonFloat(F("duty_ratio"),           state.sat.fDutyRatio, 3);
  satSendJsonFloat(F("overshoot_fraction"),   state.sat.fOvershootFraction, 3);
  satSendJsonFloat(F("underheat_fraction"),   state.sat.fUnderheatFraction, 3);
  sendJsonMapEntry(F("cycle_phase"),          satCycleGetPhaseName());
  sendJsonMapEntry(F("phase_duration_sec"),   (int32_t)satCycleGetPhaseDurationSec());
  satSendJsonFloat(F("pwm_duty"),             state.sat.fPwmDutyCycle, 2);
  sendJsonMapEntry(F("pwm_flame_req"),        state.sat.bPwmFlameRequested);
  sendJsonMapEntry(F("active_preset"),         (int32_t)state.sat.eActivePreset);
  sendJsonMapEntry(F("mod_suppressed"),        state.sat.bModSuppressed);
  sendJsonMapEntry(F("dhw_active"),             state.sat.bDhwActive);
  satSendJsonFloat(F("dhw_setpoint"),          settings.sat.fDhwSetpoint, 1);
  sendJsonMapEntry(F("control_interval_sec"),  (int32_t)settings.sat.iControlInterval);
  sendJsonMapEntry(F("fallback_active"),       state.sat.bFallbackActive);
  sendJsonMapEntry(F("fallback_reason"),       (int32_t)state.sat.eFallbackReason);
  sendJsonMapEntry(F("max_rel_modulation"),   (int32_t)settings.sat.iMaxRelModulation);
  sendJsonMapEntry(F("current_modulation"),   (int32_t)state.sat.iCurrentModulation);
  satSendJsonFloat(F("ovp_value"),            settings.sat.fOvpValue, 1);
  sendJsonMapEntry(F("ovp_enabled"),          settings.sat.bOvpEnabled);
  sendJsonMapEntry(F("ovp_calib_phase"),      (int32_t)state.sat.eCalibPhase);
  satSendJsonFloat(F("ovp_calib_max_temp"),   state.sat.fCalibMaxTemp, 1);
  sendJsonMapEntry(F("ovp_calib_samples"),    (int32_t)state.sat.iCalibSamples);
  sendJsonMapEntry(F("heating_system"),       (int32_t)settings.sat.iHeatingSystem);
  sendJsonMapEntry(F("heating_system_detected"), (int32_t)state.sat.iDetectedHeatingSystem);
  { char mfrName[12]; satGetManufacturerName(mfrName, sizeof(mfrName));
    sendJsonMapEntry(F("manufacturer"), mfrName); }
  sendJsonMapEntry(F("manufacturer_setting"), (int32_t)settings.sat.iManufacturer);
  sendJsonMapEntry(F("manufacturer_detected"), (int32_t)state.sat.iDetectedManufacturer);
  sendJsonMapEntry(F("slave_memberid"),        (int32_t)state.sat.iSlaveMemberID);
  satSendJsonFloat(F("max_setpoint_system"), satGetMaxSetpoint(), 1);
  sendJsonMapEntry(F("external_temp_valid"),  state.sat.bExternalTempValid);
  sendJsonMapEntry(F("external_outdoor_valid"), state.sat.bExternalOutdoorValid);
  sendJsonMapEntry(F("safety_tripped"),       state.sat.bSafetyTripped);
  sendJsonMapEntry(F("valves_open"),            state.sat.bValvesOpen);
  sendJsonMapEntry(F("window_open"),           state.sat.bWindowOpen);
  sendJsonMapEntry(F("window_detection"),      settings.sat.bWindowDetection);
  sendJsonMapEntry(F("push_setpoint"),         settings.sat.bPushSetpoint);
  satSendJsonFloat(F("flame_off_offset"),      settings.sat.fFlameOffOffset, 1);
  sendJsonMapEntry(F("force_pwm"),             settings.sat.bForcePWM);
  satSendJsonFloat(F("flow_offset"),           settings.sat.fFlowOffset, 1);
  satSendJsonFloat(F("pressure"),              state.sat.fSmoothedPressure, 2);
  satSendJsonFloat(F("pressure_drop_rate"),    state.sat.fPressureDropRate, 3);
  sendJsonMapEntry(F("pressure_alarm"),        state.sat.bPressureAlarm);
  sendJsonMapEntry(F("modulation_reliable"),   state.sat.bModulationReliable);
  sendJsonMapEntry(F("setpoint_mismatch"),     state.sat.bSetpointMismatch);
  { static const char* const crNames[] = { "insufficient", "increase", "decrease", "hold" };
    int crIdx = (int)state.sat.eCurveRecommendation;
    if (crIdx < 0 || crIdx > 3) crIdx = 0;
    sendJsonMapEntry(F("curve_recommendation"), crNames[crIdx]); }
  sendJsonMapEntry(F("heating_curve_recommendation"), state.sat.sHeatCurveRec);
  satSendJsonFloat(F("mean_error"),            state.sat.fMeanError, 2);
  satSendJsonFloat(F("error_stddev"),          state.sat.fErrorStdDev, 3);
  satSendJsonFloat(F("target_temp_step"),      settings.sat.fTargetTempStep, 1);
  satSendJsonFloat(F("power_kw"),              state.sat.fCurrentPower, 2);
  satSendJsonFloat(F("energy_kwh"),            state.sat.fEnergyTotal, 3);
  satSendJsonFloat(F("boiler_capacity"),       settings.sat.fBoilerCapacity, 1);
  // Gas consumption estimation (Task #232)
  satSendJsonFloat(F("boiler_rated_kw"),       settings.sat.fBoilerRatedKW, 1);
  satSendJsonFloat(F("boiler_efficiency"),     settings.sat.fBoilerEfficiency, 2);
  satSendJsonFloat(F("energy_estimated_kwh"),  state.sat.fEnergyEstimatedKWh, 3);
  // Preset sync (Task #46)
  sendJsonMapEntry(F("preset_sync"),           settings.sat.bPresetSync);
  // Thermal drop learning (Task #21)
  satSendJsonFloat(F("thermal_coeff"),         settings.sat.fThermalCoeff, 4);
  satSendJsonFloat(F("thermal_drop_rate"),     state.sat.fThermalDropRate, 4);
  sendJsonMapEntry(F("thermal_model_valid"),   state.sat.bThermalModelValid);
  satSendJsonFloat(F("estimated_room"),        state.sat.fEstimatedRoom, 1);
  satSendJsonFloat(F("last_known_room"),       state.sat.fLastKnownRoom, 1);
  // Solar gain (Task #23)
  sendJsonMapEntry(F("solar_gain_active"),     state.sat.bSolarGainActive);
  satSendJsonFloat(F("indoor_rise_rate"),      state.sat.fIndoorRiseRate, 2);
  // Summer simmer (Task #24)
  sendJsonMapEntry(F("summer_simmer"),         settings.sat.bSummerSimmer);
  sendJsonMapEntry(F("summer_active"),         state.sat.bSummerActive);
  satSendJsonFloat(F("summer_hours_above"),    state.sat.fSummerHoursAbove, 1);
  satSendJsonFloat(F("summer_threshold"),      settings.sat.fSummerThreshold, 1);
  sendJsonMapEntry(F("summer_min_hours"),      (int32_t)settings.sat.iSummerMinHours);
  // Thermal comfort (Task #28/#47)
  sendJsonMapEntry(F("comfort_adjust"),        settings.sat.bComfortAdjust);
  satSendJsonFloat(F("humidity"),              state.sat.fHumidity, 0);
  sendJsonMapEntry(F("humidity_valid"),         state.sat.bHumidityValid);
  satSendJsonFloat(F("comfort_offset"),        state.sat.fComfortOffset, 2);
  satSendJsonFloat(F("comfort_ref_humidity"),  settings.sat.fComfortHumidity, 0);
  satSendJsonFloat(F("comfort_max_offset"),    settings.sat.fComfortMaxOffset, 1);
  // Simulation (Task #37)
  sendJsonMapEntry(F("simulation"),            settings.sat.bSimulation);
  if (settings.sat.bSimulation) {
    satSendJsonFloat(F("sim_room_temp"),        state.sat.fSimRoomTemp, 1);
    satSendJsonFloat(F("sim_flow_temp"),        state.sat.fSimFlowTemp, 1);
    satSendJsonFloat(F("sim_outdoor_temp"),     state.sat.fSimOutdoorTemp, 1);
  }
  // PID auto-tuning (Task #27)
  sendJsonMapEntry(F("auto_tune"),             settings.sat.bAutoTune);
  sendJsonMapEntry(F("auto_tune_active"),      state.sat.bAutoTuneActive);
  sendJsonMapEntry(F("auto_tune_cycles"),      (int32_t)state.sat.iAutoTuneCycles);
  satSendJsonFloat(F("auto_tune_score"),       state.sat.fAutoTuneScore, 2);
  satSendJsonFloat(F("auto_tune_rate"),        settings.sat.fAutoTuneRate, 3);
  // SAT Python parity settings (Task #82)
  sendJsonMapEntry(F("sensor_max_age"),        (int32_t)settings.sat.iSensorMaxAgeS);
  sendJsonMapEntry(F("error_monitoring"),      settings.sat.bErrorMonitoring);
  satSendJsonFloat(F("auto_gains_value"),      settings.sat.fAutoGainsValue, 2);
  // TASK-193: manual gains mode
  sendJsonMapEntry(F("auto_gains"),            settings.sat.bAutoGains);
  satSendJsonFloat(F("kp_manual"),             settings.sat.fKpManual, 4);
  satSendJsonFloat(F("ki_manual"),             settings.sat.fKiManual, 6);
  satSendJsonFloat(F("kd_manual"),             settings.sat.fKdManual, 2);
  // TASK-204: thermal comfort mode (SSI as PID room temp)
  sendJsonMapEntry(F("thermal_comfort"),       settings.sat.bThermalComfort);
  sendJsonMapEntry(F("heating_mode"),          settings.sat.iHeatingMode == 1 ? "eco" : "comfort");
  sendJsonMapEntry(F("cycles_per_hour"),       (int32_t)settings.sat.iCyclesPerHour);
  satSendJsonFloat(F("valve_offset"),          settings.sat.fValveOffset, 2);
  sendJsonMapEntry(F("solar_freeze_integral"), settings.sat.bSolarFreezeIntegral);
  // Multi-area (Task #25)
  sendJsonMapEntry(F("multi_area"),            settings.sat.bMultiArea);
  sendJsonMapEntry(F("multi_area_count"),      (int32_t)settings.sat.iMultiAreaCount);
  if (settings.sat.bMultiArea && settings.sat.iMultiAreaCount > 0) {
    uint8_t cnt = settings.sat.iMultiAreaCount;
    if (cnt > SAT_MAX_AREAS) cnt = SAT_MAX_AREAS;
    for (uint8_t i = 0; i < cnt; i++) {
      char nameBuf[20];
      char numBuf[12];
      char jsonBuff[60];
      // area_N_temp
      snprintf_P(nameBuf, sizeof(nameBuf), PSTR("area_%u_temp"), i);
      dtostrf(state.sat.fAreaTemp[i], 1, 1, numBuf);
      snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": %s"), nameBuf, numBuf);
      sendBeforenext(); sendIdent(); httpServer.sendContent(jsonBuff);
      // area_N_valid
      snprintf_P(nameBuf, sizeof(nameBuf), PSTR("area_%u_valid"), i);
      sendJsonMapEntry(nameBuf, state.sat.bAreaValid[i]);
      // area_N_weight
      snprintf_P(nameBuf, sizeof(nameBuf), PSTR("area_%u_weight"), i);
      dtostrf(settings.sat.fAreaWeight[i], 1, 2, numBuf);
      snprintf_P(jsonBuff, sizeof(jsonBuff), PSTR("\"%s\": %s"), nameBuf, numBuf);
      sendBeforenext(); sendIdent(); httpServer.sendContent(jsonBuff);
    }
  }
#if defined(ESP32)
  // BLE sensor status (Task #20)
  satBLESendStatusJSON();
#endif
  sendEndJsonMap("");
}

//=====================================================================
//=== Summer Simmer Index (Task #64) ===
//=====================================================================
// Matches SAT Python summer_simmer.py formula exactly
static float satCalcSimmerIndex(float tempC, float humidity) {
  if (humidity < 0 || humidity > 100) return tempC;
  float F = tempC * 9.0f / 5.0f + 32.0f;
  if (F < 58.0f) return tempC;  // below threshold, return raw temp
  float idx = 1.98f * (F - (0.55f - 0.0055f * humidity) * (F - 58.0f)) - 56.83f;
  return (idx - 32.0f) * 5.0f / 9.0f;  // convert back to Celsius
}

static const char* satSimmerPerception(float indexC) {
  if (indexC < 21.1f) return "Cool";
  if (indexC < 25.0f) return "Slightly Cool";
  if (indexC < 28.3f) return "Comfortable";
  if (indexC < 32.8f) return "Slightly Warm";
  if (indexC < 37.8f) return "Increasing Discomfort";
  if (indexC < 44.4f) return "Extremely Warm";
  if (indexC < 51.7f) return "Danger Of Heatstroke";
  if (indexC < 65.6f) return "Extreme Danger Of Heatstroke";
  return "Circulatory Collapse Imminent";
}

//=====================================================================
//=== MQTT Publishing ===
//=====================================================================
void satPublishMQTT()
{
  if (!settings.mqtt.bEnable || !state.mqtt.bConnected) return;
  if (!settings.sat.bEnabled) return;

  SATDebugTln(F("SAT: publishing MQTT state"));

  char valBuf[16];

  // Control mode: off/continuous/pwm
  static const char* modeNames[] = { "off", "continuous", "pwm" };
  uint8_t modeIdx = (uint8_t)state.sat.eControlMode;
  sendMQTTData(F("sat/mode"), modeNames[modeIdx < 3 ? modeIdx : 0], true);

  // Key temperatures
  dtostrf(state.sat.fFinalSetpoint, 1, 1, valBuf);
  sendMQTTData(F("sat/setpoint"), valBuf, true);

  dtostrf(state.sat.fHeatingCurveValue, 1, 1, valBuf);
  sendMQTTData(F("sat/heating_curve"), valBuf, true);

  dtostrf(state.sat.fPidOutput, 1, 1, valBuf);
  sendMQTTData(F("sat/pid_output"), valBuf, true);

  dtostrf(settings.sat.fTargetTemp, 1, 1, valBuf);
  sendMQTTData(F("sat/target"), valBuf, true);

  dtostrf(state.sat.fError, 1, 2, valBuf);
  sendMQTTData(F("sat/error"), valBuf, false);

  // PID terms
  dtostrf(state.sat.fPidP, 1, 2, valBuf);
  sendMQTTData(F("sat/pid_p"), valBuf, false);

  dtostrf(state.sat.fPidI, 1, 2, valBuf);
  sendMQTTData(F("sat/pid_i"), valBuf, false);

  dtostrf(state.sat.fPidD, 1, 2, valBuf);
  sendMQTTData(F("sat/pid_d"), valBuf, false);

  // PID JSON attributes for HA (json_attributes_topic) — Task #55
  {
    char jsonBuf[128];
    snprintf_P(jsonBuf, sizeof(jsonBuf), PSTR("{\"error\":%.2f,\"proportional\":%.2f,\"integral\":%.2f,\"derivative\":%.2f}"),
      state.sat.fError, state.sat.fPidP, state.sat.fPidI, state.sat.fPidD);
    sendMQTTData(F("sat/pid_attributes"), jsonBuf, false);
  }

  dtostrf(state.sat.fRawDerivative, 1, 4, valBuf);
  sendMQTTData(F("sat/raw_derivative"), valBuf, false);

  feedWatchDog(); // ~15 publishes done; prevent WDT trip on slow broker

  // Boiler status (string label)
  { char bsName[20]; satGetBoilerStatusName(bsName, sizeof(bsName));
    sendMQTTData(F("sat/boiler_status"), bsName, false); }

  // Cycle class (string label)
  { static const char* const ccNames[] PROGMEM = {
      "none", "good", "overshoot", "underheat", "short", "uncertain"
    };
    int ccIdx = (int)state.sat.eLastCycleClass;
    if (ccIdx < 0 || ccIdx > 5) ccIdx = 0;
    sendMQTTData(F("sat/cycle_class"), ccNames[ccIdx], false);
  }

  // Cycle Status JSON attributes (Task #53)
  { static const char* const ckNames[] PROGMEM = {
      "UNKNOWN", "CENTRAL_HEATING", "DOMESTIC_HOT_WATER", "MIXED"
    };
    int ckIdx = (int)state.sat.eLastCycleKind;
    if (ckIdx < 0 || ckIdx > 3) ckIdx = 0;
    char jBuf[200];
    snprintf_P(jBuf, sizeof(jBuf),
      PSTR("{\"kind\":\"%s\",\"sample_count\":%u,\"duration_seconds\":%.1f,\"max_flow_temperature\":%.1f,\"fraction_space_heating\":%.2f,\"fraction_domestic_hot_water\":%.2f}"),
      ckNames[ckIdx],
      (unsigned)state.sat.iCycleCount,
      state.sat.fLastCycleDuration,
      state.sat.fCycleMaxFlow,
      state.sat.fLastCycleFractionCH,
      state.sat.fLastCycleFractionDHW);
    sendMQTTData(F("sat/cycle_attributes"), jBuf, false);
  }

  // PWM duty
  dtostrf(state.sat.fPwmDutyCycle, 1, 2, valBuf);
  sendMQTTData(F("sat/pwm_duty"), valBuf, false);

  // Cycle health metrics
  dtostrf(state.sat.fDutyRatio, 1, 3, valBuf);
  sendMQTTData(F("sat/duty_ratio"), valBuf, false);

  dtostrf(state.sat.fOvershootFraction, 1, 3, valBuf);
  sendMQTTData(F("sat/overshoot_fraction"), valBuf, false);

  // Cycle phase
  sendMQTTData(F("sat/cycle_phase"), satCycleGetPhaseName(), false);

  // Per-hour cycle counter (Task #203)
  snprintf_P(valBuf, sizeof(valBuf), PSTR("%u"), (unsigned)satCycleGetCyclesThisHour());
  sendMQTTData(F("sat/cycles_this_hour"), valBuf, false);

  // Rolling 4-hour window statistics (Task #227)
  snprintf_P(valBuf, sizeof(valBuf), PSTR("%u"), (unsigned)state.sat.i4hCycles);
  sendMQTTData(F("sat/4h_cycles"), valBuf, false);
  dtostrf(state.sat.f4hAvgOnSec, 1, 1, valBuf);
  sendMQTTData(F("sat/4h_avg_on_sec"), valBuf, false);
  dtostrf(state.sat.f4hAvgOffSec, 1, 1, valBuf);
  sendMQTTData(F("sat/4h_avg_off_sec"), valBuf, false);
  dtostrf(state.sat.f4hAvgFlow, 1, 1, valBuf);
  sendMQTTData(F("sat/4h_avg_flow_temp"), valBuf, false);
  dtostrf(state.sat.f4hDutyRatio, 1, 3, valBuf);
  sendMQTTData(F("sat/4h_duty_ratio"), valBuf, false);
  dtostrf(state.sat.f4hOvershootFraction, 1, 3, valBuf);
  sendMQTTData(F("sat/4h_overshoot_fraction"), valBuf, false);
  dtostrf(state.sat.f4hUnderheatFraction, 1, 3, valBuf);
  sendMQTTData(F("sat/4h_underheat_fraction"), valBuf, false);
  dtostrf(state.sat.f4hFlowRetDeltaP50, 1, 1, valBuf);
  sendMQTTData(F("sat/4h_flow_ret_delta_p50"), valBuf, false);
  dtostrf(state.sat.f4hFlowRetDeltaP90, 1, 1, valBuf);
  sendMQTTData(F("sat/4h_flow_ret_delta_p90"), valBuf, false);

  feedWatchDog(); // ~35 publishes done; prevent WDT trip on slow broker

  // Overshoot margin
  dtostrf(settings.sat.fOvershootMargin, 1, 1, valBuf);
  sendMQTTData(F("sat/overshoot_margin"), valBuf, true);

  // Active state
  sendMQTTData(F("sat/active"), state.sat.bActive ? "true" : "false", true);

  // Room and outside temps
  dtostrf(satGetRoomTemp(), 1, 1, valBuf);
  sendMQTTData(F("sat/room_temp"), valBuf, false);

  dtostrf(satGetOutsideTemp(), 1, 1, valBuf);
  sendMQTTData(F("sat/outside_temp"), valBuf, false);

  // PID gains
  dtostrf(state.sat.fKp, 1, 4, valBuf);
  sendMQTTData(F("sat/kp"), valBuf, false);

  dtostrf(state.sat.fKi, 1, 6, valBuf);
  sendMQTTData(F("sat/ki"), valBuf, false);

  dtostrf(state.sat.fKd, 1, 2, valBuf);
  sendMQTTData(F("sat/kd"), valBuf, false);

  // Safety
  sendMQTTData(F("sat/safety_tripped"), state.sat.bSafetyTripped ? "true" : "false", false);

  // Flame Status (Task #70)
  { static const char* const fsNames[] PROGMEM = {
      "INSUFFICIENT_DATA", "HEALTHY", "IDLE_OK", "STUCK_ON",
      "STUCK_OFF", "PWM_SHORT", "SHORT_CYCLING"
    };
    int fsIdx = (int)state.sat.eFlameStatus;
    if (fsIdx < 0 || fsIdx > 6) fsIdx = 0;
    sendMQTTData(F("sat/flame_status"), fsNames[fsIdx], false);
  }

  // Flame Health binary sensor (Task #71)
  { SATFlameStatus fs = state.sat.eFlameStatus;
    // Problem if stuck or short cycling; unavailable (don't publish) if insufficient data
    if (fs != SAT_FS_INSUFFICIENT_DATA) {
      bool problem = (fs == SAT_FS_STUCK_ON || fs == SAT_FS_STUCK_OFF ||
                      fs == SAT_FS_PWM_SHORT || fs == SAT_FS_SHORT_CYCLING);
      sendMQTTData(F("sat/flame_health"), problem ? "ON" : "OFF", true);
    }
  }

  // TRV valve detection (Task #29)
  sendMQTTData(F("sat/valves_open"), state.sat.bValvesOpen ? "true" : "false", false);

  // Pre-temperature tracking (Task #67)
  { char valBuf[12];
    if (state.sat.fPreCustomTemp > 0.0f) {
      dtostrf(state.sat.fPreCustomTemp, 1, 1, valBuf);
      sendMQTTData(F("sat/pre_custom_temperature"), valBuf, false);
    }
    if (state.sat.fPreActivityTemp > 0.0f) {
      dtostrf(state.sat.fPreActivityTemp, 1, 1, valBuf);
      sendMQTTData(F("sat/pre_activity_temperature"), valBuf, false);
    }
  }

  // Window detection
  sendMQTTData(F("sat/window_open"), state.sat.bWindowOpen ? "true" : "false", false);

  // Pressure monitoring
  { char pBuf[12];
    dtostrf(state.sat.fSmoothedPressure, 1, 2, pBuf);
    sendMQTTData(F("sat/pressure"), pBuf, false);
    dtostrf(state.sat.fPressureDropRate, 1, 3, pBuf);
    sendMQTTData(F("sat/pressure_drop_rate"), pBuf, false);
    sendMQTTData(F("sat/pressure_alarm"), state.sat.bPressureAlarm ? "true" : "false", false);
    sendMQTTData(F("sat/pressure_health"), state.sat.bPressureHealthy ? "ON" : "OFF", true);
    // JSON attributes for pressure_health binary sensor (Task #59)
    static char pressAttrBuf[200];
    char sBuf[12];
    int pos = 0;
    pos += snprintf_P(pressAttrBuf + pos, sizeof(pressAttrBuf) - pos,
                      PSTR("{\"pressure\":"));
    dtostrf(OTcurrentSystemState.CHPressure, 1, 2, sBuf);
    pos += snprintf_P(pressAttrBuf + pos, sizeof(pressAttrBuf) - pos,
                      PSTR("%s,\"smoothed_pressure\":"), sBuf);
    dtostrf(state.sat.fSmoothedPressure, 1, 2, sBuf);
    pos += snprintf_P(pressAttrBuf + pos, sizeof(pressAttrBuf) - pos,
                      PSTR("%s,\"pressure_drop_rate_bar_per_hour\":"), sBuf);
    dtostrf(state.sat.fPressureDropRate, 1, 3, sBuf);
    pos += snprintf_P(pressAttrBuf + pos, sizeof(pressAttrBuf) - pos,
                      PSTR("%s,\"last_pressure\":"), sBuf);
    dtostrf(state.sat.fLastPressure, 1, 2, sBuf);
    pos += snprintf_P(pressAttrBuf + pos, sizeof(pressAttrBuf) - pos,
                      PSTR("%s,\"last_pressure_timestamp\":%lu,\"last_seen_pressure_timestamp\":%lu}"),
                      sBuf, state.sat.iLastPressureMs / 1000UL,
                      state.sat.iLastSeenPressureMs / 1000UL);
    sendMQTTData(F("sat/pressure_health_attr"), pressAttrBuf, false);
  }
  // Task #226: publish sat/ch_pressure + sat/ch_pressure_status
  satPressureHealthPublish();

  // Current modulation level (published so HA auto-discovery entity has a live topic)
  snprintf_P(valBuf, sizeof(valBuf), PSTR("%d"), (int)state.sat.iCurrentModulation);
  sendMQTTData(F("sat/current_modulation"), valBuf, false);

  // Modulation reliability + setpoint sync
  sendMQTTData(F("sat/modulation_reliable"), state.sat.bModulationReliable ? "true" : "false", false);
  sendMQTTData(F("sat/setpoint_mismatch"), state.sat.bSetpointMismatch ? "true" : "false", false);

  // Heating curve recommendation
  { static const char* const crNames[] = { "insufficient", "increase", "decrease", "hold" };
    int crIdx = (int)state.sat.eCurveRecommendation;
    if (crIdx < 0 || crIdx > 3) crIdx = 0;
    sendMQTTData(F("sat/curve_recommendation"), crNames[crIdx], false);
  }

  // Curve Recommendation JSON attributes (Task #54)
  { char jBuf[180];
    snprintf_P(jBuf, sizeof(jBuf),
      PSTR("{\"error_threshold\":%.2f,\"daily_mean_error\":%.2f,\"daily_sample_count\":%u,\"recent_mean_error\":%.2f}"),
      settings.sat.fDeadband * 2.0f,
      state.sat.fMeanError,
      (unsigned)state.sat.iErrorSampleCount,
      state.sat.fError);
    sendMQTTData(F("sat/curve_recommendation_attributes"), jBuf, false);
  }

  // Daily median heating curve recommendation (Task #228) — retained so HA sees it after restart
  sendMQTTData(F("sat/heating_curve_recommendation"), state.sat.sHeatCurveRec, true);

  // Error statistics
  { char sBuf[12];
    dtostrf(state.sat.fMeanError, 1, 2, sBuf);
    sendMQTTData(F("sat/error_mean"), sBuf, false);
    dtostrf(state.sat.fErrorStdDev, 1, 3, sBuf);
    sendMQTTData(F("sat/error_stddev"), sBuf, false);
  }

  // Power and energy (Task #45)
  dtostrf(state.sat.fCurrentPower, 1, 2, valBuf);
  sendMQTTData(F("sat/power"), valBuf, false);
  dtostrf(state.sat.fEnergyTotal, 1, 3, valBuf);
  sendMQTTData(F("sat/energy_total"), valBuf, true);  // retained for HA energy dashboard

  // Gas consumption estimate (Task #232)
  if (settings.sat.fBoilerRatedKW > 0.0f) {
    dtostrf(state.sat.fEnergyEstimatedKWh, 1, 3, valBuf);
    sendMQTTData(F("sat/energy_estimated_kwh"), valBuf, true); // retained for HA energy dashboard
  }

  // Manufacturer
  { char mfrName[12]; satGetManufacturerName(mfrName, sizeof(mfrName));
    sendMQTTData(F("sat/manufacturer"), mfrName, true); }

  // Thermal drop learning (Task #21)
  // AC#10: MQTT publish thermal_drop_rate
  { char thBuf[12];
    dtostrf(settings.sat.fThermalCoeff, 1, 4, thBuf);
    sendMQTTData(F("sat/thermal_coeff"), thBuf, true);
    dtostrf(state.sat.fThermalDropRate, 1, 4, thBuf);
    sendMQTTData(F("sat/thermal_drop_rate"), thBuf, false);
    sendMQTTData(F("sat/thermal_model_valid"), state.sat.bThermalModelValid ? "true" : "false", true);
    dtostrf(state.sat.fEstimatedRoom, 1, 1, thBuf);
    sendMQTTData(F("sat/estimated_room"), thBuf, false);
  }

  // Solar gain (Task #23)
  sendMQTTData(F("sat/solar_gain"), state.sat.bSolarGainActive ? "true" : "false", false);
  { char sgBuf[12];
    dtostrf(state.sat.fIndoorRiseRate, 1, 2, sgBuf);
    sendMQTTData(F("sat/indoor_rise_rate"), sgBuf, false);
    // Sun elevation (Task #68)
    if (state.sat.bSunElevationValid) {
      dtostrf(state.sat.fSunElevation, 1, 1, sgBuf);
      sendMQTTData(F("sat/solar_gain_sun_elevation"), sgBuf, false);
    }
  }

  // Summer simmer (Task #24)
  sendMQTTData(F("sat/summer_active"), state.sat.bSummerActive ? "true" : "false", false);
  { char suBuf[12];
    dtostrf(state.sat.fSummerHoursAbove, 1, 1, suBuf);
    sendMQTTData(F("sat/summer_hours_above"), suBuf, false);
  }

  // Thermal comfort (Task #28/#47/#231)
  { char cBuf[12];
    dtostrf(state.sat.fHumidity, 1, 1, cBuf);   // 1 decimal (TASK-231)
    sendMQTTData(F("sat/humidity"), cBuf, false);
    sendMQTTData(F("sat/humidity_valid"), state.sat.bHumidityValid ? "true" : "false", false);
    dtostrf(state.sat.fComfortOffset, 1, 2, cBuf);
    sendMQTTData(F("sat/comfort_offset"), cBuf, false);
  }

  // Simulation (Task #37)
  sendMQTTData(F("sat/simulation"), settings.sat.bSimulation ? "ON" : "OFF", true);

  // PID auto-tuning (Task #27)
  sendMQTTData(F("sat/auto_tune"), settings.sat.bAutoTune ? "ON" : "OFF", true);
  if (settings.sat.bAutoTune) {
    char atBuf[12];
    dtostrf(state.sat.fAutoTuneScore, 1, 2, atBuf);
    sendMQTTData(F("sat/auto_tune_score"), atBuf, false);
    dtostrf(settings.sat.fAutoTuneRate, 1, 3, atBuf);
    sendMQTTData(F("sat/auto_tune_rate"), atBuf, false);
    sendMQTTData(F("sat/auto_tune_active"), state.sat.bAutoTuneActive ? "true" : "false", false);
  }

  // SAT Python parity settings (Task #82)
  {
    char agBuf[12];
    snprintf_P(valBuf, sizeof(valBuf), PSTR("%lu"), (unsigned long)settings.sat.iSensorMaxAgeS);
    sendMQTTData(F("sat/sensor_max_age"), valBuf, true);
    sendMQTTData(F("sat/error_monitoring"), settings.sat.bErrorMonitoring ? "true" : "false", true);
    dtostrf(settings.sat.fAutoGainsValue, 1, 2, agBuf);
    sendMQTTData(F("sat/auto_gains_value"), agBuf, true);
    sendMQTTData(F("sat/heating_mode"), settings.sat.iHeatingMode == 1 ? "eco" : "comfort", true);
    snprintf_P(valBuf, sizeof(valBuf), PSTR("%u"), (unsigned)settings.sat.iCyclesPerHour);
    sendMQTTData(F("sat/cycles_per_hour"), valBuf, true);
    dtostrf(settings.sat.fValveOffset, 1, 2, agBuf);
    sendMQTTData(F("sat/valve_offset"), agBuf, true);
    sendMQTTData(F("sat/solar_freeze_integral"), settings.sat.bSolarFreezeIntegral ? "true" : "false", true);
  }

  // Multi-area (Task #25)
  if (settings.sat.bMultiArea && settings.sat.iMultiAreaCount > 0) {
    uint8_t cnt = settings.sat.iMultiAreaCount;
    if (cnt > SAT_MAX_AREAS) cnt = SAT_MAX_AREAS;
    char topicBuf[24];
    char vBuf[12];
    for (uint8_t i = 0; i < cnt; i++) {
      snprintf_P(topicBuf, sizeof(topicBuf), PSTR("sat/area/%u"), i);
      dtostrf(state.sat.fAreaTemp[i], 1, 1, vBuf);
      sendMQTTData(topicBuf, vBuf, false);
    }
  }

  // Device Health binary sensor (Task #60): ON = problem (boiler off / no data)
  sendMQTTData(F("sat/device_health"), (state.sat.eBoilerStatus == SAT_BS_OFF) ? "ON" : "OFF", true);

  // Cycle Health binary sensor (Task #61): ON = problem (overshoot, underheat, or short)
  {
    bool cycleProb = (state.sat.eLastCycleClass == SAT_CYCLE_OVERSHOOT ||
                      state.sat.eLastCycleClass == SAT_CYCLE_UNDERHEAT ||
                      state.sat.eLastCycleClass == SAT_CYCLE_SHORT);
    sendMQTTData(F("sat/cycle_health"), cycleProb ? "ON" : "OFF", true);
  }

  // Summer Simmer Index (Task #64): requires valid humidity
  if (state.sat.bHumidityValid && state.sat.fHumidity > 0) {
    float simmerIdx = satCalcSimmerIndex(satGetRoomTemp(), state.sat.fHumidity);
    snprintf_P(valBuf, sizeof(valBuf), PSTR("%.2f"), simmerIdx);
    sendMQTTData(F("sat/ssi"), valBuf, false);          // short alias (TASK-231)
    snprintf_P(valBuf, sizeof(valBuf), PSTR("%.1f"), simmerIdx);
    sendMQTTData(F("sat/summer_simmer_index"), valBuf, false);
    sendMQTTData(F("sat/summer_simmer_perception"), satSimmerPerception(simmerIdx), false);
  }

  // Relative Modulation State (Task #65)
  {
    const char* modState = "OFF";
    if (state.sat.bActive) {
      if (state.sat.bDhwActive) {
        modState = "HOT_WATER";
      } else if (state.sat.eControlMode == SAT_MODE_PWM && !state.sat.bPwmFlameRequested) {
        modState = "PWM_OFF";
      } else if (OTcurrentSystemState.Tboiler < 22.0f) {
        modState = "COLD";
      } else {
        modState = "ACTIVE";
      }
    }
    sendMQTTData(F("sat/modulation_state"), modState, false);
  }

  // PWM Status (Task #66): ON/OFF/IDLE
  {
    const char* pwmState = "IDLE";
    if (state.sat.eControlMode == SAT_MODE_PWM) {
      pwmState = state.sat.bPwmFlameRequested ? "ON" : "OFF";
    }
    sendMQTTData(F("sat/pwm_state"), pwmState, false);
  }

  // DHW setpoint (Task #62: HA number entity)
  dtostrf(settings.sat.fDhwSetpoint, 1, 1, valBuf);
  sendMQTTData(F("sat/dhw_setpoint"), valBuf, true);

  // Max setpoint for heating system (Task #63: HA number entity)
  dtostrf(satGetMaxSetpoint(), 1, 1, valBuf);
  sendMQTTData(F("sat/max_setpoint"), valBuf, true);

  // Requested setpoint: PID output clamped to [min, max] before PWM (Task #51)
  {
    float reqSp = state.sat.fPidOutput;
    float sysMax = satGetMaxSetpoint();
    if (reqSp < SAT_MIN_SETPOINT) reqSp = SAT_MIN_SETPOINT;
    if (reqSp > sysMax)           reqSp = sysMax;
    dtostrf(reqSp, 1, 1, valBuf);
    sendMQTTData(F("sat/requested_setpoint"), valBuf, false);
  }

  // Gas consumption m3/h (Task #52) — only when min+max consumption configured
  {
    float minCons = 0.0f;  // TODO: from settings when available
    float maxCons = 0.0f;  // TODO: from settings when available
    if (minCons > 0 && maxCons > 0) {
      float consumption = 0.0f;
      bool flame = (OTcurrentSystemState.Statusflags & 0x08) != 0;
      if (state.sat.bActive && flame) {
        float modFrac = OTcurrentSystemState.RelModLevel / 100.0f;
        consumption = minCons + (modFrac * (maxCons - minCons));
      }
      snprintf_P(valBuf, sizeof(valBuf), PSTR("%.3f"), consumption);
      sendMQTTData(F("sat/consumption"), valBuf, false);
    }
  }

  // Synchronization binary sensors (Tasks #56, #57, #58)
  // Each uses a 60-second delay to avoid false positives during normal transitions.
  {
    // Task #56: Control Setpoint Synchronization
    static unsigned long syncSetpointMismatchSince = 0;
    {
      float satSetpoint = state.sat.fFinalSetpoint;
      float boilerSetpoint = OTcurrentSystemState.TSet;
      bool mismatch = (fabsf(satSetpoint - boilerSetpoint) > 0.5f) && state.sat.bActive;

      if (!mismatch) {
        syncSetpointMismatchSince = 0;
      } else if (syncSetpointMismatchSince == 0) {
        syncSetpointMismatchSince = millis();
      }

      bool problem = mismatch && syncSetpointMismatchSince > 0 &&
                     (millis() - syncSetpointMismatchSince >= 60000UL);
      sendMQTTData(F("sat/setpoint_sync"), problem ? "ON" : "OFF", true);
    }

    // Task #57: Relative Modulation Synchronization
    static unsigned long syncModulationMismatchSince = 0;
    {
      int satMod = (int)settings.sat.iMaxRelModulation;
      int boilerMod = (int)OTcurrentSystemState.MaxRelModLevelSetting;
      bool mismatch = (satMod != boilerMod) && state.sat.bActive;

      if (!mismatch) syncModulationMismatchSince = 0;
      else if (syncModulationMismatchSince == 0) syncModulationMismatchSince = millis();

      bool problem = mismatch && syncModulationMismatchSince > 0 &&
                     (millis() - syncModulationMismatchSince >= 60000UL);
      sendMQTTData(F("sat/modulation_sync"), problem ? "ON" : "OFF", true);
    }

    // Task #58: Central Heating Synchronization
    static unsigned long syncCHMismatchSince = 0;
    {
      bool boilerActive = (OTcurrentSystemState.SlaveStatus & 0x02) != 0;  // Bit 1 = CH active
      bool satHeating = state.sat.bActive;
      bool mismatch = (satHeating != boilerActive);

      if (!mismatch) syncCHMismatchSince = 0;
      else if (syncCHMismatchSince == 0) syncCHMismatchSince = millis();

      bool problem = mismatch && syncCHMismatchSince > 0 &&
                     (millis() - syncCHMismatchSince >= 60000UL);
      sendMQTTData(F("sat/ch_sync"), problem ? "ON" : "OFF", true);
    }
  }

  // --- Task #81: Publish all SAT settings as individual MQTT topics for HA entities ---
  {
    // Number settings (float)
    dtostrf(settings.sat.fHeatingCurveCoeff, 1, 1, valBuf);
    sendMQTTData(F("sat/heating_curve_coeff"), valBuf, true);

    dtostrf(settings.sat.fDeadband, 1, 2, valBuf);
    sendMQTTData(F("sat/deadband"), valBuf, true);

    snprintf_P(valBuf, sizeof(valBuf), PSTR("%u"), settings.sat.iControlInterval);
    sendMQTTData(F("sat/control_interval"), valBuf, true);

    snprintf_P(valBuf, sizeof(valBuf), PSTR("%u"), settings.sat.iMaxRelModulation);
    sendMQTTData(F("sat/max_modulation"), valBuf, true);

    dtostrf(settings.sat.fFlameOffOffset, 1, 1, valBuf);
    sendMQTTData(F("sat/flame_off_offset"), valBuf, true);

    dtostrf(settings.sat.fFlowOffset, 1, 1, valBuf);
    sendMQTTData(F("sat/flow_offset"), valBuf, true);

    dtostrf(settings.sat.fModSupDelay, 1, 1, valBuf);
    sendMQTTData(F("sat/mod_sup_delay"), valBuf, true);

    dtostrf(settings.sat.fModSupOffset, 1, 1, valBuf);
    sendMQTTData(F("sat/mod_sup_offset"), valBuf, true);

    dtostrf(settings.sat.fBoilerCapacity, 1, 1, valBuf);
    sendMQTTData(F("sat/boiler_capacity"), valBuf, true);

    dtostrf(settings.sat.fBoilerRatedKW, 1, 1, valBuf);
    sendMQTTData(F("sat/boiler_rated_kw"), valBuf, true);

    dtostrf(settings.sat.fBoilerEfficiency, 1, 2, valBuf);
    sendMQTTData(F("sat/boiler_efficiency"), valBuf, true);

    dtostrf(settings.sat.fComfortHumidity, 1, 0, valBuf);
    sendMQTTData(F("sat/comfort_humidity"), valBuf, true);

    dtostrf(settings.sat.fComfortMaxOffset, 1, 1, valBuf);
    sendMQTTData(F("sat/comfort_max_offset"), valBuf, true);

    dtostrf(settings.sat.fSummerThreshold, 1, 1, valBuf);
    sendMQTTData(F("sat/summer_threshold"), valBuf, true);

    dtostrf(settings.sat.fTargetTempStep, 1, 1, valBuf);
    sendMQTTData(F("sat/target_temp_step"), valBuf, true);

    dtostrf(settings.sat.fMinPressure, 1, 1, valBuf);
    sendMQTTData(F("sat/min_pressure"), valBuf, true);

    dtostrf(settings.sat.fMaxPressure, 1, 1, valBuf);
    sendMQTTData(F("sat/max_pressure"), valBuf, true);

    dtostrf(settings.sat.fMaxPressureDrop, 1, 2, valBuf);
    sendMQTTData(F("sat/max_pressure_drop"), valBuf, true);

    // Preset temperatures
    dtostrf(settings.sat.fPresetComfort, 1, 1, valBuf);
    sendMQTTData(F("sat/preset_comfort"), valBuf, true);

    dtostrf(settings.sat.fPresetEco, 1, 1, valBuf);
    sendMQTTData(F("sat/preset_eco"), valBuf, true);

    dtostrf(settings.sat.fPresetAway, 1, 1, valBuf);
    sendMQTTData(F("sat/preset_away"), valBuf, true);

    dtostrf(settings.sat.fPresetSleep, 1, 1, valBuf);
    sendMQTTData(F("sat/preset_sleep"), valBuf, true);

    dtostrf(settings.sat.fPresetActivity, 1, 1, valBuf);
    sendMQTTData(F("sat/preset_activity"), valBuf, true);

    dtostrf(settings.sat.fPresetHome, 1, 1, valBuf);
    sendMQTTData(F("sat/preset_home"), valBuf, true);

    // OVP value (already has cmd topic, add state for completeness)
    dtostrf(settings.sat.fOvpValue, 1, 1, valBuf);
    sendMQTTData(F("sat/ovp_value"), valBuf, true);

    // Heating system type (integer)
    snprintf_P(valBuf, sizeof(valBuf), PSTR("%u"), settings.sat.iHeatingSystem);
    sendMQTTData(F("sat/heating_system"), valBuf, true);

    // Manufacturer (integer)
    snprintf_P(valBuf, sizeof(valBuf), PSTR("%u"), settings.sat.iManufacturer);
    sendMQTTData(F("sat/manufacturer_id"), valBuf, true);

    // Switch (boolean) settings
    sendMQTTData(F("sat/solar_gain_enable"), settings.sat.bSolarGainEnable ? "true" : "false", true);
    sendMQTTData(F("sat/summer_simmer_enable"), settings.sat.bSummerSimmer ? "true" : "false", true);
    sendMQTTData(F("sat/comfort_adjust_enable"), settings.sat.bComfortAdjust ? "true" : "false", true);
    // TASK-204/231: thermal comfort mode state (SSI substitution for PID room temp)
    sendMQTTData(F("sat/thermal_comfort"), settings.sat.bThermalComfort ? "true" : "false", true);
    { char tcBuf[8];
      snprintf_P(tcBuf, sizeof(tcBuf), PSTR("%u"), (unsigned)settings.sat.iHumidityTimeoutS);
      sendMQTTData(F("sat/humidity_timeout_s"), tcBuf, true); }
    sendMQTTData(F("sat/multi_area_enable"), settings.sat.bMultiArea ? "true" : "false", true);
    sendMQTTData(F("sat/auto_tune_enable"), settings.sat.bAutoTune ? "true" : "false", true);
    sendMQTTData(F("sat/simulation_enable"), settings.sat.bSimulation ? "true" : "false", true);
    sendMQTTData(F("sat/window_detection_enable"), settings.sat.bWindowDetection ? "true" : "false", true);
    sendMQTTData(F("sat/force_pwm_enable"), settings.sat.bForcePWM ? "true" : "false", true);
    sendMQTTData(F("sat/push_setpoint_enable"), settings.sat.bPushSetpoint ? "true" : "false", true);
    sendMQTTData(F("sat/ovp_enabled"), settings.sat.bOvpEnabled ? "true" : "false", true);
    sendMQTTData(F("sat/preset_sync_enable"), settings.sat.bPresetSync ? "true" : "false", true);
    sendMQTTData(F("sat/dhw_enabled"), settings.sat.bDhwEnabled ? "true" : "false", true);
    sendMQTTData(F("sat/pwm_auto_switch_enable"), settings.sat.bPwmAutoSwitch ? "true" : "false", true);
  }

  // Climate entity extra_state_attributes JSON blob (Task #72)
  // Publishes sat/climate_attributes for HA json_attributes_topic
  {
    static char climAttrBuf[512];
    char fBuf[16];
    int pos = 0;

    // optimal_coefficient — heating curve coefficient (maps to SAT Python optimal_coefficient)
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR("{"));
    dtostrf(settings.sat.fHeatingCurveCoeff, 1, 2, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR("\"optimal_coefficient\":%s"), fBuf);

    // coefficient_derivative — not tracked in firmware; publish 0.0
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"coefficient_derivative\":0.0"));

    // minimum_setpoint — static minimum boiler setpoint (SAT_MIN_SETPOINT = 10.0)
    dtostrf(SAT_MIN_SETPOINT, 1, 1, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"minimum_setpoint\":%s"), fBuf);

    // boiler_flame_timing — duration of last completed flame cycle in seconds
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"boiler_flame_timing\":%.1f"), state.sat.fLastCycleDuration);

    // boiler_temperature_cold — boiler temp when flame is off (Tboiler when not heating)
    dtostrf(OTcurrentSystemState.Tboiler, 1, 1, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"boiler_temperature_cold\":%s"), fBuf);

    // boiler_temperature_tracking — no EMA tracking state in firmware; publish false
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"boiler_temperature_tracking\":false"));

    // boiler_temperature_derivative — not tracked; publish 0.0
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"boiler_temperature_derivative\":0.0"));

    // error_source — single zone; always "main"
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"error_source\":\"main\""));

    // error_pid — current PID error (target - room)
    dtostrf(state.sat.fError, 1, 2, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"error_pid\":%s"), fBuf);

    // integral_enabled — integral is always active when SAT is running
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"integral_enabled\":true"));

    // derivative_enabled — derivative is always active when SAT is running
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"derivative_enabled\":true"));

    // derivative_raw — raw (filtered) derivative before PID scaling
    dtostrf(state.sat.fRawDerivative, 1, 4, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"derivative_raw\":%s"), fBuf);

    // current_kp, current_ki, current_kd — current PID gains
    dtostrf(state.sat.fKp, 1, 4, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"current_kp\":%s"), fBuf);
    dtostrf(state.sat.fKi, 1, 6, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"current_ki\":%s"), fBuf);
    dtostrf(state.sat.fKd, 1, 2, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"current_kd\":%s"), fBuf);

    // relative_modulation_enabled — true unless manufacturer quirk disables it
    bool relModEnabled = !(satGetManufacturerQuirks() & SAT_QUIRK_NO_REL_MOD);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"relative_modulation_enabled\":%s}"),
                      relModEnabled ? "true" : "false");

    sendMQTTData(F("sat/climate_attributes"), climAttrBuf, false);
  }

  // Weather data (Task #50)
  weatherPublishMQTT();

#if defined(ESP32)
  // BLE sensor data (Task #20)
  satBLEPublishMQTT();
#endif
}

//=====================================================================
//=== Thermal Comfort Adjustment (Task #28/#47) ===
//=====================================================================
static void satUpdateComfort()
{
  if (!settings.sat.bComfortAdjust || !state.sat.bHumidityValid || !settings.sat.bSummerSimmer) {
    state.sat.fComfortOffset = 0.0f;
    return;
  }
  // Stale check: humidity older than 30 min is invalid
  if (millis() - state.sat.iHumidityLastMs > 1800000UL) {
    state.sat.bHumidityValid = false;
    state.sat.fComfortOffset = 0.0f;
    return;
  }
  // Use weather API humidity as fallback if direct humidity is stale
  // (already validated above, so this only applies when bHumidityValid was
  //  set from weather data in satControlLoop)

  // Comfort model: humidity above reference makes it feel warmer (reduce setpoint)
  // humidity below reference makes it feel cooler (increase setpoint)
  float delta = (state.sat.fHumidity - settings.sat.fComfortHumidity) / 100.0f;
  float offset = -delta * settings.sat.fComfortMaxOffset * 2.0f;
  // Clamp to max offset
  if (offset > settings.sat.fComfortMaxOffset) offset = settings.sat.fComfortMaxOffset;
  if (offset < -settings.sat.fComfortMaxOffset) offset = -settings.sat.fComfortMaxOffset;
  state.sat.fComfortOffset = offset;
}

//=====================================================================
//=== Power & Energy Tracking (Task #45) ===
//=====================================================================
static void satUpdatePowerEnergy()
{
  float modulation = OTcurrentSystemState.RelModLevel;
  bool flame = (OTcurrentSystemState.Statusflags & 0x08) != 0;

  // Power = modulation% * capacity (only when flame is on)
  if (flame && modulation > 0.0f) {
    state.sat.fCurrentPower = (modulation / 100.0f) * settings.sat.fBoilerCapacity;
  } else {
    state.sat.fCurrentPower = 0.0f;
  }

  // Clamp to reasonable bounds
  if (state.sat.fCurrentPower < 0.0f) state.sat.fCurrentPower = 0.0f;
  if (state.sat.fCurrentPower > settings.sat.fBoilerCapacity * 1.1f)
    state.sat.fCurrentPower = settings.sat.fBoilerCapacity;

  // Integrate energy (kWh = kW * hours)
  uint32_t now = millis();
  if (state.sat.iEnergyLastMs > 0) {
    float dtHours = (float)(now - state.sat.iEnergyLastMs) / 3600000.0f;
    if (dtHours > 0.0f && dtHours < 1.0f) { // Sanity: max 1 hour gap
      state.sat.fEnergyTotal += state.sat.fCurrentPower * dtHours;
    }
  }
  state.sat.iEnergyLastMs = now;

  // --- Gas consumption estimation (Task #232) ---
  // Only accumulate when rated power is configured (> 0) and flame is on.
  if (settings.sat.fBoilerRatedKW > 0.0f && flame) {
    float estPowerKW = 0.0f;

    // Prefer thermal power from flow rate (MsgID 19, DHW circuit) if available.
    // DHWFlowRate is in L/min; convert to L/s for P = m_dot * Cp * dT.
    float flowLmin = OTcurrentSystemState.DHWFlowRate;
    float tFlow    = OTcurrentSystemState.Tboiler; // flow water temp (MsgID 25)
    float tRet     = OTcurrentSystemState.Tret;    // return water temp (MsgID 28)
    if (flowLmin > 0.1f && tFlow > 0.0f && tRet > 0.0f && (tFlow - tRet) > 0.5f) {
      // P_thermal (kW) = (L/min / 60) * 4186 J/(kg·K) * dT / 1000
      float flowLs = flowLmin / 60.0f;
      estPowerKW = flowLs * 4186.0f * (tFlow - tRet) / 1000.0f;
    } else {
      // Fallback: modulation-based estimate
      estPowerKW = (modulation / 100.0f) * settings.sat.fBoilerRatedKW * settings.sat.fBoilerEfficiency;
    }

    // Clamp to rated power (with 10% headroom for measurement noise)
    if (estPowerKW < 0.0f)  estPowerKW = 0.0f;
    if (estPowerKW > settings.sat.fBoilerRatedKW * 1.1f) estPowerKW = settings.sat.fBoilerRatedKW;

    if (state.sat.iEstEnergyLastMs > 0) {
      float dtHours = (float)(now - state.sat.iEstEnergyLastMs) / 3600000.0f;
      if (dtHours > 0.0f && dtHours < 1.0f) {
        state.sat.fEnergyEstimatedKWh += estPowerKW * dtHours;
      }
    }
  }
  state.sat.iEstEnergyLastMs = now;
}

//=====================================================================
//=== Pressure Monitoring (Task #10, enhanced Task #39, Task #59) ===
//=====================================================================

// --- Pressure sample ring buffer for linear regression drop rate ---
static const uint8_t  PRESS_RING_SIZE = 20;          // max samples in history
struct PressSample { uint32_t ms; float val; };
static PressSample _press_ring[PRESS_RING_SIZE];
static uint8_t  _press_ringCount = 0;
static uint8_t  _press_ringHead  = 0;                // next write position
static uint32_t _press_lastSampleMs = 0;

// --- CH active state tracking for 600s suspension ---
static bool     _press_lastActive     = false;
static bool     _press_lastActiveInit = false;
static uint32_t _press_suspendUntilMs = 0;

static void satUpdatePressure()
{
  float raw = OTcurrentSystemState.CHPressure;
  uint32_t now = millis();

  // --- Track CH active state transitions (600s drop rate suspension) ---
  bool active = state.sat.bActive;
  if (!_press_lastActiveInit) {
    _press_lastActive = active;
    _press_lastActiveInit = true;
  } else if (_press_lastActive != active) {
    _press_suspendUntilMs = now + 600000UL;
    // Clear sample history on state transition
    _press_ringCount = 0;
    _press_ringHead  = 0;
    _press_lastSampleMs = 0;
  }
  _press_lastActive = active;

  // No pressure reading available
  if (raw < 0.01f) return;

  state.sat.iLastSeenPressureMs = now;

  // --- EMA smoothing (alpha=0.05) ---
  if (state.sat.fSmoothedPressure < 0.01f) {
    state.sat.fSmoothedPressure = raw;  // Initialize
  } else {
    state.sat.fSmoothedPressure = 0.05f * raw + 0.95f * state.sat.fSmoothedPressure;
  }
  float smoothed = state.sat.fSmoothedPressure;

  // --- Record raw pressure sample every ~30s ---
  if (_press_lastSampleMs == 0 || (now - _press_lastSampleMs) >= 30000UL) {
    _press_lastSampleMs = now;
    _press_ring[_press_ringHead].ms  = now;
    _press_ring[_press_ringHead].val = raw;
    _press_ringHead = (_press_ringHead + 1) % PRESS_RING_SIZE;
    if (_press_ringCount < PRESS_RING_SIZE) _press_ringCount++;

    // Evict samples older than 3600s (matches SAT Python drop_window_seconds)
    while (_press_ringCount > 0) {
      uint8_t oldest = (_press_ringCount < PRESS_RING_SIZE)
                       ? 0
                       : _press_ringHead;  // in a full ring, head IS oldest
      if ((now - _press_ring[oldest].ms) > 3600000UL) {
        if (_press_ringCount < PRESS_RING_SIZE) {
          // Partial ring: shift array left
          for (uint8_t i = 0; i < _press_ringCount - 1; i++) {
            _press_ring[i] = _press_ring[i + 1];
          }
          if (_press_ringHead > 0) _press_ringHead--;
        } else {
          // Full ring: advance head past oldest
          _press_ringHead = (_press_ringHead + 1) % PRESS_RING_SIZE;
        }
        _press_ringCount--;
      } else {
        break;
      }
    }
  }

  // --- Linear regression for drop rate (min 3 samples, 300s window) ---
  bool dropRateSuspended = (_press_suspendUntilMs != 0 && now < _press_suspendUntilMs);
  float dropRate = -1.0f;  // sentinel: not calculated

  if (!dropRateSuspended && _press_ringCount >= 3) {
    uint8_t oldestIdx = (_press_ringCount < PRESS_RING_SIZE) ? 0 : _press_ringHead;
    uint8_t newestIdx = (_press_ringHead == 0)
                        ? (PRESS_RING_SIZE - 1) : (_press_ringHead - 1);
    if (_press_ringCount < PRESS_RING_SIZE) {
      newestIdx = _press_ringCount - 1;
    }
    uint32_t elapsed = _press_ring[newestIdx].ms - _press_ring[oldestIdx].ms;

    if (elapsed >= 300000UL) {  // 300s minimum window
      float sum_t = 0.0f, sum_p = 0.0f, sum_tp = 0.0f, sum_t2 = 0.0f;
      uint8_t n = _press_ringCount;
      uint32_t t0 = _press_ring[oldestIdx].ms;

      for (uint8_t i = 0; i < n; i++) {
        uint8_t idx = (_press_ringCount < PRESS_RING_SIZE)
                      ? i
                      : (oldestIdx + i) % PRESS_RING_SIZE;
        float t = (float)(_press_ring[idx].ms - t0) / 1000.0f;
        float p = _press_ring[idx].val;
        sum_t  += t;
        sum_p  += p;
        sum_tp += t * p;
        sum_t2 += t * t;
      }

      float denom = (float)n * sum_t2 - sum_t * sum_t;
      if (denom > 0.001f || denom < -0.001f) {
        float slope = ((float)n * sum_tp - sum_t * sum_p) / denom;
        dropRate = -slope * 3600.0f;  // bar/hour, positive = dropping
      }
    }
  }

  // Update state drop rate
  if (dropRateSuspended) {
    state.sat.fPressureDropRate = 0.0f;
  } else if (dropRate >= 0.0f) {
    state.sat.fPressureDropRate = dropRate;
  }

  // Clear suspension once expired
  if (_press_suspendUntilMs != 0 && now >= _press_suspendUntilMs) {
    _press_suspendUntilMs = 0;
  }

  // --- Update last pressure tracking ---
  state.sat.fLastPressure    = raw;
  state.sat.iLastPressureMs  = now;

  // --- Alarm conditions with 120s confirmation ---
  bool dropRateHigh = (!dropRateSuspended && dropRate >= 0.0f &&
                       dropRate > settings.sat.fMaxPressureDrop);
  bool alarmCond = (smoothed < settings.sat.fMinPressure ||
                    smoothed > settings.sat.fMaxPressure ||
                    dropRateHigh);

  if (alarmCond) {
    if (state.sat.iPressureAlarmSinceMs == 0) {
      state.sat.iPressureAlarmSinceMs = now;
    }
    // 120s confirmation window before declaring problem
    if ((now - state.sat.iPressureAlarmSinceMs) >= 120000UL) {
      if (!state.sat.bPressureAlarm) {
        state.sat.bPressureAlarm = true;
        state.sat.bPressureHealthy = false;
        SATDebugTf(PSTR("SAT: PRESSURE ALARM (smoothed=%.2f drop=%.3f bar/hr)\r\n"),
                smoothed, state.sat.fPressureDropRate);
      }
    }
  } else {
    state.sat.iPressureAlarmSinceMs = 0;
    if (state.sat.bPressureAlarm) {
      state.sat.bPressureAlarm = false;
      state.sat.bPressureHealthy = true;
      SATDebugTln(F("SAT: Pressure alarm cleared"));
    }
  }
}

//=====================================================================
//=== Heating Curve Recommendation (Task #11) ===
//=====================================================================
static const uint8_t  CR_BUFFER_SIZE = 24;
static float    _cr_errorBuffer[CR_BUFFER_SIZE];
static uint8_t  _cr_bufferCount  = 0;
static uint8_t  _cr_bufferIdx    = 0;
static uint32_t _cr_lastSampleMs = 0;
static const uint32_t CR_SAMPLE_INTERVAL_MS = 3600000UL;  // 1 sample per hour

static void satUpdateCurveRecommendation()
{
  uint32_t now = millis();

  // Sample every hour
  if (_cr_lastSampleMs != 0 && (now - _cr_lastSampleMs) < CR_SAMPLE_INTERVAL_MS) return;
  _cr_lastSampleMs = now;

  // Only sample when actively controlling
  if (!state.sat.bActive) return;

  // Store error sample in ring buffer
  _cr_errorBuffer[_cr_bufferIdx] = state.sat.fError;
  _cr_bufferIdx = (_cr_bufferIdx + 1) % CR_BUFFER_SIZE;
  if (_cr_bufferCount < CR_BUFFER_SIZE) _cr_bufferCount++;

  state.sat.iErrorSampleCount = _cr_bufferCount;

  // Need at least 6 samples
  if (_cr_bufferCount < 6) {
    state.sat.eCurveRecommendation = SAT_CR_INSUFFICIENT;
    state.sat.fMeanError = 0.0f;
    return;
  }

  // Calculate mean error
  float sum = 0.0f;
  for (uint8_t i = 0; i < _cr_bufferCount; i++) {
    sum += _cr_errorBuffer[i];
  }
  float mean = sum / (float)_cr_bufferCount;
  state.sat.fMeanError = mean;

  // Standard deviation
  float sumSq = 0.0f;
  for (uint8_t i = 0; i < _cr_bufferCount; i++) {
    float d = _cr_errorBuffer[i] - mean;
    sumSq += d * d;
  }
  state.sat.fErrorStdDev = sqrtf(sumSq / (float)_cr_bufferCount);

  // Compare with 2*deadband threshold
  float threshold = settings.sat.fDeadband * 2.0f;
  if (mean > threshold) {
    state.sat.eCurveRecommendation = SAT_CR_INCREASE;  // Room too cold
  } else if (mean < -threshold) {
    state.sat.eCurveRecommendation = SAT_CR_DECREASE;  // Room too warm
  } else {
    state.sat.eCurveRecommendation = SAT_CR_HOLD;
  }
}

//=====================================================================
//=== Modulation Reliability Tracker (Task #33) ===
//=====================================================================
static float    _mod_prevValue     = -1.0f;
static uint32_t _mod_windowStartMs = 0;
static const uint32_t MOD_WINDOW_MS = 600000UL;  // 10 min observation window
static const uint8_t  MOD_MIN_CHANGES = 3;        // need at least 3 changes to be "reliable"

static void satUpdateModulationReliability()
{
  float mod = OTcurrentSystemState.RelModLevel;
  uint32_t now = millis();

  if (_mod_windowStartMs == 0) {
    _mod_windowStartMs = now;
    _mod_prevValue = mod;
    return;
  }

  // Count value changes (more than 1% difference)
  if (fabsf(mod - _mod_prevValue) > 1.0f) {
    if (state.sat.iModChangeCount < 255) state.sat.iModChangeCount++;
    _mod_prevValue = mod;
  }

  // Check at end of window
  if ((now - _mod_windowStartMs) >= MOD_WINDOW_MS) {
    state.sat.bModulationReliable = (state.sat.iModChangeCount >= MOD_MIN_CHANGES);
    state.sat.iModChangeCount = 0;
    _mod_windowStartMs = now;
  }
}

//=====================================================================
//=== OT Setpoint Sync Sensor (Task #40) ===
//=====================================================================
static void satCheckSetpointSync()
{
  // Compare what we sent (fFinalSetpoint) vs what boiler reports (Tset)
  float sent = state.sat.fFinalSetpoint;
  float reported = OTcurrentSystemState.TSet;
  uint32_t now = millis();

  if (sent < 1.0f) return;  // Not controlling

  // Mismatch if difference > 1.0C (allow rounding)
  bool mismatch = (fabsf(sent - reported) > 1.0f);

  if (mismatch) {
    if (state.sat.iMismatchSinceMs == 0) {
      state.sat.iMismatchSinceMs = now;
    }
    // Confirm after 60s delay
    if ((now - state.sat.iMismatchSinceMs) >= 60000UL) {
      state.sat.bSetpointMismatch = true;
    }
  } else {
    state.sat.iMismatchSinceMs = 0;
    state.sat.bSetpointMismatch = false;
  }
}

//=====================================================================
//=== Flame Status Tracker (Task #70) ===
//=====================================================================
static uint32_t _flame_lastUpdateMs  = 0;
static uint16_t _flame_sampleCount   = 0;

static void satUpdateFlameStatus()
{
  uint32_t now = millis();
  // Update every 10 seconds
  if (_flame_lastUpdateMs != 0 && (now - _flame_lastUpdateMs) < 10000UL) return;
  _flame_lastUpdateMs = now;

  if (_flame_sampleCount < 6) {
    _flame_sampleCount++;
    state.sat.eFlameStatus = SAT_FS_INSUFFICIENT_DATA;
    return;
  }

  bool flame = (OTcurrentSystemState.Statusflags & 0x08) != 0;  // Bit 3 = flame

  if (!state.sat.bActive) {
    state.sat.eFlameStatus = SAT_FS_IDLE_OK;
    return;
  }

  // Safety tripped = flame not responding to commands
  if (state.sat.bSafetyTripped) {
    state.sat.eFlameStatus = SAT_FS_STUCK_OFF;
    return;
  }

  // Short cycling detection from last completed cycle
  if (state.sat.eLastCycleClass == SAT_CYCLE_SHORT) {
    state.sat.eFlameStatus = SAT_FS_SHORT_CYCLING;
    return;
  }

  // Stuck ON: flame on when setpoint is very low (below 5C means we want off)
  if (flame && state.sat.fFinalSetpoint < 5.0f) {
    state.sat.eFlameStatus = SAT_FS_STUCK_ON;
    return;
  }

  // Stuck OFF: requesting heat (setpoint > 30C) but no flame for extended time
  if (!flame && state.sat.fFinalSetpoint > 30.0f) {
    uint32_t flameOffMs = satCycleGetFlameOffStartMs();
    if (flameOffMs > 0 && (now - flameOffMs) > 300000UL) {  // 5 min no flame
      state.sat.eFlameStatus = SAT_FS_STUCK_OFF;
      return;
    }
  }

  // PWM short: in PWM mode with very short flame-on bursts
  if (state.sat.eControlMode == SAT_MODE_PWM && state.sat.fPwmDutyCycle < 0.05f && flame) {
    state.sat.eFlameStatus = SAT_FS_PWM_SHORT;
    return;
  }

  state.sat.eFlameStatus = SAT_FS_HEALTHY;
}

//=====================================================================
//=== Initialize SAT ===
//=====================================================================
void initSAT()
{
  satPidReset();
  satLoadPidState();         // Restore PID state from LittleFS after reset (Task #222)
  satLoadEnergyState();      // Restore energy total from LittleFS after reset (Task #196)
  satLoadEstimatedEnergy();  // Restore estimated gas energy from LittleFS (Task #232)
  satHCRLoadState();         // Restore daily-median recommendation ring from LittleFS (Task #228)
  satCycleInit();
  state.sat.bActive = false;
  state.sat.eControlMode = SAT_MODE_OFF;
  state.sat.bSafetyTripped = false;
  _sat_consecutiveSkips = 0;
  _sat_picFailCount = 0;
  _sat_bootCS0sent = false;

  // BOOT SAFETY: Release any stale PIC control setpoint override.
  // After a crash/reboot, the PIC may still hold the last CS= value.
  // Send CS=0 so the thermostat controls the boiler until SAT's first
  // control loop iteration computes a proper setpoint (~30s).
  if (hasOTCommandInterface()) {
    addCommandToQueue("CS=0", 4, false, 0);
    _sat_bootCS0sent = true;
    SATDebugTln(F("SAT: boot safety - sent CS=0 to release stale control override"));
  }
  // If no OT command interface is ready yet, _sat_bootCS0sent stays false and
  // the control loop will send CS=0 on its first call when one becomes available.

#if defined(ESP32)
  // Initialize BLE sensor scanning (Task #20)
  satBLEInit();
#endif

  // Sync timer to configured interval
  CHANGE_INTERVAL_SEC(timerSATControl, settings.sat.iControlInterval);

  if (settings.sat.bEnabled) {
    state.sat.eControlMode = SAT_MODE_CONTINUOUS;
    DebugTln(F("SAT: initialized and enabled (continuous mode)"));
    // Boot sequence: send priority messages PM=3,15,48 per SAT Python
    if (hasOTCommandInterface()) {
      addCommandToQueue("PM=3", 4, false, 0);
      addCommandToQueue("PM=15", 5, false, 0);
      addCommandToQueue("PM=48", 5, false, 0);
      // Manufacturer boot quirk: MI=500 for faster OT polling
      if (satGetManufacturerQuirks() & SAT_QUIRK_MI_500_BOOT) {
        addCommandToQueue("MI=500", 6, false, 0);
        SATDebugTln(F("SAT: MI=500 sent (manufacturer boot quirk)"));
      }
    }
  } else {
    DebugTln(F("SAT: initialized but disabled"));
  }
}

//=====================================================================
//=== Simulation Mode (Task #37) ===
//=====================================================================
static void satUpdateSimulation()
{
  if (!settings.sat.bSimulation) return;

  uint32_t now = millis();
  if (state.sat.iSimLastUpdateMs == 0) {
    // First call — initialize timestamp, skip delta calculation
    state.sat.iSimLastUpdateMs = now;
    state.sat.bSimWarmupDone = false;
    SATDebugTln(F("SAT SIM: simulation started"));
    return;
  }

  float dtSec = (float)(now - state.sat.iSimLastUpdateMs) / 1000.0f;
  if (dtSec <= 0.0f || dtSec > 60.0f) dtSec = 1.0f; // clamp sanity
  state.sat.iSimLastUpdateMs = now;

  float targetSetpoint = state.sat.fFinalSetpoint;
  bool  heating = (targetSetpoint > SAT_MIN_SETPOINT + 1.0f) && state.sat.bActive;

  // --- Flow temperature model ---
  if (heating) {
    // Warmup: first 5 minutes, flow ramps from current toward setpoint at reduced rate
    if (!state.sat.bSimWarmupDone) {
      // Check if we've been running for SAT_SIM_WARMUP_MS
      if (state.sat.fSimFlowTemp >= (targetSetpoint - 1.0f)) {
        state.sat.bSimWarmupDone = true;
      } else {
        // Ramp at half the normal rate during warmup
        float rampRate = SAT_SIM_FLOW_HEAT_RATE * 0.5f;
        state.sat.fSimFlowTemp += rampRate * dtSec;
        if (state.sat.fSimFlowTemp > targetSetpoint)
          state.sat.fSimFlowTemp = targetSetpoint;
      }
    } else {
      // Normal operation: flow tracks toward setpoint
      if (state.sat.fSimFlowTemp < targetSetpoint) {
        state.sat.fSimFlowTemp += SAT_SIM_FLOW_HEAT_RATE * dtSec;
        if (state.sat.fSimFlowTemp > targetSetpoint)
          state.sat.fSimFlowTemp = targetSetpoint;
      } else if (state.sat.fSimFlowTemp > targetSetpoint) {
        state.sat.fSimFlowTemp -= SAT_SIM_FLOW_COOL_RATE * dtSec;
        if (state.sat.fSimFlowTemp < targetSetpoint)
          state.sat.fSimFlowTemp = targetSetpoint;
      }
    }
  } else {
    // Not heating: flow cools toward room temp
    if (state.sat.fSimFlowTemp > state.sat.fSimRoomTemp) {
      state.sat.fSimFlowTemp -= SAT_SIM_FLOW_COOL_RATE * dtSec;
      if (state.sat.fSimFlowTemp < state.sat.fSimRoomTemp)
        state.sat.fSimFlowTemp = state.sat.fSimRoomTemp;
    }
    state.sat.bSimWarmupDone = false; // reset for next heating cycle
  }

  // --- Room temperature model ---
  float dtMin = dtSec / 60.0f;
  if (heating) {
    // Room rises toward target at configured rate
    float target = settings.sat.fTargetTemp;
    if (state.sat.fSimRoomTemp < target) {
      state.sat.fSimRoomTemp += settings.sat.fSimHeatRate * dtMin;
      if (state.sat.fSimRoomTemp > target)
        state.sat.fSimRoomTemp = target;
    }
  } else {
    // Room decays toward outdoor temp
    if (state.sat.fSimRoomTemp > state.sat.fSimOutdoorTemp) {
      state.sat.fSimRoomTemp -= settings.sat.fSimCoolRate * dtMin;
      if (state.sat.fSimRoomTemp < state.sat.fSimOutdoorTemp)
        state.sat.fSimRoomTemp = state.sat.fSimOutdoorTemp;
    }
  }
}

//=====================================================================
//=== Thermal Drop Learning (Task #21) ===
//=====================================================================
static void satUpdateThermalLearning()
{
  if (!settings.sat.bEnabled && !state.sat.bFallbackActive) return;

  uint32_t now = millis();
  bool flame = (OTcurrentSystemState.Statusflags & 0x08) != 0;
  float roomTemp = satGetRoomTemp();
  float outsideTemp = satGetOutsideTemp();
  bool roomValid = (roomTemp > -10.0f && roomTemp < 50.0f);
  bool outdoorValid = (outsideTemp > -40.0f && outsideTemp < 50.0f);

  // Always track last known good room temp for fallback estimation
  if (roomValid) {
    state.sat.fLastKnownRoom = roomTemp;
    state.sat.iLastKnownRoomMs = now;
  }

  // Initialize EMA from persisted coefficient on first call
  static bool _thermal_initialized = false;
  if (!_thermal_initialized) {
    _thermal_coeffEma = settings.sat.fThermalCoeff;
    _thermal_initialized = true;
  }

  // Learning: only when flame is OFF, room temp valid, outdoor temp valid
  // AC#6: Learning only runs when SAT has valid room temp AND outside temp simultaneously
  if (!flame && roomValid && outdoorValid) {
    float delta = roomTemp - outsideTemp;
    if (delta < SAT_THERMAL_MIN_DELTA) {
      // Indoor-outdoor delta too small for meaningful measurement
      _thermal_learning = false;
      return;
    }

    if (!_thermal_learning) {
      // Flame just turned off (or first valid reading) -- start learning period
      _thermal_learning = true;
      _thermal_prevRoom = roomTemp;
      _thermal_prevMs = now;
      return;
    }

    // AC#1: Track temperature drop rate during heating-off periods
    uint32_t elapsed = now - _thermal_prevMs;
    if (elapsed >= SAT_THERMAL_SAMPLE_MS) {
      float dtHours = (float)elapsed / 3600000.0f;
      float roomDrop = _thermal_prevRoom - roomTemp;

      // Only learn from actual temperature drops (building cooling down)
      if (roomDrop >= SAT_THERMAL_MIN_DROP) {
        float dropPerHour = roomDrop / dtHours;
        float sample = dropPerHour / delta;

        // Sanity check the sample
        if (sample >= SAT_THERMAL_COEFF_MIN && sample <= SAT_THERMAL_COEFF_MAX) {
          // AC#2: EMA update of thermal coefficient
          _thermal_coeffEma = SAT_THERMAL_EMA_ALPHA * sample + (1.0f - SAT_THERMAL_EMA_ALPHA) * _thermal_coeffEma;
          state.sat.fThermalDropRate = sample;

          // Track learning duration for validity (AC#7)
          _thermal_totalLearnMs += elapsed;
          if (_thermal_totalLearnMs >= SAT_THERMAL_VALID_MS) {
            state.sat.bThermalModelValid = true;
          }

          // AC#11: Save to settings periodically (every hour)
          if ((now - _thermal_lastSaveMs) >= SAT_THERMAL_SAVE_MS) {
            settings.sat.fThermalCoeff = _thermal_coeffEma;
            char coeffBuf[12];
            dtostrf(_thermal_coeffEma, 1, 4, coeffBuf);
            updateSetting("SATthermalcoeff", coeffBuf);
            _thermal_lastSaveMs = now;
            SATDebugTf(PSTR("SAT thermal: coeff updated %.4f\r\n"), _thermal_coeffEma);
          }
        }
      }

      _thermal_prevRoom = roomTemp;
      _thermal_prevMs = now;
    }
  } else {
    // Flame on or invalid readings -- stop learning
    _thermal_learning = false;
  }
}

// AC#3: Estimate room temperature during fallback when no valid sensor data
static float satEstimateRoomTemp(float outsideTemp)
{
  uint32_t now = millis();
  if (state.sat.iLastKnownRoomMs == 0) return 0.0f;  // No data at all

  float hoursElapsed = (float)(now - state.sat.iLastKnownRoomMs) / 3600000.0f;
  float lastRoom = state.sat.fLastKnownRoom;
  float coeff = _thermal_coeffEma;

  // estimated = lastKnown - coeff * (lastKnown - outdoor) * hoursElapsed
  float estimated = lastRoom - coeff * (lastRoom - outsideTemp) * hoursElapsed;

  // Clamp to reasonable range
  if (estimated < SAT_THERMAL_EST_MIN) estimated = SAT_THERMAL_EST_MIN;
  if (estimated > SAT_THERMAL_EST_MAX) estimated = SAT_THERMAL_EST_MAX;

  state.sat.fEstimatedRoom = estimated;
  return estimated;
}

//=====================================================================
//=== Solar Gain Compensation (Task #23) ===
//=====================================================================
static float    _solar_prevRoomTemp   = 0.0f;
static uint32_t _solar_prevMs         = 0;
static float    _solar_riseRateEma    = 0.0f;
static uint32_t _solar_conditionMs    = 0;  // How long condition has persisted
static bool     _solar_wasActive      = false; // Track previous state for hysteresis

static void satUpdateSolarGain()
{
  if (!settings.sat.bSolarGainEnable) {
    state.sat.bSolarGainActive = false;
    state.sat.fIndoorRiseRate = 0.0f;
    _solar_conditionMs = 0;
    _solar_wasActive = false;
    return;
  }

  float roomTemp = satGetRoomTemp();
  uint32_t now = millis();

  // Calculate rise rate (EMA smoothed), minimum 30s between samples
  if (_solar_prevMs > 0 && (now - _solar_prevMs) > 30000UL) {
    float dtHours = (float)(now - _solar_prevMs) / 3600000.0f;
    float rawRate = (roomTemp - _solar_prevRoomTemp) / dtHours;
    _solar_riseRateEma = 0.3f * rawRate + 0.7f * _solar_riseRateEma;
    _solar_prevRoomTemp = roomTemp;
    _solar_prevMs = now;
  } else if (_solar_prevMs == 0) {
    _solar_prevRoomTemp = roomTemp;
    _solar_prevMs = now;
  }

  state.sat.fIndoorRiseRate = _solar_riseRateEma;

  // Sun elevation gating (Task #68): if we have valid sun elevation, gate on minimum elevation
  if (state.sat.bSunElevationValid) {
    // Stale check: sun elevation older than 1 hour is invalid
    if ((now - state.sat.iSunElevLastMs) > 3600000UL) {
      state.sat.bSunElevationValid = false;
      SATDebugTln(F("SAT: sun elevation data stale, falling back to rise rate"));
    } else if (state.sat.fSunElevation < settings.sat.fSolarMinElevation) {
      // Sun too low, no solar gain regardless of rise rate
      state.sat.bSolarGainActive = false;
      _solar_wasActive = false;
      _solar_conditionMs = 0;
      return;
    }
  }
  // else: no sun elevation data, rely on rise rate alone (AC#6)

  // Detect solar gain condition: rising fast + low boiler modulation
  float modulation = OTcurrentSystemState.RelModLevel;
  bool risingFast = (_solar_riseRateEma > settings.sat.fSolarMinRiseRate);
  bool lowModulation = (modulation < 20.0f);  // 20% threshold matches Python SAT reference

  if (!_solar_wasActive) {
    // Not yet active: require sustained rising + low modulation for 10 min
    if (risingFast && lowModulation) {
      if (_solar_conditionMs == 0) _solar_conditionMs = now;
      if ((now - _solar_conditionMs) > 600000UL) { // 10 min sustained
        state.sat.bSolarGainActive = true;
        _solar_wasActive = true;
        _solar_conditionMs = 0;
        SATDebugTln(F("SAT: solar gain detected"));
      }
    } else {
      _solar_conditionMs = 0;
    }
  } else {
    // Currently active: require rise rate below threshold for 10 min to clear
    if (!risingFast) {
      if (_solar_conditionMs == 0) _solar_conditionMs = now;
      if ((now - _solar_conditionMs) > 600000UL) { // 10 min below threshold
        state.sat.bSolarGainActive = false;
        _solar_wasActive = false;
        _solar_conditionMs = 0;
        SATDebugTln(F("SAT: solar gain cleared"));
      }
    } else {
      _solar_conditionMs = 0; // Still rising, stay active
    }
  }
}

//=====================================================================
//=== Summer Simmer (Task #24) ===
//=====================================================================
static uint32_t _summer_lastCheckMs = 0;
static const uint32_t SUMMER_CHECK_INTERVAL_MS = 300000UL; // Check every 5 min

static void satUpdateSummerSimmer()
{
  if (!settings.sat.bSummerSimmer) {
    state.sat.bSummerActive = false;
    state.sat.fSummerHoursAbove = 0.0f;
    _summer_lastCheckMs = 0;
    return;
  }

  uint32_t now = millis();
  if (_summer_lastCheckMs == 0) {
    _summer_lastCheckMs = now;
    return;
  }
  if ((now - _summer_lastCheckMs) < SUMMER_CHECK_INTERVAL_MS) return;

  float dtHours = (float)(now - _summer_lastCheckMs) / 3600000.0f;
  _summer_lastCheckMs = now;

  float outsideTemp = satGetOutsideTemp();
  float hysteresis = 2.0f; // Re-enable threshold is threshold - 2C

  if (!state.sat.bSummerActive) {
    // Not yet in summer mode: accumulate time above threshold
    if (outsideTemp >= settings.sat.fSummerThreshold) {
      state.sat.fSummerHoursAbove += dtHours;
      if (state.sat.fSummerHoursAbove >= (float)settings.sat.iSummerMinHours) {
        state.sat.bSummerActive = true;
        SATDebugTf(PSTR("SAT: summer simmer activated (outdoor=%.1f >= %.1f for %.1fh)\r\n"),
                outsideTemp, settings.sat.fSummerThreshold, state.sat.fSummerHoursAbove);
      }
    } else {
      // Below threshold: reset accumulator
      state.sat.fSummerHoursAbove = 0.0f;
    }
  } else {
    // Currently in summer mode: decay when temp drops below threshold - hysteresis
    if (outsideTemp < (settings.sat.fSummerThreshold - hysteresis)) {
      state.sat.fSummerHoursAbove -= dtHours;
      if (state.sat.fSummerHoursAbove <= 0.0f) {
        state.sat.fSummerHoursAbove = 0.0f;
        state.sat.bSummerActive = false;
        SATDebugTf(PSTR("SAT: summer simmer deactivated (outdoor=%.1f < %.1f)\r\n"),
                outsideTemp, settings.sat.fSummerThreshold - hysteresis);
      }
    } else {
      // Still warm enough: keep hours at max so deactivation requires sustained cold
      if (state.sat.fSummerHoursAbove < (float)settings.sat.iSummerMinHours)
        state.sat.fSummerHoursAbove = (float)settings.sat.iSummerMinHours;
    }
  }
}

//=====================================================================
//=== PID Auto-Tuning (Task #27) ===
//=====================================================================
// Performance-based PID gain self-tuning. Monitors overshoot/undershoot/
// oscillation over time and applies small conservative adjustments once
// per hour (with at least 6 heating cycles of data).

// Gain clamp ranges
static const float AT_KP_MIN = 0.5f;
static const float AT_KP_MAX = 50.0f;
static const float AT_KI_MIN = 0.0001f;
static const float AT_KI_MAX = 0.01f;
static const float AT_KD_MIN = 0.0f;
static const float AT_KD_MAX = 500.0f;

// Tracking state (static, never persisted)
static uint16_t _at_overshootCount    = 0;
static uint16_t _at_undershootCount   = 0;
static uint16_t _at_oscillationCount  = 0;
static float    _at_convergenceSum    = 0.0f;  // sum of convergence times (minutes)
static uint16_t _at_convergenceN      = 0;     // number of convergence samples
static uint32_t _at_lastTuneMs        = 0;
static uint32_t _at_cyclesSinceTune   = 0;
static bool     _at_prevOvershoot     = false;  // for oscillation detection
static const uint32_t AT_TUNE_INTERVAL_MS = 3600000UL; // 1 hour
static const uint8_t  AT_MIN_CYCLES       = 6;

static void satAutoTuneUpdate()
{
  if (!settings.sat.bAutoTune || !state.sat.bActive) {
    state.sat.bAutoTuneActive = false;
    return;
  }
  state.sat.bAutoTuneActive = true;

  // --- Accumulate per-cycle metrics from cycle tracker ---
  // Check if a new cycle just completed (cycle count changed)
  static uint32_t _at_lastCycleCount = 0;
  if (state.sat.iCycleCount > _at_lastCycleCount) {
    _at_lastCycleCount = state.sat.iCycleCount;
    _at_cyclesSinceTune++;

    // Classify the completed cycle
    bool isOvershoot = (state.sat.eLastCycleClass == SAT_CYCLE_OVERSHOOT);
    bool isUnderheat = (state.sat.eLastCycleClass == SAT_CYCLE_UNDERHEAT);

    if (isOvershoot) _at_overshootCount++;
    if (isUnderheat) _at_undershootCount++;

    // Oscillation: alternating overshoot/undershoot
    if (isOvershoot && !_at_prevOvershoot && _at_undershootCount > 0) _at_oscillationCount++;
    if (isUnderheat && _at_prevOvershoot && _at_overshootCount > 0) _at_oscillationCount++;
    _at_prevOvershoot = isOvershoot;

    // Track convergence time from cycle overshoot seconds (proxy for settling)
    if (state.sat.fCycleOvershootSec > 0.0f) {
      _at_convergenceSum += state.sat.fCycleOvershootSec / 60.0f;
      _at_convergenceN++;
    }
  }

  state.sat.iAutoTuneCycles = _at_cyclesSinceTune;

  // --- Check if it is time to tune ---
  uint32_t now = millis();
  if (_at_lastTuneMs == 0) _at_lastTuneMs = now;

  if ((now - _at_lastTuneMs) < AT_TUNE_INTERVAL_MS) return;
  if (_at_cyclesSinceTune < AT_MIN_CYCLES) return;

  // --- Analyze and adjust ---
  float rate = settings.sat.fAutoTuneRate;
  uint16_t totalCycles = _at_overshootCount + _at_undershootCount;
  if (totalCycles == 0) totalCycles = 1;  // avoid divide-by-zero

  // Score: positive = overshoot dominant, negative = undershoot dominant
  float score = (float)((int16_t)_at_overshootCount - (int16_t)_at_undershootCount) / (float)totalCycles;
  state.sat.fAutoTuneScore = score;

  float avgConvergence = (_at_convergenceN > 0) ? (_at_convergenceSum / (float)_at_convergenceN) : 0.0f;

  // Read current gains from PID auto-gain calculation (state.sat.fKp/fKi/fKd)
  // We adjust the heating curve coefficient which drives gain calculation
  float coeff = settings.sat.fHeatingCurveCoeff;
  bool adjusted = false;

  // Oscillation dominates: strong damping needed
  if (_at_oscillationCount > _at_cyclesSinceTune / 3) {
    coeff *= (1.0f - rate * 2.0f);
    adjusted = true;
    SATDebugTf(PSTR("SAT AutoTune: oscillation detected (%u/%lu), reducing coeff\r\n"),
            _at_oscillationCount, (unsigned long)_at_cyclesSinceTune);
  }
  // Overshoot dominant: reduce aggression
  else if (score > 0.3f) {
    coeff *= (1.0f - rate);
    adjusted = true;
    SATDebugTf(PSTR("SAT AutoTune: overshoot dominant (score=%.2f), reducing coeff\r\n"), score);
  }
  // Undershoot dominant: increase aggression
  else if (score < -0.3f) {
    coeff *= (1.0f + rate);
    adjusted = true;
    SATDebugTf(PSTR("SAT AutoTune: undershoot dominant (score=%.2f), increasing coeff\r\n"), score);
  }

  // Slow convergence: slightly increase coefficient (faster response)
  if (avgConvergence > 30.0f && !adjusted) {
    coeff *= (1.0f + rate * 0.5f);
    adjusted = true;
    SATDebugTf(PSTR("SAT AutoTune: slow convergence (%.1f min), increasing coeff\r\n"), avgConvergence);
  }

  // Clamp coefficient to reasonable range
  if (coeff < 0.3f) coeff = 0.3f;
  if (coeff > 5.0f) coeff = 5.0f;

  if (adjusted) {
    settings.sat.fHeatingCurveCoeff = coeff;
    SATDebugTf(PSTR("SAT AutoTune: new coefficient=%.2f (cycles=%lu, os=%u, us=%u, osc=%u)\r\n"),
            coeff, (unsigned long)_at_cyclesSinceTune,
            _at_overshootCount, _at_undershootCount, _at_oscillationCount);

    // Persist updated coefficient
    writeSettings(false);
  }

  // Reset counters for next tuning window
  _at_overshootCount   = 0;
  _at_undershootCount  = 0;
  _at_oscillationCount = 0;
  _at_convergenceSum   = 0.0f;
  _at_convergenceN     = 0;
  _at_cyclesSinceTune  = 0;
  _at_lastTuneMs       = now;
}

//=====================================================================
//=== Main Control Loop (called from doBackgroundTasks) ===
//=====================================================================
void satControlLoop()
{
  // Deferred PID state restore (Task #222): satLoadPidState() is called at initSAT()
  // but NTP may not yet be synced at that point, so the restore is skipped there.
  // Retry here once after NTP becomes available so freshness can be verified.
  static bool _pidStateRestoreAttempted = false;
  if (!_pidStateRestoreAttempted && isNTPtimeSet()) {
    _pidStateRestoreAttempted = true;
    satLoadPidState();
  }

  // --- Fallback detection (Task #19): auto-enable SAT when external control lost ---
  // Runs BEFORE the bEnabled gate because it is the entry path that flips
  // bEnabled from false to true when the boiler controller loses MQTT.
  if (!settings.sat.bEnabled && !state.sat.bFallbackActive) {
    // Only fall back if MQTT is enabled AND was previously connected but has now been
    // lost for >5 minutes. iLastConnectedMs == 0 means never connected (fresh boot or
    // MQTT not configured) — do not treat that as a loss.
    bool mqttLost = settings.mqtt.bEnable &&
                    state.mqtt.iLastConnectedMs > 0 &&
                    !state.mqtt.bConnected &&
                    (millis() - state.mqtt.iLastConnectedMs > 300000UL); // 5 min MQTT loss
    if (mqttLost) {
      state.sat.bFallbackActive = true;
      state.sat.eFallbackReason = SAT_FB_MQTT_LOST;
      settings.sat.bEnabled = true; // Temporarily enable SAT
      DebugTln(F("SAT FALLBACK: MQTT lost >5min, auto-enabling SAT"));
    }
  }
  // Check if fallback should be lifted
  if (state.sat.bFallbackActive && state.mqtt.bConnected) {
    state.sat.bFallbackActive = false;
    state.sat.eFallbackReason = SAT_FB_NONE;
    settings.sat.bEnabled = false; // Restore disabled state
    DebugTln(F("SAT FALLBACK: connectivity restored, disabling fallback"));
    satDisable();
    return;
  }

  // Early bail when SAT is disabled or a flash update is in progress.
  // Simulation and thermal-learning are SAT-internal (their consumers all
  // live in SAT code paths) and only run past this gate. PERF-L2 2026-04-18.
  if (!settings.sat.bEnabled || isFlashing()) {
    if (state.sat.bActive) {
      satDisable();
    }
    return;
  }

  // --- Simulation update (Task #37): model thermal behavior before PID ---
  satUpdateSimulation();
  // --- Thermal drop learning (Task #21): learn building thermal decay rate ---
  satUpdateThermalLearning();

  // --- OPV calibration state machine (ADR-076): poll while non-IDLE ---
  if (state.sat.eCalibPhase != SAT_CALIB_IDLE) satOvpCalibrate();

  // If safety tripped, stay disabled until explicitly re-enabled
  if (state.sat.bSafetyTripped) return;

  // Boot safety deferred: if CS=0 wasn't sent during initSAT(), send it on the
  // first call where an OT command interface is available.
  if (!_sat_bootCS0sent && hasOTCommandInterface()) {
    addCommandToQueue("CS=0", 4, false, 0);
    _sat_bootCS0sent = true;
    SATDebugTln(F("SAT: deferred boot safety — sent CS=0"));
  }

  // Flame edge detection — always process, independent of timer
  bool currentFlame = (OTcurrentSystemState.Statusflags & 0x08) != 0;
  if (currentFlame != _sat_prevFlameState) {
    satCycleOnFlameChange(currentFlame);
    _sat_prevFlameState = currentFlame;
  }

  // Sample cycle data frequently (every loop call)
  satCycleSample();

  // Update rolling 4-hour window statistics once per minute (Task #227)
  if (DUE(timerSAT4hStats)) {
    satGetWindow4hStats();
  }

  // Main control loop on timer
  if (!DUE(timerSATControl)) return;

  state.sat.bActive = true;
  if (state.sat.eControlMode == SAT_MODE_OFF) {
    state.sat.eControlMode = SAT_MODE_CONTINUOUS;
  }

  // --- DHW detection (Task #3): skip CH control when DHW is active ---
  state.sat.bDhwActive = (OTcurrentSystemState.SlaveStatus & 0x04) != 0; // Bit 2 = DHW active
  if (state.sat.bDhwActive) {
    // DHW has priority - don't adjust CH setpoint, boiler manages itself.
    // TASK-437: On transition into DHW mode, send SW= (DHW setpoint, MsgID 56).
    // Was TW=, which is not a PIC command (gateway.asm) and is not implemented
    // by OTDirect, so the command was rejected as NG on every DHW transition.
    if (!_sat_prevDhwActive && settings.sat.bDhwEnabled &&
        settings.sat.fDhwSetpoint >= 30.0f && settings.sat.fDhwSetpoint <= 70.0f &&
        hasOTCommandInterface()) {
      char swCmd[16];
      snprintf_P(swCmd, sizeof(swCmd), PSTR("SW=%d"), (int)settings.sat.fDhwSetpoint);
      addCommandToQueue(swCmd, strlen(swCmd), false, 0);
      SATDebugTf(PSTR("SAT: DHW active, sent %s\r\n"), swCmd);
    }
    _sat_prevDhwActive = true;
    return;
  }
  _sat_prevDhwActive = false;

  // --- Read inputs (staleness is checked inside these functions) ---
  float roomTemp = satGetRoomTemp();
  float outsideTemp = satGetOutsideTemp();
  float targetTemp = settings.sat.fTargetTemp;

  SATDebugTf(PSTR("SAT loop: room=%.1f target=%.1f mode=%d enabled=%d\r\n"),
             roomTemp, targetTemp, (int)state.sat.eControlMode, (int)settings.sat.bEnabled);

  // Validate room temp — count consecutive failures
  if (roomTemp < -10.0f || roomTemp > 50.0f) {
    _sat_consecutiveSkips++;
    if (_sat_consecutiveSkips >= SAT_MAX_SKIP_COUNT) {
      DebugTln(F("SAT SAFETY: too many invalid room temp readings, disabling"));
      state.sat.bSafetyTripped = true;
      satDisable();
    }
    return;
  }
  _sat_consecutiveSkips = 0;  // Valid reading -- reset counter

  // Task #38: OT error flag monitoring -- check for critical boiler faults
  if (OTcurrentSystemState.SlaveStatus & 0x01) { // Bit 0 = fault indication
    SATDebugTln(F("SAT: boiler fault flag detected, skipping control cycle"));
    return; // Skip this control cycle, let boiler handle the fault
  }

  if (outsideTemp < -40.0f || outsideTemp > 50.0f) {
    outsideTemp = 10.0f;  // Safe fallback
  }

  // --- Task #21: Thermal estimation fallback ---
  // AC#5: After 2+ hours of estimation without real data, switch to fixed safe setpoint
  float savedDeadband = settings.sat.fDeadband;
  bool thermalDeadbandWidened = false;
  if (state.sat.bFallbackActive && state.sat.iLastKnownRoomMs > 0) {
    uint32_t estElapsed = millis() - state.sat.iLastKnownRoomMs;
    if (estElapsed >= SAT_THERMAL_MAX_EST_MS) {
      // Too long without real data -- use fixed safe flow setpoint
      state.sat.fFinalSetpoint = SAT_THERMAL_SAFE_FLOW;
      float sysMax = satGetMaxSetpoint();
      if (state.sat.fFinalSetpoint > sysMax) state.sat.fFinalSetpoint = sysMax;
      if (hasOTCommandInterface()) {
        char cmd[16];
        snprintf_P(cmd, sizeof(cmd), PSTR("CS=%d"), (int)state.sat.fFinalSetpoint);
        addCommandToQueue(cmd, strlen(cmd), false, 0);
        snprintf_P(cmd, sizeof(cmd), PSTR("MM=%u"), settings.sat.iMaxRelModulation);
        addCommandToQueue(cmd, strlen(cmd), false, 0);
        addCommandToQueue("CH=1", 4, false, 0);
      }
      satPublishMQTT();
      return;
    }
    // AC#4: Widen deadband proportionally to estimation time (uncertainty grows)
    // Add 0.5C per hour of estimation on top of normal deadband
    float estHours = (float)estElapsed / 3600000.0f;
    settings.sat.fDeadband += 0.5f * estHours;
    thermalDeadbandWidened = true;
  }

  // --- Window detection timer check ---
  _satCheckWindowTimer();

  // --- Pressure monitoring ---
  satUpdatePressure();
  satPressureHealthUpdate();  // Task #226: update fBoilerPressure + sPressureStatus

  // --- Solar gain compensation (Task #23) ---
  satUpdateSolarGain();

  // --- Summer simmer (Task #24): skip heating if summer mode active ---
  satUpdateSummerSimmer();
  if (state.sat.bSummerActive && settings.sat.bSummerSimmer) {
    state.sat.fFinalSetpoint = SAT_MIN_SETPOINT;
    if (hasOTCommandInterface()) {
      addCommandToQueue("CS=10", 5, false, 0);
      char mmBuf[8];
      snprintf_P(mmBuf, sizeof(mmBuf), PSTR("MM=%u"), settings.sat.iMaxRelModulation);
      addCommandToQueue(mmBuf, strlen(mmBuf), false, 0);
      addCommandToQueue("CH=0", 4, false, 0);
    }
    return; // Skip rest of control loop
  }

  // --- TRV valve detection (Task #29): skip heating when all valves closed ---
  if (!state.sat.bValvesOpen) {
    state.sat.fFinalSetpoint = SAT_MIN_SETPOINT;
    if (hasOTCommandInterface()) {
      addCommandToQueue("CS=10", 5, false, 0);
      char mmBuf[8];
      snprintf_P(mmBuf, sizeof(mmBuf), PSTR("MM=%u"), settings.sat.iMaxRelModulation);
      addCommandToQueue(mmBuf, strlen(mmBuf), false, 0);
      addCommandToQueue("CH=0", 4, false, 0);
    }
    // Don't update PID integral or record error statistics
    satPublishMQTT();
    return;
  }

  // --- Power & energy tracking (Task #45) ---
  satUpdatePowerEnergy();

  // --- PID auto-tuning (Task #27) ---
  satAutoTuneUpdate();

  // --- Heating curve recommendation ---
  satUpdateCurveRecommendation();

  // --- Daily median recommendation (Task #228): collect intra-day sample ---
  satHCRAddSample();

  // --- Modulation reliability (skip if manufacturer doesn't support relative modulation) ---
  if (!(satGetManufacturerQuirks() & SAT_QUIRK_NO_REL_MOD))
    satUpdateModulationReliability();
  else
    state.sat.bModulationReliable = false; // Mark as unreliable so it's not used for decisions

  // --- OT setpoint sync ---
  satCheckSetpointSync();

  // --- Flame status (Task #70) ---
  satUpdateFlameStatus();

  // --- Update boiler status ---
  satUpdateBoilerStatus();

  // --- Thermal comfort adjustment (Task #28/#47) ---
  // Weather humidity fallback: if no direct humidity input, use weather API
  if (!state.sat.bHumidityValid && state.sat.weather.bValid) {
    state.sat.fHumidity = state.sat.weather.fHumidity;
    state.sat.bHumidityValid = true;
    state.sat.iHumidityLastMs = state.sat.weather.iLastUpdateMs;
  }
  satUpdateComfort();
  float effectiveTarget = targetTemp + state.sat.fComfortOffset;

  // --- Calculate heating curve ---
  float curveValue = satCalcHeatingCurve(effectiveTarget, outsideTemp);

  // --- Calculate PID output (includes heating curve value) ---
  float pidOutput = satPidUpdate(roomTemp, effectiveTarget, curveValue, OTcurrentSystemState.Tboiler);

  // --- Multi-zone PID override (Task #233) ---
  // When sat_zone_count > 1: run each zone's PID and use the most-demanding setpoint.
  // Zone 1 is always computed; its output replaces the primary PID output.
  // Zones 2-4 are only evaluated when sat_zone_count > 1.
  // If all zones are inactive, falls back to single-zone (primary) pidOutput.
  if (settings.sat.iZoneCount > 1) {
    float zoneMax = SAT_MIN_SETPOINT;
    uint8_t activeZones = 0;
    uint8_t zoneCount = settings.sat.iZoneCount;
    if (zoneCount > SAT_MAX_ZONES) zoneCount = SAT_MAX_ZONES;
    for (uint8_t z = 0; z < zoneCount; z++) {
      float zOut = satZonePidStep(z, outsideTemp);
      if (zOut > SAT_MIN_SETPOINT) {
        if (zOut > zoneMax) zoneMax = zOut;
        activeZones++;
      }
    }
    if (activeZones > 0) {
      pidOutput = zoneMax;
      SATDebugTf(PSTR("SAT: multi-zone %u active zones, max setpoint=%.1f\r\n"), activeZones, pidOutput);
    }
    // Publish zone diagnostics (retained MQTT)
    satPublishZoneDiagnostics();
  }

  // Task #21: Restore original deadband after PID used the widened value
  if (thermalDeadbandWidened) {
    settings.sat.fDeadband = savedDeadband;
  }

  // --- Clamp to valid range ---
  float maxSetpoint = OTcurrentSystemState.MaxTSet;
  if (maxSetpoint < 30.0f) maxSetpoint = SAT_MAX_SETPOINT_DEFAULT;

  // Hard safety ceiling based on heating system type -- never exceeded
  float sysMax = satGetMaxSetpoint();
  float hardMax = (satGetEffectiveHeatingSystem() == SAT_HSYS_UNDERFLOOR) ? SAT_HARD_MAX_FLOOR : SAT_HARD_MAX_RAD;
  if (maxSetpoint > sysMax) maxSetpoint = sysMax;
  if (maxSetpoint > hardMax) maxSetpoint = hardMax;

  // Global safety cap (Python MAXIMUM_SETPOINT = 65C): applies to ALL heating systems.
  // settings.sat.fMaxSetpoint defaults to 65C and is the universal ceiling before system limits.
  float globalMax = settings.sat.fMaxSetpoint;
  if (globalMax < 30.0f || globalMax > SAT_HARD_MAX_RAD) globalMax = SAT_GLOBAL_MAX_SETPOINT; // sanity
  if (maxSetpoint > globalMax) maxSetpoint = globalMax;

  if (pidOutput < SAT_MIN_SETPOINT) pidOutput = SAT_MIN_SETPOINT;
  if (pidOutput > maxSetpoint) pidOutput = maxSetpoint;

  // AC#2: When PID output reaches the system maximum setpoint, use continuous mode
  // instead of PWM — matches Python behavior where requested_setpoint >= maximum_setpoint
  // disables PWM (full power needed, no duty cycling required).
  if (pidOutput >= sysMax && state.sat.eControlMode == SAT_MODE_PWM) {
    state.sat.eControlMode = SAT_MODE_CONTINUOUS;
    SATDebugTln(F("SAT: PID at system max, switching to continuous mode"));
  }

  // --- Force PWM if configured (Task #41) ---
  if (settings.sat.bForcePWM && state.sat.eControlMode != SAT_MODE_PWM) {
    state.sat.eControlMode = SAT_MODE_PWM;
  }

  // --- Apply control mode ---
  float finalSetpoint;
  if (state.sat.eControlMode == SAT_MODE_PWM) {
    finalSetpoint = satApplyPWM(pidOutput);
  } else {
    finalSetpoint = satApplyContinuous(pidOutput);
  }

  // Final clamp (including hard ceiling)
  if (finalSetpoint < SAT_MIN_SETPOINT) finalSetpoint = SAT_MIN_SETPOINT;
  if (finalSetpoint > maxSetpoint) finalSetpoint = maxSetpoint;

  // --- Solar gain compensation: reduce setpoint (Task #23) ---
  if (state.sat.bSolarGainActive && settings.sat.bSolarGainEnable) {
    finalSetpoint -= settings.sat.fSolarSetpointOffset;
    if (finalSetpoint < SAT_MIN_SETPOINT) finalSetpoint = SAT_MIN_SETPOINT;
  }
  state.sat.fFinalSetpoint = finalSetpoint;

  // --- Auto-switch between continuous and PWM modes (Tasks #42/#43) ---
  // Delegated to satCycleCheckAutoSwitch() in SATcycles.ino which uses correct
  // thresholds (3.0C overshoot margin, 60s sustain, 300s DHW post-overshoot guard).
  if (!satAlwaysMaxModulation()) {
    satCycleCheckAutoSwitch();
  }

  // Modulation suppression is handled via CS sequence in satApplyPWM() (PWM ON only)
  state.sat.bModSuppressed = false;
  state.sat.iModSuppressionSinceMs = 0;

  // --- Compute modulation value based on mode, heating system, suppression, and quirks ---
  {
    uint8_t mmValue;
    uint8_t quirks = satGetManufacturerQuirks();
    if (satAlwaysMaxModulation()) {
      mmValue = 100;
    } else if (state.sat.bModSuppressed) {
      mmValue = 0;
    } else if (state.sat.eControlMode == SAT_MODE_PWM) {
      // ON phase: suppress modulation (MM=0); OFF phase: send floor so boiler behaves correctly
      mmValue = state.sat.bPwmFlameRequested ? 0 : settings.sat.iMaxRelModulation;
    } else {
      mmValue = settings.sat.iMaxRelModulation;
    }
    // Geminox quirk: minimum modulation 10% (never send MM=0 when flame requested)
    if ((quirks & SAT_QUIRK_MIN_MOD_10) && mmValue > 0 && mmValue < 10) {
      mmValue = 10;
    }
    // Immergas quirk: cap modulation at 80%
    if ((quirks & SAT_QUIRK_IMMERGAS_TP) && mmValue > 80) {
      mmValue = 80;
    }
    state.sat.iCurrentModulation = mmValue;
  }

  // --- Flame-off setpoint offset (anti-cycling hysteresis, Task #32) ---
  if (settings.sat.fFlameOffOffset > 0.001f) {
    bool flame = (OTcurrentSystemState.Statusflags & 0x08) != 0;
    if (!flame) {
      finalSetpoint += settings.sat.fFlameOffOffset;
      if (finalSetpoint > maxSetpoint) finalSetpoint = maxSetpoint;
      state.sat.fFinalSetpoint = finalSetpoint;
    }
  }

  // --- Send CS= and MM= commands to boiler when an OT command interface is available ---
  if (hasOTCommandInterface()) {
    char cmdBuf[16];
    snprintf_P(cmdBuf, sizeof(cmdBuf), PSTR("CS=%.1f"), finalSetpoint);
    addCommandToQueue(cmdBuf, strlen(cmdBuf), false, 0);
    // Send MM= (max relative modulation) alongside CS=
    snprintf_P(cmdBuf, sizeof(cmdBuf), PSTR("MM=%u"), state.sat.iCurrentModulation);
    addCommandToQueue(cmdBuf, strlen(cmdBuf), false, 0);
    // Send CH= (central heating enable/disable) every cycle
    bool wantHeating = (finalSetpoint > SAT_MIN_SETPOINT);
    addCommandToQueue(wantHeating ? "CH=1" : "CH=0", 4, false, 0);
    // Immergas quirk: send TP=11:12=<setpoint> alongside MM=
    if (satGetManufacturerQuirks() & SAT_QUIRK_IMMERGAS_TP) {
      snprintf_P(cmdBuf, sizeof(cmdBuf), PSTR("TP=11:12=%.0f"), finalSetpoint);
      addCommandToQueue(cmdBuf, strlen(cmdBuf), false, 0);
    }
    // Push SAT target to thermostat display via TC= command (Task #31)
    if (settings.sat.bPushSetpoint) {
      snprintf_P(cmdBuf, sizeof(cmdBuf), PSTR("TC=%.1f"), settings.sat.fTargetTemp);
      addCommandToQueue(cmdBuf, strlen(cmdBuf), false, 0);
    }
    _sat_picFailCount = 0;
  } else {
    _sat_picFailCount++;
    if (_sat_picFailCount >= SAT_MAX_PIC_FAILS) {
      DebugTln(F("SAT SAFETY: PIC unavailable for too long, disabling"));
      state.sat.bSafetyTripped = true;
      satDisable();
      return;
    }
  }

  state.sat.iLastControlMs = millis();

  // Periodically save PID state to LittleFS (every 5 min)
  if ((millis() - _pidLastSaveMs) >= 300000UL) {
    satSavePidState();
  }
  // Periodically save energy total to LittleFS (every hour, Task #196)
  if ((millis() - _energyLastSaveMs) >= SAT_ENERGY_SAVE_INTERVAL_MS) {
    satSaveEnergyState();
  }
  // Save estimated gas energy on every 0.1 kWh boundary crossed (Task #232)
  if (settings.sat.fBoilerRatedKW > 0.0f &&
      floorf(state.sat.fEnergyEstimatedKWh * 10.0f) != floorf(state.sat.fEstEnergyLastSavedKWh * 10.0f)) {
    satSaveEstimatedEnergy();
  }

  SATDebugTf(PSTR("SAT: room=%.1f target=%.1f outside=%.1f curve=%.1f pid=%.1f final=%.1f mode=%d\r\n"),
          roomTemp, targetTemp, outsideTemp, curveValue, pidOutput, finalSetpoint,
          (int)state.sat.eControlMode);

  // --- Publish to MQTT ---
  satPublishMQTT();
}

