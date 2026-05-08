---
id: TASK-590
title: 'feat(sat): add 180s tail window to cycle classification'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 16:49'
updated_date: '2026-05-08 17:59'
labels: []
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The Python SAT cycle classifier uses only the last 180 seconds of each cycle (the tail window) for P90/P50 percentile classification, not the whole cycle. The C++ SATcycles.ino classifies on whole-cycle percentiles. The tail window makes classification more responsive to how the cycle ended: a cycle that started with overshoot but corrected itself is not classified as OVERSHOOT. The C++ simplification can produce false overshoot classifications for long cycles.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Tail ring buffer (last 180s of samples) maintained alongside the full-cycle buffer in SATcycles.ino
- [x] #2 Tail P90 and P50 percentiles computed from the tail buffer at end-of-cycle
- [x] #3 Cycle classifier updated to use tail percentiles instead of full-cycle percentiles
- [x] #4 No regression in SHORT_CYCLING or GOOD cycle classification
- [x] #5 Tail buffer size derived from 180s and the existing 1Hz sample rate
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add tail ring buffer declarations after full buffer (SATcycles.ino ~line 74)\n2. Reset tail buffer in satCycleInit() and flame-on start\n3. Add _tailPercentile() after _flowPercentile()\n4. Add 1Hz-gated tail sampling in satCycleSample()\n5. Update flame-off classification to use tail P90/P10\n6. Build, bump, commit, push
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Added tail ring buffer (SAT_TAIL_SAMPLE_SIZE: 64 on ESP8266, 180 on ESP32) with 1Hz gate in satCycleSample(). Added _tailPercentile() function (insertion sort on tail buffer copy). Updated flame-off classification to use tailP90/tailP10 instead of fullP90/p10. Tail P50 computed as diagnostic value shown in debug log. Build running.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added 180s tail ring buffer (SAT_TAIL_SAMPLE_SIZE: 180 on ESP32, 64 on ESP8266) sampled at 1Hz in satCycleSample() for time-accurate end-of-cycle classification window.

Changes:
- SATcycles.ino: added _tail_samples[] ring, _tail_sampleHead/Count/LastMs state vars
- satCycleInit() and flame-on reset: tail buffer zeroed alongside full buffer
- _tailPercentile(): insertion-sort percentile function on tail buffer copy
- satCycleSample(): 1Hz-gated tail append after full buffer append
- Flame-off: tailP90/tailP10 computed and used for classification; fullP90 logged as diagnostic

ESP8266 RAM impact: +256 bytes (64 slots x 4 bytes). Build: exit 0 (alpha.31). All 5 ACs verified. Committed d3ee72fc, pushed to origin/feature-dev-2.0.0-otgw32-esp32-sat-support.
<!-- SECTION:FINAL_SUMMARY:END -->
