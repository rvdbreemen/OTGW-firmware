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

// --- SAT test-observability narration (TASK-815) ---
// Emits one plain-language line to BOTH sinks so Telnet and the Web UI live-log
// never drift. Always-on (NOT gated by state.debug.bSAT) but transition-only at
// the call sites, so volume stays low. Web UI lines carry prefix 'S'.
// sendEventToWebSocket() is a file-static in OTGW-Core.ino, visible across the TU.
#define SAT_NARRATE_BUF 96
void satNarrate_P(PGM_P msg_P) {
  if (!msg_P) return;
  char buf[SAT_NARRATE_BUF];
  snprintf_P(buf, sizeof(buf), PSTR("%s"), msg_P);
  DebugTf(PSTR("SAT: %s\r\n"), buf);          // Telnet sink
  sendEventToWebSocket('S', buf);             // Web UI live-log sink (prefix 'S')
}
void satNarratef_P(PGM_P fmt_P, ...) {
  if (!fmt_P) return;
  char buf[SAT_NARRATE_BUF];
  va_list args;
  va_start(args, fmt_P);
  vsnprintf_P(buf, sizeof(buf), fmt_P, args);
  va_end(args);
  DebugTf(PSTR("SAT: %s\r\n"), buf);          // Telnet sink
  sendEventToWebSocket('S', buf);             // Web UI live-log sink (prefix 'S')
}

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

// --- Synthetic boiler-model constants (TASK-795, plan §5.2-5.5) ---
static const float    SAT_SIM_FLAME_HYST_LO     = 0.5f;    // °C below setpoint to (re)ignite
static const uint32_t SAT_SIM_MIN_OFF_MS        = 60000UL; // anti-cycle floor between flame cycles
static const float    SAT_SIM_MOD_KP            = 20.0f;   // synthetic modulation %% per °C of error
static const float    SAT_SIM_FLOW_LOSS_K       = 0.02f;   // linear flow heat-loss coefficient
static const float    SAT_SIM_FLOW_THERMAL_GAIN = 0.4f;    // °C/s per kW net (tuned ~2°C/s at full mod, 24 kW)

// --- Synthetic diurnal outdoor-temperature model (TASK-796 / plan §12 F1) ---
// Triangle wave (no transcendental — cheap, and the params read directly):
// coldest at SAT_SIM_OUTDOOR_MIN_HOUR, warmest 12 h later, swinging
// MEAN ± AMPLITUDE. When the weather feed is valid it wins over the synthetic
// curve (mirrors the real satGetOutsideTemp() precedence).
static const float   SAT_SIM_OUTDOOR_MEAN      = 8.0f;   // °C, daily mean
static const float   SAT_SIM_OUTDOOR_AMPLITUDE = 6.0f;   // °C, peak deviation (2..14 °C swing)
static const uint8_t SAT_SIM_OUTDOOR_MIN_HOUR  = 5;      // hour-of-day of the coldest point (~05:00)

// --- Synthetic DHW-draw schedule (TASK-800 / plan §12 F5) ---
static const uint32_t SAT_SIM_DHW_PERIOD_MS = 1800000UL; // 30 min between light scheduled draws
static const uint32_t SAT_SIM_DHW_DRAW_MS   = 120000UL;  // 2 min draw length (default; F2 event can override)

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
static bool    _sat_prevOTOnline      = false;  // Track OT-bus online edge for cache reset (TASK-565)

// --- Write-on-change refresh window for SAT-emitted OT commands (TASK-565) ---
// Even when CS/MM/CH/TC values are unchanged, re-emit at this cadence so a
// rebooted PIC or a boiler whose internal state has drifted from our last
// command resyncs to the current SAT intent. 5 min keeps tester-visible
// latency on PIC reboot bounded while cutting steady-state OT traffic ~20x.
static const uint32_t SAT_CMD_REFRESH_MS = 300000UL; // 5 min

// Reset the SAT last-sent OT command cache so the next call to
// satEnqueueIfChanged*() always enqueues. Used on offline->online edges,
// safety/disable paths, and at boot.
static inline void satResetCmdCache() {
  state.sat.bLastSentValid = false;
  state.sat.iLastSentCSMs  = 0;
  state.sat.iLastSentMMMs  = 0;
  state.sat.iLastSentCHMs  = 0;
  state.sat.iLastSentTCMs  = 0;
}

// Enqueue CS=<setpoint> only if value changed (rounded to 0.1) or refresh
// window elapsed. Returns true when an enqueue happened.
static bool satEnqueueIfChangedCS(float setpoint) {
  uint32_t now = millis();
  int16_t newQ = (int16_t)lroundf(setpoint * 10.0f);
  int16_t oldQ = (int16_t)lroundf(state.sat.fLastSentCS * 10.0f);
  bool stale = (now - state.sat.iLastSentCSMs) >= SAT_CMD_REFRESH_MS;
  if (state.sat.bLastSentValid && newQ == oldQ && !stale) return false;
  char buf[16];
  snprintf_P(buf, sizeof(buf), PSTR("CS=%.1f"), setpoint);
  addCommandToQueue(buf, strlen(buf), false, 0);
  state.sat.fLastSentCS     = (float)newQ / 10.0f;
  state.sat.iLastSentCSMs   = now;
  state.sat.bLastSentValid  = true;
  return true;
}

static bool satEnqueueIfChangedMM(uint8_t mm) {
  uint32_t now = millis();
  bool stale = (now - state.sat.iLastSentMMMs) >= SAT_CMD_REFRESH_MS;
  if (state.sat.bLastSentValid && mm == state.sat.iLastSentMM && !stale) return false;
  char buf[8];
  snprintf_P(buf, sizeof(buf), PSTR("MM=%u"), mm);
  addCommandToQueue(buf, strlen(buf), false, 0);
  state.sat.iLastSentMM     = mm;
  state.sat.iLastSentMMMs   = now;
  state.sat.bLastSentValid  = true;
  return true;
}

// ch is 0 or 1 (CH=0 / CH=1). Both string forms are 4 chars.
static bool satEnqueueIfChangedCH(uint8_t ch) {
  uint32_t now = millis();
  bool stale = (now - state.sat.iLastSentCHMs) >= SAT_CMD_REFRESH_MS;
  if (state.sat.bLastSentValid && ch == state.sat.iLastSentCH && !stale) return false;
  addCommandToQueue(ch ? "CH=1" : "CH=0", 4, false, 0);
  state.sat.iLastSentCH     = ch;
  state.sat.iLastSentCHMs   = now;
  state.sat.bLastSentValid  = true;
  return true;
}

static bool satEnqueueIfChangedTC(float tc) {
  uint32_t now = millis();
  int16_t newQ = (int16_t)lroundf(tc * 10.0f);
  int16_t oldQ = (int16_t)lroundf(state.sat.fLastSentTC * 10.0f);
  bool stale = (now - state.sat.iLastSentTCMs) >= SAT_CMD_REFRESH_MS;
  if (state.sat.bLastSentValid && newQ == oldQ && !stale) return false;
  char buf[16];
  snprintf_P(buf, sizeof(buf), PSTR("TC=%.1f"), tc);
  addCommandToQueue(buf, strlen(buf), false, 0);
  state.sat.fLastSentTC     = (float)newQ / 10.0f;
  state.sat.iLastSentTCMs   = now;
  state.sat.bLastSentValid  = true;
  return true;
}

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
  float boilerTemp = satGetFlowTemp();
  bool  flameOn    = satIsFlameOn();
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
  bool flame = satIsFlameOn();
  float mod = OTcurrentSystemState.RelModLevel;
  float boilerTemp = satGetFlowTemp();
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
  bool flame = satIsFlameOn();
  float boilerTemp = satGetFlowTemp();
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
      float cs = satGetReturnTemp() + settings.sat.fFlameOffOffset;
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
      float cs = satGetFlowTemp() - settings.sat.fModSupOffset;
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
      // Publish restored target immediately so MQTT stays in sync.
      // ADR-111 exception: event-driven echo on preset-clear (latency matters
      // for HA; cannot wait for satPublishMQTT()'s next 30s tick). The next
      // satPublishMQTT() pass will pick up the same value through its shadow
      // (sat/target uses SAT_EPS_TEMP=0.05) and either no-op or heartbeat.
      if (state.mqtt.bConnected) {
        char valBuf[12];
        dtostrf(settings.sat.fTargetTemp, 1, 1, valBuf);
        // ADR-111 exception: see block comment above — event-driven echo.
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
    satNarrate_P(PSTR("Window open detected: heating paused"));
    state.sat.iWindowOpenSinceMs = millis();
    SATDebugTln(F("SAT: window opened, starting timer"));
  }
  else if (!isOpen && state.sat.bWindowOpen) {
    // Window closed - restore previous state
    state.sat.bWindowOpen = false;
    satNarrate_P(PSTR("Window closed: heating resumes"));
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
  bool flame = satIsFlameOn();
  if (!flame) {
    return pidOutput;
  }

  float boilerTemp = satGetFlowTemp();

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
    // TASK-799 F4 (2b): sensor_dropout window — return NAN so the real
    // ghost-Tr / fallback path (TASK-521/522) exercises under simulation.
    if (state.sat.iSimDropoutExpiryMs != 0 &&
        (int32_t)(millis() - state.sat.iSimDropoutExpiryMs) < 0) {
      return NAN;
    }
    return state.sat.fSimRoomTemp;
  }
  // BLE sensor has highest priority when available (Task #20). TASK-742: no
  // longer #ifdef'd — on ESP8266 bBleEnable/bBleTempValid stay false (no BLE
  // radio), so this block is naturally inert.
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
  // TASK-522: ghost-Tr detection now uses the upstream NAN-init contract.
  // OTcurrentSystemState.Tr is NAN until a MsgID 24 arrives. In OTDirect / master configs
  // without a thermostat, Tr stays NAN forever — isnan() distinguishes "never observed"
  // from a real reading without the static-bool bookkeeping that TASK-521 introduced.
  float otRoom = OTcurrentSystemState.Tr;  // OT message ID 24
  const bool trGhost = isnan(otRoom);

  // Task #21: If in fallback mode and OT room temp is invalid, use thermal estimation
  if (state.sat.bFallbackActive && (otRoom < -10.0f || otRoom > 50.0f)) {
    if (state.sat.iLastKnownRoomMs > 0) {
      float outside = satGetOutsideTemp();
      return satEstimateRoomTemp(outside);
    }
    // No last-known data either -- nothing valid to return.
    return NAN;
  }

  // TASK-204: Thermal comfort mode -- substitute Summer Simmer Index as PID room temp input.
  // Matches Python climate.py thermal_comfort: the PID targets heat-index-adjusted perceived
  // temperature rather than raw sensor temp, so e.g. 22C at 70% humidity "feels like" 23C.
  // Only active when bThermalComfort is enabled AND humidity data is fresh (< iHumidityTimeoutS).
  // TASK-521: skip SSI when otRoom is a ghost zero -- SSI(0,H) is meaningless and would mask
  // the no-source condition from the control-loop NaN guard.
  if (!trGhost && settings.sat.bThermalComfort && state.sat.bHumidityValid) {
    if ((millis() - state.sat.iHumidityLastMs) <= ((uint32_t)settings.sat.iHumidityTimeoutS * 1000UL)) {
      float ssi = satCalcSimmerIndex(otRoom, state.sat.fHumidity);
      SATDebugTf(PSTR("SAT: thermal_comfort: raw=%.1f SSI=%.1f H=%.0f%%\r\n"),
              otRoom, ssi, state.sat.fHumidity);
      return ssi;
    }
    SATDebugTln(F("SAT: thermal_comfort: humidity stale, using raw room temp"));
  }

  // TASK-521: ghost-Tr final guard. If no other source returned, and Tr has never been
  // observed as non-zero, return NAN so the control loop disengages safely instead of
  // driving the PID against a bogus 0.0 reading.
  if (trGhost) return NAN;

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
//=== SAT boiler-side wrappers (TASK-795: simulation contract) ===
//=== Return synthetic state when bSimulation, otherwise the real OT bus. ===
//=== Mirrors the satGetRoomTemp()/satGetOutsideTemp() pattern above.     ===
//=== The simulation-contract ADR is authored in commit 3 (plan §15).    ===
//=====================================================================

// Sensor noise (TASK-799 / plan §12 F4). Returns a ± noise offset in °C when the
// sensor_noise scenario is active, else 0. Applied at the wrapper read so it is
// transient per-read and never written back into the model (cannot accumulate).
// Cheap deterministic LCG (Numerical Recipes constants) — no heavy RNG lib.
static float satSimNoiseOffset()
{
  if (!settings.sat.bSimulation || state.sat.iSimNoiseExpiryMs == 0) return 0.0f;
  state.sat.iSimNoiseLcg = state.sat.iSimNoiseLcg * 1664525U + 1013904223U;
  // top 16 bits -> [-1, +1)
  float u = ((float)(state.sat.iSimNoiseLcg >> 16) / 32768.0f) - 1.0f;
  return u * state.sat.fSimNoiseAmplitudeC;
}

static float satGetFlowTemp()
{
  if (settings.sat.bSimulation) return state.sat.fSimFlowTemp + satSimNoiseOffset();
  return OTcurrentSystemState.Tboiler;     // OT MsgID 25 (flow water temp)
}

static float satGetReturnTemp()
{
  if (settings.sat.bSimulation) return state.sat.fSimReturnTemp + satSimNoiseOffset();
  return OTcurrentSystemState.Tret;        // OT MsgID 28 (return water temp)
}

static bool satIsFlameOn()
{
  if (settings.sat.bSimulation) return state.sat.bSimFlameOn;
  return (OTcurrentSystemState.Statusflags & 0x08) != 0;  // status bit 3 = flame
}

static uint8_t satGetActualModulation()
{
  if (settings.sat.bSimulation) return state.sat.iSimModulation;
  return (uint8_t)OTcurrentSystemState.RelModLevel;  // OT MsgID 17 (rel. modulation %)
}

//=====================================================================
//=== Bus-transmit gate (TASK-795 plan §4.1 + §4.3) ===
//=== Returns true when the caller must NOT emit on the boiler-side bus. ===
//=== Side effect (idempotent): records the would-be command for the    ===
//=== §4.3 trace (telnet + state) so the user sees what the regulator    ===
//=== decided even though nothing physically left the device.            ===
//=====================================================================
// cmd:    short ASCII of what would have gone out (e.g. "CS=42.5" or
//         "MID=24 VAL=4200"); caller formats, helper does not allocate.
// source: PROGMEM literal naming the call site ("loop", "gateway", "probe"...).
// Non-static + prototyped in OTGW-firmware.h: called cross-file from
// OTDirect.ino / OTGW-Core.ino, which concatenate ahead of this file.
bool satSimulationBlocksBusTx(const char* cmd,
                              const __FlashStringHelper* source)
{
  if (!(state.debug.bOTGWSimulation || settings.sat.bSimulation)) return false;
  const char* tag = settings.sat.bSimulation ? "SAT-SIM" : "OTGW-SIM";
  SATDebugTf(PSTR("%s trace: %s (src=%S)\r\n"), tag, cmd ? cmd : "", source);
  // TASK-818: no per-tx WS event here — it fired ~10x/sec and flooded the
  // live-log. Isolation is already visible via the telnet trace above, the
  // last-blocked-cmd + trace ring below (SAT dashboard), and the TASK-815
  // sim-start narration ("no bus traffic").
  if (settings.sat.bSimulation && cmd) {
    strlcpy(state.sat.sLastBlockedCmd, cmd, sizeof(state.sat.sLastBlockedCmd));
    state.sat.iLastBlockedCmdMs = millis();
    // TASK-801 F6: also push into the trace ring (newest at head).
    const uint8_t ring = (uint8_t)(sizeof(state.sat.iSimTraceMs) / sizeof(state.sat.iSimTraceMs[0]));
    strlcpy(state.sat.sSimTraceCmd[state.sat.iSimTraceHead], cmd, sizeof(state.sat.sSimTraceCmd[0]));
    state.sat.iSimTraceMs[state.sat.iSimTraceHead] = state.sat.iLastBlockedCmdMs;
    state.sat.iSimTraceHead = (uint8_t)((state.sat.iSimTraceHead + 1) % ring);
    if (state.sat.iSimTraceCount < ring) state.sat.iSimTraceCount++;
  }
  return true;
}

