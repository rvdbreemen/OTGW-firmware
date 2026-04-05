/*
***************************************************************************
**  Module   : SATpid.ino
**  Description: PID controller for the SAT (Smart Autotune Thermostat)
**
**  Ported from SAT releases/thermo-nova pid.py
**  (https://github.com/Alexwijn/SAT) with permission from the authors.
**
**  PID version 3 with automatic gain calculation, temperature-based
**  derivative (not error-based), deadband mode switching, and single
**  low-pass filtered derivative with magnitude cap.
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License. See bottom of OTGW-firmware.h
***************************************************************************
*/

// --- PID Constants ---
static const float SAT_PID_DEADBAND_DEFAULT     = 0.25f;
static const float SAT_PID_SAMPLE_TIME_LIMIT    = 10.0f;   // seconds
static const float SAT_PID_UPDATE_INTERVAL      = 60.0f;   // seconds
// Derivative alpha is now adaptive: alpha = dt / (PID_UPDATE_INTERVAL + dt)
static const float SAT_PID_DERIVATIVE_CAP       = 5.0f;    // Max raw derivative magnitude
static const float SAT_PID_AGGRESSION_V3        = 8400.0f;

// Underfloor divisor=4, radiator divisor=3 for Kp calculation
static const float SAT_PID_KP_DIVISOR_FLOOR     = 4.0f;
static const float SAT_PID_KP_DIVISOR_RAD       = 3.0f;

// --- PID Internal State (static, not exposed) ---
static float  _pid_lastError          = 0.0f;
static float  _pid_previousError      = 0.0f;
static float  _pid_integral           = 0.0f;
static float  _pid_rawDerivative      = 0.0f;
static float  _pid_lastRoomTemp       = 0.0f;
static float  _pid_lastCurveValue     = 0.0f;
static uint32_t _pid_lastUpdateMs     = 0;
static uint32_t _pid_lastDerivativeMs = 0;
static bool   _pid_initialized        = false;

//=== PID Reset ===
void satPidReset()
{
  _pid_lastError          = 0.0f;
  _pid_previousError      = 0.0f;
  _pid_integral           = 0.0f;
  _pid_rawDerivative      = 0.0f;
  _pid_lastRoomTemp       = 0.0f;
  _pid_lastCurveValue     = 0.0f;
  _pid_lastUpdateMs       = millis();
  _pid_lastDerivativeMs   = millis();
  _pid_initialized        = false;

  state.sat.fPidP = 0.0f;
  state.sat.fPidI = 0.0f;
  state.sat.fPidD = 0.0f;
  state.sat.fPidOutput = 0.0f;
  state.sat.fKp = 0.0f;
  state.sat.fKi = 0.0f;
  state.sat.fKd = 0.0f;
  state.sat.fRawDerivative = 0.0f;
}

//=== Auto-Gain Calculation (Version 3) ===
static void _pidCalculateGains(float curveValue)
{
  float coeff = settings.sat.fHeatingCurveCoeff;
  float divisor = (settings.sat.iHeatingSystem == 1) ? SAT_PID_KP_DIVISOR_FLOOR : SAT_PID_KP_DIVISOR_RAD;

  float kp = (coeff * curveValue) / divisor;
  float ki = kp / SAT_PID_AGGRESSION_V3;
  float kd = 0.07f * SAT_PID_AGGRESSION_V3 * kp;

  state.sat.fKp = kp;
  state.sat.fKi = ki;
  state.sat.fKd = kd;
}

//=== Integral Update ===
// Per SAT Python (pid.py): integral is only active when |error| > deadband
// (correcting steady-state offset). Inside deadband, target is reached and
// integral resets to 0. Clamp to [0, curveValue] — positive only.
static void _pidUpdateIntegral(float error, float curveValue, bool force)
{
  float deadband = settings.sat.fDeadband;

  // Inside deadband: target reached, reset integral
  if (fabsf(error) <= deadband) {
    _pid_integral = 0.0f;
    return;
  }

  if (state.sat.fKi < 1e-9f) return;

  // Accumulate: Ki * error * PID_UPDATE_INTERVAL (fixed 60s per SAT Python)
  _pid_integral += state.sat.fKi * error * SAT_PID_UPDATE_INTERVAL;

  // Clamp integral to [0, curveValue] — positive only (SAT Python convention)
  if (_pid_integral < 0.0f)      _pid_integral = 0.0f;
  if (_pid_integral > curveValue) _pid_integral = curveValue;

  // Hard absolute cap — defense against runaway
  static const float SAT_PID_INTEGRAL_ABS_MAX = 20.0f;
  if (_pid_integral > SAT_PID_INTEGRAL_ABS_MAX) _pid_integral = SAT_PID_INTEGRAL_ABS_MAX;
}

