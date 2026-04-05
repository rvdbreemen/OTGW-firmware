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
static bool     _bs_prevFlame       = false;
static float    _bs_prevModulation  = 0.0f;
static float    _bs_prevBoilerTemp  = 0.0f;
static uint32_t _bs_stateEntryMs    = 0;

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
static void satUpdateBoilerStatus()
{
  bool flame = (OTcurrentSystemState.Statusflags & 0x08) != 0;  // Bit 3 of slave status = flame
  float mod = OTcurrentSystemState.RelModLevel;
  float boilerTemp = OTcurrentSystemState.Tboiler;
  float setpoint = state.sat.fFinalSetpoint;
  uint32_t now = millis();
  SATBoilerStatus prev = state.sat.eBoilerStatus;

  SATBoilerStatus newStatus = SAT_BS_OFF;

  if (!flame && !_bs_prevFlame) {
    // No flame, was already off
    if (boilerTemp > setpoint + settings.sat.fOvershootMargin) {
      newStatus = SAT_BS_OVERSHOOT_COOLING;
    } else if (prev == SAT_BS_HEATING || prev == SAT_BS_AT_SETPOINT) {
      newStatus = SAT_BS_POST_CYCLE;
    } else {
      newStatus = SAT_BS_IDLE;
    }
  }
  else if (flame && !_bs_prevFlame) {
    // Flame just ignited
    if (boilerTemp < setpoint - 10.0f) {
      newStatus = SAT_BS_PREHEATING;
    } else {
      newStatus = SAT_BS_IGNITION_SURGE;
    }
  }
  else if (flame) {
    // Flame is on
    if (fabsf(boilerTemp - setpoint) < 3.0f) {
      newStatus = SAT_BS_AT_SETPOINT;
    } else if (boilerTemp < setpoint) {
      if (mod > _bs_prevModulation + 1.0f) {
        newStatus = SAT_BS_MODULATING_UP;
      } else {
        newStatus = SAT_BS_HEATING;
      }
    } else {
      if (mod < _bs_prevModulation - 1.0f) {
        newStatus = SAT_BS_MODULATING_DOWN;
      } else {
        newStatus = SAT_BS_OVERSHOOT_COOLING;
      }
    }
  }
  else {
    // Flame just turned off
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

//=====================================================================
//=== PWM Control Mode ===
//=====================================================================
static float satApplyPWM(float pidOutput)
{
  // PWM duty cycle: ratio of needed output to max capacity
  float maxSetpoint = OTcurrentSystemState.MaxTSet;
  if (maxSetpoint < 30.0f) maxSetpoint = SAT_MAX_SETPOINT_DEFAULT;

  float baseOffset = satGetBaseOffset();
  float range = maxSetpoint - baseOffset;
  if (range < 10.0f) range = 10.0f;

  float duty = (pidOutput - baseOffset) / range;
  if (duty < 0.0f) duty = 0.0f;
  if (duty > 1.0f) duty = 1.0f;

  state.sat.fPwmDutyCycle = duty;

  // Determine if flame should be on based on duty cycle
  // Simple time-proportional: within each control interval, flame on for duty% of time
  uint32_t intervalMs = (uint32_t)settings.sat.iControlInterval * 1000UL;
  uint32_t onTimeMs = (uint32_t)(duty * (float)intervalMs);
  uint32_t sinceFlameStart = millis() - satCycleGetFlameOnStartMs();

  if (duty >= 0.95f) {
    // Nearly 100% — just use full setpoint
    state.sat.bPwmFlameRequested = true;
    return pidOutput;
  } else if (duty <= 0.05f) {
    // Nearly 0% — minimum setpoint to suppress flame
    state.sat.bPwmFlameRequested = false;
    return SAT_MIN_SETPOINT;
  }

  // Flame ON phase: send the full setpoint
  if (state.sat.bPwmFlameRequested) {
    if (sinceFlameStart < onTimeMs || sinceFlameStart < (uint32_t)(SAT_PWM_MIN_ON_SEC * 1000.0f)) {
      return pidOutput;
    }
    // Switch to OFF phase
    state.sat.bPwmFlameRequested = false;
    return SAT_MIN_SETPOINT;
  }
  else {
    // Flame OFF phase — check if we should turn on again
    uint32_t sinceFlameOff = millis() - satCycleGetFlameOffStartMs();
    uint32_t offTimeMs = intervalMs - onTimeMs;
    if (sinceFlameOff >= offTimeMs) {
      state.sat.bPwmFlameRequested = true;
      return pidOutput;
    }
    return SAT_MIN_SETPOINT;
  }
}

//=====================================================================
//=== Continuous Control Mode ===
//=====================================================================
static float satApplyContinuous(float pidOutput)
{
  // Flow-temperature-aware clamping:
  // Don't drop setpoint abruptly when boiler is hot — smooth ramp down
  float boilerTemp = OTcurrentSystemState.Tboiler;
  float minAllowed = boilerTemp - SAT_FLOW_OFFSET_CONTINUOUS;

  if (pidOutput < minAllowed && boilerTemp > pidOutput + SAT_FLOW_OFFSET_CONTINUOUS) {
    // Boiler is much hotter than target — ramp down gradually
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

//=== Cleanly disable SAT and release boiler control ===
void satDisable()
{
  state.sat.eControlMode = SAT_MODE_OFF;
  state.sat.bActive = false;
  state.sat.fFinalSetpoint = 0.0f;
  satPidReset();
  // Send CS=0 to release control setpoint override — thermostat regains control
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
  sendJsonMapEntry(F("boiler_status"),        (int32_t)state.sat.eBoilerStatus);
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

  // Boiler status
  snprintf_P(valBuf, sizeof(valBuf), PSTR("%d"), (int)state.sat.eBoilerStatus);
  sendMQTTData(F("sat/boiler_status"), valBuf, false);

  // Cycle info
  snprintf_P(valBuf, sizeof(valBuf), PSTR("%d"), (int)state.sat.eLastCycleClass);
  sendMQTTData(F("sat/cycle_class"), valBuf, false);

  // PWM duty
  dtostrf(state.sat.fPwmDutyCycle, 1, 2, valBuf);
  sendMQTTData(F("sat/pwm_duty"), valBuf, false);

  // Overshoot margin
  dtostrf(settings.sat.fOvershootMargin, 1, 1, valBuf);
  sendMQTTData(F("sat/overshoot_margin"), valBuf, true);

  // Safety
  sendMQTTData(F("sat/safety_tripped"), state.sat.bSafetyTripped ? "true" : "false", false);
}

//=====================================================================
//=== Initialize SAT ===
//=====================================================================
void initSAT()
{
  satPidReset();
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
  _sat_consecutiveSkips = 0;  // Valid reading — reset counter

  if (outsideTemp < -40.0f || outsideTemp > 50.0f) {
    outsideTemp = 10.0f;  // Safe fallback
  }

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

  // --- Check auto-switch ---
  satCycleCheckAutoSwitch();

  // --- Compute modulation value based on mode and heating system ---
  {
    uint8_t mmValue;
    if (satAlwaysMaxModulation()) {
      // Heat pumps: always MM=100, let heat pump manage internal modulation
      mmValue = 100;
    } else if (state.sat.eControlMode == SAT_MODE_PWM && !state.sat.bPwmFlameRequested) {
      // PWM OFF phase (gas boilers): suppress modulation
      mmValue = 0;
    } else {
      // PWM ON or Continuous mode: use configured max
      mmValue = settings.sat.iMaxRelModulation;
    }
    state.sat.iCurrentModulation = mmValue;
  }

  // --- Send CS= and MM= commands to boiler when an OT command interface is available ---
  if (hasOTCommandInterface()) {
    char cmdBuf[16];
    snprintf_P(cmdBuf, sizeof(cmdBuf), PSTR("CS=%.1f"), finalSetpoint);
    addCommandToQueue(cmdBuf, strlen(cmdBuf), false, 0);
    // Send MM= (max relative modulation) alongside CS=
    snprintf_P(cmdBuf, sizeof(cmdBuf), PSTR("MM=%u"), state.sat.iCurrentModulation);
    addCommandToQueue(cmdBuf, strlen(cmdBuf), false, 0);
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

  DebugTf(PSTR("SAT: room=%.1f target=%.1f outside=%.1f curve=%.1f pid=%.1f final=%.1f mode=%d\r\n"),
          roomTemp, targetTemp, outsideTemp, curveValue, pidOutput, finalSetpoint,
          (int)state.sat.eControlMode);

  // --- Publish to MQTT ---
  satPublishMQTT();
}

