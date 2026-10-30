---
id: TASK-1468
title: >-
  SAT: PWM exit to continuous takes effect one tick later than thermo-nova (exit
  checked after the PWM step)
status: To Do
assignee: []
created_date: '2026-10-29T08:42:00Z'
updated_date: '2026-10-30T18:55:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 628000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1381 review (2026-10-29). thermo-nova checks the PWM exit (_maybe_disable_pwm_runtime, heating_control.py:217) before computing the control setpoint (:224-228), so the exit tick already runs the continuous rule with the previous PWM CS. The firmware calls satCycleCheckPwmExits() after satApplyPWM() (SATcontrol.ino ~5662), so the exit tick still sends a PWM CS (e.g. 10 in an OFF phase) and continuous starts one 30 s tick later. Pre-existing; changes control timing, so the fix plan needs Sergeant D's OK (quote both sides).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Plan with thermo-nova and firmware citations posted to Sergeant D and his answer recorded in docs/sat/sergeant-d-corrections.md
- [x] #2 If he agrees: on the exit tick the continuous rule computes the CS (host test red on HEAD, green on fix)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30: AC1 satisfied by entry 134 (Q47b A, plan and answer recorded). Implementation (sonnet lane, wt-1468): tick order schedule -> exits -> setpoint (SATcontrol.ino satControlLoopRun), satApplyPWM(pid, scheduleDone) default for host tests. Review (opus): nothing blocking, order matches heating_control.py:202-228, all tick paths unchanged except the exit tick's CS, red-on-base genuine (exittick cont_marker), no prototype pitfall (PlatformIO regex skips default-arg prototypes; precedent OTGW-Core.ino:166). LOW fixed: CHANGELOG CH claim (CH was already 1). Remaining LOW (spec item numbering, stale item 19 since TASK-1370) noted; pre-existing: cold-cutoff tick in PWM still runs the schedule (thermo-nova skips pwm.update when heater off).

Integrated on alpha.560 (11875157a) as alpha.561: full suite 71/71 rc=0 (with TASK-1502 and TASK-1499), evaluate EXIT 0. tests: exittick underheat/saturation red on base (cont_marker 0), green on fix. Build/bench after the alpha.559 soak.
<!-- SECTION:NOTES:END -->
