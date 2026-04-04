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
static const float SAT_PID_INTEGRAL_TIME_LIMIT  = 300.0f;  // seconds
static const float SAT_PID_UPDATE_INTERVAL      = 60.0f;   // seconds
static const float SAT_PID_DERIVATIVE_ALPHA     = 0.8f;    // EMA filter alpha
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
static uint32_t _pid_lastIntegralMs   = 0;
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
  _pid_lastIntegralMs     = millis();
  _pid_initialized        = false;

  state.sat.fPidP = 0.0f;
  state.sat.fPidI = 0.0f;
  state.sat.fPidD = 0.0f;
  state.sat.fPidOutput = 0.0f;
  state.sat.fKp = 0.0f;
  state.sat.fKi = 0.0f;
  state.sat.fKd = 0.0f;
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
// Only active when |error| <= deadband (fine-tuning near setpoint)
static void _pidUpdateIntegral(float error, float curveValue, bool force)
{
  float deadband = settings.sat.fDeadband;

  // When error transitions from outside to inside deadband, reset integral timer
  if (fabsf(_pid_lastError) > deadband && fabsf(error) <= deadband) {
    _pid_lastIntegralMs = millis();
  }

  bool integralEnabled = (fabsf(error) <= deadband);
  if (!integralEnabled) {
    _pid_integral = 0.0f;
    return;
  }

  // Respect integral time limit unless forced
  float secsSinceIntegral = (float)(millis() - _pid_lastIntegralMs) / 1000.0f;
  if (!force && secsSinceIntegral < SAT_PID_INTEGRAL_TIME_LIMIT) {
    return;
  }

  if (state.sat.fKi < 1e-9f) return;

  float deltaTime = secsSinceIntegral;
  _pid_integral += state.sat.fKi * error * deltaTime;

  // Clamp integral to prevent windup: [-curveValue, +curveValue]
  if (_pid_integral > curveValue)  _pid_integral = curveValue;
  if (_pid_integral < -curveValue) _pid_integral = -curveValue;

  // Hard absolute cap — defense against runaway regardless of curve value
  static const float SAT_PID_INTEGRAL_ABS_MAX = 20.0f;
  if (_pid_integral > SAT_PID_INTEGRAL_ABS_MAX)  _pid_integral = SAT_PID_INTEGRAL_ABS_MAX;
  if (_pid_integral < -SAT_PID_INTEGRAL_ABS_MAX) _pid_integral = -SAT_PID_INTEGRAL_ABS_MAX;

  _pid_lastIntegralMs = millis();
}

//=== Derivative Update ===
// Temperature-based (thermo-nova): tracks actual room temp change rate
// Only active when |error| > deadband (dampens rapid approach)
static void _pidUpdateDerivative(float roomTemp)
{
  float deadband = settings.sat.fDeadband;
  bool derivativeEnabled = (fabsf(_pid_lastError) > deadband);
  if (!derivativeEnabled) return;

  uint32_t now = millis();
  float timeDiff = (float)(now - _pid_lastDerivativeMs) / 1000.0f;
  if (timeDiff <= 0.0f) return;

  // Temperature-based derivative (negative slope = heating up = good)
  float rawDeriv = (roomTemp - _pid_lastRoomTemp) / timeDiff;

  // Single low-pass filter with magnitude cap
  float filtered = SAT_PID_DERIVATIVE_ALPHA * rawDeriv + (1.0f - SAT_PID_DERIVATIVE_ALPHA) * _pid_rawDerivative;

  // Cap magnitude
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
    _pid_lastIntegralMs   = now;
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

