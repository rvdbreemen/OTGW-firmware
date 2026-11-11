---
id: TASK-1796
title: >-
  SAT self-tune: phase-based signal (warm-up vs floating), Sergeant D proposal
  (Q-1787a)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T06:21:00Z'
updated_date: '2026-11-11T20:08:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 890000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-11 06:18:39 UTC, #dev-sat-mqtt msg 1569959525715542148 (entry 302): use the warm-up overdamping data for the heating curve self-tune by comparing the warm-up phase with the floating phase inside the deadband, then decide the coefficient change. Conflicts with ADR-205 Must (curve integral as the only signal) and entries 163/171, so adopting it needs a superseding ADR. Waiting for his answer to Q-1787a (asked msg 1569959952854945914: A split clean hours into warm-up and floating and decide on the difference, recommended; B warm-up only; C other). Do not implement before the answer and the follow-up threshold questions are recorded as entries.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sergeant D's answer to Q-1787a recorded as a numbered entry, and any follow-up question (thresholds, step) asked and recorded one at a time
- [x] #2 Design written with thermo-nova and firmware citations, and a Proposed ADR that supersedes ADR-205 for the changed signal
- [x] #3 Implementation with host tests that are red on the old tuner and green on the new one, and a simulation run per house type
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-11 06:28 UTC: on Robert's order all open Sergeant D questions were asked in one combined message (#dev-sat-mqtt msg 1569961879022084211): Q-1787a (repeated, phase-based signal), Q-1787b (ADR-205 tuner until then: keep / clamp 1.6-2.4 / off), Q-1771a (preset restore after reboot, thermo-nova climate.py:175-188), Q-1777g (last Greek labels). Rows in docs/sat/open-questions.md.

2026-11-11: Sergeant D answered Q-1787a at 06:28:28 UTC (entry 303): keep the 3-counted-day rule; raise only when the warm-up and the floating phase both say higher (AND rule, our interpretation). Asked Q-1787c (lowering mirrored?) msg 1569963485633323010 and Q-1787d (phase split at the 0.1 C deadband of thermo-nova const.py:16 / pid.py:187,:205; per-phase ADR-205 rule +-1.0 C, step 0.1) msg 1569963676771942532. Robert 2026-11-11: priority, aim for a new build. Next: answers to c/d -> entries -> Proposed ADR superseding ADR-205 for signal and decision -> implementation with red/green host tests and simulation -> build.

2026-11-11 06:35:27 UTC: Sergeant D 'Yes mirrored. So the answer is A' (reply to Q-1787c; entry 304). Q-1787c = A (lower only when both phases say lower). Q-1787d = A by our reading (deadband split 0.1 C per tick, per phase ADR-205 rule +-1.0 C / step 0.1, change only when both agree), confirmed to him in msg 1569964436796739676; he may still correct it. Implementer: otgw-firmware-d3 (wt-selftune).

2026-11-11 07:04-07:19 UTC: Q-1787e (one phase without data; advice B) and Q-1787f (overshoot counts as warm-up; advice A) asked; round 12 simulation posted (msg 1569974584600690750). Sergeant D asked for one question per message for tracking; all open questions re-posted separately (msgs 1569974667819618396 to 1569974694289870849).

2026-11-11: entries 305 (Q-1787e B, the phase with data decides alone), 306 (Q-1787f A, both sides of the deadband), 307 (Q-1787b A, the current tuner stays on). Entry numbers claimed by comms after checking that every tree ended at 304 (coordinator informed).
2026-11-11 d3 (wt-selftune, lane-selftune): implemented ADR-228 (Proposed, number reserved by the coordinator). Per-tick phase on state.sat.fError (|err|>0.1 warm-up, <=0.1 floating; thermo-nova const.py:16, pid.py:187/205 opened this session), per-phase tick-weighted 3-day means, change only when both phases say the same; hold reasons inside_band / phases_disagree / no_warmup / no_floating; window file v2 (v1 dropped); SATTUNEH/SATTUNED per-phase fields; docs/sat/sat-log-format.md updated. Host test tests/test_sat_curve_selftune.py: 73 OK on the lane; on origin/dev f64aba399 code (OTGW_SAT_INO + OTGW_SAT_HDR) FAILED failures=8 errors=14, core case warm-up +2.4 / floating 0.0: old raises 2.0->2.1 (mean3=1.80), new holds phases_disagree. evaluate.py --quick: 85 passed, 0 failed. test_adr_governance 17 passed. Finding: research-model smoke (round 12) house 4_1992-2012 heavy gas start 1.2: ADR-205 rule raises 4x to 1.6, rule P holds 63 days no_floating (a too-low curve never reaches the deadband). Asked Sergeant D as Q-1787e (B: the phase with data decides alone) and Q-1787f (overshoot counts as warm-up) by comms 92, msg 1569970979948601356. Full round 12 running.

