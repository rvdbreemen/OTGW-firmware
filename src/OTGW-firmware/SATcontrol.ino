/*
***************************************************************************
**  Module   : SATcontrol.ino
**  Description: SAT (Smart Autotune Thermostat) control loop
**
**  Ported from SAT releases/thermo-nova
**  (https://github.com/Alexwijn/SAT) with permission from the authors.
**
**  Standalone smart heating controller embedded in OTGW firmware.
**  Uses OpenTherm data directly from OTcurrentSystemState.
**  Optional external sensor input via MQTT/REST.
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License. See bottom of OTGW-firmware.h
***************************************************************************
*/

// --- Heating Curve Constants ---
static const float SAT_HC_BASE_OFFSET_FLOOR  = 20.0f;   // Underfloor base offset
static const float SAT_HC_BASE_OFFSET_RAD    = 27.2f;   // Radiator base offset
static const float SAT_HC_REF_TEMP           = 20.0f;   // Reference temperature

// --- Control Constants ---
static const float SAT_MIN_SETPOINT          = 10.0f;   // Minimum boiler setpoint °C
static const float SAT_MAX_SETPOINT_DEFAULT  = 75.0f;   // Default max boiler setpoint °C
static const float SAT_FLOW_OFFSET_CONTINUOUS = 5.0f;   // Flow temp offset for continuous mode smoothing
static const float SAT_PWM_MIN_ON_SEC        = 30.0f;   // Minimum flame-on time in PWM mode

// --- Safety Constants ---
static const float    SAT_HARD_MAX_FLOOR     = 50.0f;   // Absolute ceiling for underfloor heating
static const float    SAT_HARD_MAX_RAD       = 80.0f;   // Absolute ceiling for radiator heating
static const uint32_t SAT_STALE_TEMP_MS      = 300000UL; // 5 min: external indoor temp considered stale
static const uint32_t SAT_STALE_OUTDOOR_MS   = 600000UL; // 10 min: external outdoor temp considered stale
static const uint8_t  SAT_MAX_SKIP_COUNT     = 10;       // Consecutive invalid-input skips before disable
static const uint8_t  SAT_MAX_PIC_FAILS      = 5;        // Consecutive PIC comm failures before disable

// --- Heating System Helper Functions ---
// Returns the effective heating system (resolves AUTO to detected or fallback)
static uint8_t satGetEffectiveHeatingSystem()
{
  if (settings.sat.iHeatingSystem == SAT_HSYS_AUTO)
    return state.sat.iDetectedHeatingSystem;
  return settings.sat.iHeatingSystem;
}

