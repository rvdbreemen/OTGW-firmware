---
id: TASK-1517
title: >-
  SAT: a target change of exactly 0.05 C does not reset the PID (float compare
  19.549999-19.5 < 0.05f); thermo-nova resets on any change
status: Done
assignee:
  - '@claude'
created_date: '2026-10-30T22:47:00Z'
updated_date: '2026-11-01T08:41:00Z'
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
- [x] #1 Host test: a 0.05 C target change resets the PID state (red on HEAD, green on fix)
- [x] #2 An unchanged target (same value re-posted) still keeps the state
- [x] #3 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-01 (alpha.597): SAT_TARGET_CHANGE_MIN_C = 0.0495f in SATtypes.h, used for the target-change reset (SATcontrol.ino satResetControlStateIfTargetChanged and settingStuff SATtargettemp) and the zone setpoint reset. Entry 157 is our reading 'a change of at least 0.05 C'; a 0.05 step is stored as 21.0499992, so the old >= 0.05f missed it. No policy change, a float-representation fix. Entry 73 (follow thermo-nova resets) agrees. Evidence: tests/test_sat_pid_resets.py new 17/17 (incl. test_target_change_of_exactly_0_05_resets and test_target_unchanged_does_not_reset); old SATcontrol/settingStuff/SATtypes fail the 0.05 case. evaluate 0 FAIL. Open: AC3 build (pending).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1/AC2 checked earlier; test_sat_pid_resets 17/17 OK. Build: esp32-combo 2.0.0-alpha.597+d458ac3 (build/…-flash.zip 08:51, built and posted by the coordinating session); the task commit is an ancestor of d458ac3 (git merge-base --is-ancestor). evaluate.py --quick 2026-11-01: 0 FAIL, Health 100%. Bench .61 (OT-Direct) runs 2.0.0-alpha.597+d458ac3 (/api/v2/device/info), up 25 min without reboot.
<!-- SECTION:FINAL_SUMMARY:END -->
