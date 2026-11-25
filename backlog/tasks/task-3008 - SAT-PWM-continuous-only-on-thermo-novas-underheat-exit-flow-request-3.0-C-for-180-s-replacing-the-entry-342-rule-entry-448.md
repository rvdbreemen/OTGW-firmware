---
id: TASK-3008
title: >-
  SAT: PWM -> continuous only on thermo-nova's underheat exit (flow <= request -
  3.0 C for 180 s), replacing the entry-342 rule (entry 448)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-21T14:29:00Z'
updated_date: '2026-11-25T06:37:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1130000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 448 (Q-342b answered D, 2026-11-21): the PWM -> continuous exit of entry 342 (flame on and request above the flow for 60 s, ADR-236) is replaced by thermo-nova's underheat exit: leave PWM when the flow is <= request - 3.0 C (UNDERSHOOT_MARGIN_CELSIUS = -3.0, cycles/const.py:11) for 180 s (UNDERHEAT_SUSTAIN_SECONDS = 180.0, const.py:22), heating_control.py:330-344 at 7beaaf5 (no flame condition there). MM=0 in PWM and CS = flow - 1 stay. Evidence: indigo_light 21 Nov looped 3-4x per night burn (replay A 4 / D 0 rounds), marceld91d D none (OTGW-field-data/indigo_light/2026-11-21_2.0.0-beta.392/_replay/, marceld91d/2026-11-21_2.0.0-beta.392/_replay/). Code: SATcycles.ino satCycleCheckPwmExits() underheat branch + SAT_UNDERHEAT_SUSTAIN_MS. ADR-236 is rewritten/superseded by otgw-architect. Push gate: do NOT push until indigo_light's 24 h capture on beta.410 is analysed (Robert, entry 445: one change at a time).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 satCycleCheckPwmExits() leaves PWM for continuous only when flow <= request - 3.0 C for 180 s (no flame condition, as thermo-nova); a missing flow clears the timer; the existing PWM-mode / ON-phase / hot-water / start-gate gating unchanged
- [ ] #2 Host tests red on origin/dev and green on the fix: a 60 s request-above-flow case no longer exits, a 180 s flow <= request - 3 case exits, a 170 s case does not, flow at request - 2.9 does not; affected tests (test_sat_overshoot_sustain, test_sat_pwm_first_start, test_sat_lowload_contract, test_sat_cycle_classifier, grep underheat) updated
- [x] #3 Replay proof with the real code (OTGW-field-data/_replay_tools/sat_switch/switch5.py on the fix): indigo_light 21 Nov night burns 0 exits after the fail-safe loops, marceld91d 09:03-09:30 no switch
- [ ] #4 evaluate --quick no new FAIL, esp32-combo build, bench by otgw-bench-tester; push only after the indigo 24 h beta.410 analysis (Robert entry 445)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
SATcycles.ino satCycleCheckPwmExits(): underheat condition -> flowTemp <= requestedSetpoint - SAT_UNDERSHOOT_MARGIN_C (3.0), SAT_UNDERHEAT_SUSTAIN_MS 60000 -> 180000, satIsFlameOn() out of the condition (thermo-nova heating_control.py:330-344 has no flame condition; gate = heater_state ON = demand, :317, :65-72 per otgw-architect; ours = bPwmFlameRequested ON phase + ADR-199 cold gate upstream). Entry-429 latch kept, flame-guarded like the saturation exit. Tests: test_sat_overshoot_sustain + test_sat_pwm_override expectations to 180 s / 3 C, new cases (edge -3 counts, -2.9 never, flame off counts, 170 s no / 180 s yes). Proof: red on origin/dev, green on fix; replay switch5.py variant fix on indigo + marceld. Entries relied on: 448 (agrees), 429 latch (kept), 385/387 start gate (kept, per 448 interpretation), 342 replaced.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 evidence (otgw-dev): (1) Red on origin/dev SATcycles.ino (file swapped in, tests unchanged): test_sat_overshoot_sustain 17 FAIL of 89 (incl. test_underheat_fires_180s..., test_underheat_counts_only_3c_below_the_request, test_underheat_counts_with_the_flame_off_too, test_underheat_needs_the_full_180s), test_sat_pwm_override 4 FAIL of 83 (uh exit tick 7, same-tick 10/60/90 s). (2) Green on the fix: overshoot_sustain Ran 89 OK (after the request-2.9 case, flow 47.1, was added), pwm_override 83 OK. (3) Replay, real fix file via switch5.py variant fix (OTGW_FIX_INO=wt-d3-underheat SATcycles.ino, OTGW_DEV_TESTS=wt-d3-underheat/tests): indigo_light 21 Nov windows 01:39:30-02:08, 03:03-03:44, 04:25-04:46, 11:38-11:56: none (dev) 8/8/8/6 switches, fix 0/0/0/0 (= tn variant); marceld91d 09:03:10-09:30: none 2, fix 0. Files: OTGW-field-data/indigo_light/2026-11-21_2.0.0-beta.392/_replay/switch_fix3008_out.txt, marceld91d/2026-11-21_2.0.0-beta.392/_replay/switch_fix3008_out.txt (SHA256SUMS updated). Limit: a replay forces recorded flow/request, so 0 exits = the rule does not fire on these trajectories, not a closed-loop prediction. (4) Full suite first run: 358 files, 355 passed, 2 failed, 1 skipped (failing names not captured; rerun with full log in progress).

