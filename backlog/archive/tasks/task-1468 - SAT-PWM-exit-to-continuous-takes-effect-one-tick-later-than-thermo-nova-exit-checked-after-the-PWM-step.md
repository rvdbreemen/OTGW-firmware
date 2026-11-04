---
id: TASK-1468
title: >-
  SAT: PWM exit to continuous takes effect one tick later than thermo-nova (exit
  checked after the PWM step)
status: Done
assignee: []
created_date: '2026-10-29T08:42:00Z'
updated_date: '2026-10-31T05:10:00Z'
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

2026-10-31 close-out: Shipped in 2.0.0-alpha.564+d6dd798 (pushed origin/dev 8744d985a..d6dd798f1, zip Discord msg 1565955628164186185, report msg 1565955677522894960). Build: esp32-combo SUCCESS (alpha.564 includes the TASK-1414 prototype fix d6dd798f1). evaluate --quick on d6dd798f1: Failed 0, 100%. All 73 tests/test_*.py files OK on d6dd798f1 (one file at a time, no build running). Bench: OTGW32 .61, OTA app + filesystem via scripts/ota_flash.py, 173 settings keys kept. Bench limitation (Robert approved publishing with it stated): the exit tick is proven by the host test on the real SATcontrol.ino only (old 2 FAIL, fix 2 OK); a draw replay ran without fault or restart.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
On the tick an underheat or saturation exit fires, the continuous CS goes out in that tick: PWM schedule, then exits, then the setpoint (thermo-nova heating_control.py:211-228, Sergeant D entry 134). Evidence: AC#1 entry 134 (Discord msg 1565582331387977849); AC#2 tests/test_sat_pwm_override.py -k exit_tick old 98cb3c2ca FAILED (2), fix OK. alpha.561, published in alpha.564+d6dd798. Not verified on hardware: the exit tick itself (bench replay cannot line up a PWM exit).
<!-- SECTION:FINAL_SUMMARY:END -->