//=====================================================================
//=== Availability gate (TASK-795 plan §4.2) ===
//=== Simulation requires boiler-absence. A standalone bench rig with  ===
//=== no boiler is the intended use case. The moment a real boiler     ===
//=== answers the bus, simulation is forcibly disabled.                ===
//=====================================================================
// Real boiler-slave presence. Reads a DIFFERENT signal than the synthetic
// boilerOnline that probeOTBus()/loopback raise during simulation, so
// synthetic-online cannot feed back and self-disable sim (plan §4.2 dual-signal
// rule). PIC path: state.otBus.bBoilerState (set from real boiler frames, 30s
// window, OTGW-Core). OT-direct path: otDirectBoilerPresent() (boiler answered
// MsgID 3, loopback excluded). Capability-gated via HAS_* flags per the ESP
// abstraction rules — no raw platform ifdefs.
// Non-static + prototyped in OTGW-firmware.h: the REST 409 guard and the MQTT
// enable-reject (both concatenated ahead of this file) call it cross-file.
bool satBoilerHardwarePresent()
{
#if HAS_PIC
  if (state.otBus.bBoilerState) return true;
#endif
#if HAS_DIRECT_OT
  if (otDirectBoilerPresent()) return true;
#endif
  return false;
}

// Complete teardown when a real boiler appears while sim is active. One-way:
// re-enabling needs boiler-absence again (reboot with no boiler, or the bus
// layer clears its presence signal). Deferred out of the slave-RX edge hook
// (which is interrupt-adjacent) into satControlLoop via bBoilerDetectedFlag, so
// the heavy writeSettings() flush runs in cooperative context. Idempotent.
static void satOnBoilerDetected()
{
  if (!settings.sat.bSimulation) return;  // already off — nothing to tear down

  settings.sat.bSimulation = false;
  writeSettings(false);                   // LittleFS flush — survives reboot

  // Tear down synthetic boiler state; adopt the real readings.
  state.sat.bSimFlameOn        = false;
  state.sat.iSimModulation     = 0;
  state.sat.fSimFlowTemp       = OTcurrentSystemState.Tboiler;
  state.sat.fSimReturnTemp     = OTcurrentSystemState.Tret;
  state.sat.iSimFlameOnSinceMs = 0;
  state.sat.iSimFlameOffSinceMs = 0;
  state.sat.iSimLastUpdateMs   = 0;       // forces re-init if user re-enables later
  state.sat.sLastBlockedCmd[0] = '\0';    // the §4.3 trace's meaning ends with sim
  state.sat.iLastBlockedCmdMs  = 0;
  state.sat.iSimTraceHead      = 0;       // TASK-801 F6: trace ring meaning ends with sim too
  state.sat.iSimTraceCount     = 0;

  OTDebugTln(F("SAT-SIM: boiler appeared on bus — simulation disabled completely"));
  sendEventToWebSocket_P('!', PSTR("SAT-SIM: boiler appeared, simulation off"));
  satNarrate_P(PSTR("Real boiler detected on bus: simulation disabled"));
  // MQTT state topic flips OFF on the next sat-publisher tick (shadow detects it).
}

// Edge hook — called from the slave-frame RX path of both transports. Cheap and
// interrupt-safe: just raises a flag if sim is active. Defers the real work
// (writeSettings) to satConsumeBoilerDetected() in the cooperative SAT loop.
void satNotifyBoilerFrameSeen()
{
  if (settings.sat.bSimulation) state.sat.bBoilerDetectedFlag = true;
}

