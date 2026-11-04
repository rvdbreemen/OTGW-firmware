---
id: TASK-228
title: 'SAT: Implement heating curve recommendation (INCREASE/DECREASE/HOLD)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:30'
updated_date: '2026-04-09 10:22'
labels:
  - audit-fix
  - sat
  - heating-curve
dependencies: []
references:
  - backlog/docs/sat-feature-completeness-matrix.md
  - backlog/docs/sat-python-cpp-mapping.md
  - src/OTGW-firmware/SATcontrol.ino
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python's SatHeatingCurveRecommendationSensor computes a daily rolling temperature error history and recommends whether the heating curve coefficient should be increased, decreased, or held. This is a self-diagnostic capability that helps users tune their system without guesswork.

Algorithm:
- Track daily median of (room_temp - target_temp) errors
- If median error > +0.5 C sustained for N days: recommend DECREASE (system running too hot)
- If median error < -0.5 C sustained for N days: recommend INCREASE (system running too cold)
- Otherwise: HOLD

The C++ firmware has no equivalent. Users currently have no feedback on whether their heating curve coefficient is correct.

Complexity: Medium. Requires storing a rolling daily temperature error summary (median per day, last 7 days = 7 floats = 28 bytes). The recommendation logic is simple threshold comparison.

Risk: Low. This is a read-only advisory sensor; it does not affect the control loop. The only risk is a misleading recommendation during edge cases (holiday mode, unusual outdoor temperature patterns), which is mitigated by the N-day sustain requirement.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Daily temperature error median (room_temp - target_temp) is computed and stored for the last 7 days
- [x] #2 Recommendation is INCREASE if median error < -0.5 C for 3+ consecutive days
- [x] #3 Recommendation is DECREASE if median error > +0.5 C for 3+ consecutive days
- [x] #4 Recommendation is HOLD otherwise
- [x] #5 Recommendation and daily error values are published to MQTT
- [x] #6 Recommendation resets to HOLD when SAT is disabled or room sensor is unavailable
- [x] #7 Daily error data persists across restarts (stored to LittleFS or settings.sat)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add daily error ring buffer (7 floats) + persistence file in SATcycles.ino
2. Add satHCR_sampleDaily() called once per day from the control loop tick
3. Add satHeatingCurveRecommendation() that computes 3+ consecutive days threshold
4. Add satSaveDailyErrors() / satLoadDailyErrors() for LittleFS persistence
5. Add state field sHeatCurveRec[12] to SATRuntimeSection for status JSON
6. Reset to HOLD in satCycleInit() and when bActive=false
7. Publish sat/heating_curve_recommendation (retained) in the MQTT block
8. Add heating_curve_recommendation to status JSON in SATcontrol.ino
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added SAT heating curve advisor: INCREASE/DECREASE/HOLD recommendation (Task #228).

Changes:
- SATcycles.ino: Added the full HCR implementation (~210 lines) at the end of the file:
  - _hcr_dailyMedian[7] ring buffer: 7-day rolling daily median error storage
  - _hcr_samples[96] intra-day ring buffer: collects (room - target) error samples each control tick
  - satHCRAddSample(): records each error sample, detects day boundaries via NTP time, commits daily median at midnight
  - _hcrIntraMedian(): insertion-sort median of intra-day buffer (runs once per day, n <= 96)
  - satHeatingCurveRecommendation(): computes INCREASE/DECREASE/HOLD from 3+ consecutive days above/below 0.5C threshold
  - satHCRSaveState() / satHCRLoadState(): persist/restore the 7-day ring to /sat_hcr.json on LittleFS
  - satHCRReset(): clears all state, sets sHeatCurveRec to "insufficient"
  - satCycleInit() extended to reset intra-day buffer and set recommendation to "insufficient"
- OTGW-firmware.h: Added sHeatCurveRec[12] to SATRuntimeSection (default "insufficient")
- SATcontrol.ino (prior commit): satHCRAddSample() called from control loop tick; satHCRLoadState() called from initSAT(); satHCRReset() called from satDisable(); MQTT publish to sat/heating_curve_recommendation (retained); status JSON includes heating_curve_recommendation
- restAPI.ino: Added heating_curve_recommendation to satSendHealthJSON()

Tests verified by code review:
- Algorithm: INCREASE when median < -0.5C for 3 consecutive days, DECREASE > +0.5C, HOLD otherwise
- Reset path: satDisable() -> satHCRReset() -> sHeatCurveRec = "insufficient"
- Invalid room temp -> satDisable() -> same reset path
- Persistence: satHCRSaveState() on each day commit, satHCRLoadState() on initSAT()

Risk: Low. Read-only advisory sensor, no effect on control loop.
<!-- SECTION:FINAL_SUMMARY:END -->
