---
id: TASK-1428
title: >-
  SAT sustained switches: no flow-validity check (thermo-nova resets on flow
  None)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T18:15:00Z'
updated_date: '2026-11-03T12:19:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.5
priority: low
ordinal: 592000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1420 review. satGetFlowTemp() returns raw Tboiler (SATcontrol.ino ~1756-1760). With flow 0/unknown, the PWM underheat exit counts (flow <= requested - 3) in PWM ON. thermo-nova resets the overshoot start and the underheat timer when flow_temperature is None (heating_control.py:284-286, :333-335). Pre-existing (old code had the same gap with <). Needs: what counts as 'no flow' in the firmware (Tboiler not received / stale), then reset like thermo-nova.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: no valid flow resets the overshoot and underheat starts, red on HEAD, green on fix
- [x] #2 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 dependency reconciliation (Robert's go: reconcile the graph alongside the wave-3 work; scout+verifier report): depends on TASK-1412.5: 'what counts as no flow' is the LL-011 measurement-validity contract.

2026-11-02 QD-6 answered A (entry 218): flow missing resets the sustain timers. Waits only on TASK-1412.5's validity contract now.

2026-11-03 (agent, verified by lead): satCycleCheckPwmExits clears the underheat start on a missing flow (thermo-nova heating_control.py:333-335); satCycleCheckOvershootSwitch resets explicitly (:284-286); saturation untouched (:353-367 reads no flow). satGetFlowTemp already applies the 1412.5 validity (stale > 60 s = missing, entry 241). Function level: 4 new test_missing_flow_* tests, old 3 FAIL, fix 50/50. Loop level: no behaviour change (satPwmSchedule already resets PWM on a missing flow first; test_missing_flow_never_exits_pwm_at_loop_level green on old and new). evaluate 0 failed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The sustained overshoot and underheat timers reset when no valid flow exists, as thermo-nova (heating_control.py:284-286, :333-335). Function-level host tests red on the old code (3 FAIL), green on the fix; at loop level the running firmware already reset, so no unit changes behaviour (loop-level test green on both).
<!-- SECTION:FINAL_SUMMARY:END -->