// Returns max boiler setpoint for the current heating system
static float satGetMaxSetpoint()
{
  switch (satGetEffectiveHeatingSystem()) {
    case SAT_HSYS_HEAT_PUMP:  return 40.0f;
    case SAT_HSYS_UNDERFLOOR: return 45.0f;
    case SAT_HSYS_RADIATORS:
    default:                  return 62.0f;
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

// Returns the name string for the current heating system
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

// --- OPV Calibration Constants ---
static const uint32_t SAT_CALIB_TIMEOUT_MS    = 1800000UL; // 30 min total timeout
static const uint32_t SAT_CALIB_FLAME_WAIT_MS = 180000UL;  // 3 min wait for flame
static const uint32_t SAT_CALIB_MEASURE_MS    = 1200000UL; // 20 min measuring phase
static const uint32_t SAT_CALIB_SAMPLE_MS     = 10000UL;   // Sample every 10s
static const float    SAT_CALIB_WARM_DELTA    = 5.0f;      // Temp must rise 5C above start

// OPV calibration state machine - called from control loop when calibration is active
static void satOvpCalibrate()
{
  float boilerTemp = OTcurrentSystemState.Tboiler;
  bool  flameOn    = (OTcurrentSystemState.MasterStatus & 0x08) != 0;
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
      DebugTf(PSTR("OPV: calibration started, CS=%.1f MM=0, boiler=%.1f\r\n"), calibSetpoint, boilerTemp);
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
        DebugTf(PSTR("OPV: warming done, boiler=%.1f, starting measurement\r\n"), boilerTemp);
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
        // Measurement complete
        settings.sat.fOvpValue = state.sat.fCalibMaxTemp;
        settings.sat.bOvpEnabled = true;
        DebugTf(PSTR("OPV: calibration DONE! OPV=%.1f from %u samples\r\n"),
                state.sat.fCalibMaxTemp, state.sat.iCalibSamples);
        state.sat.eCalibPhase = SAT_CALIB_DONE;
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
  DebugTln(F("OPV: calibration requested"));
}

// Cancel OPV calibration
static void satOvpStopCalibration()
{
  if (state.sat.eCalibPhase == SAT_CALIB_IDLE) return;
  DebugTln(F("OPV: calibration cancelled"));
  state.sat.eCalibPhase = SAT_CALIB_FAILED; // Will trigger recovery on next call
}

// --- Boiler Status Tracking ---
static bool     _bs_prevFlame          = false;
static float    _bs_prevModulation     = 0.0f;
static float    _bs_prevBoilerTemp     = 0.0f;
static uint32_t _bs_stateEntryMs       = 0;
static uint32_t _bs_flameOnMs          = 0;
static uint32_t _bs_flameOffMs         = 0;
static uint32_t _bs_lastCycleDurationMs = 0;

// --- Previous flame state for edge detection ---
static bool     _sat_prevFlameState = false;

// --- Timer for control loop (initial value, updated from settings in initSAT) ---
DECLARE_TIMER_SEC(timerSATControl, settings.sat.iControlInterval, CATCH_UP_MISSED_TICKS);

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
static const uint32_t BS_ANTI_CYCLE_MIN_OFF_MS   = 180000UL;  // 180s min OFF between cycles
static const uint32_t BS_STALLED_IGNITION_MIN_MS  = 600000UL;  // 600s default stalled threshold
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
    if (_bs_flameOnMs > 0) {
      _bs_lastCycleDurationMs = now - _bs_flameOnMs;
    }
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
      uint32_t stalledThreshold = BS_STALLED_IGNITION_MIN_MS;
      if (_bs_lastCycleDurationMs > 0 && _bs_lastCycleDurationMs * 3 > stalledThreshold) {
        stalledThreshold = _bs_lastCycleDurationMs * 3;
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
  strlcpy_P(buf, (PGM_P)pgm_read_ptr(&_bsNames[idx]), bufLen);
}

//=====================================================================
//=== PWM Control Mode ===
//=====================================================================
// PWM state tracking for effective temperature and flame timing
static float    _pwm_effectiveBoilerTemp = 0.0f;
static uint32_t _pwm_flameOnMs          = 0;
static uint32_t _pwm_lastSampleMs       = 0;
static bool     _pwm_waitingForFlame    = false;

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
  bool flame = (OTcurrentSystemState.MasterStatus & 0x08) != 0;
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
  }
  if (!flame && !_pwm_waitingForFlame && state.sat.bPwmFlameRequested) {
    _pwm_waitingForFlame = true;
  }

  // --- 5-range duty cycle mapper ---
  uint32_t onTimeMs, offTimeMs;

  if (duty >= 0.95f) {
    // Range 5: Over-max - continuous ON
    state.sat.bPwmFlameRequested = true;
    return pidOutput;
  } else if (duty <= 0.05f) {
    // Range 1: Ultra-low - minimum on, long off
    onTimeMs  = minOnMs;
    offTimeMs = maxMs - minOnMs;
  } else if (duty < 0.3f) {
    // Range 2: Low - on=min, off scaled
    onTimeMs  = minOnMs;
    offTimeMs = (uint32_t)((float)minOnMs * (1.0f - duty) / duty);
    if (offTimeMs > maxMs - minOnMs) offTimeMs = maxMs - minOnMs;
  } else if (duty < 0.7f) {
    // Range 3: Mid - proportional
    onTimeMs  = (uint32_t)(duty * (float)upperMs);
    offTimeMs = upperMs - onTimeMs;
    if (onTimeMs < minOnMs) onTimeMs = minOnMs;
  } else {
    // Range 4: High - long on, short off
    offTimeMs = (uint32_t)((1.0f - duty) * (float)upperMs);
    onTimeMs  = upperMs - offTimeMs;
    if (onTimeMs < minOnMs) onTimeMs = minOnMs;
  }

  // --- PWM state machine ---
  uint32_t sinceFlameStart = millis() - satCycleGetFlameOnStartMs();
  if (state.sat.bPwmFlameRequested) {
    // ON phase: keep flame on for calculated duration (minimum: minOnMs)
    if (sinceFlameStart < onTimeMs || sinceFlameStart < minOnMs) {
      return pidOutput;
    }
    state.sat.bPwmFlameRequested = false;
    return SAT_MIN_SETPOINT;
  } else {
    // OFF phase: wait for off duration then restart
    uint32_t sinceFlameOff = millis() - satCycleGetFlameOffStartMs();
    if (sinceFlameOff >= offTimeMs) {
      state.sat.bPwmFlameRequested = true;
      _pwm_waitingForFlame = true;
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
  else if (strcasecmp_P(value, PSTR("none")) == 0)     { newPreset = SAT_PRESET_NONE; }
  else return; // Unknown preset

  state.sat.eActivePreset = newPreset;
  if (newPreset != SAT_PRESET_NONE) {
    settings.sat.fTargetTemp = newTarget;
    // Reset PID integral to prevent overshoot on large temp jumps
    state.sat.fPidI = 0.0f;
    DebugTf(PSTR("SAT: preset '%s' -> target %.1f, integral reset\r\n"), satGetPresetName(newPreset), newTarget);
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
    DebugTln(F("SAT: window opened, starting timer"));
  }
  else if (!isOpen && state.sat.bWindowOpen) {
    // Window closed - restore previous state
    state.sat.bWindowOpen = false;
    state.sat.iWindowOpenSinceMs = 0;
    if (state.sat.fPreWindowTarget > 0.0f) {
      settings.sat.fTargetTemp = state.sat.fPreWindowTarget;
      state.sat.eActivePreset = (SATPreset)state.sat.iPreWindowPreset;
      state.sat.fPreWindowTarget = 0.0f;
      satResetIntegral();
      DebugTf(PSTR("SAT: window closed, restored target %.1f\r\n"), settings.sat.fTargetTemp);
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
    state.sat.iPreWindowPreset = (uint8_t)state.sat.eActivePreset;
    state.sat.eActivePreset = SAT_PRESET_ACTIVITY;
    settings.sat.fTargetTemp = settings.sat.fPresetActivity;
    satResetIntegral();
    DebugTf(PSTR("SAT: window open > %us, switched to Activity (%.1f)\r\n"),
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
  // Prevents setpoint spikes on overheat and ensures smooth ramp-down
  float boilerTemp = OTcurrentSystemState.Tboiler;
  float flowOffset = settings.sat.fFlowOffset;
  float minAllowed = boilerTemp - flowOffset;

  if (pidOutput < minAllowed && boilerTemp > pidOutput + flowOffset) {
    return minAllowed;
  }

  return pidOutput;
}

//=====================================================================
//=== Get Room Temperature (OT bus or external) ===
//=====================================================================
static float satGetRoomTemp()
{
  if (settings.sat.bUseExternalTemp && state.sat.bExternalTempValid) {
    // Check staleness — if no update for 5 min, fall back to OT bus
    if ((millis() - state.sat.iExternalTempLastMs) > SAT_STALE_TEMP_MS) {
      state.sat.bExternalTempValid = false;
      DebugTln(F("SAT: external indoor temp stale, falling back to OT bus"));
    } else {
      return state.sat.fExternalTemp;
    }
  }
  return OTcurrentSystemState.Tr;  // OT message ID 24
}

//=== Get Outside Temperature (OT bus or external) ===
static float satGetOutsideTemp()
{
  if (state.sat.bExternalOutdoorValid) {
    // Check staleness — if no update for 10 min, fall back to OT bus
    if ((millis() - state.sat.iExternalOutdoorLastMs) > SAT_STALE_OUTDOOR_MS) {
      state.sat.bExternalOutdoorValid = false;
      DebugTln(F("SAT: external outdoor temp stale, falling back to OT bus"));
    } else {
      return state.sat.fExternalOutdoor;
    }
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
    DebugTf(PSTR("SAT: external indoor temp set to %.1f°C\r\n"), temp);
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
    DebugTf(PSTR("SAT: external outdoor temp set to %.1f°C\r\n"), temp);
    return true;
  }
  return false;
}

// Returns true if the value was valid and applied
bool satHandleTargetTemp(const char* value)
{
  if (!value || !*value) return false;
  char* endp = nullptr;
  float temp = strtof(value, &endp);
  if (endp == value || *endp != '\0') return false;  // non-numeric input
  if (temp >= 5.0f && temp <= 30.0f) {
    // Go through updateSetting() to persist via deferred flush
    updateSetting("SATtargettemp", value);
    DebugTf(PSTR("SAT: target temp set to %.1f°C\r\n"), temp);
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
  DebugTf(PSTR("SAT: %s\r\n"), enabled ? "enabled" : "disabled");
}

//=== PID State Persistence (Tasks #6, #49) ===
static const char* SAT_PID_STATE_FILE PROGMEM = "/sat_pid_state.json";
static uint32_t _pidLastSaveMs = 0;

void satSavePidState()
{
  File f = LittleFS.open(FPSTR(SAT_PID_STATE_FILE), "w");
  if (!f) return;
  char buf[128];
  snprintf_P(buf, sizeof(buf), PSTR("{\"i\":%.4f,\"d\":%.4f,\"err\":%.2f}"),
             state.sat.fPidI, state.sat.fPidD, state.sat.fError);
  f.print(buf);
  f.close();
  _pidLastSaveMs = millis();
}

void satLoadPidState()
{
  File f = LittleFS.open(FPSTR(SAT_PID_STATE_FILE), "r");
  if (!f) return;
  char buf[128];
  size_t len = f.readBytes(buf, sizeof(buf) - 1);
  buf[len] = 0;
  f.close();
  // Simple parse: extract values from {"i":X,"d":Y,"err":Z}
  float i = 0, d = 0, err = 0;
  char* p;
  if ((p = strstr(buf, "\"i\":")) != nullptr) i = atof(p + 4);
  if ((p = strstr(buf, "\"d\":")) != nullptr) d = atof(p + 4);
  if ((p = strstr(buf, "\"err\":")) != nullptr) err = atof(p + 6);
  state.sat.fPidI = i;
  state.sat.fPidD = d;
  state.sat.fError = err;
  DebugTf(PSTR("SAT: PID state restored (I=%.4f D=%.4f err=%.2f)\r\n"), i, d, err);
}

//=== Cleanly disable SAT and release boiler control ===
void satDisable()
{
  state.sat.eControlMode = SAT_MODE_OFF;
  state.sat.bActive = false;
  state.sat.fFinalSetpoint = 0.0f;
  satSavePidState(); // Persist PID state before reset
  satPidReset();
  // Send CS=0 to release control setpoint override -- thermostat regains control
  addCommandToQueue("CS=0", 4, false, 0);
  DebugTln(F("SAT: disabled, sent CS=0 to release boiler control"));
}

void satHandleControlMode(const char* value)
{
  if (!value || !*value) return;
  if (strcasecmp_P(value, PSTR("continuous")) == 0 || strcmp(value, "1") == 0) {
    state.sat.eControlMode = SAT_MODE_CONTINUOUS;
  } else if (strcasecmp_P(value, PSTR("pwm")) == 0 || strcmp(value, "2") == 0) {
    state.sat.eControlMode = SAT_MODE_PWM;
  } else if (strcasecmp_P(value, PSTR("auto")) == 0 || strcmp(value, "0") == 0) {
    state.sat.eControlMode = SAT_MODE_CONTINUOUS; // Start in continuous, auto-switch
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
  sendJsonMapEntry(F("last_cycle_class"),     (int32_t)state.sat.eLastCycleClass);
  satSendJsonFloat(F("cycle_max_flow"),       state.sat.fCycleMaxFlow, 1);
  satSendJsonFloat(F("cycle_overshoot_sec"),  state.sat.fCycleOvershootSec, 0);
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
  satSendJsonFloat(F("max_setpoint_system"), satGetMaxSetpoint(), 1);
  sendJsonMapEntry(F("external_temp_valid"),  state.sat.bExternalTempValid);
  sendJsonMapEntry(F("external_outdoor_valid"), state.sat.bExternalOutdoorValid);
  sendJsonMapEntry(F("safety_tripped"),       state.sat.bSafetyTripped);
  sendJsonMapEntry(F("window_open"),           state.sat.bWindowOpen);
  sendJsonMapEntry(F("window_detection"),      settings.sat.bWindowDetection);
  sendJsonMapEntry(F("push_setpoint"),         settings.sat.bPushSetpoint);
  satSendJsonFloat(F("flame_off_offset"),      settings.sat.fFlameOffOffset, 1);
  sendJsonMapEntry(F("force_pwm"),             settings.sat.bForcePWM);
  satSendJsonFloat(F("flow_offset"),           settings.sat.fFlowOffset, 1);
  satSendJsonFloat(F("pressure"),              state.sat.fSmoothedPressure, 2);
  satSendJsonFloat(F("pressure_drop_rate"),    state.sat.fPressureDropRate, 3);
  sendJsonMapEntry(F("pressure_alarm"),        state.sat.bPressureAlarm);
  sendEndJsonMap("");
}

//=====================================================================
//=== MQTT Publishing ===
//=====================================================================
void satPublishMQTT()
{
  if (!settings.mqtt.bEnable || !state.mqtt.bConnected) return;
  if (!settings.sat.bEnabled) return;

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

  dtostrf(state.sat.fRawDerivative, 1, 4, valBuf);
  sendMQTTData(F("sat/raw_derivative"), valBuf, false);

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

  // PWM duty
  dtostrf(state.sat.fPwmDutyCycle, 1, 2, valBuf);
  sendMQTTData(F("sat/pwm_duty"), valBuf, false);

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

  // Window detection
  sendMQTTData(F("sat/window_open"), state.sat.bWindowOpen ? "true" : "false", false);

  // Pressure monitoring
  { char pBuf[12];
    dtostrf(state.sat.fSmoothedPressure, 1, 2, pBuf);
    sendMQTTData(F("sat/pressure"), pBuf, false);
    dtostrf(state.sat.fPressureDropRate, 1, 3, pBuf);
    sendMQTTData(F("sat/pressure_drop_rate"), pBuf, false);
    sendMQTTData(F("sat/pressure_alarm"), state.sat.bPressureAlarm ? "true" : "false", false);
  }
}

//=====================================================================
//=== Pressure Monitoring (Task #10) ===
//=====================================================================
static float _press_prevSmoothed = 0.0f;
static uint32_t _press_prevMs    = 0;

static void satUpdatePressure()
{
  float raw = OTcurrentSystemState.CHPressure;
  if (raw < 0.01f) return;  // No pressure reading available

  uint32_t now = millis();

  // EMA smoothing (alpha=0.05 per SAT Python)
  if (state.sat.fSmoothedPressure < 0.01f) {
    state.sat.fSmoothedPressure = raw;  // Initialize
    _press_prevSmoothed = raw;
    _press_prevMs = now;
    return;
  }

  state.sat.fSmoothedPressure = 0.05f * raw + 0.95f * state.sat.fSmoothedPressure;

  // Calculate drop rate (bar/hour) from smoothed readings
  float dt = (float)(now - _press_prevMs) / 1000.0f;
  if (dt > 30.0f) {  // Update drop rate every 30s minimum
    float delta = _press_prevSmoothed - state.sat.fSmoothedPressure;
    state.sat.fPressureDropRate = (delta / dt) * 3600.0f;  // Convert to bar/hour
    _press_prevSmoothed = state.sat.fSmoothedPressure;
    _press_prevMs = now;
  }

  // Check alarm conditions
  bool alarmCond = (state.sat.fSmoothedPressure < settings.sat.fMinPressure ||
                    state.sat.fSmoothedPressure > settings.sat.fMaxPressure ||
                    state.sat.fPressureDropRate > settings.sat.fMaxPressureDrop);

  if (alarmCond) {
    if (state.sat.iPressureAlarmSinceMs == 0) {
      state.sat.iPressureAlarmSinceMs = now;
    }
    // Confirm after 120s persistent
    if ((now - state.sat.iPressureAlarmSinceMs) >= 120000UL) {
      if (!state.sat.bPressureAlarm) {
        state.sat.bPressureAlarm = true;
        DebugTf(PSTR("SAT: PRESSURE ALARM (smoothed=%.2f drop=%.3f bar/hr)\r\n"),
                state.sat.fSmoothedPressure, state.sat.fPressureDropRate);
      }
    }
  } else {
    state.sat.iPressureAlarmSinceMs = 0;
    state.sat.bPressureAlarm = false;
  }
}

//=====================================================================
//=== Initialize SAT ===
//=====================================================================
void initSAT()
{
  satPidReset();
  satLoadPidState(); // Restore PID state from LittleFS after reset
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
    DebugTln(F("SAT: boot safety - sent CS=0 to release stale control override"));
  }
  // If no OT command interface is ready yet, _sat_bootCS0sent stays false and
  // the control loop will send CS=0 on its first call when one becomes available.

  // Sync timer to configured interval
  CHANGE_INTERVAL_SEC(timerSATControl, settings.sat.iControlInterval);

  if (settings.sat.bEnabled) {
    state.sat.eControlMode = SAT_MODE_CONTINUOUS;
    DebugTln(F("SAT: initialized and enabled (continuous mode)"));
  } else {
    DebugTln(F("SAT: initialized but disabled"));
  }
}

//=====================================================================
//=== Main Control Loop (called from doBackgroundTasks) ===
//=====================================================================
void satControlLoop()
{
  // --- Fallback detection (Task #19): auto-enable SAT when external control lost ---
  if (!settings.sat.bEnabled && !state.sat.bFallbackActive) {
    // Check if we should auto-enable as fallback
    bool mqttLost = !state.mqtt.bConnected &&
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

  if (!settings.sat.bEnabled || isFlashing()) {
    if (state.sat.bActive) {
      satDisable();
    }
    return;
  }

  // If safety tripped, stay disabled until explicitly re-enabled
  if (state.sat.bSafetyTripped) return;

  // Boot safety deferred: if CS=0 wasn't sent during initSAT(), send it on the
  // first call where an OT command interface is available.
  if (!_sat_bootCS0sent && hasOTCommandInterface()) {
    addCommandToQueue("CS=0", 4, false, 0);
    _sat_bootCS0sent = true;
    DebugTln(F("SAT: deferred boot safety — sent CS=0"));
  }

  // Flame edge detection — always process, independent of timer
  bool currentFlame = (OTcurrentSystemState.Statusflags & 0x08) != 0;
  if (currentFlame != _sat_prevFlameState) {
    satCycleOnFlameChange(currentFlame);
    _sat_prevFlameState = currentFlame;
  }

  // Sample cycle data frequently (every loop call)
  satCycleSample();

  // Main control loop on timer
  if (!DUE(timerSATControl)) return;

  state.sat.bActive = true;
  if (state.sat.eControlMode == SAT_MODE_OFF) {
    state.sat.eControlMode = SAT_MODE_CONTINUOUS;
  }

  // --- DHW detection (Task #3): skip CH control when DHW is active ---
  state.sat.bDhwActive = (OTcurrentSystemState.SlaveStatus & 0x04) != 0; // Bit 2 = DHW active
  if (state.sat.bDhwActive) {
    // DHW has priority - don't adjust CH setpoint, boiler manages itself
    return;
  }

  // --- Read inputs (staleness is checked inside these functions) ---
  float roomTemp = satGetRoomTemp();
  float outsideTemp = satGetOutsideTemp();
  float targetTemp = settings.sat.fTargetTemp;

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
    DebugTln(F("SAT: boiler fault flag detected, skipping control cycle"));
    return; // Skip this control cycle, let boiler handle the fault
  }

  if (outsideTemp < -40.0f || outsideTemp > 50.0f) {
    outsideTemp = 10.0f;  // Safe fallback
  }

  // --- Window detection timer check ---
  _satCheckWindowTimer();

  // --- Pressure monitoring ---
  satUpdatePressure();

  // --- Update boiler status ---
  satUpdateBoilerStatus();

  // --- Calculate heating curve ---
  float curveValue = satCalcHeatingCurve(targetTemp, outsideTemp);

  // --- Calculate PID output (includes heating curve value) ---
  float pidOutput = satPidUpdate(roomTemp, targetTemp, curveValue, OTcurrentSystemState.Tboiler);

  // --- Clamp to valid range ---
  float maxSetpoint = OTcurrentSystemState.MaxTSet;
  if (maxSetpoint < 30.0f) maxSetpoint = SAT_MAX_SETPOINT_DEFAULT;

  // Hard safety ceiling based on heating system type -- never exceeded
  float sysMax = satGetMaxSetpoint();
  float hardMax = (satGetEffectiveHeatingSystem() == SAT_HSYS_UNDERFLOOR) ? SAT_HARD_MAX_FLOOR : SAT_HARD_MAX_RAD;
  if (maxSetpoint > sysMax) maxSetpoint = sysMax;
  if (maxSetpoint > hardMax) maxSetpoint = hardMax;

  if (pidOutput < SAT_MIN_SETPOINT) pidOutput = SAT_MIN_SETPOINT;
  if (pidOutput > maxSetpoint) pidOutput = maxSetpoint;

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
  state.sat.fFinalSetpoint = finalSetpoint;

  // --- Auto-switch between continuous and PWM modes (Tasks #42/#43) ---
  if (settings.sat.bPwmAutoSwitch && !satAlwaysMaxModulation()) {
    float boilerTemp = OTcurrentSystemState.Tboiler;
    static uint32_t _autoSwOvershootSince = 0;
    static uint32_t _autoSwUnderheatSince = 0;

    if (state.sat.eControlMode == SAT_MODE_CONTINUOUS) {
      // Task #42: Auto-enable PWM on sustained overshoot
      if (boilerTemp >= finalSetpoint + 0.5f) {
        if (_autoSwOvershootSince == 0) _autoSwOvershootSince = millis();
        if ((millis() - _autoSwOvershootSince) >= 300000UL) { // 300s sustained
          state.sat.eControlMode = SAT_MODE_PWM;
          _autoSwOvershootSince = 0;
          DebugTln(F("SAT: auto-switch continuous->PWM (sustained overshoot 5min)"));
        }
      } else {
        _autoSwOvershootSince = 0;
      }
    } else if (state.sat.eControlMode == SAT_MODE_PWM) {
      // Task #43: Auto-disable PWM on sustained underheat
      if (boilerTemp <= finalSetpoint - 0.3f) {
        if (_autoSwUnderheatSince == 0) _autoSwUnderheatSince = millis();
        if ((millis() - _autoSwUnderheatSince) >= 180000UL) { // 180s sustained
          state.sat.eControlMode = SAT_MODE_CONTINUOUS;
          _autoSwUnderheatSince = 0;
          DebugTln(F("SAT: auto-switch PWM->continuous (sustained underheat 3min)"));
        }
      } else {
        _autoSwUnderheatSince = 0;
      }
    }
  }

  // --- Modulation suppression: prevent overshoot in continuous mode ---
  if (state.sat.eControlMode == SAT_MODE_CONTINUOUS && !satAlwaysMaxModulation()) {
    float boilerTemp = OTcurrentSystemState.Tboiler;
    float suppressThreshold = finalSetpoint - settings.sat.fModSupOffset;
    float recoveryThreshold = suppressThreshold - 0.5f; // 0.5C hysteresis

    if (!state.sat.bModSuppressed) {
      // Check if we should start suppressing
      if (boilerTemp >= suppressThreshold) {
        if (state.sat.iModSuppressionSinceMs == 0)
          state.sat.iModSuppressionSinceMs = millis();
        if ((millis() - state.sat.iModSuppressionSinceMs) >= (uint32_t)(settings.sat.fModSupDelay * 1000.0f)) {
          state.sat.bModSuppressed = true;
          DebugTf(PSTR("SAT: modulation suppressed (boiler=%.1f >= threshold=%.1f)\r\n"), boilerTemp, suppressThreshold);
        }
      } else {
        state.sat.iModSuppressionSinceMs = 0; // Reset timer
      }
    } else {
      // Check if we should recover
      if (boilerTemp < recoveryThreshold) {
        state.sat.bModSuppressed = false;
        state.sat.iModSuppressionSinceMs = 0;
        DebugTf(PSTR("SAT: modulation suppression lifted (boiler=%.1f < recovery=%.1f)\r\n"), boilerTemp, recoveryThreshold);
      }
    }
  } else {
    state.sat.bModSuppressed = false;
    state.sat.iModSuppressionSinceMs = 0;
  }

  // --- Compute modulation value based on mode, heating system, and suppression ---
  {
    uint8_t mmValue;
    if (satAlwaysMaxModulation()) {
      mmValue = 100;
    } else if (state.sat.bModSuppressed) {
      // Modulation suppression active: send MM=0
      mmValue = 0;
    } else if (state.sat.eControlMode == SAT_MODE_PWM && !state.sat.bPwmFlameRequested) {
      mmValue = 0;
    } else {
      mmValue = settings.sat.iMaxRelModulation;
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

  DebugTf(PSTR("SAT: room=%.1f target=%.1f outside=%.1f curve=%.1f pid=%.1f final=%.1f mode=%d\r\n"),
          roomTemp, targetTemp, outsideTemp, curveValue, pidOutput, finalSetpoint,
          (int)state.sat.eControlMode);

  // --- Publish to MQTT ---
  satPublishMQTT();
}