Cold-setpoint gate (ADR-199, upstream in SATcontrol.ino, unchanged): evidence test_sat_overshoot_sustain.py test_pwm_exit_gate_above_and_below_cold_setpoint: above the cold setpoint the underheat exit fires at t=181000 (180 s), at or below it nothing fires (t=-1, mode 2). Green on the fix, red on origin/dev (expected 181000, dev fires at 61000).

2026-11-21 (otgw-dev): committed 2f26146ce (local, lane/d3-underheat, beta.412). evaluate.py --quick: 95 checks, 0 failed, 0 warnings. esp32-combo build 17:01: 4x 'Successfully created ESP32S3 image', 'Build completed successfully!', app RAM 134620 B, flash 2154107/2293760; build/OTGW-firmware-esp32-combo-2.0.0-beta.412+2f26146-flash.zip. Bench (simulation, 3 C/180 s exit, 1-2 C no exit) and NAS full sweep requested from 09 via the coordinator. Laptop full-suite rerun stopped (Robert: full suites on the NAS). Push gate: indigo 24 h beta.410 analysis (entry 445).

2026-11-21 (otgw-dev): MSVC-only SAT tests (do not run on the NAS container, no compiler) run on the laptop as single files on 2f26146ce: 11 PASS (cycle_classifier 44, pid_semantics 24, pressure_alarm_replay 9, pwm_first_start 15, telemetry_wire 18, thermal_learning_gate 2, ha_sat_sim_availability 6, sat_health_discovery 6, pwm_live_flow_duty_2993 17, overshoot_sustain 89, pwm_override 83); test_sat_lowload_contract FAILED 41 (LL-004 / LL-020b still pinned the 60 s underheat threshold, e.g. 180.0 != 60.0): missed in the first sweep, fixed in the test (u threshold 180, LL-004 renamed ..._60_180_300_s), now Ran 35 OK (skipped=1). NAS sweep (09, no-host, python:3.12-alpine): 27 fails all environment (no compiler 13, numpy 6, git 2, other-projects 1, env path 1, TZ 1, gzip OS byte 1, timing 2); test_capture_tool 63 OK on the laptop. Not changing the rule until Sergeant D answers the coordinator's 180 s flame-established gate question (ba515c8 vs 7beaaf5).

