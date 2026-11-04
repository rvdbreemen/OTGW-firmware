---
id: TASK-1380
title: >-
  SAT PWM: time the OFF phase from PWM's own ON->OFF switch, not from the last
  boiler flame-off (audit #59)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T10:14:00Z'
updated_date: '2026-10-28T09:25:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.3
  - TASK-1382
references:
  - TASK-1412
ordinal: 519000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
thermo-nova times the OFF phase from its own ON->OFF transition (pwm.py:208); hot-water and relight flame edges do not move it. Firmware reads satCycleGetFlameOffStartMs() (SATcontrol.ino:974 at HEAD 143609173), so a hot-water flame-off re-anchors the OFF timer. Evidence: gruijter alpha.500 run 2026-10-25 (msg 1563846779928055839), OFF periods ending at tap/start boundaries. Changes how SAT controls the boiler: plan quotes pwm.py and waits for Sergeant D's OK; scope together with Q35 (ON anchor). Blocked on Q35.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OFF phase timed from PWM's own ON->OFF switch; a DHW flame edge during OFF does not move it (host test red on HEAD, green on fix)
- [x] #2 Sergeant D's OK recorded (Q35 or a follow-up) before implementation
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-28: Field reproduction (entry 92, gruijter alpha.514 capture 20261027-093222.zip): OFF wait counted from the last flame-off incl. hot water (SATcontrol.ino:1046, SATcycles.ino:786-789); five draws 12:26-12:48 CET delayed CH ~23.5 min (ON 12:57:54 vs ~12:34:24). thermo-nova times OFF from _last_update (pwm.py:134,:155,:208). Q51 A (entry 91) + scope question Q51b. Proof: replay these draws old vs fix.

AC#2: Sergeant D's OK recorded: entry 43 (OFF wait from flame-off is not the design) + entry 80 (Q35 A) + entry 91 (Q51 A); field reproduction entry 92.

2026-10-28 Q51b (entry 97): follow thermo-nova, confirmed.

2026-10-28 bench OLD (Classic + S3 Mini, live PIC 6.8, .63, alpha.514+5b2afb5, /simulate replay 2324 lines, cph 5 = period 720 s, target tracked room+0.27; logs C:/Users/rvdbr/AppData/Local/Temp/otgw-bench-1382/old/): PWM entry 09:29:46.77, last heating flame-off 09:31:33.4, ON->OFF 09:34:16.87 (duty 0.314 -> off ~495 s), DHW bursts 09:34:45-35:03, 36:09-36:36, 37:42-38:09, 39:15-39:42.8, next ON request 09:48:47.19 = 870 s after the switch, 544 s after the last DHW flame-off (~360 s later than switch + off time, ~09:42:47). Settings restored, key diff empty.

2026-10-28 bench NEW: Bench (Classic+S3 .63, live PIC, same /simulate replay, logs C:/Users/rvdbr/AppData/Local/Temp/otgw-bench-1382/{old,new}): OLD alpha.514 next ON 870 s after the ON->OFF switch / 544 s after the last DHW flame-off; NEW alpha.516+ece10d3 (OTA, settings unchanged) 450 s after the switch / 94.5 s after the last DHW flame-off, at the first tick with elapsed >= 720*(1-live duty).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OFF wait timed from PWM's own ON->OFF switch (_pwm_lastUpdateMs, pwm.py:208/:155); hot-water flame-offs no longer move it (entries 43, 92, 97). Commit de23e3ea1 (alpha.515). AC#1: Host: tests/test_sat_pwm_anchors.py 10 OK on the fix; 7 failures on the pre-fix source (D1-D5, D7, D8); r3 expectedFailure removed in test_sat_hp_pwm.py (22 OK); all tests/test_sat_*.py OK; evaluate --quick 0 failed; build.bat esp32-combo SUCCESS (alpha.515+de23e3e, alpha.516+ece10d3). D1 replays gruijter's 2026-10-27 draws. Bench (Classic+S3 .63, live PIC, same /simulate replay, logs C:/Users/rvdbr/AppData/Local/Temp/otgw-bench-1382/{old,new}): OLD alpha.514 next ON 870 s after the ON->OFF switch / 544 s after the last DHW flame-off; NEW alpha.516+ece10d3 (OTA, settings unchanged) 450 s after the switch / 94.5 s after the last DHW flame-off, at the first tick with elapsed >= 720*(1-live duty). AC#2: entries 43/80/91/97. Not verified: real boiler (tester alpha next). Known gap: PWM frozen during hot water in an ON phase -> TASK-1422 (entry 95).
<!-- SECTION:FINAL_SUMMARY:END -->
