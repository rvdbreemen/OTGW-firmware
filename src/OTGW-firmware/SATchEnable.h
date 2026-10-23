/*
***************************************************************************
**  Program  : SATchEnable.h
**  Version  : v2.0.0-alpha.473
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

#include <math.h>
#include <stdint.h>

// CH enable bit for the control setpoint SAT sends: 1 at or above the per-system cold setpoint
// (28.2 radiators, 21.0 underfloor), 0 below it. Sergeant D, 2026-10-23: CH off below the cold
// setpoint instead of thermo-nova's COLD_SETPOINT 22 (custom_components/sat/const.py:34 at
// 7beaaf5; heating_control.py:238 sends HeaterState.ON when control_setpoint > COLD_SETPOINT).
// Both values are quantized to 0.1 C like the CS=%.1f command, so the decision follows the
// setpoint the boiler actually receives.
static inline uint8_t satChEnableFor(float setpoint, float coldSetpoint)
{
  return (lroundf(setpoint * 10.0f) >= lroundf(coldSetpoint * 10.0f)) ? 1 : 0;
}

// CH enable bit for the main SAT control block (TASK-1316). Rule confirmed by Sergeant D, 2026-10-23:
//  1. PWM ON phase -> CH=1 whatever CS is sent: the low flame-keeping CS never turns CH off.
//  2. PWM OFF phase -> CS=10 and CH=0.
//  3. Continuous mode, and any PWM tick whose CS the PWM step does not drive -> CH from the setpoint
//     against the per-system cold setpoint (satChEnableFor).
//  4. CH is decided on the setpoint BEFORE the solar offset is subtracted, and the cold cutoff
//     (requested setpoint below the cold setpoint, CS=10) still forces CH=0.
// Source: thermo-nova sends CH after every control step from control_setpoint > COLD_SETPOINT 22
// (custom_components/sat/climate/heating_control.py:236-238, coordinator/serial/__init__.py:210-212,
// const.py:34 at 7beaaf5); the cold setpoint per heating system, the PWM ON exception and the
// pre-solar decision are Sergeant D's decisions of 2026-10-23.
//   preSolarSetpoint  control setpoint before the solar adjustment
//   pwmDrivesSetpoint the PWM step returned its ON/OFF setpoint (false when it returned prevCS:
//                     overshoot protection off, DHW active, requested setpoint <= cold, no flow temp)
//   pwmOn             PWM mode and bPwmFlameRequested
//   coldCutoff        requested setpoint below the cold setpoint
static inline uint8_t satChEnableDecide(float preSolarSetpoint, float coldSetpoint,
                                        bool pwmDrivesSetpoint, bool pwmOn, bool coldCutoff)
{
  if (coldCutoff) return 0;
  if (pwmDrivesSetpoint) return pwmOn ? 1 : 0;
  return satChEnableFor(preSolarSetpoint, coldSetpoint);
}
