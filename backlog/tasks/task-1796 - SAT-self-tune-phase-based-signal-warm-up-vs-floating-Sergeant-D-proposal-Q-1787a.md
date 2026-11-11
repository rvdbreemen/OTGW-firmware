---
id: TASK-1796
title: >-
  SAT self-tune: phase-based signal (warm-up vs floating), Sergeant D proposal
  (Q-1787a)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T06:21:00Z'
updated_date: '2026-11-11T13:12:00Z'
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
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: Sergeant D's answers recorded as entries 302-307 (303 keep the 3-day rule, change only when both phases agree; 304 mirrored lowering, phase split by our reading; 305 Q-1787e B the phase with data decides alone; 306 Q-1787f A overshoot is warm-up; 307 Q-1787b A ADR-205 tuner stays until this ships), on origin/dev. AC2: ADR-228 Proposed (docs/adr/ADR-228-...md) cites thermo-nova const.py:16 DEADBAND=0.1, pid.py:187/205, and the firmware (SATtypes.h SAT_PID_DEADBAND, SATpid.ino fError); partly supersedes ADR-205 decisions 1 and 3. Robert's acceptance pending. AC3: tests/test_sat_curve_selftune.py 74 OK on a736f2b44; on the old origin/dev tuner FAILED (failures=9, errors=14), core case warm-up +2.4/floating 0.0: old raises 2.0->2.1, new holds phases_disagree. Round 12 simulation per house type (5 houses x gas/hp30/hp60, 2 winters, tools/research/sat/sat_curve_selftune_study12.py, docs/sat/sim-1519/data/round12_runs.json). evaluate --quick 86/0. Build 2.0.0-beta.176+a736f2b SUCCESS; bench .61 OTA: SATTUNEH and SATTUNED lines with per-phase fields on the device, no reboot; a phase decision was not seen on the device (hours excluded by sun), only in the host test. Pushed a736f2b44, zip posted (msg 1570063472945922071).
<!-- SECTION:FINAL_SUMMARY:END -->
