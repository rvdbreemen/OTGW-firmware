#if defined(ENABLE_SAT)
/*
***************************************************************************
**  Module   : SATpressure.ino
**  Description: SAT CH water pressure health monitoring (Task #226)
**
**  Reads OT MsgID 18 (CH water pressure) from OTcurrentSystemState,
**  updates state.sat.fBoilerPressure and state.sat.sPressureStatus,
**  and publishes sat/ch_pressure + sat/ch_pressure_status to MQTT.
**
**  The heavy lifting (EMA smoothing, linear regression drop rate,
**  120s confirmation alarm) is done by satUpdatePressure() in
**  SATcontrol.ino. This module adds the named state fields and the
**  task-specific MQTT topics required by Task #226.
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  TERMS OF USE: MIT License. See bottom of OTGW-firmware.h
***************************************************************************
*/

// ---------------------------------------------------------------------------
// satPressureHealthUpdate()
//
// Called once per satControlLoop() pass (after satUpdatePressure() has run).
// Updates state.sat.fBoilerPressure and state.sat.sPressureStatus from the
// existing pressure state computed by satUpdatePressure().
//
// sPressureStatus is:
//   "ok"   — pressure within [fMinPressure, fMaxPressure]
//   "low"  — smoothed pressure < fMinPressure
//   "high" — smoothed pressure > fMaxPressure
//   (alarm is confirmed after 120s — mirrors bPressureHealthy)
// ---------------------------------------------------------------------------
void satPressureHealthUpdate()
{
  // Mirror the raw reading into the AC#1-required field.
  state.sat.fBoilerPressure = OTcurrentSystemState.CHPressure;

  // Derive sPressureStatus from the smoothed value and alarm state.
  // We only report non-ok when the alarm has been confirmed (bPressureAlarm=true
  // and bPressureHealthy=false), to respect the 120s confirmation window (AC#6).
  if (!state.sat.bPressureAlarm) {
    strlcpy(state.sat.sPressureStatus, "ok", sizeof(state.sat.sPressureStatus));
  } else {
    // Distinguish low vs high from the smoothed reading.
    if (state.sat.fSmoothedPressure < settings.sat.fMinPressure) {
      strlcpy(state.sat.sPressureStatus, "low",  sizeof(state.sat.sPressureStatus));
    } else if (state.sat.fSmoothedPressure > settings.sat.fMaxPressure) {
      strlcpy(state.sat.sPressureStatus, "high", sizeof(state.sat.sPressureStatus));
    } else {
      // Alarm is from drop rate, not band — still report ok for status
      strlcpy(state.sat.sPressureStatus, "ok", sizeof(state.sat.sPressureStatus));
    }
  }
}

// ---------------------------------------------------------------------------
// satPressureHealthPublish()
//
// Publishes sat/ch_pressure and sat/ch_pressure_status to MQTT (AC#7).
// Called from satPublishMQTT() in SATcontrol.ino.
// ---------------------------------------------------------------------------
void satPressureHealthPublish()
{
  char buf[12];

  // sat/ch_pressure — current raw OT MsgID 18 reading (bar)
  dtostrf(state.sat.fBoilerPressure, 1, 2, buf);
  sendMQTTData(F("sat/ch_pressure"), buf, false);

  // sat/ch_pressure_status — "ok", "low", or "high"
  sendMQTTData(F("sat/ch_pressure_status"), state.sat.sPressureStatus, false);
}

#endif // ENABLE_SAT
