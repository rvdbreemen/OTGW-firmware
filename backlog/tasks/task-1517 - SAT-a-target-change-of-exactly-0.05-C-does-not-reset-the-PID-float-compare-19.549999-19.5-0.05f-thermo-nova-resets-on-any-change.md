---
id: TASK-1517
title: >-
  SAT: a target change of exactly 0.05 C does not reset the PID (float compare
  19.549999-19.5 < 0.05f); thermo-nova resets on any change
status: To Do
assignee: []
created_date: '2026-10-30T22:47:00Z'
labels:
  - sat
  - pid
dependencies: []
priority: low
ordinal: 673000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the bench 2026-10-30 (alpha.564+d6dd798, OTGW32 .61): SATtargettemp 19.5 -> 19.55 kept the control state; settingStuff.ino:1055 and SATcontrol.ino:1734 test fabsf(new - before) >= 0.05f, and 19.55 is stored as 19.549999, so the difference is 0.049999. thermo-nova resets on every real change: climate.py:643 'if self._target_temperature == temperature: return', then reset_control_state() (:660). A 0.1 step does reset (bench: I -0.000728 -> 0 within 6 s). Check against Sergeant D entries 73 and 157 before changing; the 0.05 threshold is a firmware choice, not found in thermo-nova.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: a 0.05 C target change resets the PID state (red on HEAD, green on fix)
- [ ] #2 An unchanged target (same value re-posted) still keeps the state
- [ ] #3 build + evaluate green
<!-- AC:END -->
