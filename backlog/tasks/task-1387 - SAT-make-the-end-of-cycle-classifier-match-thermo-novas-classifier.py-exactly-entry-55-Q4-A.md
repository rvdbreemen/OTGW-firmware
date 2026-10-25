---
id: TASK-1387
title: >-
  SAT: make the end-of-cycle classifier match thermo-nova's classifier.py
  exactly (entry 55, Q4 A)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T11:34:00Z'
updated_date: '2026-10-25T23:24:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
ordinal: 526000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-25 (Q4: A, entry 55). Remove the firmware-only rules (a continuous cycle labelled SHORT after the overshoot test; any cycle under 120 s always UNCERTAIN) and add classifier.py's UNCERTAIN results: kind UNKNOWN (cycles/classifier.py:22-23), PWM tail setpoint error p50/p90 missing (:42-43), continuous tail control-setpoint error p50/p90 missing (:71-72). The label switches PWM/continuous (entry on Q2), so it must follow classifier.py. Prerequisite for TASK-1331.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Classifier results equal classifier.py for every rule (host test table, red on HEAD)
- [x] #2 No SHORT for continuous cycles and no blanket under-120-s UNCERTAIN
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Entry 55 ('exactly'), checked against entries 7, 11, 15, 18, 36, 37, 55: no conflicts. Rewrite _cycleClassify (SATcycles.ino ~659-717) in classifier.py order (cls = cycles/classifier.py @7beaaf5):
R1 dur<=0 INSUFFICIENT (cls:19-20; keep <3 samples as firmware-only INSUFFICIENT record).
R2 kind UNKNOWN -> UNCERTAIN (cls:22-23): port tracker.py:314-324 kind rules (needs CH sample count; CH source = master CH-enable bit, as the serial coordinator coordinator/serial/__init__.py:54-55; verify MasterStatus is filled on OT-Direct first). Compute kind before classify; count kind samples in the 1 Hz gate (also fixes the dead <3 floor and uint16 wrap).
R3 DHW in tail -> UNCERTAIN (cls:25-26): bool ring per tail sample.
R4 PWM IDLE -> UNCERTAIN (cls:28-29): satPwmIsIdle() flag (approximation of pwm.py IDLE).
PWM: req p50/p90 missing -> UNCERTAIN (cls:42-43); reqErrP90<=-3 -> UNDERHEAT_PWM or UNCERTAIN below cold (cls:45-54, cold 28.2/21 entry 11); only when PWM ON: on<=0 or dur<on*0.8 -> SHORT (cls:56-64, drop the 30 s floor and 60 s fallback); else GOOD (no OVERSHOOT for PWM, cls:66).
Continuous: ctrl p50/p90 missing -> UNCERTAIN (cls:71-72, controlSp<=0 as firmware stand-in); ctrlErrP90>=margin -> OVERSHOOT first, no overshootSec gate (cls:74-78; completes entry 37); ctrlErrP50<=-3 -> UNDERHEAT/UNCERTAIN (cls:80-88); else GOOD. Remove continuous SHORT and blanket <120 s UNCERTAIN.
Out of scope: per-sample control setpoint (TASK-1405), per-sample requested setpoint, tail warm-up exclusion and interpolated percentiles (new task), dropping <3-sample cycles, the on_cycle_end switch (TASK-1331).
Test: tests/test_sat_cycle_classifier.py cuts the real satCycleOnFlameChange/satCycleSample/_cycleClassify, 19 cases one per rule, red on HEAD by assertion, green on fix. No enum changes; UI/MQTT labels stay valid but counts shift (fewer SHORT, more OVERSHOOT/UNCERTAIN).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-26 implemented (workflow wf_fdae7eb9: implement, two refuting reviews Opus correctness + Sonnet SAT records/rules, fix). Reviews: 6 HIGH/MEDIUM findings; 5 fixed (switch-tick IDLE, simulation cycles all UNCERTAIN, DHW early in a short cycle, stale docs, idle flag without real-code test), 1 deferred (per-sample setpoint, TASK-1405), 1 rejected with evidence (IDLE kept at the hour-limit return, pwm.py:156-158 + entry 59). Main-session re-run on commit 9f91a8812: tests/test_sat_cycle_classifier.py 25 OK on the fix; same file against HEAD~ copies FAILED (failures=18); test_sat_pwm_override 72 OK; all tests/test_sat_*.py and test_otd_ch_override OK; evaluate --quick Failed 0. Deviations kept and documented: cold 28.2/21 (entry 11), margin 2.0 (entries 15/37), UNDERHEAT_PWM label, controlSp/requestedSp <= 0 as missing-data stand-ins, approximated PWM IDLE, no tail warm-up exclusion in the percentiles, floor-index percentiles, per-cycle setpoints. Visible effects: fewer SHORT (no continuous SHORT, PWM SHORT only in the ON phase), more OVERSHOOT (no 10 s gate), more UNCERTAIN; fLastCycleFractionCH is now the real CH fraction (ch/n). ORDERING: TASK-1331 must not land before TASK-1405.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The SAT end-of-cycle classifier now follows thermo-nova cycles/classifier.py rule by rule and in its order (Sergeant D entry 55, Q4 A); commit 9f91a8812, alpha.510. Kind UNKNOWN, hot water in the tail, PWM IDLE and missing setpoint errors give UNCERTAIN; PWM SHORT only when the cycle ends in the ON phase (no 30 s floor, no 60 s fallback); PWM never OVERSHOOT; continuous OVERSHOOT is tested first at p90 >= margin without the overshoot-seconds gate (completes entry 37); the continuous SHORT and the blanket under-120 s UNCERTAIN are gone. Cycle kind follows tracker.py:314-324 (master CH-enable, slave DHW), counted at 1 Hz. AC#1 evidence: tests/test_sat_cycle_classifier.py drives the real satCycleOnFlameChange/satCycleSample/_cycleClassify and cross-checks expected classes with the real classifier.py loaded without Home Assistant: 25 OK on the fix, FAILED (failures=18) against the pre-fix sources. AC#2 evidence: cases C4b (40 s and 90 s continuous in band -> GOOD) and C2order pass on the fix and fail on the old code. All tests/test_sat_*.py and test_otd_ch_override OK, evaluate --quick 0 failed, build.bat esp32-combo SUCCESS (firmware + fs). Two refuting reviews: 5 findings fixed, 1 deferred (TASK-1405), 1 rejected with evidence (entry 59). Not verified: on a device (no boiler on the bench); whether master bit 8 follows SAT's CH= on a PIC board. Kept deviations: cold 28.2/21 (entry 11), margin 2.0, UNDERHEAT_PWM label, missing-data stand-ins, approximated IDLE, tail percentiles without warm-up exclusion. TASK-1331 must not land before TASK-1405.
<!-- SECTION:FINAL_SUMMARY:END -->
