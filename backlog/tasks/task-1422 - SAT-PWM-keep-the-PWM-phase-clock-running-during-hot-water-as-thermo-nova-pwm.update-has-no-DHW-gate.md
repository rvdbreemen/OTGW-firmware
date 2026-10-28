---
id: TASK-1422
title: >-
  SAT PWM: pause the ON timer while the boiler DHW-active bit is set, then
  restart the start-up sequence (entry 95, Q48b)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T07:36:00Z'
updated_date: '2026-10-28T13:39:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1421
references:
  - docs/sat/sergeant-d-corrections.md
priority: medium
ordinal: 586000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Review finding on the TASK-1382/1380/1383 candidate (correctness + SAT-decision lenses, 2026-10-28). The SAT control loop returns early while the boiler DHW bit is set (SATcontrol.ino ~5096-5113), so satPwmSchedule() is frozen during a draw. An ON phase whose ON time runs out during a draw only switches to OFF at the first tick after the draw, so _pwm_lastUpdateMs gets the end of the draw and the next ON comes up to a full off time late (example: duty 0.22 at 1200 s, ON from T, 20-min draw from T+10: thermo-nova ON again ~T+1210, firmware ~T+2146). thermo-nova calls pwm.update() whenever heater_state is ON (heating_control.py:202-215); heater_state has no DHW term (heating_control.py:64-72). The draws in gruijter's 2026-10-27 capture all fell in OFF phases, which the candidate fixes. Running the schedule during DHW interacts with start counting: an ON start during a draw with the DHW flame lit (entry 89, Q48 'do not count flame when DHW enable is triggered', Q48b open), so plan it with TASK-1421.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: an ON phase interrupted by a hot-water draw keeps its used ON time and continues with the remaining ON time after the CH flame relights (freeze-clock: lastUpdate shifted by the draw length, no BT reseed, no new EMA window or hold); red on alpha.516, green on the fix
- [x] #2 No CS=10/CH=0 sent during the draw; OFF phase during a draw unchanged (timed from the ON->OFF switch)
- [x] #3 Sergeant D's OK on the open points (post-draw 180 s wait then resume anyway; no new hold/BT reseed; flame already burning after the draw resumes at once) recorded in docs/sat/sergeant-d-corrections.md before implementation
- [x] #4 build + evaluate green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Decision: entry 95 (Q48b), his deviation from thermo-nova (pwm.update runs during DHW, heating_control.py:202-215). During the DHW early return (SATcontrol.ino ~5096-5113) with PWM active and an ON phase requested: on the DHW rising edge store the ON time already used (now - _pwm_lastUpdateMs, clamp >= 0) and mark the phase paused; keep sending nothing new (no CS=10/CH=0; the PIC CS keepalive keeps the last ON CS). On the DHW falling edge: the ON phase resumes as a waiting phase: _pwm_waitingForFlame = true, _pwm_flameOnMs = 0, a 'resume' anchor so that at the CH ignition _pwm_lastUpdateMs = now - usedOnMs (remaining ON time continues) and _pwm_flameOnMs = now (180 s hold, entry 31); if no CH flame within 180 s the timer resumes anyway at that point (entry 80 rule, our interpretation in entry 95); if the flame is already burning at the first tick after DHW (no DHW bit), resume at once. The start-up sequence (flame-off hold setpoint / entry 45 flood) runs through satApplyPWM's existing flame-off path. OFF phase during DHW: unchanged (wall-clock from the ON->OFF switch, entry 97). Not counted as a new start.
Proof: host test: ON 264 s (duty 0.22, 1200 s), draw from +100 s to +400 s, CH ignition at +430 s: ON ends at 430 + (264 - 100) = 594 s (+-1 tick), not at the first tick after the draw (red on HEAD: ends ~+420); no CS=10 during the draw; draw in OFF still gives ON at switch + off (D1 stays green).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 scope rewritten by Sergeant D (entry 95, msg 1564902129896792156): not thermo-nova parity. During a PWM ON phase with boiler DHW active: pause the ON-seconds counter, keep sending the PWM ON values (no CS=10/CH=0); when DHW active drops, run the start-up sequence (ignition wait, entry 45 flood without return temp) and resume the remaining ON time once the CH flame is on. OFF phase during DHW: unchanged, timed from the ON->OFF switch (entry 97). AC#1 now: ON phase paused by a draw resumes with its remaining ON time after the CH ignition.

