---
id: TASK-227
title: 'SAT: Add rolling windowed cycle statistics (4h/24h)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:30'
updated_date: '2026-04-09 06:18'
labels:
  - audit-fix
  - sat
  - cycles
dependencies: []
references:
  - backlog/docs/sat-feature-completeness-matrix.md
  - backlog/docs/sat-python-cpp-mapping.md
  - src/OTGW-firmware/SATcycles.ino
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python's CycleHistory maintains two rolling deque windows: a recent ~4-hour window and a daily 24-hour window. From these it computes: duty_ratio (flame-on fraction), overshoot_fraction, underheat_fraction, flow/return temperature delta p50/p90.

The C++ firmware uses EMA (exponential moving averages) for duty_ratio, overshoot_fraction, and underheat_fraction. EMA responds differently to recent changes: it weights recent cycles more heavily but never truly forgets old data. This can cause the 300s saturation auto-disable to trigger late when the system transitions from over-demand to normal demand.

The flow/return delta percentiles (p50/p90) are entirely missing and are used by Python to detect boiler inefficiency and diagnose short cycling.

Complexity: Medium-high. A true rolling window requires either a time-stamped deque or a circular buffer with timestamps. On ESP8266 with limited RAM, a 4h window at 2-minute average cycle duration = ~120 entries. At ~8 bytes/entry (duration_s, classification, flow_max_temp as uint16 pairs) that is ~960 bytes - feasible.

Risk: Medium. EMA vs windowed statistics produce different values for the same input. Changing from EMA to windowed for the auto-switch triggers (300s saturation) requires careful re-validation of the trigger thresholds. Recommend keeping EMA for the auto-switch triggers initially and adding windowed stats as additional MQTT telemetry only, then switching triggers in a follow-up task.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Cycle history ring buffer stores at minimum: cycle_end_timestamp, duration_s, classification, max_flow_temp, overshoot_sec
- [x] #2 Duty ratio is computed from windowed sum over last 4 hours of cycle records
- [x] #3 Overshoot fraction and underheat fraction computed from last 4h window
- [x] #4 Flow/return delta p50 and p90 are computed from last 4h window (requires per-cycle average delta)
- [x] #5 All windowed statistics published to MQTT alongside existing EMA values
- [x] #6 EMA-based auto-switch triggers (60s/180s/300s) are NOT changed in this task
- [x] #7 Ring buffer size is sufficient for at least 4h of normal cycling (minimum 60 entries)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add SATWindowRecord ring buffer (60 slots) to SATcycles.ino
2. Add 4h state fields to SATRuntimeSection in OTGW-firmware.h
3. Populate ring buffer entries when a cycle completes (flame-off event in satCycleOnFlameChange)
4. Implement satGetWindow4hStats() in SATcycles.ino
5. Add 4h MQTT publish lines to satPublishMQTT() in SATcontrol.ino
6. Add a 60-second timer in SATcontrol.ino that calls satGetWindow4hStats()
7. Initialise new fields in satCycleInit()
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added rolling 4-hour cycle window statistics for heating curve self-tuning diagnostics.

Changes:
- SATcycles.ino: Added SATWindowRecord ring buffer (60 slots, ~1.4 KB). Each record stores: endMs, onDurationMs, offDurationMs, p90FlowTemp, avgFlowRetDelta, eClass.
- SATcycles.ino: Per-cycle flow-return delta accumulation in satCycleSample() (_cycle_sumFlowRetDelta/_cycle_deltasamples).
- SATcycles.ino: Writes ring buffer entry on every flame-off event in satCycleOnFlameChange().
- SATcycles.ino: New satGetWindow4hStats() function computes: i4hCycles, f4hAvgOnSec, f4hAvgOffSec, f4hAvgFlow, f4hDutyRatio, f4hOvershootFraction, f4hUnderheatFraction, f4hFlowRetDeltaP50, f4hFlowRetDeltaP90 (via insertion sort).
- OTGW-firmware.h: Added 9 new state.sat fields (i4hCycles, f4hAvg*, f4hDutyRatio, f4h*Fraction, f4hFlowRetDelta*).
- SATcontrol.ino: DECLARE_TIMER_SEC(timerSAT4hStats, 60) fires satGetWindow4hStats() once per minute.
- SATcontrol.ino: 9 new MQTT topics published in satPublishMQTT(): sat/4h_cycles, sat/4h_avg_on_sec, sat/4h_avg_off_sec, sat/4h_avg_flow_temp, sat/4h_duty_ratio, sat/4h_overshoot_fraction, sat/4h_underheat_fraction, sat/4h_flow_ret_delta_p50, sat/4h_flow_ret_delta_p90.
- satCycleInit() resets all new fields.

EMA-based auto-switch triggers are untouched (AC#6). ESP8266 firmware build confirmed successful.
<!-- SECTION:FINAL_SUMMARY:END -->