// Drains the edge-hook flag in cooperative context (called from satControlLoop).
static void satConsumeBoilerDetected()
{
  if (!state.sat.bBoilerDetectedFlag) return;
  state.sat.bBoilerDetectedFlag = false;
  satOnBoilerDetected();
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

// --- PV-surplus boost handlers (TASK-640) ---
bool satHandlePvSurplus(const char* value)
{
  if (!value || !*value) return false;
  char* endp = nullptr;
  float w = strtof(value, &endp);
  if (endp == value || *endp != '\0') return false;  // non-numeric input
  if (w < 0.0f || w > 50000.0f) return false;
  state.sat.fExternalPvSurplusW = w;
  state.sat.bExternalPvSurplusValid = true;
  state.sat.iExternalPvSurplusLastMs = millis();
  SATDebugTf(PSTR("SAT: PV surplus set to %.0fW\r\n"), w);
  return true;
}

bool satHandlePvBoostEnabled(const char* value)
{
  if (!value || !*value) return false;
  bool en = (strcasecmp_P(value, PSTR("true")) == 0 || atoi(value) != 0);
  settings.sat.bPvBoostEnabled = en;
  if (!en) {
    state.sat.bPvBoostActive = false;
    state.sat.fPvBoostAppliedC = 0.0f;
    state.sat.iPvBoostStartedMs = 0;
  }
  SATDebugTf(PSTR("SAT: PV boost %s\r\n"), en ? "enabled" : "disabled");
  return true;
}

// satUpdatePvBoost — TASK-640. Called once per control cycle BEFORE the
// heating-curve calculation. Owns activation/deactivation logic including:
//   - stale-input expiry (reuses iSensorMaxAgeS)
//   - hold-time before activation
//   - 20% hysteresis on deactivation
//   - safety guards (safety_tripped, window_open, dhw_active)
//   - indoor temp ceiling
//   - max continuous duration + 30-min cooldown
// Never mutates settings.sat.fTargetTemp; only sets state.sat.fPvBoostAppliedC
// which is added to effectiveTarget by the caller.
static void satUpdatePvBoost()
{
  if (!settings.sat.bPvBoostEnabled) {
    state.sat.bPvBoostActive = false;
    state.sat.fPvBoostAppliedC = 0.0f;
    state.sat.iPvBoostStartedMs = 0;
    return;
  }

  uint32_t now = millis();

  // Stale-input expiry (reuses iSensorMaxAgeS for symmetry with other external inputs)
  if (state.sat.bExternalPvSurplusValid &&
      (now - state.sat.iExternalPvSurplusLastMs) > ((uint32_t)settings.sat.iSensorMaxAgeS * 1000UL)) {
    state.sat.bExternalPvSurplusValid = false;
    SATDebugTln(F("SAT: PV surplus stale, deactivating boost"));
  }

  // Cooldown guard (30-min cooldown after max-duration reached)
  if (state.sat.iPvBoostCooldownMs != 0 && now < state.sat.iPvBoostCooldownMs) {
    state.sat.bPvBoostActive = false;
    state.sat.fPvBoostAppliedC = 0.0f;
    return;
  }
  if (state.sat.iPvBoostCooldownMs != 0 && now >= state.sat.iPvBoostCooldownMs) {
    state.sat.iPvBoostCooldownMs = 0;  // cooldown expired
  }

  // Safety guards: no boost when safety tripped, window open, or DHW priority
  if (state.sat.bSafetyTripped || state.sat.bWindowOpen || state.sat.bDhwActive) {
    state.sat.bPvBoostActive = false;
    state.sat.fPvBoostAppliedC = 0.0f;
    state.sat.iPvBoostStartedMs = 0;
    return;
  }

  float surplus = state.sat.bExternalPvSurplusValid ? state.sat.fExternalPvSurplusW : 0.0f;
  float threshold = (float)settings.sat.iPvBoostThresholdW;
  float roomTemp = satGetRoomTemp();
  bool atCeiling = (!isnan(roomTemp) && roomTemp >= settings.sat.fPvBoostMaxIndoorC);

  if (!state.sat.bPvBoostActive) {
    // Deactivated: check if conditions are met to start the hold-time
    if (surplus >= threshold && !atCeiling) {
      if (state.sat.iPvBoostStartedMs == 0)
        state.sat.iPvBoostStartedMs = now;
      if ((now - state.sat.iPvBoostStartedMs) >= ((uint32_t)settings.sat.iPvBoostHoldS * 1000UL)) {
        state.sat.bPvBoostActive = true;
        state.sat.fPvBoostAppliedC = settings.sat.fPvBoostDeltaC;
        SATDebugTf(PSTR("SAT: PV boost activated (+%.1fC, surplus=%.0fW)\r\n"),
                   settings.sat.fPvBoostDeltaC, surplus);
      }
    } else {
      state.sat.iPvBoostStartedMs = 0;  // surplus dropped, reset hold-time
    }
  } else {
    // Active: deactivate if surplus dropped below 80% threshold or ceiling reached
    if (surplus < (threshold * 0.8f) || atCeiling) {
      state.sat.bPvBoostActive = false;
      state.sat.fPvBoostAppliedC = 0.0f;
      state.sat.iPvBoostStartedMs = 0;
      SATDebugTln(F("SAT: PV boost deactivated"));
    } else {
      // Max-duration cap
      uint32_t maxMs = (uint32_t)settings.sat.iPvBoostMaxDurationMin * 60000UL;
      if ((now - state.sat.iPvBoostStartedMs) >= maxMs) {
        state.sat.bPvBoostActive = false;
        state.sat.fPvBoostAppliedC = 0.0f;
        state.sat.iPvBoostCooldownMs = now + 1800000UL;  // 30-min cooldown
        state.sat.iPvBoostStartedMs = 0;
        SATDebugTln(F("SAT: PV boost max-duration reached, 30-min cooldown"));
      }
    }
  }
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

// Float variant for sensor polling path (TASK-587)
void satSetAreaTemp(uint8_t area, float temp)
{
  if (area >= SAT_MAX_AREAS) return;
  if (temp > -50.0f && temp < 100.0f) {
    state.sat.fAreaTemp[area] = temp;
    state.sat.bAreaValid[area] = true;
    state.sat.iAreaLastMs[area] = millis();
    SATDebugTf(PSTR("SAT: area %u temp set to %.1f (sensor)\r\n"), area, temp);
  }
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

uint8_t satGetMaxZones()
{
  return SAT_MAX_ZONES;
}

bool satShouldDiscoverZone(uint8_t zoneIndex)
{
  if (zoneIndex >= SAT_MAX_ZONES) return false;

  uint8_t zoneCount = settings.sat.iZoneCount;
  if (zoneCount > SAT_MAX_ZONES) zoneCount = SAT_MAX_ZONES;
  if (zoneIndex < zoneCount) return true;

  const uint32_t timeoutMs = static_cast<uint32_t>(settings.sat.iZoneTimeoutS) * 1000UL;
  const SATZoneState& z = satZones[zoneIndex];
  return z.bRoomValid &&
         z.bSpValid &&
         z.iLastUpdateMs != 0 &&
         ((millis() - z.iLastUpdateMs) <= timeoutMs);
}

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

// Handler: push HVAC mode for zone n (1-based). TASK-593 / SAT Python PR #172.
// Accepts an HA-style HVACMode string: "off" marks the zone OFF (excluded from PID
// + P75 aggregation); any heating mode ("heat", "auto", "heat_cool", ...) clears the
// OFF flag. The freshness timestamp is updated so a mode message alone keeps the zone
// from going stale. The actual exclusion happens in satZonePidStep() via z.bOff.
bool satHandleZoneMode(uint8_t zone, const char* value)
{
  if (zone < 1 || zone > SAT_MAX_ZONES) return false;
  if (!value || !*value) return false;
  uint8_t idx = zone - 1;
  bool wasOff = satZones[idx].bOff;
  bool nowOff = (strcasecmp_P(value, PSTR("off")) == 0);
  satZones[idx].bOff          = nowOff;
  satZones[idx].iLastUpdateMs = millis();
  if (nowOff && !wasOff) {
    // HEAT -> OFF edge: reset PID memory eagerly (AC#3). The next control cycle's
    // satZonePidExclude() would reset it anyway, so this only guarantees a clean
    // integral for any reader in the window before that tick (e.g. diagnostics).
    satZones[idx].fPidIntegral = 0.0f;
    satZones[idx].fPrevError   = 0.0f;
    satZones[idx].fPidOutput   = SAT_MIN_SETPOINT;
  }
  SATDebugTf(PSTR("SAT zone %u: mode=%s (off=%d)\r\n"), zone, value, nowOff ? 1 : 0);
  return true;
}

// Reset a zone's PID memory so re-activation starts from a clean integral.
// Called on every early-return (OFF / invalid / stale) so an excluded zone does
// not carry a stale integral when it later returns to HEAT. Zeroing an already-
// zero integral is a no-op, so this is safe to call unconditionally (TASK-593).
static inline float satZonePidExclude(SATZoneState& z)
{
  z.fPidIntegral = 0.0f;
  z.fPrevError   = 0.0f;
  z.fPidOutput   = SAT_MIN_SETPOINT;  // diagnostics reflect the excluded state
  return SAT_MIN_SETPOINT;
}

// Run a simplified PID step for a single zone and return requested CH setpoint.
// Uses zone-local integral + prev_error state. Reuses heating curve from zone setpoint
// and shared gain calculation for consistent behavior.
// Returns SAT_MIN_SETPOINT (10°C) if the zone is OFF (HVACMode.OFF), inactive, or
// its inputs are invalid. Returning SAT_MIN_SETPOINT makes the caller's
// `zOut > SAT_MIN_SETPOINT` gate (satControlLoop) drop the zone from the P75
// aggregation, so excluded zones contribute neither output nor overshoot (TASK-593,
// ports SAT Python PR #172 which returns None for OFF zones in area.py).
static float satZonePidStep(uint8_t idx, float outsideTemp)
{
  SATZoneState& z = satZones[idx];
  uint32_t timeoutMs = (uint32_t)settings.sat.iZoneTimeoutS * 1000UL;
  uint32_t now = millis();

  // TASK-593: zone thermostat in HVACMode.OFF -- exclude entirely and reset PID
  // memory, mirroring SAT Python PR #172 (area.py returns None when HVACMode.OFF).
  if (z.bOff) return satZonePidExclude(z);

  // Mark zone inactive if stale
  if (!z.bRoomValid || !z.bSpValid) return satZonePidExclude(z);
  if ((now - z.iLastUpdateMs) > timeoutMs) return satZonePidExclude(z);

  float roomTemp = z.fRoomTemp;
  float target   = z.fSetpoint;

  // Validate inputs
  if (roomTemp < -10.0f || roomTemp > 50.0f) return satZonePidExclude(z);
  if (target < 5.0f || target > 30.0f) return satZonePidExclude(z);

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

  // Integral: only inside deadband (matches SAT Python convention).
  // Symmetric clamp [-curveValue, +curveValue] mirrors primary PID (TASK-588).
  // Negative accumulation is needed when the zone overshoots its setpoint.
  if (fabsf(error) <= deadband) {
    z.fPidIntegral += ki * error * 60.0f;   // SAT_PID_UPDATE_INTERVAL = 60s
    if (z.fPidIntegral < -curveValue) z.fPidIntegral = -curveValue;
    if (z.fPidIntegral >  curveValue) z.fPidIntegral =  curveValue;
    if (z.fPidIntegral >  20.0f)     z.fPidIntegral =  20.0f;
    if (z.fPidIntegral < -20.0f)     z.fPidIntegral = -20.0f;
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
    // TASK-593: an OFF zone (HVACMode.OFF) reports inactive, matching its exclusion
    // from PID + P75 aggregation in satZonePidStep().
    bool active = !z.bOff && z.bRoomValid && z.bSpValid && ((now - z.iLastUpdateMs) <= timeoutMs);

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
    satNarrate_P(PSTR("Safety cleared: SAT may resume"));
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
  char buf[192];
  time_t ts = time(nullptr);
  snprintf_P(buf, sizeof(buf), PSTR("{\"i\":%.4f,\"d\":%.4f,\"rd\":%.4f,\"err\":%.2f,\"ts\":%lu}"),
             state.sat.fPidI, state.sat.fPidD, state.sat.fRawDerivative, state.sat.fError, (unsigned long)ts);
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
  // Simple parse: extract values from {"i":X,"d":Y,"rd":Z,"err":W,"ts":N}
  float i = 0, d = 0, rd = 0, err = 0;
  unsigned long savedTs = 0;
  char* p;
  if ((p = strstr(buf, "\"i\":"))   != nullptr) i      = atof(p + 4);
  if ((p = strstr(buf, "\"d\":"))   != nullptr) d      = atof(p + 4);
  if ((p = strstr(buf, "\"rd\":"))  != nullptr) rd     = atof(p + 5);
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
  state.sat.fPidD  = d;
  state.sat.fError = err;
  // satPidRestoreState() sets _pid_integral and _pid_rawDerivative directly
  // so the next satPidUpdate() warm-starts instead of cold-starting at zero.
  satPidRestoreState(i, rd);
  SATDebugTf(PSTR("SAT: PID state restored (I=%.4f D=%.4f rd=%.4f err=%.2f age=%lus)\r\n"),
          i, d, rd, err, (unsigned long)(nowTs - (time_t)savedTs));
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
  satPidReset();
  satSavePidState();         // Persist zeros after reset so next boot cold-starts (AC #5)
  satSaveEnergyState();      // Persist energy total before reset (Task #196)
  satSaveEstimatedEnergy();  // Persist estimated gas energy before reset (Task #232)
  satHCRReset();        // Reset daily-median recommendation (Task #228)
  // Intentional: release boiler control to the thermostat (CS=0) rather than holding
  // a warm-idle setpoint like Python SAT's COLD_SETPOINT=22C. OTGW is a gateway
  // sitting between the thermostat and the boiler -- when SAT is disabled, the physical
  // thermostat resumes authority. Python SAT uses a warm-idle setpoint because it *is*
  // the thermostat (standalone HA replacement); OTGW firmware is not, so it defers.
  addCommandToQueue("CS=0", 4, false, 0);
  // TASK-565: clear the write-on-change cache so the next satControlLoop
  // (re-enable, fallback, or recommissioning) re-emits all four SAT commands.
  satResetCmdCache();
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
  satSavePidState();         // Persist zeros so next boot does not restore stale integral (AC #5)
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
    // Transition-only narration: fire on an actual mode change, not every set call.
    satNarratef_P(PSTR("Control mode: %s"),
                  (state.sat.eControlMode == SAT_MODE_PWM)        ? "PWM"
                  : (state.sat.eControlMode == SAT_MODE_CONTINUOUS) ? "Continuous"
                  :                                                   "Off");
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
// TASK-883: per-response snapshot for the chunked /v2/sat/status stream. Same
// DETERMINISM CONTRACT as DeviceInfoSnap (jsonChunked.h): the closure re-runs per
// TCP window, so every AUTONOMOUS input is frozen ONCE here — the full state copy
// freezes all state.sat.* (incl. the volatile PID/cycle/sim floats and the
// sim-trace ring), and the captured fields freeze the external OT-bus globals
// (OTcurrentSystemState.*), the millis() clock used for sim-trace ages, the BLE
// failover flag, and the results of the computed/side-effecting helpers
// (satGetRoomTemp/OutsideTemp/MaxSetpoint, satCycleGet*, satBoilerHardwarePresent,
// satGet*Name). settings.sat.* stays live (event-driven).
struct SatStatusSnap {
  OTGWState st;                       // frozen copy of every state.* the emit reads
  float    otToutside;                // OTcurrentSystemState.Toutside
  uint16_t otSlaveConfigMemberIDcode; // OTcurrentSystemState.SlaveConfigMemberIDcode
  float    roomTemp;
  float    outsideTemp;
  float    maxSetpoint;
  bool     boilerHwPresent;
  uint8_t  cyclesThisHour;
  uint32_t phaseDurationSec;
  uint32_t nowMs;                     // millis() frozen for all sim-trace age maths
  bool     bleFailoverActive;
  char     boilerStatus[20];
  char     manufacturer[12];
  char     phaseName[24];
};

void satSendStatusJSON()
{
  const uint32_t startMs = millis();
  restPerfBegin(REST_PERF_SAT_STATUS);
  // TASK-885: emit the full SAT status field-by-field through the embedded-robust
  // JsonEmit streaming writer (no JsonDocument, no whole-response buffer). The
  // writer serialises NaN/Inf as null (matching the old satSendJsonFloat null
  // handling) and uses the default 3-decimal float width; per-field decimal
  // precision is intentionally not reintroduced. satBLESendStatusJSON() appends
  // its ble_* fields into the same open root object before the final endObject().
  // The chunked path allocates only the small fixed snapshot (no whole-response
  // cbuf); guard the snapshot alloc against a fragmented heap (mirrors device/info).
  if (platformMaxFreeBlock() < 8192) { sendApiError(503, F("low heap")); return; }

  // Freeze every volatile input ONCE (see SatStatusSnap above). Helpers are called
  // in their original relative order so any side-effects (satGetOutsideTemp resolves
  // its own staleness AFTER satGetRoomTemp) match the pre-chunked behaviour.
  auto snap = std::make_shared<SatStatusSnap>();
  snap->st            = state;                          // en-bloc freeze of all state.*
  satGetBoilerStatusName(snap->boilerStatus, sizeof(snap->boilerStatus));
  snap->roomTemp      = satGetRoomTemp();
  snap->outsideTemp   = satGetOutsideTemp();
  snap->cyclesThisHour   = satCycleGetCyclesThisHour();
  strlcpy(snap->phaseName, satCycleGetPhaseName(), sizeof(snap->phaseName));
  snap->phaseDurationSec = satCycleGetPhaseDurationSec();
  satGetManufacturerName(snap->manufacturer, sizeof(snap->manufacturer));
  snap->maxSetpoint   = satGetMaxSetpoint();
  snap->boilerHwPresent = satBoilerHardwarePresent();
  snap->otToutside    = OTcurrentSystemState.Toutside;
  snap->otSlaveConfigMemberIDcode = OTcurrentSystemState.SlaveConfigMemberIDcode;
  snap->nowMs         = millis();
  snap->bleFailoverActive = satBLEFailoverActive();

  // DETERMINISM GATE (TASK-883): this closure reads ONLY snap->*, settings.sat.*,
  // and F()/PSTR literals. No live state.*, OTcurrentSystemState.*, millis(), or
  // satGet*()/satCycleGet*() — those would shift a field's text width between
  // window passes and corrupt the wire JSON.
  restSendChunked("application/json", [snap](JsonEmit& je) {
    je.beginObject();
    je.field(F("enabled"),              settings.sat.bEnabled);
    je.field(F("active"),               snap->st.sat.bActive);
    je.field(F("control_mode"),         (int32_t)snap->st.sat.eControlMode);
    je.field(F("boiler_status"),        snap->boilerStatus);
    je.field(F("target_temp"),          settings.sat.fTargetTemp);
    je.field(F("room_temp"),            snap->roomTemp);
    // TASK-886 review M1: distinguish "no reading" from a real 0 °C. room_temp
    // already arrives as NaN->null when no thermostat exists; mirror that for the
    // other two no-source-prone tiles so the UI shows "--" instead of a misleading
    // "0.00°C". The outside-temp helper was called FIRST at snapshot time (it
    // resolves its own staleness side-effects), then if no live source remains AND it fell
    // through to the bare OT-bus 0.0f (the firmware's own no-sensor convention, see
    // line ~1113) it is emitted as null. final_setpoint is only meaningful active.
    {
      float outsideDisp = snap->outsideTemp;
      bool outsideHasSource = settings.sat.bSimulation || snap->st.sat.bExternalOutdoorValid ||
                              (snap->st.sat.weather.bValid && snap->otToutside == 0.0f);
      if (!outsideHasSource && outsideDisp == 0.0f) outsideDisp = NAN;
      je.field(F("outside_temp"),       outsideDisp);
    }
    je.field(F("heating_curve"),        snap->st.sat.fHeatingCurveValue);
    je.field(F("pid_output"),           snap->st.sat.fPidOutput);
    je.field(F("final_setpoint"),       snap->st.sat.bActive ? snap->st.sat.fFinalSetpoint : NAN);
    je.field(F("error"),                snap->st.sat.fError);
    je.field(F("pid_p"),                snap->st.sat.fPidP);
    je.field(F("pid_i"),                snap->st.sat.fPidI);
    je.field(F("pid_d"),                snap->st.sat.fPidD);
    je.field(F("kp"),                   snap->st.sat.fKp, 6);
    je.field(F("ki"),                   snap->st.sat.fKi, 6);
    je.field(F("kd"),                   snap->st.sat.fKd, 6);
    je.field(F("raw_derivative"),       snap->st.sat.fRawDerivative);
    je.field(F("coefficient"),          settings.sat.fHeatingCurveCoeff);
    je.field(F("deadband"),             settings.sat.fDeadband);
    je.field(F("overshoot_margin"),     settings.sat.fOvershootMargin);
    je.field(F("cycle_count"),          snap->st.sat.iCycleCount);
    je.field(F("cycles_this_hour"),     (int32_t)snap->cyclesThisHour);
    je.field(F("last_cycle_class"),     (int32_t)snap->st.sat.eLastCycleClass);
    je.field(F("cycle_max_flow"),       snap->st.sat.fCycleMaxFlow);
    je.field(F("cycle_overshoot_sec"),  snap->st.sat.fCycleOvershootSec);
    je.field(F("duty_ratio"),           snap->st.sat.fDutyRatio);
    je.field(F("overshoot_fraction"),   snap->st.sat.fOvershootFraction);
    je.field(F("underheat_fraction"),   snap->st.sat.fUnderheatFraction);
    je.field(F("cycle_phase"),          snap->phaseName);
    je.field(F("phase_duration_sec"),   (int32_t)snap->phaseDurationSec);
    je.field(F("pwm_duty"),             snap->st.sat.fPwmDutyCycle);
    je.field(F("pwm_flame_req"),        snap->st.sat.bPwmFlameRequested);
    je.field(F("active_preset"),        (int32_t)snap->st.sat.eActivePreset);
    je.field(F("mod_suppressed"),       snap->st.sat.bModSuppressed);
    je.field(F("dhw_active"),           snap->st.sat.bDhwActive);
    je.field(F("dhw_setpoint"),         settings.sat.fDhwSetpoint);
    // TASK-516: boiler-gated master DHW enable. dhw_config_tank is derived from
    // MsgID 3 HB3 (bit 11 of the uint16 SlaveConfigMemberIDcode); the UI uses it to
    // decide whether to render the toggle. dhw_enable mirrors the user setting; only
    // acted on (HW=) when dhw_config_tank=true.
    je.field(F("dhw_config_tank"),      (bool)(snap->otSlaveConfigMemberIDcode & 0x0800));
    je.field(F("dhw_enable"),           settings.sat.bDhwEnable);
    je.field(F("control_interval_sec"), (int32_t)settings.sat.iControlInterval);
    je.field(F("fallback_active"),      snap->st.sat.bFallbackActive);
    je.field(F("fallback_reason"),      (int32_t)snap->st.sat.eFallbackReason);
    je.field(F("max_rel_modulation"),   (int32_t)settings.sat.iMaxRelModulation);
    je.field(F("current_modulation"),   (int32_t)snap->st.sat.iCurrentModulation);
    je.field(F("ovp_value"),            settings.sat.fOvpValue);
    je.field(F("ovp_enabled"),          settings.sat.bOvpEnabled);
    je.field(F("ovp_calib_phase"),      (int32_t)snap->st.sat.eCalibPhase);
    je.field(F("ovp_calib_max_temp"),   snap->st.sat.fCalibMaxTemp);
    je.field(F("ovp_calib_samples"),    (int32_t)snap->st.sat.iCalibSamples);
    je.field(F("heating_system"),       (int32_t)settings.sat.iHeatingSystem);
    je.field(F("heating_system_detected"), (int32_t)snap->st.sat.iDetectedHeatingSystem);
    je.field(F("manufacturer"),         snap->manufacturer);
    je.field(F("manufacturer_setting"), (int32_t)settings.sat.iManufacturer);
    je.field(F("manufacturer_detected"), (int32_t)snap->st.sat.iDetectedManufacturer);
    je.field(F("slave_memberid"),       (int32_t)snap->st.sat.iSlaveMemberID);
    je.field(F("max_setpoint_system"),  snap->maxSetpoint);
    je.field(F("external_temp_valid"),  snap->st.sat.bExternalTempValid);
    je.field(F("external_outdoor_valid"), snap->st.sat.bExternalOutdoorValid);
    // PV-surplus boost (TASK-640)
    je.field(F("pv_surplus_w"),         snap->st.sat.fExternalPvSurplusW);
    je.field(F("pv_surplus_valid"),     snap->st.sat.bExternalPvSurplusValid);
    je.field(F("pv_boost_active"),      snap->st.sat.bPvBoostActive);
    je.field(F("pv_boost_applied_c"),   snap->st.sat.fPvBoostAppliedC);
    je.field(F("pv_boost_enabled"),     settings.sat.bPvBoostEnabled);
    je.field(F("safety_tripped"),       snap->st.sat.bSafetyTripped);
    je.field(F("valves_open"),          snap->st.sat.bValvesOpen);
    je.field(F("window_open"),          snap->st.sat.bWindowOpen);
    je.field(F("window_detection"),     settings.sat.bWindowDetection);
    je.field(F("push_setpoint"),        settings.sat.bPushSetpoint);
    je.field(F("flame_off_offset"),     settings.sat.fFlameOffOffset);
    je.field(F("force_pwm"),            settings.sat.bForcePWM);
    je.field(F("flow_offset"),          settings.sat.fFlowOffset);
    je.field(F("pressure"),             snap->st.sat.fSmoothedPressure);
    je.field(F("pressure_drop_rate"),   snap->st.sat.fPressureDropRate);
    je.field(F("pressure_alarm"),       snap->st.sat.bPressureAlarm);
    je.field(F("modulation_reliable"),  snap->st.sat.bModulationReliable);
    je.field(F("setpoint_mismatch"),    snap->st.sat.bSetpointMismatch);
    { static const char* const crNames[] = { "insufficient", "increase", "decrease", "hold" };
      int crIdx = (int)snap->st.sat.eCurveRecommendation;
      if (crIdx < 0 || crIdx > 3) crIdx = 0;
      je.field(F("curve_recommendation"), crNames[crIdx]); }
    je.field(F("heating_curve_recommendation"), snap->st.sat.sHeatCurveRec);
    je.field(F("mean_error"),           snap->st.sat.fMeanError);
    je.field(F("error_stddev"),         snap->st.sat.fErrorStdDev);
    je.field(F("target_temp_step"),     settings.sat.fTargetTempStep);
    je.field(F("power_kw"),             snap->st.sat.fCurrentPower);
    je.field(F("energy_kwh"),           snap->st.sat.fEnergyTotal);
    je.field(F("boiler_capacity"),      settings.sat.fBoilerCapacity);
    // Gas consumption estimation (Task #232)
    je.field(F("boiler_rated_kw"),      settings.sat.fBoilerRatedKW);
    je.field(F("boiler_efficiency"),    settings.sat.fBoilerEfficiency);
    je.field(F("energy_estimated_kwh"), snap->st.sat.fEnergyEstimatedKWh);
    // Preset sync (Task #46)
    je.field(F("preset_sync"),          settings.sat.bPresetSync);
    // Thermal drop learning (Task #21)
    je.field(F("thermal_coeff"),        settings.sat.fThermalCoeff);
    je.field(F("thermal_drop_rate"),    snap->st.sat.fThermalDropRate);
    je.field(F("thermal_model_valid"),  snap->st.sat.bThermalModelValid);
    je.field(F("estimated_room"),       snap->st.sat.fEstimatedRoom);
    je.field(F("last_known_room"),      snap->st.sat.fLastKnownRoom);
    // Solar gain (Task #23)
    je.field(F("solar_gain_active"),    snap->st.sat.bSolarGainActive);
    je.field(F("indoor_rise_rate"),     snap->st.sat.fIndoorRiseRate);
    // Summer simmer (Task #24)
    je.field(F("summer_simmer"),        settings.sat.bSummerSimmer);
    je.field(F("summer_active"),        snap->st.sat.bSummerActive);
    je.field(F("summer_hours_above"),   snap->st.sat.fSummerHoursAbove);
    je.field(F("summer_threshold"),     settings.sat.fSummerThreshold);
    je.field(F("summer_min_hours"),     (int32_t)settings.sat.iSummerMinHours);
    // Thermal comfort (Task #28/#47)
    je.field(F("comfort_adjust"),       settings.sat.bComfortAdjust);
    je.field(F("humidity"),             snap->st.sat.fHumidity);
    je.field(F("humidity_valid"),       snap->st.sat.bHumidityValid);
    je.field(F("comfort_offset"),       snap->st.sat.fComfortOffset);
    je.field(F("comfort_ref_humidity"), settings.sat.fComfortHumidity);
    je.field(F("comfort_max_offset"),   settings.sat.fComfortMaxOffset);
    // Simulation (Task #37 + TASK-795)
    je.field(F("simulation"),           settings.sat.bSimulation);
    // §4.2: mirrors the inverse of the boiler-hardware-present check (frozen above)
    // so the Web UI can hide the simulation card when a real boiler is attached.
    je.field(F("sim_available"),        !snap->boilerHwPresent);
    if (settings.sat.bSimulation) {
      je.field(F("sim_room_temp"),       snap->st.sat.fSimRoomTemp);
      je.field(F("sim_flow_temp"),       snap->st.sat.fSimFlowTemp);
      je.field(F("sim_outdoor_temp"),    snap->st.sat.fSimOutdoorTemp);
      je.field(F("sim_return_temp"),     snap->st.sat.fSimReturnTemp);
      je.field(F("sim_flame_on"),        snap->st.sat.bSimFlameOn);
      je.field(F("sim_modulation"),      (int32_t)snap->st.sat.iSimModulation);
      // §4.3 command trace
      je.field(F("last_blocked_cmd"),    snap->st.sat.sLastBlockedCmd);
      je.field(F("last_blocked_cmd_age_ms"),
                       snap->st.sat.iLastBlockedCmdMs == 0 ? (int32_t)0
                         : (int32_t)(snap->nowMs - snap->st.sat.iLastBlockedCmdMs));
      // TASK-801 F6: last_blocked_cmds[] ring, newest-first. Each element
      // {"cmd":"..","age_ms":N}. JsonEmit nested array-of-object (no manual buffer).
      {
        const uint8_t ring  = (uint8_t)(sizeof(snap->st.sat.iSimTraceMs) / sizeof(snap->st.sat.iSimTraceMs[0]));
        const uint8_t count = snap->st.sat.iSimTraceCount;
        const uint32_t nowMs = snap->nowMs;
        je.beginArray(F("last_blocked_cmds"));
        for (uint8_t k = 0; k < count; k++) {
          // newest-first: head-1-k, wrapping
          uint8_t idx = (uint8_t)((snap->st.sat.iSimTraceHead + ring - 1 - k) % ring);
          uint32_t age = (snap->st.sat.iSimTraceMs[idx] == 0) ? 0 : (nowMs - snap->st.sat.iSimTraceMs[idx]);
          je.beginObject();
          je.field(F("cmd"),    snap->st.sat.sSimTraceCmd[idx]);
          je.field(F("age_ms"), age);
          je.endObject();
        }
        je.endArray();
      }
    }
    // PID auto-tuning (Task #27)
    je.field(F("auto_tune"),            settings.sat.bAutoTune);
    je.field(F("auto_tune_active"),     snap->st.sat.bAutoTuneActive);
    je.field(F("auto_tune_cycles"),     (int32_t)snap->st.sat.iAutoTuneCycles);
    je.field(F("auto_tune_score"),      snap->st.sat.fAutoTuneScore);
    je.field(F("auto_tune_rate"),       settings.sat.fAutoTuneRate);
    // SAT Python parity settings (Task #82)
    je.field(F("sensor_max_age"),       (int32_t)settings.sat.iSensorMaxAgeS);
    je.field(F("error_monitoring"),     settings.sat.bErrorMonitoring);
    je.field(F("auto_gains_value"),     settings.sat.fAutoGainsValue);
    // TASK-193: manual gains mode
    je.field(F("auto_gains"),           settings.sat.bAutoGains);
    je.field(F("kp_manual"),            settings.sat.fKpManual, 6);
    je.field(F("ki_manual"),            settings.sat.fKiManual, 6);
    je.field(F("kd_manual"),            settings.sat.fKdManual, 6);
    // TASK-204: thermal comfort mode (SSI as PID room temp)
    je.field(F("thermal_comfort"),      settings.sat.bThermalComfort);
    je.field(F("heating_mode"),         settings.sat.iHeatingMode == 1 ? "eco" : "comfort");
    je.field(F("cycles_per_hour"),      (int32_t)settings.sat.iCyclesPerHour);
    je.field(F("valve_offset"),         settings.sat.fValveOffset);
    je.field(F("solar_freeze_integral"), settings.sat.bSolarFreezeIntegral);
    // Multi-area (Task #25)
    je.field(F("multi_area"),           settings.sat.bMultiArea);
    je.field(F("multi_area_count"),     (int32_t)settings.sat.iMultiAreaCount);
    if (settings.sat.bMultiArea && settings.sat.iMultiAreaCount > 0) {
      uint8_t cnt = settings.sat.iMultiAreaCount;
      if (cnt > SAT_MAX_AREAS) cnt = SAT_MAX_AREAS;
      for (uint8_t i = 0; i < cnt; i++) {
        // Dynamic per-area keys (area_0_temp ...): the key is formatted at
        // runtime, so F() cannot be used. The key buffer is passed to je.field().
        char nameBuf[20];
        // area_N_temp
        snprintf_P(nameBuf, sizeof(nameBuf), PSTR("area_%u_temp"), i);
        je.field(nameBuf, snap->st.sat.fAreaTemp[i]);
        // area_N_valid
        snprintf_P(nameBuf, sizeof(nameBuf), PSTR("area_%u_valid"), i);
        je.field(nameBuf, snap->st.sat.bAreaValid[i]);
        // area_N_weight
        snprintf_P(nameBuf, sizeof(nameBuf), PSTR("area_%u_weight"), i);
        je.field(nameBuf, settings.sat.fAreaWeight[i]);
      }
    }
    // BLE sensor status (Task #20). Appends ble_* fields from the frozen snapshot.
    satBLESendStatusJSON(je, snap->st.sat, snap->bleFailoverActive);
    je.endObject();
  });
  const uint32_t totalMs = millis() - startMs;
  restPerfCommit(REST_PERF_SAT_STATUS, totalMs);
  if (state.debug.bRestAPI) {
    DebugTf(PSTR("REST PERF sat/status total=%lums send=%lums render=%lums chunks=%lu\r\n"),
            (unsigned long)state.restperf.satStatus.iLastTotalMs,
            (unsigned long)state.restperf.satStatus.iLastSendMs,
            (unsigned long)state.restperf.satStatus.iLastRenderMs,
            (unsigned long)state.restperf.satStatus.iLastChunkCount);
  }
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

  SATDebugTln(F("SAT: publishing MQTT state (ADR-111 on-change + heartbeat)"));

  // ---------------------------------------------------------------------------
  // ADR-111: per-topic shadows.
  // All shadows are BSS zero-initialised on first call to this function; the
  // helpers publish on first-seen, on change (with per-field tolerance), or
  // when the per-topic heartbeat (uniform random 7-11 min) is due.
  // ---------------------------------------------------------------------------
  static SATShadowS s_mode, s_boiler_status, s_cycle_class, s_cycle_phase;
  static SATShadowS s_flame_status, s_manufacturer, s_heating_curve_rec;
  static SATShadowS s_curve_recommendation, s_modulation_state, s_pwm_state;
  static SATShadowS s_summer_simmer_perception, s_heating_mode;
  static SATShadowF s_setpoint, s_heating_curve, s_pid_output, s_target;
  static SATShadowF s_error, s_pid_p, s_pid_i, s_pid_d, s_raw_derivative;
  static SATShadowF s_pwm_duty, s_duty_ratio, s_overshoot_fraction;
  static SATShadowF s_4h_avg_on, s_4h_avg_off, s_4h_avg_flow, s_4h_duty_ratio;
  static SATShadowF s_4h_overshoot, s_4h_underheat, s_4h_delta_p50, s_4h_delta_p90;
  static SATShadowF s_overshoot_margin, s_room_temp, s_outside_temp;
  static SATShadowF s_kp, s_ki, s_kd;
  static SATShadowF s_pre_custom_temp, s_pre_activity_temp;
  static SATShadowF s_pv_surplus_w, s_pv_boost_applied_c, s_pv_boost_delta_c;
  static SATShadowF s_pv_boost_max_indoor_c;
  static SATShadowF s_pressure, s_pressure_drop_rate;
  static SATShadowF s_error_mean, s_error_stddev;
  static SATShadowF s_power, s_energy_total, s_energy_estimated_kwh;
  static SATShadowF s_thermal_coeff, s_thermal_drop_rate, s_estimated_room;
  static SATShadowF s_indoor_rise_rate, s_sun_elevation;
  static SATShadowF s_summer_hours_above, s_humidity, s_comfort_offset;
  static SATShadowF s_auto_tune_score, s_auto_tune_rate, s_auto_gains_value;
  static SATShadowF s_valve_offset;
  static SATShadowF s_area_0, s_area_1, s_area_2, s_area_3;
  static SATShadowF s_ssi, s_summer_simmer_index;
  static SATShadowF s_dhw_setpoint, s_max_setpoint, s_requested_setpoint, s_consumption;
  static SATShadowF s_heating_curve_coeff, s_deadband;
  static SATShadowF s_flame_off_offset, s_flow_offset, s_mod_sup_delay, s_mod_sup_offset;
  static SATShadowF s_boiler_capacity, s_boiler_rated_kw, s_boiler_efficiency;
  static SATShadowF s_comfort_humidity, s_comfort_max_offset;
  static SATShadowF s_summer_threshold, s_target_temp_step;
  static SATShadowF s_min_pressure, s_max_pressure, s_max_pressure_drop;
  static SATShadowF s_preset_comfort, s_preset_eco, s_preset_away;
  static SATShadowF s_preset_sleep, s_preset_activity, s_preset_home;
  static SATShadowF s_ovp_value;
  static SATShadowI s_cycles_this_hour, s_4h_cycles, s_current_modulation;
  static SATShadowI s_pv_boost_threshold_w, s_pv_boost_hold_s, s_pv_boost_max_duration_min;
  static SATShadowI s_sensor_max_age, s_cycles_per_hour, s_humidity_timeout_s;
  static SATShadowI s_control_interval, s_max_modulation;
  static SATShadowI s_heating_system, s_manufacturer_id;
  static SATShadowB s_active, s_safety_tripped, s_flame_health, s_valves_open;
  static SATShadowB s_window_open;
  static SATShadowB s_pv_surplus_valid, s_pv_boost_active, s_pv_boost_enabled;
  static SATShadowB s_pressure_alarm, s_pressure_health;
  static SATShadowB s_modulation_reliable, s_setpoint_mismatch;
  static SATShadowB s_thermal_model_valid;
  static SATShadowB s_solar_gain_active, s_summer_active;
  static SATShadowB s_humidity_valid;
  static SATShadowB s_simulation, s_auto_tune, s_auto_tune_active;
  static SATShadowS s_sim_last_cmd;  // TASK-795 §4.3: sat/sim/last_cmd trace
  static SATShadowB s_error_monitoring, s_solar_freeze_integral;
  static SATShadowB s_device_health, s_cycle_health;
  static SATShadowB s_setpoint_sync, s_modulation_sync, s_ch_sync;
  static SATShadowB s_solar_gain_enable, s_summer_simmer_enable, s_comfort_adjust_enable;
  static SATShadowB s_thermal_comfort, s_multi_area_enable, s_auto_tune_enable;
  static SATShadowB s_simulation_enable, s_window_detection_enable, s_force_pwm_enable;
  static SATShadowB s_push_setpoint_enable, s_ovp_enabled, s_preset_sync_enable;
  static SATShadowB s_dhw_enabled, s_dhw_enable, s_pwm_auto_switch_enable;
  // JSON-blob heartbeat counters (BSS zero-init == first-seen sentinel).
  static uint32_t s_pid_attrs_hb        = 0;
  static uint32_t s_cycle_attrs_hb      = 0;
  static uint32_t s_curve_rec_attrs_hb  = 0;
  static uint32_t s_climate_attrs_hb    = 0;
  // Local "last cycle count" tracker — when iCycleCount changes a new cycle
  // finished, which is the natural trigger for cycle_attributes republish.
  static uint32_t s_last_cycle_count    = 0;

  // ---------------------------------------------------------------------------
  // Control mode
  // ---------------------------------------------------------------------------
  static const char* const modeNames[] = { "off", "continuous", "pwm" };
  {
    uint8_t modeIdx = (uint8_t)state.sat.eControlMode;
    publishIfChangedS(F("sat/mode"), modeNames[modeIdx < 3 ? modeIdx : 0], s_mode, true);
  }

  // ---------------------------------------------------------------------------
  // Key temperatures + PID state
  // ---------------------------------------------------------------------------
  publishIfChangedF(F("sat/setpoint"),       state.sat.fFinalSetpoint,    s_setpoint,      SAT_EPS_TEMP,        1, true);
  publishIfChangedF(F("sat/heating_curve"),  state.sat.fHeatingCurveValue,s_heating_curve, SAT_EPS_TEMP_COARSE, 1, true);
  publishIfChangedF(F("sat/pid_output"),     state.sat.fPidOutput,        s_pid_output,    SAT_EPS_PID_OUTPUT,  1, true);
  publishIfChangedF(F("sat/target"),         settings.sat.fTargetTemp,    s_target,        SAT_EPS_TEMP,        1, true);

  bool pidChanged = false;
  pidChanged |= publishIfChangedF(F("sat/error"), state.sat.fError, s_error,       SAT_EPS_ERROR,    2, false);
  pidChanged |= publishIfChangedF(F("sat/pid_p"), state.sat.fPidP,  s_pid_p,       SAT_EPS_PID_TERM, 2, false);
  pidChanged |= publishIfChangedF(F("sat/pid_i"), state.sat.fPidI,  s_pid_i,       SAT_EPS_PID_TERM, 2, false);
  pidChanged |= publishIfChangedF(F("sat/pid_d"), state.sat.fPidD,  s_pid_d,       SAT_EPS_PID_TERM, 2, false);

  // PID JSON attributes (Task #55) — coherent with the 4 individual publishes above.
  {
    char jsonBuf[128];
    snprintf_P(jsonBuf, sizeof(jsonBuf),
      PSTR("{\"error\":%.2f,\"proportional\":%.2f,\"integral\":%.2f,\"derivative\":%.2f}"),
      state.sat.fError, state.sat.fPidP, state.sat.fPidI, state.sat.fPidD);
    publishJsonAttrIfChanged(F("sat/pid_attributes"), jsonBuf, s_pid_attrs_hb, pidChanged, false);
  }

  publishIfChangedF(F("sat/raw_derivative"), state.sat.fRawDerivative, s_raw_derivative, SAT_EPS_DERIVATIVE, 4, false);

  feedWatchDog();

  // ---------------------------------------------------------------------------
  // Boiler status, cycle class + JSON, PWM duty, cycle metrics, 4h stats
  // ---------------------------------------------------------------------------
  { char bsName[20]; satGetBoilerStatusName(bsName, sizeof(bsName));
    publishIfChangedS(F("sat/boiler_status"), bsName, s_boiler_status, false); }

  {
    static const char* const ccNames[] = {
      "none", "good", "overshoot", "underheat", "short", "uncertain"
    };
    int ccIdx = (int)state.sat.eLastCycleClass;
    if (ccIdx < 0 || ccIdx > 5) ccIdx = 0;
    publishIfChangedS(F("sat/cycle_class"), ccNames[ccIdx], s_cycle_class, false);
  }

  // Cycle attributes JSON — re-publish on cycle-count change (a new cycle
  // finished) or heartbeat. Source values are not all individually published,
  // so we track iCycleCount locally as the "anything new?" signal.
  {
    bool cycleAdvanced = ((uint32_t)state.sat.iCycleCount != s_last_cycle_count);
    s_last_cycle_count = (uint32_t)state.sat.iCycleCount;

    static const char* const ckNames[] = {
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
    publishJsonAttrIfChanged(F("sat/cycle_attributes"), jBuf, s_cycle_attrs_hb, cycleAdvanced, false);
  }

  publishIfChangedF(F("sat/pwm_duty"),           state.sat.fPwmDutyCycle,      s_pwm_duty,           SAT_EPS_FRACTION, 2, false);
  publishIfChangedF(F("sat/duty_ratio"),         state.sat.fDutyRatio,         s_duty_ratio,         SAT_EPS_FRACTION, 3, false);
  publishIfChangedF(F("sat/overshoot_fraction"), state.sat.fOvershootFraction, s_overshoot_fraction, SAT_EPS_FRACTION, 3, false);

  publishIfChangedS(F("sat/cycle_phase"),       satCycleGetPhaseName(),                            s_cycle_phase,       false);
  publishIfChangedI(F("sat/cycles_this_hour"),  (int32_t)satCycleGetCyclesThisHour(),              s_cycles_this_hour,  false);

  publishIfChangedI(F("sat/4h_cycles"),               (int32_t)state.sat.i4hCycles,             s_4h_cycles,     false);
  publishIfChangedF(F("sat/4h_avg_on_sec"),           state.sat.f4hAvgOnSec,                    s_4h_avg_on,     SAT_EPS_DURATION, 1, false);
  publishIfChangedF(F("sat/4h_avg_off_sec"),          state.sat.f4hAvgOffSec,                   s_4h_avg_off,    SAT_EPS_DURATION, 1, false);
  publishIfChangedF(F("sat/4h_avg_flow_temp"),        state.sat.f4hAvgFlow,                     s_4h_avg_flow,   SAT_EPS_TEMP_COARSE, 1, false);
  publishIfChangedF(F("sat/4h_duty_ratio"),           state.sat.f4hDutyRatio,                   s_4h_duty_ratio, SAT_EPS_FRACTION, 3, false);
  publishIfChangedF(F("sat/4h_overshoot_fraction"),   state.sat.f4hOvershootFraction,           s_4h_overshoot,  SAT_EPS_FRACTION, 3, false);
  publishIfChangedF(F("sat/4h_underheat_fraction"),   state.sat.f4hUnderheatFraction,           s_4h_underheat,  SAT_EPS_FRACTION, 3, false);
  publishIfChangedF(F("sat/4h_flow_ret_delta_p50"),   state.sat.f4hFlowRetDeltaP50,             s_4h_delta_p50,  SAT_EPS_TEMP_COARSE, 1, false);
  publishIfChangedF(F("sat/4h_flow_ret_delta_p90"),   state.sat.f4hFlowRetDeltaP90,             s_4h_delta_p90,  SAT_EPS_TEMP_COARSE, 1, false);

  feedWatchDog();

  // ---------------------------------------------------------------------------
  // Overshoot margin, active, temps, gains, safety, flame
  // ---------------------------------------------------------------------------
  publishIfChangedF(F("sat/overshoot_margin"), settings.sat.fOvershootMargin, s_overshoot_margin, SAT_EPS_TEMP_COARSE, 1, true);
  publishIfChangedB(F("sat/active"),           state.sat.bActive,             s_active,           true);

  publishIfChangedF(F("sat/room_temp"),    satGetRoomTemp(),    s_room_temp,    SAT_EPS_TEMP, 1, false);
  publishIfChangedF(F("sat/outside_temp"), satGetOutsideTemp(), s_outside_temp, SAT_EPS_TEMP, 1, false);

  publishIfChangedF(F("sat/kp"), state.sat.fKp, s_kp, SAT_EPS_KP, 4, false);
  publishIfChangedF(F("sat/ki"), state.sat.fKi, s_ki, SAT_EPS_KI, 6, false);
  publishIfChangedF(F("sat/kd"), state.sat.fKd, s_kd, SAT_EPS_KD, 2, false);

  publishIfChangedB(F("sat/safety_tripped"), state.sat.bSafetyTripped, s_safety_tripped, false);

  // Flame Status (Task #70)
  {
    static const char* const fsNames[] = {
      "INSUFFICIENT_DATA", "HEALTHY", "IDLE_OK", "STUCK_ON",
      "STUCK_OFF", "PWM_SHORT", "SHORT_CYCLING"
    };
    int fsIdx = (int)state.sat.eFlameStatus;
    if (fsIdx < 0 || fsIdx > 6) fsIdx = 0;
    publishIfChangedS(F("sat/flame_status"), fsNames[fsIdx], s_flame_status, false);
  }

  // Flame Health binary sensor (Task #71). Skipped entirely when status is
  // INSUFFICIENT_DATA — that intentional "no publish at all" path means HA
  // shows the sensor as unavailable rather than incorrectly healthy.
  {
    SATFlameStatus fs = state.sat.eFlameStatus;
    if (fs != SAT_FS_INSUFFICIENT_DATA) {
      bool problem = (fs == SAT_FS_STUCK_ON || fs == SAT_FS_STUCK_OFF ||
                      fs == SAT_FS_PWM_SHORT || fs == SAT_FS_SHORT_CYCLING);
      publishIfChangedBStr(F("sat/flame_health"), problem, s_flame_health, "ON", "OFF", true);
    }
  }

  publishIfChangedB(F("sat/valves_open"), state.sat.bValvesOpen, s_valves_open, false);

  // Pre-temperature tracking (Task #67) — only published when > 0.
  if (state.sat.fPreCustomTemp > 0.0f) {
    publishIfChangedF(F("sat/pre_custom_temperature"), state.sat.fPreCustomTemp, s_pre_custom_temp, SAT_EPS_TEMP, 1, false);
  }
  if (state.sat.fPreActivityTemp > 0.0f) {
    publishIfChangedF(F("sat/pre_activity_temperature"), state.sat.fPreActivityTemp, s_pre_activity_temp, SAT_EPS_TEMP, 1, false);
  }

  publishIfChangedB(F("sat/window_open"), state.sat.bWindowOpen, s_window_open, false);

  // ---------------------------------------------------------------------------
  // PV-surplus boost (TASK-640). Runtime telemetry only when feature enabled.
  // ---------------------------------------------------------------------------
  if (settings.sat.bPvBoostEnabled) {
    publishIfChangedF(F("sat/pv_surplus_w"),       state.sat.fExternalPvSurplusW,     s_pv_surplus_w,       SAT_EPS_PV_W, 0, true);
    publishIfChangedB(F("sat/pv_surplus_valid"),   state.sat.bExternalPvSurplusValid, s_pv_surplus_valid,   true);
    publishIfChangedB(F("sat/pv_boost_active"),    state.sat.bPvBoostActive,          s_pv_boost_active,    true);
    publishIfChangedF(F("sat/pv_boost_applied_c"), state.sat.fPvBoostAppliedC,        s_pv_boost_applied_c, SAT_EPS_TEMP, 1, true);
  }
  // PV-boost settings always published so HA discovery entities have a state topic.
  // pv_boost_enabled historically uses "1"/"0" payload (not true/false).
  publishIfChangedBStr(F("sat/pv_boost_enabled"), settings.sat.bPvBoostEnabled, s_pv_boost_enabled, "1", "0", true);
  publishIfChangedI(F("sat/pv_boost_threshold_w"),    (int32_t)settings.sat.iPvBoostThresholdW,    s_pv_boost_threshold_w,    true);
  publishIfChangedI(F("sat/pv_boost_hold_s"),         (int32_t)settings.sat.iPvBoostHoldS,         s_pv_boost_hold_s,         true);
  publishIfChangedF(F("sat/pv_boost_delta_c"),        settings.sat.fPvBoostDeltaC,                 s_pv_boost_delta_c,        SAT_EPS_TEMP, 1, true);
  publishIfChangedF(F("sat/pv_boost_max_indoor_c"),   settings.sat.fPvBoostMaxIndoorC,             s_pv_boost_max_indoor_c,   SAT_EPS_TEMP, 1, true);
  publishIfChangedI(F("sat/pv_boost_max_duration_min"), (int32_t)settings.sat.iPvBoostMaxDurationMin, s_pv_boost_max_duration_min, true);

  // ---------------------------------------------------------------------------
  // Pressure monitoring + ch_pressure delegate
  // ---------------------------------------------------------------------------
  publishIfChangedF(F("sat/pressure"),           state.sat.fSmoothedPressure, s_pressure,           SAT_EPS_PRESSURE, 2, false);
  publishIfChangedF(F("sat/pressure_drop_rate"), state.sat.fPressureDropRate, s_pressure_drop_rate, SAT_EPS_FRACTION, 3, false);
  publishIfChangedB(F("sat/pressure_alarm"),     state.sat.bPressureAlarm,    s_pressure_alarm,     false);
  // pressure_health uses "ON"/"OFF" payload.
  publishIfChangedBStr(F("sat/pressure_health"), state.sat.bPressureHealthy, s_pressure_health, "ON", "OFF", true);
  satPressureHealthPublish();

  // ---------------------------------------------------------------------------
  // Modulation + setpoint sync
  // ---------------------------------------------------------------------------
  publishIfChangedI(F("sat/current_modulation"),  (int32_t)state.sat.iCurrentModulation,   s_current_modulation,    false);
  publishIfChangedB(F("sat/modulation_reliable"), state.sat.bModulationReliable,           s_modulation_reliable,   false);
  publishIfChangedB(F("sat/setpoint_mismatch"),   state.sat.bSetpointMismatch,             s_setpoint_mismatch,     false);

  // ---------------------------------------------------------------------------
  // Heating curve recommendation + JSON attributes
  // ---------------------------------------------------------------------------
  bool curveRecChanged = false;
  {
    static const char* const crNames[] = { "insufficient", "increase", "decrease", "hold" };
    int crIdx = (int)state.sat.eCurveRecommendation;
    if (crIdx < 0 || crIdx > 3) crIdx = 0;
    curveRecChanged = publishIfChangedS(F("sat/curve_recommendation"), crNames[crIdx], s_curve_recommendation, false);
  }
  {
    char jBuf[180];
    snprintf_P(jBuf, sizeof(jBuf),
      PSTR("{\"error_threshold\":%.2f,\"daily_mean_error\":%.2f,\"daily_sample_count\":%u,\"recent_mean_error\":%.2f}"),
      settings.sat.fDeadband * 2.0f,
      state.sat.fMeanError,
      (unsigned)state.sat.iErrorSampleCount,
      state.sat.fError);
    publishJsonAttrIfChanged(F("sat/curve_recommendation_attributes"), jBuf, s_curve_rec_attrs_hb, curveRecChanged, false);
  }

  publishIfChangedS(F("sat/heating_curve_recommendation"), state.sat.sHeatCurveRec, s_heating_curve_rec, true);

  publishIfChangedF(F("sat/error_mean"),   state.sat.fMeanError,   s_error_mean,   SAT_EPS_ERROR,    2, false);
  publishIfChangedF(F("sat/error_stddev"), state.sat.fErrorStdDev, s_error_stddev, SAT_EPS_FRACTION, 3, false);

  // ---------------------------------------------------------------------------
  // Power + energy + manufacturer + thermal model
  // ---------------------------------------------------------------------------
  publishIfChangedF(F("sat/power"),        state.sat.fCurrentPower, s_power,        SAT_EPS_POWER,  2, false);
  publishIfChangedF(F("sat/energy_total"), state.sat.fEnergyTotal,  s_energy_total, SAT_EPS_ENERGY, 3, true);

  if (settings.sat.fBoilerRatedKW > 0.0f) {
    publishIfChangedF(F("sat/energy_estimated_kwh"), state.sat.fEnergyEstimatedKWh, s_energy_estimated_kwh, SAT_EPS_ENERGY, 3, true);
  }

  { char mfrName[12]; satGetManufacturerName(mfrName, sizeof(mfrName));
    publishIfChangedS(F("sat/manufacturer"), mfrName, s_manufacturer, true); }

  publishIfChangedF(F("sat/thermal_coeff"),       settings.sat.fThermalCoeff,    s_thermal_coeff,       SAT_EPS_DERIVATIVE,  4, true);
  publishIfChangedF(F("sat/thermal_drop_rate"),   state.sat.fThermalDropRate,    s_thermal_drop_rate,   SAT_EPS_DERIVATIVE,  4, false);
  publishIfChangedB(F("sat/thermal_model_valid"), state.sat.bThermalModelValid,  s_thermal_model_valid, true);
  publishIfChangedF(F("sat/estimated_room"),      state.sat.fEstimatedRoom,      s_estimated_room,      SAT_EPS_TEMP,        1, false);

  // ---------------------------------------------------------------------------
  // Solar gain + summer + thermal comfort
  // ---------------------------------------------------------------------------
  publishIfChangedB(F("sat/solar_gain"),      state.sat.bSolarGainActive, s_solar_gain_active, false);
  publishIfChangedF(F("sat/indoor_rise_rate"), state.sat.fIndoorRiseRate, s_indoor_rise_rate,  SAT_EPS_FRACTION, 2, false);
  if (state.sat.bSunElevationValid) {
    publishIfChangedF(F("sat/solar_gain_sun_elevation"), state.sat.fSunElevation, s_sun_elevation, SAT_EPS_TEMP_COARSE, 1, false);
  }

  publishIfChangedB(F("sat/summer_active"),       state.sat.bSummerActive,    s_summer_active,       false);
  publishIfChangedF(F("sat/summer_hours_above"),  state.sat.fSummerHoursAbove, s_summer_hours_above,  SAT_EPS_DURATION, 1, false);

  publishIfChangedF(F("sat/humidity"),       state.sat.fHumidity,       s_humidity,        SAT_EPS_TEMP_COARSE, 1, false);
  publishIfChangedB(F("sat/humidity_valid"), state.sat.bHumidityValid,  s_humidity_valid,  false);
  publishIfChangedF(F("sat/comfort_offset"), state.sat.fComfortOffset,  s_comfort_offset,  SAT_EPS_ERROR, 2, false);

  // ---------------------------------------------------------------------------
  // Simulation, auto-tune. Both use "ON"/"OFF" payloads historically.
  // ---------------------------------------------------------------------------
  publishIfChangedBStr(F("sat/simulation"), settings.sat.bSimulation, s_simulation, "ON", "OFF", true);
  // TASK-795 §4.3: surface the last would-be (blocked) command while simulating.
  // Publish-on-change, NOT retained — its meaning is transient and ends with sim.
  if (settings.sat.bSimulation) {
    publishIfChangedS(F("sat/sim/last_cmd"), state.sat.sLastBlockedCmd, s_sim_last_cmd, false);
  }
  publishIfChangedBStr(F("sat/auto_tune"),  settings.sat.bAutoTune,   s_auto_tune,  "ON", "OFF", true);
  if (settings.sat.bAutoTune) {
    publishIfChangedF(F("sat/auto_tune_score"),  state.sat.fAutoTuneScore,    s_auto_tune_score,  SAT_EPS_FRACTION, 2, false);
    publishIfChangedF(F("sat/auto_tune_rate"),   settings.sat.fAutoTuneRate,  s_auto_tune_rate,   SAT_EPS_FRACTION, 3, false);
    publishIfChangedB(F("sat/auto_tune_active"), state.sat.bAutoTuneActive,   s_auto_tune_active, false);
  }

  // ---------------------------------------------------------------------------
  // SAT Python parity settings (Task #82)
  // ---------------------------------------------------------------------------
  publishIfChangedI(F("sat/sensor_max_age"),       (int32_t)settings.sat.iSensorMaxAgeS,  s_sensor_max_age,       true);
  publishIfChangedB(F("sat/error_monitoring"),     settings.sat.bErrorMonitoring,         s_error_monitoring,     true);
  publishIfChangedF(F("sat/auto_gains_value"),     settings.sat.fAutoGainsValue,          s_auto_gains_value,     SAT_EPS_FRACTION, 2, true);
  publishIfChangedS(F("sat/heating_mode"),         settings.sat.iHeatingMode == 1 ? "eco" : "comfort", s_heating_mode, true);
  publishIfChangedI(F("sat/cycles_per_hour"),      (int32_t)settings.sat.iCyclesPerHour,  s_cycles_per_hour,      true);
  publishIfChangedF(F("sat/valve_offset"),         settings.sat.fValveOffset,             s_valve_offset,         SAT_EPS_FRACTION, 2, true);
  publishIfChangedB(F("sat/solar_freeze_integral"), settings.sat.bSolarFreezeIntegral,    s_solar_freeze_integral, true);

  // ---------------------------------------------------------------------------
  // Multi-area (Task #25). Hard-coded fan-out for up to 4 areas (SAT_MAX_AREAS
  // is the upper bound; helpers require compile-time F() topics). Each area
  // shadow is independent, so per-area on-change + heartbeat works naturally.
  // ---------------------------------------------------------------------------
  if (settings.sat.bMultiArea && settings.sat.iMultiAreaCount > 0) {
    uint8_t cnt = settings.sat.iMultiAreaCount;
    if (cnt > SAT_MAX_AREAS) cnt = SAT_MAX_AREAS;
    if (cnt > 0) publishIfChangedF(F("sat/area/0"), state.sat.fAreaTemp[0], s_area_0, SAT_EPS_TEMP, 1, false);
    if (cnt > 1) publishIfChangedF(F("sat/area/1"), state.sat.fAreaTemp[1], s_area_1, SAT_EPS_TEMP, 1, false);
    if (cnt > 2) publishIfChangedF(F("sat/area/2"), state.sat.fAreaTemp[2], s_area_2, SAT_EPS_TEMP, 1, false);
    if (cnt > 3) publishIfChangedF(F("sat/area/3"), state.sat.fAreaTemp[3], s_area_3, SAT_EPS_TEMP, 1, false);
  }

  // ---------------------------------------------------------------------------
  // Device + Cycle health binary sensors ("ON"/"OFF" payloads).
  // ---------------------------------------------------------------------------
  publishIfChangedBStr(F("sat/device_health"),
                       (state.sat.eBoilerStatus == SAT_BS_OFF),
                       s_device_health, "ON", "OFF", true);
  {
    bool cycleProb = (state.sat.eLastCycleClass == SAT_CYCLE_OVERSHOOT ||
                      state.sat.eLastCycleClass == SAT_CYCLE_UNDERHEAT ||
                      state.sat.eLastCycleClass == SAT_CYCLE_SHORT);
    publishIfChangedBStr(F("sat/cycle_health"), cycleProb, s_cycle_health, "ON", "OFF", true);
  }

  // ---------------------------------------------------------------------------
  // Summer Simmer Index (Task #64): requires valid humidity.
  // ---------------------------------------------------------------------------
  if (state.sat.bHumidityValid && state.sat.fHumidity > 0) {
    float simmerIdx = satCalcSimmerIndex(satGetRoomTemp(), state.sat.fHumidity);
    publishIfChangedF(F("sat/ssi"),                 simmerIdx, s_ssi,                 SAT_EPS_ERROR,       2, false);
    publishIfChangedF(F("sat/summer_simmer_index"), simmerIdx, s_summer_simmer_index, SAT_EPS_TEMP_COARSE, 1, false);
    publishIfChangedS(F("sat/summer_simmer_perception"), satSimmerPerception(simmerIdx), s_summer_simmer_perception, false);
  }

  // ---------------------------------------------------------------------------
  // Modulation state + PWM state (string enums)
  // ---------------------------------------------------------------------------
  {
    const char* modState = "OFF";
    if (state.sat.bActive) {
      if (state.sat.bDhwActive) {
        modState = "HOT_WATER";
      } else if (state.sat.eControlMode == SAT_MODE_PWM && !state.sat.bPwmFlameRequested) {
        modState = "PWM_OFF";
      } else if (satGetFlowTemp() < 22.0f) {
        modState = "COLD";
      } else {
        modState = "ACTIVE";
      }
    }
    publishIfChangedS(F("sat/modulation_state"), modState, s_modulation_state, false);
  }
  {
    const char* pwmState = "IDLE";
    if (state.sat.eControlMode == SAT_MODE_PWM) {
      pwmState = state.sat.bPwmFlameRequested ? "ON" : "OFF";
    }
    publishIfChangedS(F("sat/pwm_state"), pwmState, s_pwm_state, false);
  }

  // ---------------------------------------------------------------------------
  // DHW + max setpoint + requested setpoint + (optional) gas consumption.
  // ---------------------------------------------------------------------------
  publishIfChangedF(F("sat/dhw_setpoint"), settings.sat.fDhwSetpoint, s_dhw_setpoint, SAT_EPS_TEMP, 1, true);
  publishIfChangedF(F("sat/max_setpoint"), satGetMaxSetpoint(),       s_max_setpoint, SAT_EPS_TEMP_COARSE, 1, true);

  {
    float reqSp = state.sat.fPidOutput;
    float sysMax = satGetMaxSetpoint();
    if (reqSp < SAT_MIN_SETPOINT) reqSp = SAT_MIN_SETPOINT;
    if (reqSp > sysMax)           reqSp = sysMax;
    publishIfChangedF(F("sat/requested_setpoint"), reqSp, s_requested_setpoint, SAT_EPS_TEMP_COARSE, 1, false);
  }

  // Gas consumption m3/h (Task #52). The original code reads minCons/maxCons
  // as 0.0 placeholders — the entire block is currently dead. Refactor it
  // through the helper anyway, so when minCons/maxCons get real settings
  // hooks the on-change pattern is already in place.
  {
    float minCons = 0.0f;
    float maxCons = 0.0f;
    if (minCons > 0 && maxCons > 0) {
      float consumption = 0.0f;
      bool flame = satIsFlameOn();
      if (state.sat.bActive && flame) {
        float modFrac = OTcurrentSystemState.RelModLevel / 100.0f;
        consumption = minCons + (modFrac * (maxCons - minCons));
      }
      publishIfChangedF(F("sat/consumption"), consumption, s_consumption, SAT_EPS_FRACTION, 3, false);
    }
  }

  // ---------------------------------------------------------------------------
  // Sync binary sensors (Tasks #56/#57/#58). The 60-second mismatch debounce
  // is unchanged — only the publish itself goes through the helper.
  // ---------------------------------------------------------------------------
  {
    static unsigned long syncSetpointMismatchSince = 0;
    {
      float satSetpoint = state.sat.fFinalSetpoint;
      float boilerSetpoint = OTcurrentSystemState.TSet;
      bool mismatch = (fabsf(satSetpoint - boilerSetpoint) > 0.5f) && state.sat.bActive;
      if (!mismatch) syncSetpointMismatchSince = 0;
      else if (syncSetpointMismatchSince == 0) syncSetpointMismatchSince = millis();
      bool problem = mismatch && syncSetpointMismatchSince > 0 &&
                     (millis() - syncSetpointMismatchSince >= 60000UL);
      publishIfChangedBStr(F("sat/setpoint_sync"), problem, s_setpoint_sync, "ON", "OFF", true);
    }

    static unsigned long syncModulationMismatchSince = 0;
    {
      int satMod = (int)settings.sat.iMaxRelModulation;
      int boilerMod = (int)OTcurrentSystemState.MaxRelModLevelSetting;
      bool mismatch = (satMod != boilerMod) && state.sat.bActive;
      if (!mismatch) syncModulationMismatchSince = 0;
      else if (syncModulationMismatchSince == 0) syncModulationMismatchSince = millis();
      bool problem = mismatch && syncModulationMismatchSince > 0 &&
                     (millis() - syncModulationMismatchSince >= 60000UL);
      publishIfChangedBStr(F("sat/modulation_sync"), problem, s_modulation_sync, "ON", "OFF", true);
    }

    static unsigned long syncCHMismatchSince = 0;
    {
      bool boilerActive = (OTcurrentSystemState.SlaveStatus & 0x02) != 0;
      bool satHeating = state.sat.bActive;
      bool mismatch = (satHeating != boilerActive);
      if (!mismatch) syncCHMismatchSince = 0;
      else if (syncCHMismatchSince == 0) syncCHMismatchSince = millis();
      bool problem = mismatch && syncCHMismatchSince > 0 &&
                     (millis() - syncCHMismatchSince >= 60000UL);
      publishIfChangedBStr(F("sat/ch_sync"), problem, s_ch_sync, "ON", "OFF", true);
    }
  }

  feedWatchDog();

  // ---------------------------------------------------------------------------
  // SAT settings as individual HA-entity state topics (Task #81)
  // ---------------------------------------------------------------------------
  publishIfChangedF(F("sat/heating_curve_coeff"), settings.sat.fHeatingCurveCoeff, s_heating_curve_coeff, SAT_EPS_TEMP_COARSE, 1, true);
  publishIfChangedF(F("sat/deadband"),            settings.sat.fDeadband,          s_deadband,            SAT_EPS_ERROR,       2, true);
  publishIfChangedI(F("sat/control_interval"),    (int32_t)settings.sat.iControlInterval, s_control_interval, true);
  publishIfChangedI(F("sat/max_modulation"),      (int32_t)settings.sat.iMaxRelModulation, s_max_modulation, true);
  publishIfChangedF(F("sat/flame_off_offset"),    settings.sat.fFlameOffOffset,    s_flame_off_offset,    SAT_EPS_TEMP_COARSE, 1, true);
  publishIfChangedF(F("sat/flow_offset"),         settings.sat.fFlowOffset,        s_flow_offset,         SAT_EPS_TEMP_COARSE, 1, true);
  publishIfChangedF(F("sat/mod_sup_delay"),       settings.sat.fModSupDelay,       s_mod_sup_delay,       SAT_EPS_TEMP_COARSE, 1, true);
  publishIfChangedF(F("sat/mod_sup_offset"),      settings.sat.fModSupOffset,      s_mod_sup_offset,      SAT_EPS_TEMP_COARSE, 1, true);
  publishIfChangedF(F("sat/boiler_capacity"),     settings.sat.fBoilerCapacity,    s_boiler_capacity,     SAT_EPS_POWER,       1, true);
  publishIfChangedF(F("sat/boiler_rated_kw"),     settings.sat.fBoilerRatedKW,     s_boiler_rated_kw,     SAT_EPS_POWER,       1, true);
  publishIfChangedF(F("sat/boiler_efficiency"),   settings.sat.fBoilerEfficiency,  s_boiler_efficiency,   SAT_EPS_FRACTION,    2, true);
  publishIfChangedF(F("sat/comfort_humidity"),    settings.sat.fComfortHumidity,   s_comfort_humidity,    SAT_EPS_DURATION,    0, true);
  publishIfChangedF(F("sat/comfort_max_offset"),  settings.sat.fComfortMaxOffset,  s_comfort_max_offset,  SAT_EPS_TEMP_COARSE, 1, true);
  publishIfChangedF(F("sat/summer_threshold"),    settings.sat.fSummerThreshold,   s_summer_threshold,    SAT_EPS_TEMP_COARSE, 1, true);
  publishIfChangedF(F("sat/target_temp_step"),    settings.sat.fTargetTempStep,    s_target_temp_step,    SAT_EPS_TEMP_COARSE, 1, true);
  publishIfChangedF(F("sat/min_pressure"),        settings.sat.fMinPressure,       s_min_pressure,        SAT_EPS_PRESSURE,    1, true);
  publishIfChangedF(F("sat/max_pressure"),        settings.sat.fMaxPressure,       s_max_pressure,        SAT_EPS_PRESSURE,    1, true);
  publishIfChangedF(F("sat/max_pressure_drop"),   settings.sat.fMaxPressureDrop,   s_max_pressure_drop,   SAT_EPS_PRESSURE,    2, true);
  publishIfChangedF(F("sat/preset_comfort"),      settings.sat.fPresetComfort,     s_preset_comfort,      SAT_EPS_TEMP,        1, true);
  publishIfChangedF(F("sat/preset_eco"),          settings.sat.fPresetEco,         s_preset_eco,          SAT_EPS_TEMP,        1, true);
  publishIfChangedF(F("sat/preset_away"),         settings.sat.fPresetAway,        s_preset_away,         SAT_EPS_TEMP,        1, true);
  publishIfChangedF(F("sat/preset_sleep"),        settings.sat.fPresetSleep,       s_preset_sleep,        SAT_EPS_TEMP,        1, true);
  publishIfChangedF(F("sat/preset_activity"),     settings.sat.fPresetActivity,    s_preset_activity,     SAT_EPS_TEMP,        1, true);
  publishIfChangedF(F("sat/preset_home"),         settings.sat.fPresetHome,        s_preset_home,         SAT_EPS_TEMP,        1, true);
  publishIfChangedF(F("sat/ovp_value"),           settings.sat.fOvpValue,          s_ovp_value,           SAT_EPS_TEMP_COARSE, 1, true);
  publishIfChangedI(F("sat/heating_system"),      (int32_t)settings.sat.iHeatingSystem,  s_heating_system,    true);
  publishIfChangedI(F("sat/manufacturer_id"),     (int32_t)settings.sat.iManufacturer,   s_manufacturer_id,   true);

  publishIfChangedB(F("sat/solar_gain_enable"),       settings.sat.bSolarGainEnable,  s_solar_gain_enable,       true);
  publishIfChangedB(F("sat/summer_simmer_enable"),    settings.sat.bSummerSimmer,     s_summer_simmer_enable,    true);
  publishIfChangedB(F("sat/comfort_adjust_enable"),   settings.sat.bComfortAdjust,    s_comfort_adjust_enable,   true);
  publishIfChangedB(F("sat/thermal_comfort"),         settings.sat.bThermalComfort,   s_thermal_comfort,         true);
  publishIfChangedI(F("sat/humidity_timeout_s"),      (int32_t)settings.sat.iHumidityTimeoutS, s_humidity_timeout_s, true);
  publishIfChangedB(F("sat/multi_area_enable"),       settings.sat.bMultiArea,        s_multi_area_enable,       true);
  publishIfChangedB(F("sat/auto_tune_enable"),        settings.sat.bAutoTune,         s_auto_tune_enable,        true);
  publishIfChangedB(F("sat/simulation_enable"),       settings.sat.bSimulation,       s_simulation_enable,       true);
  publishIfChangedB(F("sat/window_detection_enable"), settings.sat.bWindowDetection,  s_window_detection_enable, true);
  publishIfChangedB(F("sat/force_pwm_enable"),        settings.sat.bForcePWM,         s_force_pwm_enable,        true);
  publishIfChangedB(F("sat/push_setpoint_enable"),    settings.sat.bPushSetpoint,     s_push_setpoint_enable,    true);
  publishIfChangedB(F("sat/ovp_enabled"),             settings.sat.bOvpEnabled,       s_ovp_enabled,             true);
  publishIfChangedB(F("sat/preset_sync_enable"),      settings.sat.bPresetSync,       s_preset_sync_enable,      true);
  publishIfChangedB(F("sat/dhw_enabled"),             settings.sat.bDhwEnabled,       s_dhw_enabled,             true);
  publishIfChangedB(F("sat/dhw_enable"),              settings.sat.bDhwEnable,        s_dhw_enable,              true);
  publishIfChangedB(F("sat/pwm_auto_switch_enable"),  settings.sat.bPwmAutoSwitch,    s_pwm_auto_switch_enable,  true);

  // ---------------------------------------------------------------------------
  // Climate entity extra_state_attributes JSON blob (Task #72). Aggregates 13
  // fields; we don't try to track individual change — heartbeat-only suffices.
  // ---------------------------------------------------------------------------
  {
    static char climAttrBuf[512];
    char fBuf[16];
    int pos = 0;
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR("{"));
    dtostrf(settings.sat.fHeatingCurveCoeff, 1, 2, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR("\"optimal_coefficient\":%s"), fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR(",\"coefficient_derivative\":0.0"));
    dtostrf(SAT_MIN_SETPOINT, 1, 1, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR(",\"minimum_setpoint\":%s"), fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR(",\"boiler_flame_timing\":%.1f"), state.sat.fLastCycleDuration);
    dtostrf(satGetFlowTemp(), 1, 1, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR(",\"boiler_temperature_cold\":%s"), fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR(",\"boiler_temperature_tracking\":false"));
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR(",\"boiler_temperature_derivative\":0.0"));
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR(",\"error_source\":\"main\""));
    dtostrf(state.sat.fError, 1, 2, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR(",\"error_pid\":%s"), fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR(",\"integral_enabled\":true"));
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR(",\"derivative_enabled\":true"));
    dtostrf(state.sat.fRawDerivative, 1, 4, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR(",\"derivative_raw\":%s"), fBuf);
    dtostrf(state.sat.fKp, 1, 4, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR(",\"current_kp\":%s"), fBuf);
    dtostrf(state.sat.fKi, 1, 6, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR(",\"current_ki\":%s"), fBuf);
    dtostrf(state.sat.fKd, 1, 2, fBuf);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos, PSTR(",\"current_kd\":%s"), fBuf);
    bool relModEnabled = !(satGetManufacturerQuirks() & SAT_QUIRK_NO_REL_MOD);
    pos += snprintf_P(climAttrBuf + pos, sizeof(climAttrBuf) - pos,
                      PSTR(",\"relative_modulation_enabled\":%s}"),
                      relModEnabled ? "true" : "false");
    publishJsonAttrIfChanged(F("sat/climate_attributes"), climAttrBuf, s_climate_attrs_hb, /*anyChanged=*/false, false);
  }

  // Weather data (Task #50) — uses its own helpers internally.
  weatherPublishMQTT();

  // BLE sensor data (Task #20) — uses its own helpers internally.
  // TASK-742: satBLEPublishMQTT() is a no-op stub on ESP8266.
  satBLEPublishMQTT();
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
  bool flame = satIsFlameOn();

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
    float tFlow    = satGetFlowTemp(); // flow water temp (MsgID 25)
    float tRet     = satGetReturnTemp();    // return water temp (MsgID 28)
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

  bool flame = satIsFlameOn();  // Bit 3 = flame

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
  _sat_prevOTOnline = false;
  // TASK-565: cache starts invalid; first call to satEnqueueIfChanged*()
  // will always emit. Defensive reset in case the struct's default
  // initialiser is bypassed by a non-zero state.sat memcpy somewhere.
  satResetCmdCache();

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

  // Initialize BLE sensor scanning (Task #20).
  // TASK-742: satBLEInit() is a no-op stub on ESP8266 (no BLE radio).
  satBLEInit();

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
// Synthetic minimum modulation floor: Geminox-class boilers idle at 10%, the
// rest at 5%. Reuses the manufacturer-quirk infrastructure (plan §5.3).
static uint8_t satSimMinMod()
{
  return (satGetManufacturerQuirks() & SAT_QUIRK_MIN_MOD_10) ? 10 : 5;
}

// Synthetic boiler model (TASK-795, plan §5.2-§5.6). Drives the boiler-side
// state that the satGetFlowTemp/satGetReturnTemp/satIsFlameOn/
// satGetActualModulation wrappers expose, so the cycle classifier, flame
// health, 4h stats, heating-curve recommendation and OPV calibration run under
// simulation. No bus traffic and no availability gate here — those land in
// commit 3 (plan §4).
// Synthetic outdoor temperature (TASK-796 / plan §12 F1). Returns the weather
// feed when valid, otherwise a triangle-wave diurnal curve. Fractional
// hour-of-day comes from wall-clock when NTP is set; otherwise a free-running
// 24 h phase off millis() so a bench rig with no NTP still oscillates.
static float satSimOutdoorTemp()
{
  if (state.sat.weather.bValid) return state.sat.weather.fTemperature;

  float hod;  // hour-of-day in [0,24)
  if (isNTPtimeSet()) {
    time_t ts = time(nullptr);
    struct tm lt;
    localtime_r(&ts, &lt);
    hod = lt.tm_hour + lt.tm_min / 60.0f;
  } else {
    // Free-running: map a 24 h period onto millis() so the curve still moves.
    hod = (float)((millis() / 1000UL) % 86400UL) / 3600.0f;
  }

  // Triangle wave: 0 at the coldest hour, peak 1 twelve hours later, back to 0.
  float ph = hod - (float)SAT_SIM_OUTDOOR_MIN_HOUR;   // hours past the coldest point
  while (ph < 0.0f)   ph += 24.0f;
  while (ph >= 24.0f) ph -= 24.0f;
  // tri in [-1, +1]: -1 at ph=0 (coldest), +1 at ph=12 (warmest), linear between
  float tri = (ph <= 12.0f) ? (-1.0f + ph / 6.0f) : (3.0f - ph / 6.0f);
  return SAT_SIM_OUTDOOR_MEAN + SAT_SIM_OUTDOOR_AMPLITUDE * tri;
}

static void satUpdateSimulation()
{
  if (!settings.sat.bSimulation) return;

  uint32_t now = millis();
  if (state.sat.iSimLastUpdateMs == 0) {
    // First call — initialize timestamp, skip delta calculation
    state.sat.iSimLastUpdateMs = now;
    state.sat.bSimWarmupDone = false;
    SATDebugTln(F("SAT SIM: simulation started"));
    satNarrate_P(PSTR("Simulation started: synthetic boiler, no bus traffic"));
    return;
  }

  float dtSec = (float)(now - state.sat.iSimLastUpdateMs) / 1000.0f;
  if (dtSec <= 0.0f || dtSec > 60.0f) dtSec = 1.0f; // clamp sanity
  state.sat.iSimLastUpdateMs = now;

  // TASK-796: drive the synthetic outdoor temperature (diurnal or weather feed)
  // before the room model consumes it below.
  state.sat.fSimOutdoorTemp = satSimOutdoorTemp();

  // TASK-800 F5: during a DHW draw the boiler targets the DHW setpoint, not the
  // CH setpoint — so synthetic flow climbs toward hot-water temp (AC#2). Use the
  // configured fDhwSetpoint when valid, else a sane 55 °C default. This effective
  // sp feeds the flame SM + modulation below, so the whole flow model follows it.
  const bool dhwDraw = (state.sat.iSimDhwExpiryMs != 0);
  float sp = state.sat.fFinalSetpoint;
  if (dhwDraw) {
    sp = (settings.sat.fDhwSetpoint >= 30.0f && settings.sat.fDhwSetpoint <= 70.0f)
           ? settings.sat.fDhwSetpoint : 55.0f;
  }

  // --- Flame state machine (plan §5.2) ---
  // An active DHW draw demands the flame regardless of CH demand (the boiler
  // fires for hot water), so cycles run even with no heat call.
  const bool heatRequested = dhwDraw ||
                             (state.sat.bActive && sp > (SAT_MIN_SETPOINT + 1.0f));
  const bool flowBelowSP   = state.sat.fSimFlowTemp <  (sp - SAT_SIM_FLAME_HYST_LO);
  const bool flowAboveSP   = state.sat.fSimFlowTemp >= (sp + settings.sat.fOvershootMargin);
  const bool offLongEnough = (now - state.sat.iSimFlameOffSinceMs) >= SAT_SIM_MIN_OFF_MS;
  if (!state.sat.bSimFlameOn) {
    if (heatRequested && flowBelowSP && offLongEnough) {
      state.sat.bSimFlameOn        = true;
      state.sat.iSimFlameOnSinceMs = now;
      satCycleOnFlameChange(true);
      SATDebugTln(F("SAT SIM: flame ON"));
    }
  } else {
    if (!heatRequested || flowAboveSP) {
      state.sat.bSimFlameOn         = false;
      state.sat.iSimFlameOffSinceMs = now;
      satCycleOnFlameChange(false);
      SATDebugTln(F("SAT SIM: flame OFF"));
    }
  }

  // --- Modulation (plan §5.3) ---
  if (state.sat.bSimFlameOn) {
    float mod = SAT_SIM_MOD_KP * (sp - state.sat.fSimFlowTemp);
    if (mod < satSimMinMod())                      mod = satSimMinMod();
    if (mod > settings.sat.iMaxRelModulation)      mod = settings.sat.iMaxRelModulation;
    state.sat.iSimModulation = (uint8_t)mod;
  } else {
    state.sat.iSimModulation = 0;
  }

  // --- Flow temperature (plan §5.4, modulation-coupled) ---
  const float power_in  = ((float)state.sat.iSimModulation / 100.0f) * settings.sat.fBoilerCapacity;
  const float heat_loss = SAT_SIM_FLOW_LOSS_K * (state.sat.fSimFlowTemp - state.sat.fSimRoomTemp);
  state.sat.fSimFlowTemp += (power_in - heat_loss) * SAT_SIM_FLOW_THERMAL_GAIN * dtSec;
  if (state.sat.fSimFlowTemp < state.sat.fSimRoomTemp)   state.sat.fSimFlowTemp = state.sat.fSimRoomTemp;
  if (state.sat.fSimFlowTemp > settings.sat.fMaxSetpoint) state.sat.fSimFlowTemp = settings.sat.fMaxSetpoint;

  // --- Return temperature (plan §5.5) ---
  const float delta_tr = 5.0f + 15.0f * ((float)state.sat.iSimModulation / 100.0f);
  state.sat.fSimReturnTemp = state.sat.fSimFlowTemp - delta_tr;
  if (state.sat.fSimReturnTemp < state.sat.fSimRoomTemp) state.sat.fSimReturnTemp = state.sat.fSimRoomTemp;

  // --- Room temperature (plan §5.6) — heating keyed on the synthetic flame ---
  const float dtMin = dtSec / 60.0f;
  if (state.sat.bSimFlameOn) {
    const float target = settings.sat.fTargetTemp;
    if (state.sat.fSimRoomTemp < target) {
      state.sat.fSimRoomTemp += settings.sat.fSimHeatRate * dtMin;
      if (state.sat.fSimRoomTemp > target) state.sat.fSimRoomTemp = target;
    }
  } else {
    // window_open (TASK-797): multiply the cooling rate for the active window.
    float coolRate = settings.sat.fSimCoolRate;
    if (state.sat.iSimWindowExpiryMs != 0) coolRate *= state.sat.fSimWindowLossMult;
    if (state.sat.fSimRoomTemp > state.sat.fSimOutdoorTemp) {
      state.sat.fSimRoomTemp -= coolRate * dtMin;
      if (state.sat.fSimRoomTemp < state.sat.fSimOutdoorTemp) state.sat.fSimRoomTemp = state.sat.fSimOutdoorTemp;
    }
  }

  // --- Scenario injection (TASK-797 / plan §12 F2) ---
  // solar_gain: additive room warming, independent of flame, while active.
  if (state.sat.iSimSolarExpiryMs != 0) {
    state.sat.fSimRoomTemp += state.sat.fSimSolarGainC * dtMin;
  }
  // Expire elapsed perturbations (millis()-rollover-safe via signed diff).
  if (state.sat.iSimWindowExpiryMs != 0 &&
      (int32_t)(now - state.sat.iSimWindowExpiryMs) >= 0) {
    state.sat.iSimWindowExpiryMs = 0;
    state.sat.fSimWindowLossMult = 1.0f;
    SATDebugTln(F("SAT SIM: window_open expired"));
  }
  if (state.sat.iSimSolarExpiryMs != 0 &&
      (int32_t)(now - state.sat.iSimSolarExpiryMs) >= 0) {
    state.sat.iSimSolarExpiryMs = 0;
    state.sat.fSimSolarGainC = 0.0f;
    SATDebugTln(F("SAT SIM: solar_gain expired"));
  }
  if (state.sat.iSimNoiseExpiryMs != 0 &&
      (int32_t)(now - state.sat.iSimNoiseExpiryMs) >= 0) {
    state.sat.iSimNoiseExpiryMs = 0;
    state.sat.fSimNoiseAmplitudeC = 0.0f;
    SATDebugTln(F("SAT SIM: sensor_noise expired"));
  }
  if (state.sat.iSimDropoutExpiryMs != 0 &&
      (int32_t)(now - state.sat.iSimDropoutExpiryMs) >= 0) {
    state.sat.iSimDropoutExpiryMs = 0;
    SATDebugTln(F("SAT SIM: sensor_dropout expired"));
  }

  // --- DHW demand (TASK-800 F5): light periodic schedule + expiry ---
  // A short DHW draw every SAT_SIM_DHW_PERIOD_MS keeps the classifier seeing
  // DHW/MIXED cycles passively; the F2 dhw_demand event can also trigger one.
  if (state.sat.iSimDhwNextSchedMs == 0) {
    state.sat.iSimDhwNextSchedMs = now + SAT_SIM_DHW_PERIOD_MS;  // seed first draw
  }
  if (state.sat.iSimDhwExpiryMs == 0 &&
      (int32_t)(now - state.sat.iSimDhwNextSchedMs) >= 0) {
    state.sat.iSimDhwExpiryMs    = now + SAT_SIM_DHW_DRAW_MS;
    if (state.sat.iSimDhwExpiryMs == 0) state.sat.iSimDhwExpiryMs = 1;
    state.sat.iSimDhwNextSchedMs = now + SAT_SIM_DHW_PERIOD_MS;
    SATDebugTln(F("SAT SIM: scheduled DHW draw start"));
  }
  if (state.sat.iSimDhwExpiryMs != 0 &&
      (int32_t)(now - state.sat.iSimDhwExpiryMs) >= 0) {
    state.sat.iSimDhwExpiryMs = 0;
    SATDebugTln(F("SAT SIM: DHW draw end"));
  }

  // --- Multi-zone synthetic room model (TASK-798 / plan §12 F3) ---
  // Shared boiler/flow (plan default), per-zone room response, so the P75
  // aggregation (satControlLoop, iZoneCount>1) runs under simulation. OFF zones
  // (TASK-593 bOff) are left untouched so they stay excluded from P75.
  //
  // Scheme B (maintainer-confirmed), modelled on a real house: zone 1 is the
  // living room at the full target; zones 2+ are bedrooms/secondary rooms ~2 °C
  // lower with a small deterministic per-zone variation. Secondary rooms are
  // smaller (lower thermal mass) so they heat AND cool faster — they reach
  // target and switch off sooner, and drop quicker once the flame is off.
  if (settings.sat.iZoneCount > 1) {
    uint8_t zc = settings.sat.iZoneCount;
    if (zc > SAT_MAX_ZONES) zc = SAT_MAX_ZONES;
    for (uint8_t zi = 0; zi < zc; zi++) {
      SATZoneState& z = satZones[zi];
      if (z.bOff) continue;  // TASK-593: OFF zone stays excluded — do not synthesize
      // Deterministic per-zone variation (no RNG): -0.3/-0.6/-0.9 °C for zones
      // 2/3/4 on top of the 2 °C secondary-room drop. Reproducible per index.
      const float zoneVar = (float)zi * 0.3f;
      // Synthesize a setpoint if none was pushed externally.
      if (!z.bSpValid) {
        z.fSetpoint = (zi == 0)
                        ? settings.sat.fTargetTemp                       // living room
                        : settings.sat.fTargetTemp - 2.0f - zoneVar;     // secondary rooms ~2 °C lower
        z.bSpValid  = true;
      }
      // Smaller secondary rooms respond faster (lower thermal mass): scale the
      // shared heat/cool rates up for zones 2+ (1.0 / 1.3 / 1.6 / 1.9x).
      const float zoneRateMult = (zi == 0) ? 1.0f : (1.0f + 0.3f * (float)zi);
      // Seed room temp on first sim touch from the shared sim room temp.
      if (!z.bRoomValid) z.fRoomTemp = state.sat.fSimRoomTemp;
      // Drive room toward this zone's setpoint on the shared synthetic flame.
      if (state.sat.bSimFlameOn) {
        if (z.fRoomTemp < z.fSetpoint) {
          z.fRoomTemp += settings.sat.fSimHeatRate * zoneRateMult * dtMin;
          if (z.fRoomTemp > z.fSetpoint) z.fRoomTemp = z.fSetpoint;
        }
      } else {
        if (z.fRoomTemp > state.sat.fSimOutdoorTemp) {
          z.fRoomTemp -= settings.sat.fSimCoolRate * zoneRateMult * dtMin;
          if (z.fRoomTemp < state.sat.fSimOutdoorTemp) z.fRoomTemp = state.sat.fSimOutdoorTemp;
        }
      }
      z.bRoomValid    = true;
      z.iLastUpdateMs = now;  // keep fresh so the staleness gate does not drop it
    }
  }

  // --- Multi-area synthetic temps (TASK-798 / plan §12 F3, 1a) ---
  // The multi-AREA path (satGetWeightedRoomTemp) is distinct from multi-ZONE:
  // it weight-averages several room sensors into ONE PID input. Under sim with
  // no external area feed every area is invalid, so the weighted average returns
  // NAN and the multi-area code never runs. Synthesize area temps with the same
  // house model as zones (area 0 = living room at target, areas 1+ ~2 °C lower
  // with per-area variation and faster response) so satGetWeightedRoomTemp
  // exercises under simulation.
  if (settings.sat.bMultiArea && settings.sat.iMultiAreaCount > 0) {
    uint8_t ac = settings.sat.iMultiAreaCount;
    if (ac > SAT_MAX_AREAS) ac = SAT_MAX_AREAS;
    for (uint8_t ai = 0; ai < ac; ai++) {
      const float areaSp   = (ai == 0) ? settings.sat.fTargetTemp
                                       : settings.sat.fTargetTemp - 2.0f - (float)ai * 0.3f;
      const float rateMult = (ai == 0) ? 1.0f : (1.0f + 0.3f * (float)ai);
      if (!state.sat.bAreaValid[ai]) state.sat.fAreaTemp[ai] = state.sat.fSimRoomTemp;
      if (state.sat.bSimFlameOn) {
        if (state.sat.fAreaTemp[ai] < areaSp) {
          state.sat.fAreaTemp[ai] += settings.sat.fSimHeatRate * rateMult * dtMin;
          if (state.sat.fAreaTemp[ai] > areaSp) state.sat.fAreaTemp[ai] = areaSp;
        }
      } else {
        if (state.sat.fAreaTemp[ai] > state.sat.fSimOutdoorTemp) {
          state.sat.fAreaTemp[ai] -= settings.sat.fSimCoolRate * rateMult * dtMin;
          if (state.sat.fAreaTemp[ai] < state.sat.fSimOutdoorTemp) state.sat.fAreaTemp[ai] = state.sat.fSimOutdoorTemp;
        }
      }
      state.sat.bAreaValid[ai]  = true;
      state.sat.iAreaLastMs[ai] = now;  // keep fresh vs SAT_AREA_STALE_MS
    }
  }
}

// Scenario-event injector (TASK-797 / plan §12 F2). Called from the REST
// handler. Returns false on unknown event or when simulation is inactive.
// value/durationS are pre-parsed; durationS<=0 falls back to a default window.
bool satSimInjectEvent(const char* event, float value, int32_t durationS)
{
  if (!settings.sat.bSimulation || !event) return false;
  const uint32_t now = millis();
  const uint32_t durMs = (durationS > 0 ? (uint32_t)durationS : 600U) * 1000UL;  // default 10 min

  if (strcasecmp_P(event, PSTR("window_open")) == 0) {
    // value = loss multiplier (default 3x); clamp to a sane band.
    float m = (value > 0.0f) ? value : 3.0f;
    if (m < 1.0f) m = 1.0f;
    if (m > 20.0f) m = 20.0f;
    state.sat.fSimWindowLossMult = m;
    state.sat.iSimWindowExpiryMs = now + durMs;
    if (state.sat.iSimWindowExpiryMs == 0) state.sat.iSimWindowExpiryMs = 1;  // avoid the 0 sentinel
    SATDebugTf(PSTR("SAT SIM: window_open x%.1f for %lds\r\n"), m, (long)(durMs / 1000UL));
    satNarratef_P(PSTR("Scenario: window-open x%.1f for %lds"), m, (long)(durMs/1000UL));
    return true;
  }
  if (strcasecmp_P(event, PSTR("solar_gain")) == 0) {
    // value = additive warming °C/min (default 0.5); clamp.
    float g = (value != 0.0f) ? value : 0.5f;
    if (g < 0.0f) g = 0.0f;
    if (g > 5.0f) g = 5.0f;
    state.sat.fSimSolarGainC = g;
    state.sat.iSimSolarExpiryMs = now + durMs;
    if (state.sat.iSimSolarExpiryMs == 0) state.sat.iSimSolarExpiryMs = 1;
    SATDebugTf(PSTR("SAT SIM: solar_gain %.2fC/min for %lds\r\n"), g, (long)(durMs / 1000UL));
    satNarratef_P(PSTR("Scenario: solar gain %.2fC/min for %lds"), g, (long)(durMs/1000UL));
    return true;
  }
  if (strcasecmp_P(event, PSTR("sensor_noise")) == 0) {
    // value = ± noise amplitude °C on flow/return reads (default 0.5); clamp.
    // value 0 with an explicit event turns noise OFF immediately.
    float a = (value != 0.0f) ? value : 0.5f;
    if (a < 0.0f) a = 0.0f;
    if (a > 5.0f) a = 5.0f;
    state.sat.fSimNoiseAmplitudeC = a;
    if (a == 0.0f) {
      state.sat.iSimNoiseExpiryMs = 0;  // off
      SATDebugTln(F("SAT SIM: sensor_noise off"));
    } else {
      state.sat.iSimNoiseExpiryMs = now + durMs;
      if (state.sat.iSimNoiseExpiryMs == 0) state.sat.iSimNoiseExpiryMs = 1;
      SATDebugTf(PSTR("SAT SIM: sensor_noise +/-%.2fC for %lds\r\n"), a, (long)(durMs / 1000UL));
      satNarratef_P(PSTR("Scenario: sensor noise +/-%.2fC for %lds"), a, (long)(durMs/1000UL));
    }
    return true;
  }
  if (strcasecmp_P(event, PSTR("sensor_dropout")) == 0) {
    // TASK-799 F4 (2b): drop the room sensor for duration_s — satGetRoomTemp
    // returns NAN, exercising the ghost-Tr / fallback path. value 0 = cancel.
    if (value == 0.0f && durationS == 0) {
      state.sat.iSimDropoutExpiryMs = 0;
      SATDebugTln(F("SAT SIM: sensor_dropout off"));
    } else {
      state.sat.iSimDropoutExpiryMs = now + durMs;
      if (state.sat.iSimDropoutExpiryMs == 0) state.sat.iSimDropoutExpiryMs = 1;
      SATDebugTf(PSTR("SAT SIM: sensor_dropout for %lds\r\n"), (long)(durMs / 1000UL));
    }
    return true;
  }
  if (strcasecmp_P(event, PSTR("dhw_demand")) == 0) {
    // TASK-800 F5: trigger a DHW draw for duration_s (default 2 min). Forces
    // DHW-active in satControlLoop (flame-steal, CH suppressed) so the cycle
    // classifier sees DHW/MIXED. value 0 + no duration cancels an active draw.
    if (value == 0.0f && durationS == 0) {
      state.sat.iSimDhwExpiryMs = 0;
      SATDebugTln(F("SAT SIM: dhw_demand off"));
    } else {
      uint32_t d = (durationS > 0 ? (uint32_t)durationS * 1000UL : SAT_SIM_DHW_DRAW_MS);
      state.sat.iSimDhwExpiryMs = now + d;
      if (state.sat.iSimDhwExpiryMs == 0) state.sat.iSimDhwExpiryMs = 1;
      SATDebugTf(PSTR("SAT SIM: dhw_demand for %lds\r\n"), (long)(d / 1000UL));
      satNarrate_P(PSTR("Scenario: DHW draw simulated"));
    }
    return true;
  }
  // pressure_drop deferred (SATpressure coupling); pv_surplus already has its
  // own /api/v2/sat/pvsurplus endpoint. Unknown event -> false.
  return false;
}

//=====================================================================
//=== Thermal Drop Learning (Task #21) ===
//=====================================================================
static void satUpdateThermalLearning()
{
  if (!settings.sat.bEnabled && !state.sat.bFallbackActive) return;

  uint32_t now = millis();
  bool flame = satIsFlameOn();
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

  // TASK-521: NaN room temp means no valid source -- skip rate calc to avoid corrupting EMA.
  if (isnan(roomTemp)) {
    state.sat.fIndoorRiseRate = _solar_riseRateEma;
    return;
  }

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

  // Detect solar gain condition: rising fast + low boiler modulation + at least one valve open.
  // Python solar_gain.py ANDs valves_open into the detection; bValvesOpen defaults true when no
  // TRV data is available, so this only suppresses detection when valves are known to be closed.
  float modulation = OTcurrentSystemState.RelModLevel;
  bool risingFast = (_solar_riseRateEma > settings.sat.fSolarMinRiseRate);
  bool lowModulation = (modulation < 20.0f);  // 20% threshold matches Python SAT reference
  bool valvesOpen = state.sat.bValvesOpen;

  if (!_solar_wasActive) {
    // Not yet active: require sustained rising + low modulation + valves open for 10 min
    if (risingFast && lowModulation && valvesOpen) {
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

  // TASK-795 §4.2: drain the boiler-detected edge flag in cooperative context.
  // Runs the heavy writeSettings() teardown here, not in the RX edge hook. Also
  // a defensive backstop: re-checks presence each loop in case the edge hook was
  // missed (e.g. a frame type not routed through it).
  satConsumeBoilerDetected();
  if (settings.sat.bSimulation && satBoilerHardwarePresent()) satOnBoilerDetected();

  // TASK-565: detect OT-bus offline->online edge and reset write-on-change
  // cache so a recovering boiler immediately receives the current SAT state
  // (CS/MM/CH/TC) instead of waiting for the staleness window to expire.
  bool otOnline = state.otBus.bOnline;
  if (otOnline && !_sat_prevOTOnline) {
    satResetCmdCache();
  }
  _sat_prevOTOnline = otOnline;

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
  bool currentFlame = satIsFlameOn();
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
  // TASK-800 F5: under simulation, a synthetic DHW draw (F2 dhw_demand event or
  // the light schedule, both managed in satUpdateSimulation) forces DHW-active
  // so CH is suppressed (flame-steal) and the cycle classifier sees DHW/MIXED.
  if (settings.sat.bSimulation && state.sat.iSimDhwExpiryMs != 0) {
    state.sat.bDhwActive = true;
  } else {
    state.sat.bDhwActive = (OTcurrentSystemState.SlaveStatus & 0x04) != 0; // Bit 2 = DHW active
  }
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

  // Validate room temp — count consecutive failures.
  // TASK-521: NaN means satGetRoomTemp() found no valid source (no BLE, no MQTT external,
  // no MultiArea, no MsgID 24 from a thermostat -- only a boot-time 0.0 ghost). Treat as
  // invalid and let the existing skip-count machinery hold last-good for SAT_MAX_SKIP_COUNT
  // cycles, then trip safety (CS=0 via satDisable -> boiler returns to thermostat control).
  // Log once on entry to the skip window so the operator understands the disengagement.
  if (isnan(roomTemp) || roomTemp < -10.0f || roomTemp > 50.0f) {
    if (_sat_consecutiveSkips == 0) {
      if (isnan(roomTemp)) {
        DebugTln(F("SAT: all room-temp sources lost (no BLE/external/MultiArea/Tr) -- holding last-good, will disengage if not restored"));
      }
    }
    _sat_consecutiveSkips++;
    if (_sat_consecutiveSkips >= SAT_MAX_SKIP_COUNT) {
      DebugTln(F("SAT SAFETY: too many invalid room temp readings, disabling"));
      state.sat.bSafetyTripped = true;
      satNarrate_P(PSTR("SAFETY TRIPPED: SAT regulation halted"));
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
        // TASK-565: write-on-change. Thermal safe-fallback path.
        satEnqueueIfChangedCS((float)(int)state.sat.fFinalSetpoint);
        satEnqueueIfChangedMM(settings.sat.iMaxRelModulation);
        satEnqueueIfChangedCH(1);
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
      // TASK-565: write-on-change. Summer-simmer path.
      satEnqueueIfChangedCS(10.0f);
      satEnqueueIfChangedMM(settings.sat.iMaxRelModulation);
      satEnqueueIfChangedCH(0);
    }
    return; // Skip rest of control loop
  }

  // --- TRV valve detection (Task #29): skip heating when all valves closed ---
  if (!state.sat.bValvesOpen) {
    state.sat.fFinalSetpoint = SAT_MIN_SETPOINT;
    if (hasOTCommandInterface()) {
      // TASK-565: write-on-change. Valves-closed path.
      satEnqueueIfChangedCS(10.0f);
      satEnqueueIfChangedMM(settings.sat.iMaxRelModulation);
      satEnqueueIfChangedCH(0);
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

  // --- PV-surplus setpoint boost (TASK-640) ---
  // Run before heating-curve calc so the boost is reflected in this cycle.
  // satUpdatePvBoost() never mutates settings.sat.fTargetTemp; the boost is
  // additive on effectiveTarget only.
  satUpdatePvBoost();

  float effectiveTarget = targetTemp + state.sat.fComfortOffset;
  if (state.sat.bPvBoostActive)
    effectiveTarget += state.sat.fPvBoostAppliedC;

  // --- Calculate heating curve ---
  float curveValue = satCalcHeatingCurve(effectiveTarget, outsideTemp);

  // --- Calculate PID output (includes heating curve value) ---
  float pidOutput = satPidUpdate(roomTemp, effectiveTarget, curveValue, satGetFlowTemp());

  // --- Multi-zone PID override (Task #233) ---
  // When sat_zone_count > 1: run each zone's PID and use the most-demanding setpoint.
  // Zone 1 is always computed; its output replaces the primary PID output.
  // Zones 2-4 are only evaluated when sat_zone_count > 1.
  // If all zones are inactive, falls back to single-zone (primary) pidOutput.
  if (settings.sat.iZoneCount > 1) {
    float zoneOutputs[SAT_MAX_ZONES];
    uint8_t activeZones = 0;
    float maxOvershoot = 0.0f;
    uint8_t zoneCount = settings.sat.iZoneCount;
    if (zoneCount > SAT_MAX_ZONES) zoneCount = SAT_MAX_ZONES;

    for (uint8_t z = 0; z < zoneCount; z++) {
      float zOut = satZonePidStep(z, outsideTemp);
      if (zOut > SAT_MIN_SETPOINT) {
        zoneOutputs[activeZones++] = zOut;
        // Track overshoot: room above setpoint means this zone is over-heated
        float overshoot = satZones[z].fRoomTemp - satZones[z].fSetpoint;
        if (overshoot > maxOvershoot) maxOvershoot = overshoot;
      }
    }

    if (activeZones == 0) {
      // No active zones: keep primary pidOutput
    } else if (activeZones < 2) {
      // Single active zone: use its output directly (backward compat, AC#5)
      pidOutput = zoneOutputs[0];
      SATDebugTf(PSTR("SAT: multi-zone 1 active zone, setpoint=%.1f\r\n"), pidOutput);
    } else {
      // P75 aggregation: sort ascending (bubble sort, max 4 elements)
      for (uint8_t i = 0; i < activeZones - 1; i++) {
        for (uint8_t j = 0; j < activeZones - 1 - i; j++) {
          if (zoneOutputs[j] > zoneOutputs[j + 1]) {
            float tmp = zoneOutputs[j];
            zoneOutputs[j] = zoneOutputs[j + 1];
            zoneOutputs[j + 1] = tmp;
          }
        }
      }
      // P75 index: ceiling rank method — ceil(0.75 * N) - 1, 0-based
      uint8_t p75idx = (uint8_t)(ceilf(0.75f * (float)activeZones)) - 1;
      float aggregate = zoneOutputs[p75idx] + settings.sat.fZoneAggregationHeadroom;

      // Overshoot cap: reduce aggregate by the maximum zone overshoot
      // so over-heated zones are not driven further above their setpoint
      if (maxOvershoot > 0.0f) {
        aggregate -= maxOvershoot;
        SATDebugTf(PSTR("SAT: overshoot cap %.1f applied\r\n"), maxOvershoot);
      }

      if (aggregate < SAT_MIN_SETPOINT) aggregate = SAT_MIN_SETPOINT;
      pidOutput = aggregate;
      SATDebugTf(PSTR("SAT: multi-zone P75[%u/%u]=%.1f hdroom=%.1f overshoot=%.1f -> %.1f\r\n"),
                 p75idx, activeZones, zoneOutputs[p75idx],
                 settings.sat.fZoneAggregationHeadroom, maxOvershoot, pidOutput);
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
    bool flame = satIsFlameOn();
    if (!flame) {
      finalSetpoint += settings.sat.fFlameOffOffset;
      if (finalSetpoint > maxSetpoint) finalSetpoint = maxSetpoint;
      state.sat.fFinalSetpoint = finalSetpoint;
    }
  }

  // --- Send CS= and MM= commands to boiler when an OT command interface is available ---
  if (hasOTCommandInterface()) {
    // TASK-565: write-on-change. Main control block.
    satEnqueueIfChangedCS(finalSetpoint);
    satEnqueueIfChangedMM(state.sat.iCurrentModulation);
    bool wantHeating = (finalSetpoint > SAT_MIN_SETPOINT);
    satEnqueueIfChangedCH(wantHeating ? 1 : 0);
    // Immergas quirk: send TP=11:12=<setpoint> alongside MM=. Kept on
    // unconditional enqueue per TASK-565: rare quirk path, not steady loop.
    if (satGetManufacturerQuirks() & SAT_QUIRK_IMMERGAS_TP) {
      char cmdBuf[16];
      snprintf_P(cmdBuf, sizeof(cmdBuf), PSTR("TP=11:12=%.0f"), finalSetpoint);
      addCommandToQueue(cmdBuf, strlen(cmdBuf), false, 0);
    }
    // Push SAT target to thermostat display via TC= command (Task #31)
    if (settings.sat.bPushSetpoint) {
      satEnqueueIfChangedTC(settings.sat.fTargetTemp);
    }
    _sat_picFailCount = 0;
  } else {
    _sat_picFailCount++;
    if (_sat_picFailCount >= SAT_MAX_PIC_FAILS) {
      DebugTln(F("SAT SAFETY: PIC unavailable for too long, disabling"));
      state.sat.bSafetyTripped = true;
      satNarrate_P(PSTR("SAFETY TRIPPED: SAT regulation halted"));
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