2026-10-28 advisor review: design defects in the first plan (flame-sync would give a full ON time and reseed BT with hot DHW flow; wait timed from lastUpdate would end the phase at once; end-time contradiction 594 vs 610). Freeze-clock model chosen: on DHW rising edge capture used ON ms; on falling edge (before _sat_prevDhwActive clears, before satPwmSchedule) shift _pwm_lastUpdateMs by the draw length and enter a post-draw wait timed from the DHW falling edge, bypassing flame-sync; clear in satPwmReset. Waiting for Sergeant D's OK on the three open points (asked after Q42).

2026-10-28 Sergeant D Q52 A, B, C yes (entry 103, msg 1564953189969829993): 180 s post-draw wait from the draw end then resume anyway; resume with the remaining ON time, no new hold, no BT reseed; flame still lit after the draw resumes at once. Implementation unblocked. D (start-count after a draw) asked separately (TASK-1421).

2026-10-28 implemented: PwmDhwPhase + satPwmOnDhwStart/satPwmOnDhwEnd/satPwmDhwHold/satPwmDhwResume, _pwm_dhwResumed (entries 95, 103, 104). Host: tests/test_sat_pwm_anchors.py 24 OK; on the alpha.517 source 8 failures (f1, f2, f3, f6, g1-g4); f4 (draw in OFF) and f5 (no BT reseed) characterization. Review (opus correctness, sonnet rules, sonnet tests): draw during first-ignition wait reseeded BT and gave a full ON time -> fixed (resume clears the wait, blocks sync/EMA/hold; entry 104); dead guards removed; entry 45 comment corrected (flood is TASK-1371); docs updated. All tests/test_sat_*.py OK; evaluate --quick 0 failed; compile esp32-combo SUCCESS (pre-fix version). AC#2: no CS=10/CH=0 during the draw (early return unchanged), OFF draw unchanged (f4).

Bench old vs new, Classic+S3 Mini .63, same gen_1422 /simulate replay (logs C:/Users/rvdbr/AppData/Local/Temp/otgw-bench-1422/old/ and C:/Users/rvdbr/AppData/Local/Temp/otgw-bench-1422/new/).
OLD alpha.517: PWM ON 12:42:08 (pwm_on 268 s), DHW 12:43:19-12:51:13, PWM OFF 12:51:38 = first tick after the draw (ON time used up by the draw).
NEW alpha.519+49135fe: PWM ON 13:50:00 (pwm_on 285 s); DHW bit 13:51:34.8-13:59:29.1; 'SAT: PWM ON paused for hot water (used 120 s of 298 s)' 13:52:00; 'SAT: PWM ON resumed after hot water (178 s left)' 13:59:30; PWM OFF 14:03:00 (pwm_on recomputed to 310 s -> resume+190 s = 14:02:40, next 30 s tick 14:03:00). Used 120 s = pause recorded at the next control tick, not at the bit edge (tick granularity).
AC2: during the draw only CS=47.0 keepalives every 30 s, no CS=10; CS=10.0 only at the PWM OFF.
Settings before/after diff empty (177 keys). Host suites rerun 2026-10-28: anchors 32 OK, hp_pwm 22 OK, override 72 OK, start_limit 17 OK, ot_loss 21 OK. evaluate --quick: Failed 0, exit 0.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Pause the PWM ON timer while the boiler DHW-active bit is set and resume with the remaining ON time (entries 95, 103, 104). Commit 8372293f2 (alpha.518), shipped in 2.0.0-alpha.519+49135fe.
AC1: host tests D/e/f/g in tests/test_sat_pwm_anchors.py red against the alpha.516 copy, green on the fix; rerun 2026-10-28 anchors 32 OK, hp_pwm 22, override 72, start_limit 17, ot_loss 21 all OK. Bench old vs new (same replay, C:/Users/rvdbr/AppData/Local/Temp/otgw-bench-1422/old and C:/Users/rvdbr/AppData/Local/Temp/otgw-bench-1422/new): alpha.517 ended ON at the first tick after the draw (12:51:38); alpha.519 logged 'paused (used 120 s of 298 s)', 'resumed (178 s left)', PWM OFF 14:03:00 = resume + remaining (recomputed) ON time, next tick.
AC2: bench: during the draw only CS=47.0 keepalives, no CS=10; CS=10.0 only at PWM OFF.
AC3: entries 95/103/104 recorded before implementation.
AC4: build.bat esp32-combo SUCCESS (fw 304 s, fs 18 s, artefacts 14:38, +49135fe); evaluate --quick Failed 0, exit 0.
Not verified: a real boiler (field run pending); pause is recorded at the next 30 s control tick, so up to one tick of ON time is counted as used.
<!-- SECTION:FINAL_SUMMARY:END -->
