---
id: TASK-1887
title: >-
  SAT: hot tolerance 0.1 / cold tolerance -0.1 (Sergeant D entry 326, meaning
  pending Q-326a)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T07:25:00Z'
updated_date: '2026-11-16T19:52:00Z'
updated_date: '2026-11-16T19:53:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
ordinal: 960000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D decision 2026-11-13 (https://discord.com/channels/812969634638725140/1105556725714649128/1570698600952893454), entry 326. No tolerance setting exists for SAT in the firmware; thermo-nova has no hot/cold tolerance key (only DEADBAND 0.1, const.py:16); HA MQTT climate has no such option. Blocked on Q-326a (asked https://discord.com/channels/812969634638725140/1105556725714649128/1570700362187542539): A hysteresis on heating demand / B PID-self-tune deadband / C HA display only. SAT control change if A or B: plan with citations and Sergeant D's OK before implementation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Pure helpers in SATchEnable.h (satToleranceMaxError, satToleranceDemand, satChEnableDecide band input), host-tested: exact boundaries 0.1/-0.1, hold inside the band, NAN never blocks heating, eco (main room only) vs comfort (any usable room) per entry 378, band stop clears only the CH bit
- [x] #2 Loop-level host test on the real control-tick cut, red on origin/dev and green on the fix: warm room gives CH=0 with a live setpoint and unchanged PID request; PWM stays PWM and its ON/OFF schedule keeps toggling during the stop (entry 384); a lit PWM phase and a continuous burn both run 180 s first (entries 275, 376); resume below target - 0.1; cold cutoff unchanged
- [x] #3 Seam sweep of the SAT tests that cut the control tick and the LL contract (all 18 ids) green; evaluate.py --quick no new FAIL
- [x] #4 ADR-250 (Proposed via adr-kit) records the decision and amends ADR-199; accepted by Robert before push
- [x] #5 esp32-combo build and a bench run by the bench-tester (health; the band behaviour is host-proven, no SAT scenario on hardware)
- [ ] #6 Follow-ups in the post build after the 2.0.0 beta tag, benched on .63: the band narration shows the deciding value (X.XX matches room/target of the same SATTICK), and ON phases that start during a band stop leave starts=/cycles_this_hour unchanged (entry 398; host-proven by test_sat_tolerance_band if .63 does not run PWM)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Plan (ba, 2026-11-16), built on Sergeant D's recorded answers; no open interpretation left except the follow-up below.

Ground truth checked this session: entries 326 (values 0.1 / -0.1), 328 (HA generic thermostat: stop at target + tol, start below target - tol), 361 (band decides on/off only, PID keeps running, cold cutoff unchanged), 376 (Q-361c B: continuous burn runs >= 180 s before a band stop), 377 + 384 (Q-361a / Q-377a "yes, B": band stop = CH=0, PWM stays active), 378 (Q-361b: eco = main room decides, comfort = any usable room). thermo-nova 7beaaf5 has no band: heater-off acts at once (heating_control.py:229-234), its 180 s hold is PWM-only (pwm.py:201-203), zones only raise the requested setpoint (climate.py:384-385). Firmware: iHeatingMode 0 = COMFORT, 1 = ECO (SATtypes.h:582); the only tick-time CH sender besides the fixed thermal-safe (CH=1), summer and valves-closed (CH=0) paths is the main satChEnableDecide() call; satSendCH() is on change with a repair, so no between-tick CH=1.

Design (lane/ba-1887b, fresh from origin/dev; the first lane's heater-off design is dropped):
1. SATchEnable.h: satToleranceMaxError(roomValid, roomErr, zoneErr, zoneUse, n, ecoMode), satToleranceDemand(wasOn, err) with SAT_TOL_COLD_C = SAT_TOL_HOT_C = 0.1, satChEnableDecide(..., bandOff = false) returns 0 when bandOff (after the cold cutoff).
2. SATcontrol.ino: _sat_tolErr in the zone block (eco = iHeatingMode 1); latch + narration next to the cold-cut latch; _sat_tolBandOff = !demand && !satBandWaitsForMinBurn() (PWM: satPwmColdCutWaitsForMinBurn(), entry 275 applied to the band as our reading of 376's 'the same minimum as a PWM ON phase'; continuous: lit CH flame younger than 180 s since satCycleGetFlameOnStartMs()); only the main CH command reads it; reset to demand on after satDisable().
3. Cold cutoff code unchanged (satColdThreshold/satColdCutActive byte-identical to origin).
Follow-up for Sergeant D (non-blocking, with Robert's 'this release or next' question): flameless PWM ON phases during a pause count as starts in the hourly limit (entries 59, 90, 95), so the first restart after a long pause can wait for the limit window.
Tests: tests/test_sat_tolerance_band.py (helper + real tick cut + source checks), seam sweep, LL contract; ADR-250 amends ADR-199; build + bench (health).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13: meaning settled by entry 328 (Q-326a answered A, Sergeant D msg 1570701593073025099): Home Assistant generic-thermostat hysteresis on the heating demand, start below target - 0.1, stop at or above target + 0.1. Open for the plan (entry 328): how the band combines with the PID and the cold cutoff (ADR-199). Plan with citations and Sergeant D's OK before code.

Q-328a decided 2026-11-15 by the coordinator under Robert's directive (CLI) 'Do not wait for Sergeant D. Finish everything that can be finished.'; Sergeant D may overrule, his answer later becomes a follow-up task; asked in the 18-question batch in #dev-sat-mqtt (item 8, https://discord.com/channels/812969634638725140/1105556725714649128/1571633456381694074): the tolerance band of entry 328 only decides CH on/off; the PID keeps running; the cold cutoff (ADR-199) is unchanged. Unblocked.

2026-11-16 implemented on lane/ba-1887b (rebased onto origin/dev 42980de8f; code commit d37e94aa0, its non-version diff equals the pre-rebase commit, 368 lines, diff 0). Evidence: tests/test_sat_tolerance_band.py 18/18 on the fix (7 helper, 8 loop on the real tick cut, 3 source); with OTGW_SAT_INO = origin/dev SATcontrol.ino 10 of the 11 loop/source tests fail, the one that passes is the cold-cutoff control. Seam sweep, one file at a time, 35 SAT files: all OK after two seam fixes (test_sat_pwm_override: the CH call now passes _sat_tolBandOff; test_sat_max_tset: stubs for the band latch inside its cut). LL contract, foreground chunks OTGW_LL_ONLY: 001-004 5 pass, 010-013 7 pass, 014-017 3 pass + LL-016 skip (as designed), 018 10 pass, 019-023 9 pass. evaluate.py --quick 0 FAIL / 0 WARN. ADR-250 rewritten (Proposed, amends ADR-199), README entry + index. docs/sat/firmware-sat-spec.md step 18 sub-bullet. The first lane (lane/ba-1887, heater-off design, entries 361 only) is superseded and stays local. Open: AC4 (Robert's accept of ADR-250), AC5 (build + bench).

2026-11-16 build + bench: esp32-combo 2.0.0-beta.349+ae8626c (HEAD ae8626cff on 55dd8f015), EXIT=0, app version check OK, RAM 40.0 %, Flash 93.5 %; after the rebase onto 95's 55dd8f015 (which touches SATcontrol.ino/SATchEnable.h) re-ran tolerance_band 18, pwm_override 83, max_tset 37, loop_integration 21, ot_loss 24, restart_quiet 20, narrate_fits 1, all passed. Archive RvdB/OTGW-build-archive/beta349-1887/ (8 files + SHA256SUMS). 09 on .63 (Classic, PIC 6.8, SAT on, no boiler) 12:04-12:10: artifacts 8/8 OK, OTA app+fs OK with 'settings: 152 keys, changed: none' (the band adds no setting), fwversion 2.0.0-beta.349+ae8626c, appelfsha256 = .elf sha256, no crash fields, MQTT connected, heap ~47 KB; none of the three band narration lines in the boot capture (no room at target + 0.1). Band behaviour itself host-proven only (no SAT scenario on hardware). Pushed 55dd8f015..ae8626cff, posted Discord 1571845527916646502. 1 h soak on .63 12:10-13:11 for the 20:00 beta decision. Remaining: AC4 (Robert's accept of ADR-250, asked via the coordinator).

2026-11-16 follow-up Q-384a (flameless PWM ON phases during a band pause count in the hourly start limit per entries 59/90/95; options A keep, B do not count during a band pause, our advice B; with the 2.0.0-or-2.1.0 scope line) posted to Sergeant D by 92 at 12:05:20 UTC: https://discord.com/channels/812969634638725140/1105556725714649128/1571858712564465715. Non-blocking: the code follows the recorded entries; if he picks B it becomes its own task.

Entry 398 edge case, OUR interpretation (ba, recorded by comms 92 on 2026-11-16; not Sergeant D's words, not an entry): a PWM ON phase that starts during a band pause is not counted. If the band resumes heating (CH=1 again) while that ON phase still runs, the phase is counted once at that moment, because from then on the burner may start, and entries 59/90/95 count every ON phase in which the burner may start, lit or not. The count is at the moment CH is allowed again, not at the flame edge (a flame edge never adds a count: SATcontrol.ino:1287-1291, checked by 92). During the 180 s minimum-burn wait CH is still 1, so phases in that wait count as before. Not asked to Sergeant D (non-blocking); ask him if a field capture shows it matters.

2026-11-16 AC4: ADR-250 Accepted by Robert, Discord DM 1571883109157310475 ('A', reply to the accept question for the amended text at 6d221e474, after the coordinator's notice that a yes covers the amended text). Set via adr-kit accept (status history: changed_by Robert, reason with the DM id); Status line keeps the ADR-080 guideline-level classification; README entry Accepted; lint --strict clean. Same commit: tests/test_sat_published_curve.py was red on origin/dev since ae8626cff (its zone-block slice reached the new _sat_tolErr line without the band symbols; my 1887 seam sweep did not include it, reported by d3): stub added only when the slice contains _sat_tolErr; 5/5 on origin, on the code before 1887 (55dd8f015) and on lane/ba-1887n.

2026-11-16 tests/test_sat_zone_aggregation.py was red on origin/dev since ae8626cff for the same reason as test_sat_published_curve (zone-block slice without the band symbols; reported by d3): same conditional stub; 16/16 on origin HEAD, on 55dd8f015 and on lane/ba-1887n. grep shows no other test slicing that block.

2026-11-16 for ba (comms 92, coordinator's request): the entry 398 case on a REAL boiler, indigo_light beta.350 capture (Vitodens, simulation off; RvdB/OTGW-field-data/indigo_light/20261116-144953/analysis/). 16:00:58 (tester local time) during a band pause (ch=0, room 20.66, target 20.5): satCycleCheckOvershootSwitch fired 'Control mode: PWM (flow 67.3 stayed above setpoint 31.0 + 1.5 for 30 s)'; the flow was boiler water still hot from DHW burns (last DHW flame 15:53:24, so the 300 s DHW guard satDhwGuardActive, SATcycles.ino:1378, had expired), no CH water flowing. The PWM ON phase 16:00:58-16:03:58 had no flame and counted as a start (pwm starts/hr=2 at the CH ignition 16:18:36). The firmware switch rule checks the DHW guard but not whether heating is on; thermo-nova skips the switch when the heater is not ON or the request <= COLD_SETPOINT (heating_control.py:276-282). Question Q-1887c (ba's text) is queued for Sergeant D after Q-2005a and Q-1998b.

2026-11-16 field (indigo_light, Vitodens, beta.350, sim off; OTGW-field-data/indigo_light/20261116-144953/analysis/sat_story.txt, read by ba): band pause 14:53:58 to 16:14:58; inside it, 16:00:58 'Control mode: PWM (flow 67.3 stayed above setpoint 31.0 + 1.5 for 30 s)' and 'PWM ON: pwm_on=180 s pwm_off=1743 s duty=0.09' with CH=0: the flameless ON phase counted as a start (beta.350 predates entry 398), the exact case entry 398 removes. Second observation, NOT decided: the continuous->PWM overshoot switch (TASK-1888, entry 327) evaluated the flow during the band pause (CH=0, flow 67.3, likely hot water or residual heat); whether it should hold during a band pause is a SAT design question for Sergeant D (raised to the coordinator for 92).

2026-11-16 entry 399 (Sergeant D 18:37/18:39 UTC, after indigo's room-vs-setpoint chart): 'normally SAT should had started heating when the room temp was below 20.6' (target 20.5), i.e. resume below target + 0.1, which differs from the entry-328 reading the band implements (resume below target - 0.1; indigo resumed 16:14:58, room below 20.6 from about 16:05). Q-399a asked (A below target + 0.1 / B as now / C below target; our advice A; scope this release). No change before his answer.

2026-11-16 corrections and review: (1) the band observations of the 349 soak on .63 (12:22/12:39/12:48, room 19.88-19.91, noted earlier as a real room sensor) were SIMULATED: satsimulation was on until 16:27 (09's S0.ini); they show the real code deciding on the filtered value under the simulator, not a real room. (2) d3 second-reader review of the follow-up commit: F4 our reading of the entry-398 edge case is now recorded as entry 400 (interpretation, not his words; pushed 9601c1f60 by 92) and asked as Q-398a; F5 no one-tick lag: the band latch and satPwmCountOnWhenChAllowed() run before satPwmSchedule() in the same tick (checked in the post branch), covered by test_on_phase_that_starts_in_the_resume_tick_counts_once. Citations: the count in thermo-nova is pwm.py:160 (self._current_cycle += 1). (3) .63 with satsimulation off has SAT active=false (ot_link_lost, outside temp null; a real BLE room temperature is present), so AC6 narration value and the 398 count are host-proven (test_sat_tolerance_band 23/23, red on the old code) with the field check open after the 2.0.0 beta (indigo/gruijter captures).
2026-11-16 entry 401 (Sergeant D 19:41 UTC, Q-399a answered A): heating restarts as soon as the room is below target + 0.1, not target - 0.1; corrects the entry-328 reading the band implements. Stop and restart are now at the same line; Q-401a asked (1571976402285240323): stop line A +0.2 / B +0.3 / C +0.1 with a minimum CH-off time / D no gap; our advice A; scope this release. Implement the restart change together with his Q-401a answer.
<!-- SECTION:NOTES:END -->
