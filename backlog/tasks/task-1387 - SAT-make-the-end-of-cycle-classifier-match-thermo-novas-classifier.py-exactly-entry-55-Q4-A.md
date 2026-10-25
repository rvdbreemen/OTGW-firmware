---
id: TASK-1387
title: >-
  SAT: make the end-of-cycle classifier match thermo-nova's classifier.py
  exactly (entry 55, Q4 A)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-25T11:34:00Z'
updated_date: '2026-10-25T21:57:00Z'
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
- [ ] #1 Classifier results equal classifier.py for every rule (host test table, red on HEAD)
- [ ] #2 No SHORT for continuous cycles and no blanket under-120-s UNCERTAIN
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