2026-11-21 (otgw-dev) entry 456 (Q-448a = A): the underheat timer counts only once the current flame has burned 180 s (SAT_UNDERHEAT_FLAME_MIN_MS; satIsFlameOn() && tickMs - satCycleGetFlameOnStartMs() >= 180 s; the flame edge is processed earlier in the same satControlLoopRun() pass, SATcontrol.ino:8023 before :8454). Reference only (ADR-292): SAT develop ba515c8 heating_control.py:418-422 (_maybe_disable_pwm_on_sustained_underheat clears the start while not _flame_established) and :455-460 (_flame_established, HEATER_STARTUP_TIMEFRAME 180, const.py:17); 7beaaf5 had no such gate. Earliest underheat exit about 360 s after ignition. Start gate of entries 385/387 KEPT (redundant for underheat now, unchanged behaviour, smaller diff). Lane rebuilt on origin/dev 826fdb20b (old commits kept as lane/d3-underheat-pre-rebase; only SATcycles.ino and the test diffs reapplied, dev's own test edits preserved). Evidence: test_sat_overshoot_sustain 91 OK; the 3 new cases (flame off never counts, waits 180 s of flame: fires at 420 s, flame-off restarts the wait: 510 s) FAIL on the entry-448-only SATcycles.ino (2f26146ce), pass on the fix. test_sat_pwm_first_start: mutant gate raised 200 -> 400 s (underheat can no longer fire inside 200 s) and its build keys renamed gate400 (the old shared key gate200 gave an order-dependent cached build); 15 OK. pwm_override 83, hp_pwm 35, pwm_live_flow_duty 17, cycle_classifier 44, classifier_parity 12, pid_semantics 24, slave_lowload 20, telemetry_wire 18 PASS; lowload_contract 34 OK, 1 FAIL from the environment (LL-023 delegated test_sat_ground_truth_block skips: other-projects submodule not checked out in this worktree). Replay (switch5.py fix variant now feeds the recorded flame-on edge): indigo 4 windows 0 exits (dev 8/8/8/6), marceld 0 (dev 2): OTGW-field-data/*/2026-11-21_2.0.0-beta.392/_replay/switch_fix3008_456_out.txt. evaluate --quick 96 checks, 0 failed.

2026-11-21 bench (otgw-bench-tester, beta.415+b00daef on .61, SAT sim, 57 min): PASS githash/no restart/crashlog none, old 60 s text 0x, no PWM->continuous in the 0-3 C band (4 min stint stayed PWM); underheat exit NOT exercised in the run (host tests + replay carry it). Evidence OTGW-field-data/bench-otgw32/2026-11-21_task3008-b00daef/. NAS suite b00daef60: 0 new failures vs 2f26146ce. Finding: at the cold start PWM ended after 70 s through the at-max rule (SATcontrol.ino:8398-8405, entry 256), which has only the 60 s start gate (entries 385/387). Sergeant D decided (entry 463, Q-456g = A): the at-max rule keeps only the 60 s start gate; the 180 s flame gate belongs to the underheat exit only. No change to TASK-3008. Push still waits for indigo's beta.410 capture analysis (entry 445).

2026-11-25 rebuilt on origin/dev fb917599a for the beta candidate: SATcycles.ino (SAT_UNDERHEAT_SUSTAIN_MS 180000, SAT_UNDERHEAT_FLAME_MIN_MS 180000, flameEstablished gate from satCycleGetFlameOnStartMs), 4 tests. Basis: entries 448, 456, 462, 463 and ADR-297; no entry 475. Tests on this tree: test_sat_overshoot_sustain, test_sat_pwm_override, test_sat_lowload_contract (skipped=1), test_sat_pwm_first_start, test_sat_pwm_anchors, test_sat_hp_pwm all OK; red on origin/dev earlier (18/4/43 failures). Open checks from otgw-architect: heating demand upstream and the flame-on edge at takeover.

2026-11-25 otgw-architect: the planned ADR-236 References commit 2cbb195e4 (architect's local branch, dropped) is obsolete: ADR-236 is Accepted and immutable; ADR-297 (Accepted) is the record that names this task. The red/green + replay evidence goes in this task's final summary.
<!-- SECTION:NOTES:END -->