2026-11-11 round 12 done (research model, tools/research/sat/sat_curve_selftune_study12.py, data docs/sat/sim-1519/data/round12_runs.json, 1200 runs). Out-band min/day 2025-26 / 2010-11: none 146/210, A (ADR-205) 100/124, P (built, phase without data holds) 114/168, Q (phase without data does not veto) 100/125; changes per winter A 2.2/4.0, P 1.3/1.9, Q 2.1/3.0. Low starts 1.0/1.2 never corrected: A 20/60 18/60, P 38/60 37/60, Q 20/60 18/60. Example 4_1992-2012 heavy gas 2010-11: A and Q 1.0->1.8 (39 min/d), P stays 1.0 (129 min/d, no_floating 368x). Sent to comms 92 for the Q-1787e follow-up; recommendation B (= Q).

2026-11-11 Sergeant D answers recorded by comms 92 as entries 305 (Q-1787e B: phase with data decides alone), 306 (Q-1787f A: overshoot is warm-up), 307 (Q-1787b A: ADR-205 tuner stays on until this ships), wt-comms 119d9f206. Built B: SAT_CT_MISSING_PHASE_VETO 0, reason no_phases when no phase data. Tests 74 OK; previous lane commit (A for e) fails the two new PhaseSplit tests (holds no_floating where B raises 2.0->2.1); origin/dev tuner failures=9 errors=14. ADR-228 cites entries 303-307; adr-lint 0 FAIL; test_adr_governance 17 passed.

2026-11-11 d3: rebased twice onto moved origin/dev (119d9f206, then 2154cce46; docs-only, no bump conflict), commit a736f2b44 (2.0.0-beta.176). Host test tests/test_sat_curve_selftune.py on a736f2b44: 74 OK. evaluate.py --quick: 86 passed, 0 failed. Build esp32-combo via build.bat: SUCCESS, 2.0.0-beta.176+a736f2b (zip 13:44).

2026-11-11 bench .61 (OT-Direct, master mode; .63 left to TASK-1306): settings saved first (/api/v2/settings), OTA app+fs via scripts/ota_flash.py: running 2.0.0-beta.176+a736f2b, 164 settings kept. Scenario TASK-1525 run 3: satenabled true, satboilercapacity 10, satsimcoolrate 0.2, satcoefficient 1.0, satinterval 10, satsimulation true, sim event tune_hour 60. Telnet 13:46-: 24 SATTUNEH hours with the per-phase fields, e.g. 'SATTUNEH hour=1 mean=2.33 ticks=6 counted=0 reason=sun wmean=9.27 wticks=1 fmean=0.94 fticks=5'; 14:10:40 'SATTUNED day=0 clean=0 counted=0 days_in_window=0 mean3=na decision=hold reason=day_not_counted wmean3=na wticks3=0 wsay=hold fmean3=na fticks3=0 fsay=hold'. No reboot. All hours so far excluded by the real sun elevation (reason=sun), so no phase decision (raise/hold on phases) seen on the device yet; the decision logic is proven by the host test only. Log: scratchpad bench176_telnet.log.

2026-11-11 pushed a736f2b44 to origin/dev (fast-forward from 2154cce46); zip posted by scripts/discord-post-build.sh, message 1570063472945922071. ADR-228 stays Proposed until Robert accepts it (coordinator puts it to him).

2026-11-11 follow-up (found by the full run of scripts/run_tests.py, which I had not run before the beta.176 push): tests/test_sat_narrate_fits.py failed on the new narration line, worst case 134 > 127 with o/n bounded by their char[12] buffers. In practice the line never overflowed in beta.176: satTuneCoeffStr writes at most "5.00" for the 0.1..5.0 range, so the real worst case is 120. Fix: o and n become char[8] (bound 7, worst case 126), the test's width entry follows; test_sat_narrate_fits OK, test_sat_curve_selftune 74 OK. Ships in the next tag after beta.179.

