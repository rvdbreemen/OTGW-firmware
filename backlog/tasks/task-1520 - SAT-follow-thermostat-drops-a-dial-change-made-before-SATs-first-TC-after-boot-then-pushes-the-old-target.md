---
id: TASK-1520
title: >-
  SAT follow-thermostat drops a dial change made before SAT's first TC= after
  boot, then pushes the old target
status: Done
assignee:
  - '@claude'
created_date: '2026-10-31T09:30:00Z'
updated_date: '2026-10-31T10:14:00Z'
labels:
  - sat
  - bug
dependencies: []
priority: high
ordinal: 676000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter alpha.567 (Discord #alpha-testing msg 1566018068998783016, capture 20261031-092435): OTA reboot 09:26:04; thermostat MsgID16 T frame 15.00 -> 20.00 at 09:28:39; SAT's first TC= after boot at 09:31:08 was TC=15.0 (restored target); SAT target stayed 15.0 until the next dial change (19.5 at 09:58:11, adopted 09:58:52). Cause: satFollowThermostatDecide (SATcontrol.ino ~1845) adopts a confirmed edge only when lastSentTC > 0 (a TC= sent since boot); before that it only records. So a change in the first ~5 min after boot is lost and SAT then overrides the thermostat with its stored value. Fix must keep the entry-2 echo protection (old override mirrored after reboot).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: with push setpoint on, TC=<target> goes out at the boot decision (t=1 s), also in the hand-back branch; red on HEAD, green on the fix (tests/test_sat_restart_quiet.py)
- [x] #2 Push off: no TC= at boot; existing restart, follow-thermostat and cmd-cache tests stay green (full tests folder)
- [x] #3 Bench: after an OTA reboot with push on, the telnet log shows TC= at the boot decision, before SAT's first control tick
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Maintainer chose 'Push TC= at boot first' (entry 169). satRestartStep(): after the boot decision (valid or hand-back branch), enqueue TC=<target> when push setpoint is on, via satEnqueueIfChangedTC. Host test in tests/test_sat_restart_quiet.py (TC at t=1 s, red on HEAD). Full tests + evaluate --quick, bump, combo build, OTA bench.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Graph: docs/evidence/task-1520-gruijter-alpha567-follow-gap.png (room, thermostat setpoint, TrSet T frames, SAT target, flame/CH/DHW). Replied to gruijter msg 1566021977792061461.

2026-10-31 Robert in session: 'prioritize task 1520' (maintainer OK).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
alpha.568 (a6022396e): satRestartStep() sends TC=<target> at the boot decision when push setpoint is on (maintainer decision, entry 169). Evidence: AC1 tests/test_sat_restart_quiet.py new cases red on HEAD (2 failures: [] != [(1,'TC=15.0')]), green on the fix (20 tests OK). AC2 full tests folder 0 failing files, evaluate --quick 100 %, push-off case sends no TC=. AC3 bench OTGW32 .61 OTA alpha.568, SAT simulation + push on, reboot: boot decision 11:11:14.817, OTD cmd TC=20.0 at .821, first control tick .845, exactly one TC= (docs/evidence/task-1520-bench-alpha568-boot-tc.txt; graph of the field case docs/evidence/task-1520-gruijter-alpha567-follow-gap.png). Not verified: a PIC board (Classic .63 offline); the field case itself (gruijter). Remaining gap: a dial change in the first <=60 s after boot is still only recorded. Posted msg 1566032509127360536.
<!-- SECTION:FINAL_SUMMARY:END -->
