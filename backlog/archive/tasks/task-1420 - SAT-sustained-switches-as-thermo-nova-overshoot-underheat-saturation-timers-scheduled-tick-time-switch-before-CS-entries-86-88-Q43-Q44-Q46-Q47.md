---
id: TASK-1420
title: >-
  SAT: sustained switches as thermo-nova: overshoot >=, underheat/saturation
  timers, scheduled tick time, switch before CS (entries 86, 88; Q43, Q44, Q46,
  Q47)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T06:47:00Z'
updated_date: '2026-10-29T19:15:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies: []
references:
  - docs/sat/sergeant-d-corrections.md
priority: medium
ordinal: 584000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-28: Q43 A (>=), Q44 A (underheat/saturation timed from the first qualifying sample, no 60 s bail-out, <= for underheat), Q46 B (time the 60 s switch on the tick's scheduled time), Q47 A (run continuous->PWM switch before the CS of the same tick). All in satCycleCheckAutoSwitch(). Q46b (scope for other timers) open. Feeds TASK-1412.8.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host tests per rule, red on HEAD and green on the fix: equality counts; underheat fires at 180 s and saturation at 300 s at a 30 s and a 60 s tick; switch at the first tick >= 60 s scheduled; first PWM CS on the switch tick
- [x] #2 build + evaluate green
- [x] #3 DHW guard: the 300 s overshoot guard after a draw arms (red on HEAD), and the exit timers reset during a draw and for 300 s after it (entry 108)
- [x] #4 Every path that clears the overshoot start (satCycleSkipAutoSwitch callers, mode change) also clears the underheat and saturation starts; regression: underheat running, one bypassed tick, ~200 s gap, no immediate switch
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Entries relied on: 86 (Q43 A >=, Q44 A thermo-nova timing, Q46 B scheduled tick time), 88 (Q47 A switch before CS), 95 (Q46b: scheduled time also for underheat/saturation), 41 (60 s from first sample), 37/15 (margin 2.0 stays), 87 (heat pumps use the same switch), 22 (>= settled). No conflict found.

thermo-nova (pinned 7beaaf5): update() order heating_control.py:202-228 (max disable :203-204, overshoot enable :206-209, pwm.update :211-215, exits :217-220, setpoint :224-228); overshoot :262-305 (reset only on flow < threshold :288-291); exits :307-329 (reset when PWM not enabled :308-311, hot water or 300 s after :313-315, heater off/cold :317-319, status != ON :321-323); underheat :331-352 (reset on flow > req-3 :337-340, timer :342-345, 180 s); saturation :354-372 (off_time > 0 resets :359-362, 300 s); sustained_runtime helpers.py:51-56 (first sample starts at 0, no gap bail-out).

Changes (SATcycles.ino, SATcontrol.ino):
1. Tick time: SATcontrol.ino captures the scheduled time of the control tick right after DUE(timerSATControl): timerSATControl_due - timerSATControl_interval (CATCH_UP_MISSED_TICKS has just advanced due by one interval, safeTimers.h:192-193). Passed to the switch functions as tickMs.
2. Split satCycleCheckAutoSwitch() into satCycleCheckOvershootSwitch(req, tickMs) (continuous->PWM) and satCycleCheckPwmExits(req, tickMs) (PWM->continuous). The overshoot switch moves before the 'Apply control mode' step, after the max-setpoint check, so the first PWM CS goes out on the switch tick (Q47 A, thermo-nova order). The exits stay after satApplyPWM, because saturation reads this tick's off time (entry 88 interpretation; moving them too is open in entry 88). Both keep the cold-setpoint gate; in the else branch the exit timers are reset too (thermo-nova :317-319).
3. Overshoot: flow >= req + margin counts (Q43 A); 60 s measured as tickMs - start (Q46 B).
4. Underheat/saturation: start timestamps instead of dt sums (Q44 A, Q46b): first qualifying tick starts at 0, fires when tickMs - start >= 180 s / 300 s; no gap bail-out; underheat counts while flow <= req - 3 (resets on >). Reset when mode != PWM (thermo-nova :308-311; firmware only reset in the PWM OFF phase), in the OFF phase (as now), and on satCycleResetSustainGuards().
5. Hot water: the DHW tick (SATcontrol.ino DHW early return) also resets the exit timers, as thermo-nova :313-315. Needed because a start timestamp would otherwise count the draw; the old dt>60 bail-out hid that. Recorded as our interpretation (entry 108), offered to Sergeant D. The 300 s after-DHW window for the exits stays open (new question).
6. Bug found while reading: _sustain_dhwEndMs (the 300 s overshoot guard after hot water, thermo-nova :272-274) is refreshed only inside satCycleCheckAutoSwitch when bDhwActive, but that function is never reached during DHW (the DHW branch returns first). So the guard never arms. Fix: refresh it on the DHW tick. Separate AC, red/green test.
7. Comments: satSetControlMode 'first schedule step one tick later' and the pwmOn capture comment updated to the new order.

Verification: host tests calling the real extracted functions (tests/test_sat_overshoot_sustain.py extended, plus a loop-order test cut from SATcontrol.ino): equality counts; underheat fires at 180 s and saturation at 300 s at 30 s and 60 s ticks; switch at first tick >= 60 s scheduled (tick late by ms does not matter); first PWM CS on the switch tick; DHW guard arms after a draw. Red on HEAD copies, green on fix. build.bat + evaluate --quick + related SAT tests. Not cross-line (SAT is 2.0.0 only).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 Q46b (entry 95): scheduled tick time applies to the underheat and saturation timers too.

2026-10-28 picked up ahead of dependency TASK-1412.3 (To Do): the maintainer-approved TASK-1412 wave plan puts TASK-1420 in wave 2. Advisor review of the plan: (1) skip paths must clear the exit starts too; (2) both halves of the thermo-nova hot-water guard (:313-315) -> entry 108 recorded first; (3) one sustained_runtime helper (helpers.py:51-56); (4) catch-up grid caveat queued as Q46c; (5) red run behavioural via a HEAD adapter. All adopted.

2026-10-28 review round 1. Opus correctness: HIGH - timerSATControl runs SKIP_MISSED_TICKS (initSAT CHANGE_INTERVAL_SEC without type, SATcontrol.ino:4277, settingStuff.ino:1005, safeTimers.h:96), so due-interval == millis(); there is no catch-up grid. Verified by me (grep + safeTimers.h). Decision (A, no behaviour change): keep SKIP, tick time = millis() once per tick; under SKIP ticks are never closer than one interval, so the timers already fire on the first tick >= their length after the first sample (Q46 B outcome). Recorded as entry 109; Q46c withdrawn; Q46 premise corrected. B (switch to catch-up) rejected: it would add CS/MM/CH bursts after a stall for no gain. Also fixed in this round: DHW sentinel false guard before millis wrap (bool flag), stale docs (rules review), loop-level switch-tick test. Filed TASK-1428 (flow validity, thermo-nova :284-286, :333-335) and TASK-1429 (DHW guard sub-tick draws / real end time).

2026-10-28 dependency reconciliation (Robert's go: reconcile the graph alongside the wave-3 work; scout+verifier report): TASK-1412.3 dependency removed: the task is Done with its own red/green host tests (same waiver as TASK-1419); TASK-1412.3 absorbs them into LL-004/LL-020.

2026-10-28 bench old vs new on the Classic+S3 (.63), replayed boiler, old = alpha.519+49135fe (same SAT code as alpha.520 for gas), new = alpha.521+2425108, overshoot margin 3.0 (bench value, not the decided default 2.0). Logs: %TEMP%\otgw-bench-1420\{A_519,C_521}. Raw lines checked by the main session. S13 overshoot: old 'sustained overshoot (flow=41.6 > sp=34.6 + 3.0)' 20:55:47, CS still continuous, 'PWM ON' one tick later 20:56:17; new '(flow=41.6 >= sp=35.6 + 3.0)' 22:22:51.194 with 'PWM ON' and cmd[CS=40.6] on the same tick. Underheat: old 150.05 s after the first qualifying tick (20:56:47 -> 20:59:17), new 180.02 s (22:23:51 -> 22:26:51). S4 saturation: old 270.13 s, new 300.03 s after the PWM ON with pwm_off=0. S2 hot-water guard: old switch 90 s after the last DHW tick (guard never arms), new 360.1 s (300 s guard + 60 s). Not shown on the bench: equality boundaries, jitter, skip paths, the exits guard (entry 108); host tests only.

TASK-1412.1 records review (R27): 2026-10-29. evidence-class clarification (TASK-1412.1 R27/LL-023): the Final Summary line 'Not verified: a real boiler/bench run' means no real heating plant. The notes record a bench replay of old vs new on the Classic+S3 (.63) with a replayed boiler and overshoot margin 3.0, which is not the 2.0 default (commit 42796032f). Evidence classes: host red/green = yes; bench replay = yes; physical heating plant = no. This Done task is not a physical-performance certificate. Entry 108 (exit DHW guard) is our interpretation pending its confirmation (already queued, open-questions.md Conf-108/104/107); the exit order after the CS is our reading pending Q47b.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT sustained mode switches now follow thermo-nova timing (Sergeant D entries 86, 88, 95; our readings 108, 109).

Changes (SATcycles.ino, SATcontrol.ino):
- satCycleCheckAutoSwitch split into satCycleCheckOvershootSwitch (continuous->PWM, before 'Apply control mode', so the first PWM CS goes out on the switch tick, Q47 A) and satCycleCheckPwmExits (PWM->continuous, after satApplyPWM because saturation reads this tick's off time).
- One sustained_runtime port (satSustainedFor, helpers.py:51-56) for all three timers: start timestamp from the first qualifying tick, no dt, no 60 s bail-out. Overshoot counts at >= (Q43 A), underheat at <= req-3 (Q44 A), 60/180/300 s.
- Tick time: _sat_tickMs = millis() once per control tick. The timer runs SKIP_MISSED_TICKS (initSAT re-sets it without a type), so ticks are never closer than one interval and each timer fires on the first tick at least its length after the first sample (Q46 B / Q46b outcome). Our earlier catch-up premise was wrong: entry 109; Q46c withdrawn.
- DHW guard: the 300 s guard after a draw never armed (end time refreshed only in code not reached during DHW; audit #11). Now refreshed on the DHW tick (satCycleNoteDhwTick), covers the PWM exits too (thermo-nova :313-315, entry 108), and a seen-flag replaces the boot sentinel (no false guard before a millis wrap).
- Every skip path / mode change clears all three starts.

Evidence (this session):
- AC1: tests/test_sat_overshoot_sustain.py 43 OK on the fix; on alpha.520 HEAD copies FAILED (failures=22, errors=1), behavioural: equality not counted (-1 != 90000), underheat fires 150 s / 120 s after the first sample at 30 s / 60 s ticks instead of 180 s ((180000) != (210000)/(240000)), 61 s tick never fires, saturation likewise. Loop-level test_sat_pwm_override 'swtick': on the switch tick mode=PWM, CS 48.0 (PWM ON), not continuous 62, CH=1; red on HEAD. Note: the late-tick test passes on HEAD too (under SKIP the old millis() timing gives the same drift); it is a regression guard, not a red/green proof.
- AC2: build.bat esp32-combo SUCCESS (firmware + fs), evaluate --quick Failed 0, health 100%; all 24 tests/test_sat_*.py OK.
- AC3: 4 DHW-guard tests + wrap test red on HEAD/old sentinel, green on fix.
- AC4: bypass tests red on HEAD and on a mutant that clears only the overshoot start; green on fix.
Reviews: opus correctness (found the SKIP premise, wrap, guard edges, switch-tick gap: fixed or filed), sonnet rules + SAT entries (stale docs fixed; no unnamed entry conflict).

Not verified: a real boiler/bench run. Follow-ups: TASK-1428 (flow validity, thermo-nova :284-286, :333-335), TASK-1429 (guard for sub-tick draws / real end time). verify_sat_low_load.py LL-003 (PID, SATpid.ino, untouched) fails; not checked whether it already failed before.
<!-- SECTION:FINAL_SUMMARY:END -->