//=== Derivative Update ===
// Per SAT Python (pid.py): temperature-based derivative with negative sign
// (rising temp = negative derivative = damping). Uses adaptive alpha for
// low-pass filter that scales with sample rate. Only active outside deadband.
static void _pidUpdateDerivative(float roomTemp)
{
  float deadband = settings.sat.fDeadband;

  // Inside deadband: reset derivative to 0, update timestamp only
  if (fabsf(_pid_lastError) <= deadband) {
    _pid_rawDerivative = 0.0f;
    _pid_lastDerivativeMs = millis();
    return;
  }

  uint32_t now = millis();
  float deltaTime = (float)(now - _pid_lastDerivativeMs) / 1000.0f;

  // Skip if insufficient time elapsed (prevents noise at fast sampling)
  if (deltaTime <= SAT_PID_UPDATE_INTERVAL) return;

  // No temperature change: just update timestamp
  float tempDelta = roomTemp - _pid_lastRoomTemp;
  if (fabsf(tempDelta) < 0.001f) {
    _pid_lastDerivativeMs = now;
    return;
  }

  // Temperature-based derivative with NEGATIVE sign (SAT Python convention):
  // rising temp -> negative derivative -> reduces PID output (damping)
  float rawDeriv = -tempDelta / deltaTime;

  // Hard cap first (SAT Python applies cap before filter on extreme values)
  if (fabsf(rawDeriv) >= SAT_PID_DERIVATIVE_CAP) {
    if (rawDeriv > SAT_PID_DERIVATIVE_CAP)  rawDeriv = SAT_PID_DERIVATIVE_CAP;
    if (rawDeriv < -SAT_PID_DERIVATIVE_CAP) rawDeriv = -SAT_PID_DERIVATIVE_CAP;
    _pid_rawDerivative = rawDeriv;
    _pid_lastDerivativeMs = now;
    return;
  }

  // Adaptive alpha low-pass filter (SAT Python: alpha = dt / (interval + dt))
  // Longer dt -> alpha closer to 1.0 (trusts new reading more)
  float alpha = deltaTime / (SAT_PID_UPDATE_INTERVAL + deltaTime);
  float filtered = alpha * rawDeriv + (1.0f - alpha) * _pid_rawDerivative;

  // Final cap
  if (filtered > SAT_PID_DERIVATIVE_CAP) filtered = SAT_PID_DERIVATIVE_CAP;
  if (filtered < -SAT_PID_DERIVATIVE_CAP) filtered = -SAT_PID_DERIVATIVE_CAP;

  _pid_rawDerivative = filtered;
  _pid_lastDerivativeMs = now;
}

//=== Main PID Update ===
// Returns the PID output = heatingCurveValue + P + I + D
float satPidUpdate(float roomTemp, float targetTemp, float heatingCurveValue, float boilerTemp)
{
  float error = targetTemp - roomTemp;
  uint32_t now = millis();

  // Initialize on first call
  if (!_pid_initialized) {
    _pid_lastError        = error;
    _pid_previousError    = error;
    _pid_lastRoomTemp     = roomTemp;
    _pid_lastCurveValue   = heatingCurveValue;
    _pid_lastUpdateMs     = now;
    _pid_lastDerivativeMs = now;
    _pid_initialized      = true;
  }

  // Check sample time limit — don't update too frequently
  float elapsed = (float)(now - _pid_lastUpdateMs) / 1000.0f;
  if (elapsed < SAT_PID_SAMPLE_TIME_LIMIT) {
    // Return previous output
    return state.sat.fPidOutput;
  }

  // Skip if error hasn't changed meaningfully
  if (fabsf(error - _pid_lastError) < 0.001f && elapsed < SAT_PID_UPDATE_INTERVAL) {
    return state.sat.fPidOutput;
  }

  // Calculate gains based on current heating curve
  _pidCalculateGains(heatingCurveValue);

  // Update integral (force update on each PID cycle)
  _pidUpdateIntegral(error, heatingCurveValue, true);

  // Update derivative (temperature-based)
  _pidUpdateDerivative(roomTemp);

  // Calculate P, I, D terms
  float P = state.sat.fKp * error;
  float I = _pid_integral;
  float D = state.sat.fKd * _pid_rawDerivative;

  // Store state
  state.sat.fPidP = P;
  state.sat.fPidI = I;
  state.sat.fPidD = D;
  state.sat.fError = error;
  state.sat.fRawDerivative = _pid_rawDerivative;

  // PID output = heatingCurveValue + P + I + D (thermo-nova style)
  float output = heatingCurveValue + P + I + D;
  state.sat.fPidOutput = output;

  // Update tracking variables
  _pid_previousError  = _pid_lastError;
  _pid_lastError      = error;
  _pid_lastRoomTemp   = roomTemp;
  _pid_lastCurveValue = heatingCurveValue;
  _pid_lastUpdateMs   = now;

  return output;
}

