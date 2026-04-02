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

static uint8_t _sat_consecutiveSkips  = 0;
static uint8_t _sat_picFailCount      = 0;
static bool    _sat_bootCS0sent       = false;  // One-shot: ensure CS=0 is sent once PIC is available

// --- Boiler Status Tracking ---
static bool     _bs_prevFlame       = false;
static float    _bs_prevModulation  = 0.0f;
static float    _bs_prevBoilerTemp  = 0.0f;
static uint32_t _bs_stateEntryMs    = 0;

// --- Previous flame state for edge detection ---
static bool     _sat_prevFlameState = false;

// --- Timer for control loop ---
DECLARE_TIMER_SEC(timerSATControl, 30, CATCH_UP_MISSED_TICKS);

//=====================================================================
//=== Heating Curve Calculation ===
//=====================================================================
static float satCalcHeatingCurve(float targetTemp, float outsideTemp)
{
  float baseOffset = (settings.sat.iHeatingSystem == 1) ? SAT_HC_BASE_OFFSET_FLOOR : SAT_HC_BASE_OFFSET_RAD;
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
    float offDur = (float)(now - _bs_stateEntryMs) / 1000.0f;
    if (boilerTemp > setpoint + 2.0f) {
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
    float tempDelta = boilerTemp - _bs_prevBoilerTemp;
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

  float baseOffset = (settings.sat.iHeatingSystem == 1) ? SAT_HC_BASE_OFFSET_FLOOR : SAT_HC_BASE_OFFSET_RAD;
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
  uint32_t sinceFlameStart = millis() - _cycle_flameOnStartMs;

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
    uint32_t sinceFlameOff = millis() - _cycle_flameOffStartMs;
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
void satHandleExternalTemp(const char* value)
{
  if (!value || !*value) return;
  float temp = atof(value);
  if (temp > -50.0f && temp < 100.0f) {
    state.sat.fExternalTemp = temp;
    state.sat.bExternalTempValid = true;
    state.sat.iExternalTempLastMs = millis();
    DebugTf(PSTR("SAT: external indoor temp set to %.1f°C\r\n"), temp);
  }
}

void satHandleExternalOutdoor(const char* value)
{
  if (!value || !*value) return;
  float temp = atof(value);
  if (temp > -50.0f && temp < 100.0f) {
    state.sat.fExternalOutdoor = temp;
    state.sat.bExternalOutdoorValid = true;
    state.sat.iExternalOutdoorLastMs = millis();
    DebugTf(PSTR("SAT: external outdoor temp set to %.1f°C\r\n"), temp);
  }
}

void satHandleTargetTemp(const char* value)
{
  if (!value || !*value) return;
  float temp = atof(value);
  if (temp >= 5.0f && temp <= 30.0f) {
    settings.sat.fTargetTemp = temp;
    DebugTf(PSTR("SAT: target temp set to %.1f°C\r\n"), temp);
  }
}

void satHandleEnabled(const char* value)
{
  if (!value || !*value) return;
  bool enabled = EVALBOOLEAN(value);
  settings.sat.bEnabled = enabled;
  if (enabled) {
    // Clear safety trip so SAT can resume
    state.sat.bSafetyTripped = false;
    _sat_consecutiveSkips = 0;
    _sat_picFailCount = 0;
  } else {
    satDisable();
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
  addOTWGcmdtoqueue("CS=0", 4, false, 0);
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
void satSendStatusJSON(Print& client)
{
  static const char fmt[] PROGMEM =
    "{\"enabled\":%s,\"active\":%s,\"control_mode\":%d,"
    "\"boiler_status\":%d,\"target_temp\":%.1f,"
    "\"room_temp\":%.1f,\"outside_temp\":%.1f,"
    "\"heating_curve\":%.1f,\"pid_output\":%.1f,"
    "\"final_setpoint\":%.1f,\"error\":%.2f,"
    "\"pid_p\":%.2f,\"pid_i\":%.2f,\"pid_d\":%.2f,"
    "\"kp\":%.4f,\"ki\":%.6f,\"kd\":%.2f,"
    "\"coefficient\":%.1f,\"deadband\":%.2f,"
    "\"cycle_count\":%lu,\"last_cycle_class\":%d,"
    "\"cycle_max_flow\":%.1f,\"cycle_overshoot_sec\":%.0f,"
    "\"pwm_duty\":%.2f,\"pwm_flame_req\":%s,"
    "\"heating_system\":%d,\"external_temp_valid\":%s,"
    "\"external_outdoor_valid\":%s,\"safety_tripped\":%s}";

  char buf[512];
  snprintf_P(buf, sizeof(buf), fmt,
    CBOOLEAN(settings.sat.bEnabled), CBOOLEAN(state.sat.bActive),
    (int)state.sat.eControlMode, (int)state.sat.eBoilerStatus,
    settings.sat.fTargetTemp,
    satGetRoomTemp(), satGetOutsideTemp(),
    state.sat.fHeatingCurveValue, state.sat.fPidOutput,
    state.sat.fFinalSetpoint, state.sat.fError,
    state.sat.fPidP, state.sat.fPidI, state.sat.fPidD,
    state.sat.fKp, state.sat.fKi, state.sat.fKd,
    settings.sat.fHeatingCurveCoeff, settings.sat.fDeadband,
    (unsigned long)state.sat.iCycleCount, (int)state.sat.eLastCycleClass,
    state.sat.fCycleMaxFlow, state.sat.fCycleOvershootSec,
    state.sat.fPwmDutyCycle, CBOOLEAN(state.sat.bPwmFlameRequested),
    (int)settings.sat.iHeatingSystem,
    CBOOLEAN(state.sat.bExternalTempValid),
    CBOOLEAN(state.sat.bExternalOutdoorValid),
    CBOOLEAN(state.sat.bSafetyTripped));

  client.print(buf);
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
  sendMQTTData(F("sat/mode"), modeNames[state.sat.eControlMode], true);

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

  // Boiler status
  snprintf_P(valBuf, sizeof(valBuf), PSTR("%d"), (int)state.sat.eBoilerStatus);
  sendMQTTData(F("sat/boiler_status"), valBuf, false);

  // Cycle info
  snprintf_P(valBuf, sizeof(valBuf), PSTR("%d"), (int)state.sat.eLastCycleClass);
  sendMQTTData(F("sat/cycle_class"), valBuf, false);

  // PWM duty
  dtostrf(state.sat.fPwmDutyCycle, 1, 2, valBuf);
  sendMQTTData(F("sat/pwm_duty"), valBuf, false);

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
  if (isPICEnabled()) {
    addOTWGcmdtoqueue("CS=0", 4, false, 0);
    _sat_bootCS0sent = true;
    DebugTln(F("SAT: boot safety — sent CS=0 to release stale PIC override"));
  }
  // If PIC not ready yet, _sat_bootCS0sent stays false and the control loop
  // will send CS=0 on its first call when PIC becomes available.

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

  // Boot safety deferred: if CS=0 wasn't sent during initSAT() (PIC not ready),
  // send it now on the first call where PIC is available.
  if (!_sat_bootCS0sent && isPICEnabled()) {
    addOTWGcmdtoqueue("CS=0", 4, false, 0);
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

  // Hard safety ceiling based on heating system type — never exceeded
  float hardMax = (settings.sat.iHeatingSystem == 1) ? SAT_HARD_MAX_FLOOR : SAT_HARD_MAX_RAD;
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

  // --- Send CS= command to boiler (with PIC comm check) ---
  if (isPICEnabled()) {
    char cmdBuf[16];
    snprintf_P(cmdBuf, sizeof(cmdBuf), PSTR("CS=%.1f"), finalSetpoint);
    addOTWGcmdtoqueue(cmdBuf, strlen(cmdBuf), false, 0);
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
