---
id: TASK-1382
title: >-
  SAT PWM: time the ON phase from its start and keep waiting for ignition as
  thermo-nova (Q35)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T10:38:00Z'
updated_date: '2026-10-28T09:25:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.3
references:
  - TASK-1412
ordinal: 521000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From TASK-1273 / TASK-1366 R3. Firmware times ON expiry from the last flame-on edge and aborts the ON phase after a 180 s ignition timeout; thermo-nova times from the phase start, resyncs to the first ignition and continues (pwm.py:181-215, :197-199). Blocked on Q35. Scope together with TASK-1380 (OFF anchor).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ON phase timed from the phase start/first-ignition sync, no flip on a slow ignition (r3 host scenario green; expected-failure marker in tests/test_sat_hp_pwm.py removed)
- [x] #2 Sergeant D's OK recorded in docs/sat/sergeant-d-corrections.md before implementation (CLAUDE.md SAT rules)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Scope: TASK-1382 + TASK-1380 in one candidate (both PWM phase anchors), entries 43, 80 (Q35 A), 91 (Q51 A), 92 (field report). thermo-nova pwm.py (7beaaf5) keeps one timestamp _last_update: set at OFF/IDLE->ON (:162), at ignition sync while waiting (:188), at ON->OFF (:208) and on missing flow (:123); OFF->ON when elapsed >= off_time (:155); while waiting for flame the ON phase stays up to 180 s from its start (:186-199), then 'starting timer anyway' (no abort); min flame-on hold 180 s from _flame_on_at (:201-203); ON->OFF when elapsed >= on_time (:205-209).
Firmware change (SATcontrol.ino satPwmSchedule/satPwmBeginOn/satPwmEndOn): add _pwm_lastUpdateMs with those four set points; ON phase: waiting -> on flame sync (lastUpdate=now, flameOn=now, reseed BT, keep TASK-1401 first-flame count until TASK-1421), else stay while elapsed<180 s, else stop waiting and continue timing from the phase start; hold while now-flameOn<180 s; end when elapsed>=onTime. OFF phase: elapsed = now - lastUpdate, never satCycleGetFlameOffStartMs(). Remove the 180 s ignition abort, the onAnchor fallback to satCycleGetFlameOnStartMs() and _pwm_offStartMs. Unchanged: entry-ON (entry 43), hour limit (entry 59, TASK-1421 later), Q38 start gate (TASK-1383 next), suppression hold satPwmHoldElapsed (entry 45), DHW early return.
Proof: new tests/test_sat_pwm_anchors.py on the real-code cut of tests/test_sat_pwm_override.py: (1) replay of gruijter 2026-10-27 12:18-12:58 CET: five DHW flame bursts during OFF do not delay the ON start (HEAD: ON ~12:57:54-equivalent; fix: at lastUpdate+offTime); (2) ON start flame-off after an old DHW ignition does not end on the first tick (HEAD: CS 48->10->48); (3) slow ignition at 120 s keeps ON and times from the ignition; (4) no flame by 180 s: no abort, ON ends at onTime from phase start. Red on HEAD (OTGW_SAT_INO=git show HEAD copy), green on fix; r3 expectedFailure in test_sat_hp_pwm.py removed; all existing SAT PWM tests stay green. Then build.bat (combo), evaluate --quick, bench old-vs-fix on the Classic (boiler emulator DHW bursts) before the alpha goes to testers. Not on otgw-1.x.x (SAT is 2.0.0 only).

Self-review (advisor) changes: (a) TASK-1383 Q38 gate bundled: OFF branch starts ON only when onTimeMs >= minOnMs && (entryOn || elapsed >= offTimeMs), as pwm.py:155. (b) _pwm_lastUpdateMs also set in satPwmReset() (pwm.py:91 reset), covering the no-flow IDLE path. (c) Remove the mid-phase re-arm of waiting on a flame-out (relight keeps timing, pwm.py:144-146); a first flame after the 180 s wait timed out still sets _pwm_flameOnMs so the 180 s hold applies. (d) Test replay moves g_flameOffStart on every DHW flame-off (SATcycles.ino:786-789) and skips satApplyPWM while DHW is active (SATcontrol.ino:5096-5112); cut the real flame-edge handler if feasible, else cite. (e) Baseline existing SAT PWM tests on HEAD first; tests that encode the 180 s abort or flame-off timing are updated with the entry citation and listed in notes. (f) Bench: Classic + live PIC /simulate replay as for TASK-1401 (2026-10-27), with the 10-27 DHW draw pattern, old vs fix. A DHW flame carrying into a waiting ON phase syncs and counts: Q48 case, belongs to TASK-1421.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-28: Q35 A decided (entry 80). Field reproduction (entry 92): gruijter alpha.514 capture 2026-10-27, an ON phase that starts with the flame off anchors on the last hot-water ignition (SATcontrol.ino:883/:1024) and ends on its first tick: CS=48 10:14:24, CS=10 10:14:54, CS=48 10:15:24 CET, flame 10:15:35. Blocks TASK-1419 (Q45) and TASK-1421 (Q49 B).

AC#2: Sergeant D's OK recorded as entry 80 (Q35 A, msg 1564866308317773845, commit 874588e4a).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ON phase timed from its own start; waits up to 180 s for ignition, syncs at the first ignition, then times on without the old 180 s abort; 180 s flame-on hold; relight keeps timing (pwm.py:144-146, :162, :186-203; entry 80 Q35 A). Commit de23e3ea1 (alpha.515). AC#1: Host: tests/test_sat_pwm_anchors.py 10 OK on the fix; 7 failures on the pre-fix source (D1-D5, D7, D8); r3 expectedFailure removed in test_sat_hp_pwm.py (22 OK); all tests/test_sat_*.py OK; evaluate --quick 0 failed; build.bat esp32-combo SUCCESS (alpha.515+de23e3e, alpha.516+ece10d3). D2/D3/D4/D7/D8 cover ON timing; r3 scenario green, expectedFailure removed. Bench (Classic+S3 .63, live PIC, same /simulate replay, logs C:/Users/rvdbr/AppData/Local/Temp/otgw-bench-1382/{old,new}): OLD alpha.514 next ON 870 s after the ON->OFF switch / 544 s after the last DHW flame-off; NEW alpha.516+ece10d3 (OTA, settings unchanged) 450 s after the switch / 94.5 s after the last DHW flame-off, at the first tick with elapsed >= 720*(1-live duty). AC#2: entry 80. Not verified: slow ignition on a real boiler.
<!-- SECTION:FINAL_SUMMARY:END -->
