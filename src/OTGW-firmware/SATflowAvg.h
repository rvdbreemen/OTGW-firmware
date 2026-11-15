/*
***************************************************************************
**  Program  : SATflowAvg.h
**  Version  : v2.0.0-beta.330
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  The 2 minute average of the boiler flow temperature (MsgID 25) that the
**  SAT PWM ON suppression setpoint uses (TASK-1990, Sergeant D entry 364:
**  "A, 2 minutes window"). thermo-nova uses the latest flow value
**  (heating_control.py:435-441); this average is a deliberate deviation.
**  No Arduino or firmware state in here, so a host test compiles this
**  header as it is.
**
**  Samples: valid boiler MsgID 25 replies, at most one stored every 2 s,
**  none while the boiler reports hot water. The mean counts the samples of
**  the last 120 s that are not older than the current flame-on, so readings
**  from the OFF phase (standing water) never enter the ON average.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <stdint.h>

static const uint32_t SAT_FLOW_AVG_WINDOW_MS  = 120000UL;   // entry 364: 2 minutes
static const uint32_t SAT_FLOW_AVG_SPACING_MS = 2000UL;     // one stored sample per 2 s at most
static const uint8_t  SAT_FLOW_AVG_SLOTS      = 64;         // 120 s / 2 s = 60, plus headroom

struct SatFlowAvg {
  uint32_t ms[SAT_FLOW_AVG_SLOTS];
  float    value[SAT_FLOW_AVG_SLOTS];
  uint8_t  head  = 0;          // next slot to write
  uint8_t  count = 0;          // stored samples, at most SAT_FLOW_AVG_SLOTS
  uint32_t lastMs = 0;         // millis() of the last stored sample; 0 = none
};

// Store a valid flow sample. A sample within SAT_FLOW_AVG_SPACING_MS of the last stored one is skipped, so a thermostat that reads
// MsgID 25 every second cannot push the window past the ring size. nowMs == 0 is stored as 1 (0 means "none").
static inline void satFlowAvgNote(SatFlowAvg &a, uint32_t nowMs, float flow)
{
  if (!nowMs) nowMs = 1;
  if (a.count && (uint32_t)(nowMs - a.lastMs) < SAT_FLOW_AVG_SPACING_MS) return;
  a.ms[a.head] = nowMs;
  a.value[a.head] = flow;
  a.head = (uint8_t)((a.head + 1) % SAT_FLOW_AVG_SLOTS);
  if (a.count < SAT_FLOW_AVG_SLOTS) a.count++;
  a.lastMs = nowMs;
}

// Mean of the stored samples younger than SAT_FLOW_AVG_WINDOW_MS and taken at or after sinceMs (the current flame-on time; 0 = no
// lower bound). Returns the number of samples used; *mean is written only when that number is above 0.
static inline uint8_t satFlowAvgMean(const SatFlowAvg &a, uint32_t nowMs, uint32_t sinceMs, float *mean)
{
  float sum = 0.0f;
  uint8_t n = 0;
  for (uint8_t i = 0; i < a.count; i++) {
    const uint8_t slot = (uint8_t)((a.head + SAT_FLOW_AVG_SLOTS - 1 - i) % SAT_FLOW_AVG_SLOTS);   // newest first
    const uint32_t age = nowMs - a.ms[slot];
    if (age >= SAT_FLOW_AVG_WINDOW_MS) break;                    // older ones are older still
    if (sinceMs && (int32_t)(a.ms[slot] - sinceMs) < 0) break;   // taken before the current flame-on
    sum += a.value[slot];
    n++;
  }
  if (n) *mean = sum / (float)n;
  return n;
}
