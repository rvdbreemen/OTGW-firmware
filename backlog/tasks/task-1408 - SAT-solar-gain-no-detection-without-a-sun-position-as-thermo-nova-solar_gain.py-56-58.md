---
id: TASK-1408
title: >-
  SAT solar gain: no detection without a sun position, as thermo-nova
  (solar_gain.py:56-58)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-26T18:41:00Z'
updated_date: '2026-10-29T05:39:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
ordinal: 546000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in gruijter's alpha.513 capture 2026-10-26 (second reader checked the citations): with no sun elevation available the firmware detects solar gain from the room rise rate alone (SATcontrol.ino:4616-4629, :4635-4643) and was active 10:11:55-10:45:25 while the rise came from the boiler heating the room; while active it lowers the requested setpoint by 2 C (SATcontrol.ino:5298-5300) and freezes the PID integral (SATpid.ino:178). thermo-nova returns inactive when the sun elevation is unknown (solar_gain.py:56-58), uses the raw rate between two samples (:66-78), a 600 s hold (:60-63) and is off by default (const.py:132). SAT behaviour change: needs Sergeant D's OK before implementing (CLAUDE.md SAT rules).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Plan quotes thermo-nova solar_gain.py and gets Sergeant D's OK (recorded entry)
- [ ] #2 Without a sun position solar gain stays inactive (host test red on HEAD)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
thermo-nova solar_gain.py:56-58: 'if sun_elevation is None: self._hold_until = None; return SolarGainSnapshot(active=False, ...)'. Firmware SATcontrol.ino:4786-4800 falls back to rise rate alone when no sun elevation is valid (comment 'AC#6'), and on a stale (>1 h) elevation it only clears the valid flag and continues. Change: after the stale check, when !state.sat.bSunElevationValid -> bSolarGainActive=false, _solar_wasActive=false, _solar_conditionMs=0, return (the rise-rate EMA above keeps updating, so fIndoorRiseRate stays published). Out of scope (separate differences, not decided here): the EMA rate vs thermo-nova's two-sample rate (:66-78), the 10 min sustain vs the 600 s hold (:60-63), default off (const.py:132). Test: host test compiling the real satUpdateSolarGain with stubs: no sun elevation + fast rise + low modulation + valves open for 15 min stays inactive (red on HEAD, green on fix); stale elevation (>1 h) clears an active state; valid high elevation still detects. Starts after TASK-1282 is committed (same file).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-26 MAINTAINER OVERRIDE (Robert, in session): implement without waiting for Sergeant D's OK, because the change aligns with thermo-nova (ground truth #2, solar_gain.py:56-58) and no recorded entry conflicts. Sergeant D is informed with the citations; if he objects, revert. AC#1 is satisfied by this recorded override instead of his OK.

2026-10-29 AC#1: Robert's maintainer override of 2026-10-26 (commit 8b2e154c0) replaces the wait for Sergeant D; plan quotes solar_gain.py:56-58 (opened in session).
<!-- SECTION:NOTES:END -->
