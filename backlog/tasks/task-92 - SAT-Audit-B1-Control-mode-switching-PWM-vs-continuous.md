---
id: TASK-92
title: 'SAT Audit B1: Control mode switching (PWM vs continuous)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:49'
updated_date: '2026-04-09 05:18'
labels:
  - audit
  - sat
  - phase-2
milestone: m-0
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Compare the logic for switching between PWM mode and continuous mode in Python SAT (thermo-nova) with the C++ implementation in SATcontrol.ino. Verify thresholds, hysteresis and timing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Python control mode switching logic fully documented
- [x] #2 C++ equivalent identified and compared
- [x] #3 Deviations in thresholds, timing or hysteresis documented
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
B1 audit: Control mode switching (PWM vs continuous) - GAPS FOUND

Python: heating_control.py uses event-driven architecture with `_maybe_enable_pwm_on_sustained_overshoot()` (OVERSHOOT_SUSTAIN_SECONDS=60s) and `_maybe_disable_pwm_runtime()` (UNDERHEAT_SUSTAIN_SECONDS=180s, SATURATION_SUSTAIN_SECONDS=300s). Overshoot threshold: OVERSHOOT_MARGIN_CELSIUS=3.0C. Saturation: detected when PWM off_time_seconds==0 for 300s sustained. Python has DHW_OVERSHOOT_GUARD_SECONDS=300 guard: overshoot switch is suppressed for 300s after DHW ends.

C++ (SATcontrol.ino): Has TWO parallel auto-switch implementations that can conflict:
1. satCycleCheckAutoSwitch() in SATcycles.ino: Uses SAT_OVERSHOOT_SUSTAIN_SEC=60s, SAT_UNDERHEAT_SUSTAIN_SEC=180s, SAT_SATURATION_SUSTAIN_SEC=300s — matches Python.
2. Inline logic in satControlLoop() at line ~3087: Uses 180s (3 min) for BOTH overshoot->PWM and underheat->continuous with tighter thresholds (0.5C for overshoot, 0.3C for underheat).

GAPS:
1. DUPLICATE auto-switch logic: satCycleCheckAutoSwitch() and inline satControlLoop() code both handle mode switching independently — only one is called (satCycleCheckAutoSwitch is NOT called from satControlLoop). The inline logic uses different thresholds (180s overshoot vs Python's 60s; 0.5C margin vs 3.0C).
2. MISSING DHW guard: C++ has no 300s post-DHW suppression guard for overshoot-to-PWM switching (Python: DHW_OVERSHOOT_GUARD_SECONDS=300).
3. Overshoot threshold mismatch: Python uses OVERSHOOT_MARGIN_CELSIUS=3.0C from cycles/const.py; C++ inline uses +0.5C.
4. Python `on_cycle_end` callback: PWM enables on overshoot cycle, disables on underheat cycle — C++ has no equivalent event-driven callback on cycle completion.

Created audit-fix tasks for items 1-4.
<!-- SECTION:FINAL_SUMMARY:END -->
