/*
***************************************************************************
**  Program  : SATchEnable.h
**  Version  : v2.0.0-alpha.577
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
static inline uint8_t satChEnableDecide(float requestedSetpoint, float coldSetpoint,
                                        bool pwmDrivesSetpoint, bool pwmOn)
{
  if (satColdCutoff(requestedSetpoint, coldSetpoint)) return 0;
  if (pwmDrivesSetpoint && !pwmOn) return 0;
  return 1;
}
