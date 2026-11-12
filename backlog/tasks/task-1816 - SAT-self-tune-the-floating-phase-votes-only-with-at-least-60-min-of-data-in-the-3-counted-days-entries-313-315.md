---
id: TASK-1816
title: >-
  SAT self-tune: the floating phase votes only with at least 60 min of data in
  the 3 counted days (entries 313, 315)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T20:09:00Z'
updated_date: '2026-11-12T13:52:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 909000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, entry 313 (Q-1796a B, msg 1570167074670780447) and entry 315 (Q-1796a2 B, msg 1570167982762491906): the floating phase may vote only when it has at least 1 hour of data over the 3 counted days; below that the warm-up phase decides alone, as for a phase without data (entry 305). Field case (gruijter 10 Nov, beta.167): 12 floating minutes vetoed 650 warm-up minutes (phases_disagree). Implement as a named constant (60 min, e.g. SAT_CT_FLOAT_MIN_TICKS from the tick period) in the satCurveTune block (SATcurveTune.h / SATcontrol.ino), and narrate it in SATTUNED. ADR-228 decision text on a phase without data is extended by this; check whether that needs a superseding ADR or fits ADR-228 (Accepted) as an implementation of entry 305's rule. Entries 313-315 must be on origin/dev (wt-comms 2f7c8477f, ee4f57eec) before the code commit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Entries 313 and 315 are on origin/dev in docs/sat/sergeant-d-corrections.md before the implementation commit
- [x] #2 tests/test_sat_curve_selftune.py: a window with 12 floating minutes and a warm-up raise fails on the old code (phases_disagree, hold) and passes on the fix (raise); exactly 60 min floating still votes
- [x] #3 ADR question settled (fits ADR-228 or a new ADR via adr-kit) and recorded in the task notes
- [x] #4 Build, evaluate --quick, OTA bench on .61 with the TASK-1525 run-3 scenario shows the new reason in SATTUNED
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Entries 313/315 (Q-1796a B, Q-1796a2 B): the floating phase votes only with at least 1 hour of data over the counted days of the window; below that the warm-up phase decides alone, as entry 305 does for a phase without data. Warm-up gets no minimum (entry 313 names floating only). Implementation: named constant SAT_CT_FLOAT_MIN_MS 3600000 in SATcurveTune.h; satTuneCloseDay() gets the control tick length (satTuneRollHour passes intervalMs, the same value its minTicks uses) and treats floating as without data when ticks x tick < the minimum; SatTuneDayOut.floatVotes is logged as SATTUNED fvote. Our tuner, no thermo-nova code (entry 298).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented locally on lane-1815: SATcurveTune.h SAT_CT_FLOAT_MIN_MS + SatTuneDayOut.floatVotes; SATcontrol.ino satTuneCloseDay(t, coeff, tickMs, out), floating votes only with phTicks3[FLOATING] x tickMs >= 1 h; SATTUNED gets fvote. Tests (tests/test_sat_curve_selftune.py, 76 OK): test_short_floating_sample_does_not_veto (12 floating minutes vs 72 warm-up hours at a 60 s interval: raise) fails on the pre-1816 code with (2.0, 'hold', 'phases_disagree') != (2.1, 'raise', 'phases_agree'); test_one_hour_of_floating_data_votes (60 ticks = 60 min votes and holds, 59 does not) is the boundary guard; test_record_formats pins fvote. evaluate --quick 86/0, test_sat_narrate_fits OK. docs/sat/sat-log-format.md documents fvote. Entries 313-315 are on origin (19d70dd58), AC1 holds.

AC3 (ADR question): settled with the coordinator: one new ADR via adr-kit amends ADR-228 for entries 313/315 and 316. ADR-230 (number assigned by the coordinator), Proposed, gate tests/test_sat_curve_selftune.py; relate ADR-228/ADR-205 written on both sides by adr relate; strict lint ADVISORY only (its 3 open questions: Q-1817a, Q-1817b, acceptance by Robert); tests/test_adr_governance.py OK.

ADR-230 split on Robert's choice (floating minimum only) and Accepted 2026-11-12 by Robert in session d3 (AskUserQuestion 'Ja, accepteer'; also DM 'A' to the coordinator, msg 1570216668469133343). Clarity gate fixed (PID/PWM expanded), Status paragraph repaired after adr accept cut it; strict lint: ADR-228 and ADR-230 PASS strictly; governance OK.