2026-11-11 field capture (gruijter, #alpha-testing msg 1570113823061577850 + csv 1570114673003729047; beta.171, coefficient 1.80, gas boiler, 10 Nov 21:18 to 11 Nov 17:26, 20.1 h, no reboot). Self-tune: 20 SATTUNEH hours, counted 0. First reasons: min_mod 11 (22:00-07:17 no heat demand, room above the 17.5 target; 07-08 and 09-10 min output), sun 5 (11:17-16:17), pwm 3, dhw 1, not_sat 1. No SATTUNED line (no day closed). Hour means: night -17.9 rising to -0.9 (praw below the curve), day +0.3 to +8.7 (praw 31-43 vs curve 29-32 at outside 3-6 C). Exclusion order in dev satTuneTickReason (SATcontrol.ino:5972) is the same as beta.171, so beta.176+ likely holds too in this house: the entry 174 low-load blind spot seen in the field. Boiler: MsgID 15 and 28 Unknown-DataID (minMod falls back to 5 %, no return temp); relmod with flame on 4654 samples, 721 at <=5 %, most 6-8 %; daytime PWM in 40-73 % of ticks, CH 37 runs 05:49-17:26, 29 of 2 min or more, daytime about 10 min on (ends on time) and 10 min off. Comfort: target 19.0 from 07:46, room 18.93 at 10:00; target 19.5 from 12:02, room 0.1-0.35 C below until about 17:00. Coredump in the upload is an old TWDT (loopTask + async_tcp CPU 1, app sha 661d01720), not from this window; no matching ELF here. Raw data local only: D:\Users\Robert\Documents\GitHub\RvdB\capture-gruijter-1111 (not committed).

2026-11-11 20:03 UTC: Sergeant D Q-1796a B (entry 313): the floating phase votes only above a minimum amount of data; minimum asked as Q-1796a2 (advice 2 h). Entry 312: wait one more day before judging the tuner holding in gruijter's house. Q-1796b (PWM hours on gas boilers) is with Robert via the coordinator (entry 174); Q-1796c open.

2026-11-11 20:05-20:07 UTC: Sergeant D Q-1796c A (entry 314: PWM ON time from flame ignition, thermo-nova pwm.py:186-198) and Q-1796a2 B (entry 315: minimum 1 hour of floating data in the 3 counted days). Q-1796d (tuner ceiling, TASK-1815) asked in #dev-sat-mqtt msg 1570168069114826913.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: Sergeant D's answers recorded as entries 302-307 (303 keep the 3-day rule, change only when both phases agree; 304 mirrored lowering, phase split by our reading; 305 Q-1787e B the phase with data decides alone; 306 Q-1787f A overshoot is warm-up; 307 Q-1787b A ADR-205 tuner stays until this ships), on origin/dev. AC2: ADR-228 Proposed (docs/adr/ADR-228-...md) cites thermo-nova const.py:16 DEADBAND=0.1, pid.py:187/205, and the firmware (SATtypes.h SAT_PID_DEADBAND, SATpid.ino fError); partly supersedes ADR-205 decisions 1 and 3. Robert's acceptance pending. AC3: tests/test_sat_curve_selftune.py 74 OK on a736f2b44; on the old origin/dev tuner FAILED (failures=9, errors=14), core case warm-up +2.4/floating 0.0: old raises 2.0->2.1, new holds phases_disagree. Round 12 simulation per house type (5 houses x gas/hp30/hp60, 2 winters, tools/research/sat/sat_curve_selftune_study12.py, docs/sat/sim-1519/data/round12_runs.json). evaluate --quick 86/0. Build 2.0.0-beta.176+a736f2b SUCCESS; bench .61 OTA: SATTUNEH and SATTUNED lines with per-phase fields on the device, no reboot; a phase decision was not seen on the device (hours excluded by sun), only in the host test. Pushed a736f2b44, zip posted (msg 1570063472945922071).
<!-- SECTION:FINAL_SUMMARY:END -->
