---
id: TASK-1520
title: >-
  SAT follow-thermostat drops a dial change made before SAT's first TC= after
  boot, then pushes the old target
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-31T09:30:00Z'
updated_date: '2026-10-31T09:37:00Z'
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
- [ ] #1 Host test: with push setpoint on, TC=<target> goes out at the boot decision (t=1 s), also in the hand-back branch; red on HEAD, green on the fix (tests/test_sat_restart_quiet.py)
- [ ] #2 Push off: no TC= at boot; existing restart, follow-thermostat and cmd-cache tests stay green (full tests folder)
- [ ] #3 Bench: after an OTA reboot with push on, the telnet log shows TC= at the boot decision, before SAT's first control tick
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
