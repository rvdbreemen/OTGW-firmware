/*
***************************************************************************
**  Program  : SATchEnable.h
**  Version  : v2.0.0-beta.364
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Pure helper for the CH enable bit SAT sends after every control step
**  (TASK-1316). No Arduino or firmware state in here, so a host test
**  compiles this header as it is.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <stdint.h>
#include <math.h>

// Tolerance band on the heating demand (TASK-1887, ADR-250; Sergeant D entries 326, 328, 361, 376, 378, 384). Home Assistant
// generic-thermostat tolerances as Sergeant D reads them (entries 326, 401): heating stops when the room is at or above
// target + SAT_TOL_HOT_C and starts again as soon as it is below target + SAT_TOL_COLD_C (both 0.1 above the target; entry 401
// corrects the earlier target - 0.1 reading of entry 328). After a band stop CH stays off for SAT_TOL_MIN_OFF_MS before it may
// restart (entry 402), which takes the place of a hysteresis gap. A stop only clears the CH bit: PWM stays active, the PID and
// the setpoint keep running (entries 361, 384). Fixed values, no setting. thermo-nova has no band.
static const float SAT_TOL_COLD_C = 0.1f;
static const float SAT_TOL_HOT_C  = 0.1f;
// Entry 403 (Q-402a A, with 10 minutes): 600 s, counted from the moment CH actually goes off for the band.
static const uint32_t SAT_TOL_MIN_OFF_MS = 600000UL;

// The band input: the error (target - room, rounded like satPidError) of the rooms that decide. Entry 378: in ECO only the
// main room decides; in COMFORT the largest error over the main room and every usable zone, so a cold zone keeps heating on.
// NAN when no deciding room is usable.
static inline float satToleranceMaxError(bool roomValid, float roomErr, const float* zoneErr, const bool* zoneUse, uint8_t n,
                                         bool ecoMode)
{
  float m = roomValid ? roomErr : NAN;
  if (ecoMode) return m;
  for (uint8_t i = 0; i < n; i++) {
    if (zoneUse[i] && (isnan(m) || zoneErr[i] > m)) m = zoneErr[i];
  }
  return m;
}

// Demand of the band. No usable room temperature (NAN) never blocks heating: the other rules decide. maxErr is target - room:
// a stop at maxErr <= -0.1 (every deciding room at or above target + 0.1), demand again at maxErr > -0.1 (entry 401). The
// minimum CH-off time of entry 402 is applied by the control tick, which knows when CH went off.
static inline bool satToleranceDemand(bool wasOn, float maxErr)
{
  if (isnan(maxErr)) return true;
  return wasOn ? (maxErr > -SAT_TOL_HOT_C) : (maxErr > -SAT_TOL_COLD_C);
}

// Cold cutoff: the requested setpoint (PID output after the solar offset) at or below the per-system
// cold setpoint (28.2 radiators, 21.0 underfloor) means no heating. thermo-nova uses the same
// semantics: the heater state is ON only when requested_setpoint > COLD_SETPOINT
// (custom_components/sat/heating_control.py:72) and the PWM override returns when
// requested_setpoint <= COLD_SETPOINT (heating_control.py:385) at 7beaaf5. The per-system cold value
// instead of COLD_SETPOINT 22 is Sergeant D's decision of 2026-10-23.
static inline bool satColdCutoff(float requestedSetpoint, float coldSetpoint)
{
  return requestedSetpoint <= coldSetpoint;
}

// Latched cold cutoff with hysteresis (TASK-1547, Sergeant D entry 199). The bare rule above flips every
// tick when the requested setpoint hovers on the cold setpoint (gruijter, alpha.597: off at 28.1, on at
// 28.8, off again a minute later, a 60 s burn each time). Once cut off, heating comes back only above
// cold + band; while heating, it cuts off at or below cold as before. thermo-nova has no band
// (heating_control.py:72) but uses COLD_SETPOINT 22, which Sergeant D calls a mistake (entry 200).
static inline bool satColdCutoffLatched(float requestedSetpoint, float coldSetpoint, bool wasCut, float band)
{
  return wasCut ? (requestedSetpoint <= coldSetpoint + band) : (requestedSetpoint <= coldSetpoint);
}

// CH enable bit of the main SAT control block (TASK-1323, simplified from TASK-1316). Sergeant D,
// 2026-10-23: "If the requested setpoint is lower than the thresholds then we send CH=0 CS=10".
//  1. Requested setpoint at or below the cold setpoint -> CS=10, CH=0 (the cold cutoff).
//  2. PWM drives the CS and the PWM phase is OFF -> CS=10, CH=0.
//  3. Everything else -> CH=1: PWM ON whatever low CS keeps the flame, continuous mode, and a PWM tick
//     that keeps the previous CS (overshoot protection off, DHW, no flow temperature) while the
//     requested setpoint is above the cold setpoint.
// The CH bit no longer depends on the sent CS value, so a held CS such as 24 never turns CH off.
// Source: thermo-nova heating_control.py:72 (ON only above the cold setpoint), :236-238 (CH after every
// control step), coordinator/serial/__init__.py:210-214, at 7beaaf5.
//   requestedSetpoint PID output after the multi-zone block and the solar offset, clamped
//   pwmDrivesSetpoint the PWM step returned its ON/OFF setpoint (false when it returned prevCS or the
//                     continuous setpoint: overshoot protection off, DHW active, requested at or
//                     below cold, no flow temperature)
//   pwmOn             PWM mode and bPwmFlameRequested, captured right after the PWM step
//   bandOff           TASK-1887 (entry 384): the tolerance band holds heating off (after the 180 s minimum burn of
//                     entries 275/376); only the CH bit follows it, the CS and PWM are not touched
static inline uint8_t satChEnableDecide(float requestedSetpoint, float coldSetpoint,
                                        bool pwmDrivesSetpoint, bool pwmOn, bool bandOff = false)
{
  if (satColdCutoff(requestedSetpoint, coldSetpoint)) return 0;
  if (bandOff) return 0;
  if (pwmDrivesSetpoint && !pwmOn) return 0;
  return 1;
}