Bench by otgw-firmware-09 on .61, 2.0.0-beta.190+1a4c0a4 (2026-11-12): ota_flash.py app + fs, githash 1a4c0a4, bootcount 384 -> 386, health 200, MQTT connected, crashlog none, settings 164 keys unchanged, firmware_ota=safeboot; restored afterwards (settings diff 0 of 518 flattened keys, sim files deleted, reboot, bootcount 387). Pushed origin/dev ba41264e6..1a4c0a48b; zip posted #alpha-testing msg 1570248008279523399. Evidence: 09 scratchpad b190/ (telnet-A.log, phaseA-sattick.txt, sattuned.txt, log-toolbar.png). Shipped in beta.190. Bench shows the new field only: '02:19:51 SATTUNED day=0 clean=24 counted=1 days_in_window=1 mean3=na decision=hold reason=few_days ... fvote=0' (satsimcoolrate 0.2, coefficient 1.0, interval 10, sim tune_hour 60). AC4 NOT ticked: it asks for the TASK-1525 run-3 scenario, which needs at least 3 tuner days on the sim (~75 min); the decision itself is host-proven (AC2). AC4 wording 'new reason' is stale: the change adds fvote=, the reason strings are unchanged. Follow-up bench run requested via the coordinator.

Correction (2026-11-12, otgw-firmware-d3): the zip post -m for beta.190 (msg 1570248008279523399) said 'at least 60 min of floating ticks per day'. Wrong: SATcontrol.ino:6148-6157 sums the floating ticks over the 3 counted days (phTicks3), so the floating phase votes with at least 1 hour over the counted days (SAT_CT_FLOAT_MIN_MS 3600000, SATcurveTune.h:31). 92 posts the public correction.

AC4 bench by otgw-firmware-09 on .61, beta.214, SAT sim, TASK-1525 run-3 scenario (2026-11-12 11:58-13:10): first daylight run void (every hour reason=sun); rerun with satweatherlat=70 temporarily (sun gate only; sim outside temp is state.sat.fSimOutdoorTemp, SATcontrol.ino:2549). SATTUNED day=2 clean=23 counted=1 days_in_window=3 mean3=24.94 decision=raise reason=phases_agree wmean3=24.94 wticks3=390 wsay=higher fmean3=na fticks3=0 fsay=hold fvote=0; SATTUNE old=1.0 new=1.1. Days 0-1 hold few_days fvote=0. 72 SATTUNEH: 65 none, 6 dhw, 1 not_sat. Bootcount 466 stable, health 200, MQTT connected, crashlog none; min free heap low point 3896 B (trend reported to 95, not this task). Settings restored, diff 0. reason=phases_agree with a floating phase without a vote is the ADR-228/entry 305 label (host test test_a_phase_without_ticks_lets_the_other_decide pins it). Not benched: a real floating phase near 60 min, the boundary (host only), the real sun gate. Evidence: 09 scratchpad b1816/ (telnet.log, poll.log).

Correction from 09: the heap was not trending down. 3896 B is the since-boot low-water mark of boot 466 (it covers pyotgw and the SAT sim as well), not comparable with the per-boot values on 211. The AC4 PASS stands.

Correction from 09 (2026-11-12): the sim outside temperature does come from the weather fetch. fSimOutdoorTemp is filled by satSimOutdoorTemp() (SATcontrol.ino:5729-5731: weather.bValid ? weather.fTemperature : triangle wave). After satweatherlat=70 the device fetched the weather for that place (12:06:13), and SATTICK out= fell from 7.8 to 3.8. So mean3=24.94 is not comparable with the 10.08 of the TASK-1525 run. AC4 is unaffected: it checks the decision path (warm-up decides alone with fticks3=0, fvote=0), not the size of mean3.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Floating phase votes only with >= 1 h of floating tick time over the 3 counted days (entries 313, 315; SAT_CT_FLOAT_MIN_MS, SATcontrol.ino:6148-6157), shipped in beta.190. AC1: entries 313-315 on origin (19d70dd58) before the code. AC2: tests/test_sat_curve_selftune.py test_short_floating_sample_does_not_veto fails on pre-1816 code ((2.0,'hold','phases_disagree') != (2.1,'raise','phases_agree')), passes on the fix; test_one_hour_of_floating_data_votes guards 60/59 ticks. AC3: ADR-230 via adr-kit, Accepted by Robert 2026-11-12. AC4: 09 bench .61 beta.214, run-3 sim scenario: day 2 decision=raise with fticks3=0 fvote=0 (the warm-up phase decides alone, a phase without floating data does not veto), SATTUNE 1.0->1.1, no reboot, no crashlog. AC4 wording 'new reason' was stale: the change adds the fvote= field; the reason strings are unchanged. Not benched: a near-60-min floating sample (host test covers the boundary), the real sun gate (lat 70 used).
<!-- SECTION:FINAL_SUMMARY:END -->
