---
id: TASK-1273
title: >-
  SAT parity with thermo-nova: startup sequence and PWM differences (needs
  Sergeant D input)
status: Done
assignee:
  - '@claude'
created_date: 2026-10-20 18:17
updated_date: '2026-10-25T10:40:00Z'
labels:
  - sat
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 403000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
First real SAT field run (gruijter, Intergas HReco 30 + Honeywell Round + PIC Classic, alpha.439/441, #alpha-testing 2026-10-20), captures in %TEMP%\discord-1020. Line-by-line comparison with other-projects/SAT-releases-thermo-nova (SAT 5.0.0) found, besides the flame-off offset and PID-first-call bugs: continuous clamp lacks the previous-CS term (heating_control.py:486-489 vs satApplyContinuous ~981-987), so CS follows a rising flow; PWM ON expiry timed from the previous cycle's flame-on (~750-756) instead of the ignition sync; no on_time>=180 start gate; effective boiler temp not seeded with flow at flame-on, EMA 30 s vs 180 s; overshoot->PWM auto-switch compares flow with fFinalSetpoint (SATcycles.ino ~949), which the clamp keeps within 2 C of flow, so it practically never fires; no cycle-end OVERSHOOT/UNDERHEAT switch; 30 s tick vs 5 s loop; MM=max in PWM-OFF vs 0; CH threshold CS>10 vs >22; no PM=25 after CS; PM/MI boot commands not sent on runtime enable. Open design questions for Sergeant D: should the PWM CS override be gated by overshoot_protection (Python default off)? keep the 28.2 cold cutoff (ADR-150) and coefficient 1.5 (Python: 22 and 2.0)?
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sergeant D answered the open design questions (recorded in notes)
- [x] #2 Each agreed difference has its own task or is fixed here with a simulator or host test
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
PWM item 1, duty-cycle BT (docs/sat/sergeant-d-corrections.md entry 1, resolved 2026-10-22): SATcontrol.ino satApplyPWM(): (a) seed _pwm_effectiveBoilerTemp from the live flow at the first flame (value 0 and flame on) and at every flame sync (pwm.py:130-131, 191); (b) before any seed use the live flow (pwm.py:232); (c) EMA 0.3/0.7 for PWM_IGNITION_TIMEOUT_MS = 180 s after flame-on instead of 30 s (pwm.py:148-149, const.py:18), hold afterwards; (d) floor base+1 only, remove the base+20 placeholder and base+5 threshold (pwm.py:235); (e) satPwmReset() clears the learned BT (pwm.py:100). Verification: tests/test_sat_pwm_override.py duty scenario (real satApplyPWM, red on HEAD, green on fix); bench OTGW32 .88.61 with the TASK-1277 long-burn replay, force PWM, protection on: implied BT = base + (pid-base)/duty per tick, old 47.2 vs new = flow at the first flame tick; docs firmware-sat-spec.md, sat-comparison.md, CHANGELOG. Not on otgw-1.x.x (SAT is 2.0.0 only).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-20 Sergeant D answers (#alpha-testing 18:26 UTC): a) PWM CS override behaviour must be identical to thermo-nova, i.e. gated by overshoot_protection (Python default off). b) Keep cold cutoff 28.2 C (radiators); change the heating-curve coefficient default from 1.5 to 2.0. Note: a default change only affects units without a saved value; existing settings.ini keep 1.5 unless migrated (decide with maintainer).

2026-10-20 18:54 UTC Sergeant D: leave existing settings alone; the coefficient default 2.0 applies only to units without a stored value, no migration of stored 1.5.

From the TASK-1268 round-2 review (opus, by reading, pre-existing): (a) PWM ON-phase expiry is measured from the previous cycle's flame-on (SATcontrol.ino ~813-817), not from the ON-phase start as thermo-nova pwm.py (_last_update): when ignition takes longer than one 30 s tick the phase flips ON/OFF every tick (CS hold <-> 10) and the 180 s ignition timeout is never reached. (b) force PWM with the PID at the system maximum switches PWM->CONTINUOUS->PWM in the same tick (~4753-4761), so PWM state is reset every tick and the ignition timeout never runs. Both belong to the PWM ON timing scope of this task.

2026-10-22: TASK-1292 ground-truth spec is on dev: docs/sat/thermo-nova-spec.md (cited, Alexwijn/SAT releases/thermo-nova @ 7beaaf5), docs/sat/firmware-sat-spec.md and docs/sat/sat-comparison.md (443 rows, 434 verified). This task waits for Sergeant D's validation of the thermo-nova spec; the plan must quote the thermo-nova code for its area with file:line citations (CLAUDE.md 'SAT ground truth' block) before it is resumed.

2026-10-22 12:5x: Sergeant D asked whether the firmware uses (setpoint - 27.2) / (BT - 27.2) for the duty cycle. Answered on #alpha-testing with citations: same formula (SATcontrol.ino:734 vs pwm.py:237); BT differs: thermo-nova seeds _effective_on_temperature from the live flow temperature at the first flame (pwm.py:130-131), EMA 0.3 in the start-up window (148-149), floor base+1 (235); the firmware's _pwm_effectiveBoilerTemp starts at 0 every boot, is never seeded, and a fixed base+20 (47.2) is used while it is below base+5 (SATcontrol.ino:731-732, 752). Asked him to confirm the intent (BT = real smoothed flow from the first cycle, no placeholder). When confirmed, that is the first PWM change to implement under this task, with a replay old-vs-fix on the bench (gen_sat_pwm_replay.py fixture).

2026-10-22 13:58 Sergeant D confirmed the duty formula and the BT rule (docs/sat/sergeant-d-corrections.md entry 1): 28.2 floor only against divide-by-zero; BT filtered at the start of each ON cycle, then raw. Settled: no 47.2 placeholder, seed BT from the live flow at flame-on. Open, asked 2026-10-22: window 30 s (his words, firmware) vs 180 s (thermo-nova HEATER_STARTUP_TIMEFRAME), and after the window raw (his words) vs frozen filtered until the next ON phase (thermo-nova pwm.py:166-167, 191). Implement the duty BT change once answered; replay old-vs-fix on the bench.

2026-10-22 16:12-16:14 Sergeant D resolved both open points: window 180 s (thermo-nova HEATER_STARTUP_TIMEFRAME), and keep the thermo-nova behaviour after the window (filtered value held until the next ON phase / ignition sync resets it to live flow). The duty-cycle BT change is fully specified and gated (docs/sat/sergeant-d-corrections.md entry 1): seed _pwm_effectiveBoilerTemp from live flow at first flame, ON start with flame, ignition sync; EMA 0.3 for 180 s after flame-on; floor base+1; remove the base+20 placeholder and the base+5 threshold (SATcontrol.ino:731-732, 750). Next SAT change in the cycle after TASK-1294; evidence: host test on the real satPwmSchedule if extractable, replay fixture old vs fix (duty 0.64 -> 0.39 case), bench.

2026-10-22 duty BT implemented (SATcontrol.ino ~731-768, 939). Host: tests/test_sat_pwm_override.py 31 OK on the fix; 6 FAIL on HEAD (4 duty cases: placeholder 0.64 vs 0.39, live flow before seed, floor 28.2 -> 1.00, EMA at 100 s; 2 reset cases: HEAD keeps BT 55 after satPwmReset). evaluate --quick health 100. adr-kit plugin not reachable this session (connection failure), ADR context step skipped; no ADR needed (behaviour fix inside an existing pattern).

2026-10-22 bench RED (alpha.458 build with the old duty code, OTGW32 .88.61, underfloor system base 20.0, force PWM, protection on, external room temp 19.0 via /sat/externaltemp with satexternaltemp=1, TASK-1277 long-burn replay): first duty tick t=53 s: duty 0.3225, pid 26.45, flow 41.5 -> implied BT = 20 + 6.45/0.3225 = 40.0 = the base+20 placeholder, not the flow. Evidence %TEMP%\bench-1273\red-alpha458.json (134 rows, 0 errors). Note: an earlier run had SAT active but pid 0 because the external room temperature source was off (room_temp None); and 1 s sampling gave HTTP 429, now 2 s. Second fix after the docs read: OFF->ON with the flame already burning now seeds BT and the start-up window at once (pwm.py:164-167); host test 32 OK, 7 FAIL on HEAD.

2026-10-22 bench GREEN (alpha.459+6ba4a51 working-tree build, flashed via safeboot, settings kept; same replay, same settings as the red run): duty ticks t=65/96/124 s: duty 0.3086/0.3034/0.2949 at pid 26.45 -> implied BT 40.9/41.3/41.9 while the replayed flow read 42.1/43.6/44.5: BT is the flow seeded at the first flame tick and filtered upward, where the old build showed a fixed 40.0 (= underfloor base 20 + 20 placeholder) at flow 41.5. Evidence %TEMP%\bench-1273\green-alpha459.json (138 rows, 0 errors) next to red-alpha458.json. Bench restored: fixture deleted, force_pwm and external temp source back off.

2026-10-22 review round (50 agents: correctness opus, rules, tests, ground-truth second reader; 2 verifiers per finding). Confirmed and fixed: (1) BT was reseeded and the 180 s window reopened on every relight inside an ON phase (pre-existing re-arm of _pwm_waitingForFlame on a mid-phase flame-out); now the sync reseeds only on the first ignition of the ON phase, modelled on thermo-nova _flame_on_at (pwm.py:145-146, 189-191, 206): _pwm_flameOnMs == 0 means not ignited, cleared at ON->OFF and at an ON start without flame; the EMA is also gated on that anchor (no filtering in the OFF phase, pwm.py:144). (2) A missing flow temperature (Tboiler 0, MsgID 25 unseen) hit the base+1 floor and gave duty 1 = continuous ON; now PWM goes idle and satApplyPWM() sends the continuous setpoint, as thermo-nova (pwm.py:122-128, heating_control.py:149-150). (3) Tests: the sync reseed and the ON-start anchor were unasserted (mutants survived), the floor test did not pin +1: added sync, relight, no-flow scenarios and a 0.80 floor case; 35 OK on the fix, 10 FAIL on HEAD. (4) Citation pwm.py:230-237 -> 229-239 (formula at 238), corrections.md entry 1 cite 237 -> 238; docs line shifts and the '30 samples' (36) sent to the docs agent. Rejected: 'comment describes removed history' (0/2 verifiers); the legacy 47.2 seed in the harness main() is deliberate (older scenarios model a learned BT, documented). Noted, not this task: ignition-timeout parity comment (predates), range 5 without reseed (accepted gap, in sat-comparison), 30 s tick cadence.

2026-10-22 bench GREEN 2 on the build with the review fixes (21:42 .ino.bin, relight anchor + no-flow idle), same replay and settings: first duty tick t=61 s duty 0.3086 at pid 26.45 -> implied BT 40.9 with the flow at 42.1 (seed from the first-flame tick), old build 40.0 fixed. Evidence %TEMP%\bench-1273\green2-alpha459.json (111 rows, 0 errors). Bench restored, settings diff vs baseline {}. The last one-line change (anchor cleared in the ignition-timeout abort) is covered by the host test test_ignition_timeout_abort_clears_the_anchor_and_keeps_bt (36 OK on the fix, 11 FAIL on HEAD) and is flashed to the bench without a further replay.

2026-10-22 PWM item 1 (duty-cycle BT) shipped: see the commit 'SAT PWM: take the duty-cycle boiler temperature from the real flow' (alpha.460). Remaining items of this task, each needing Sergeant D's OK before implementation: continuous previous-CS term; PWM ON expiry timed from the ignition sync; on_time >= 180 s start gate; cycle-end OVERSHOOT/UNDERHEAT switch; CH threshold 22; PM=25 after CS; PM/MI on runtime enable; 28.2 cold cutoff and curve coefficient; ignition timeout abort vs thermo-nova continuing the phase (pwm.py:197-199); range 5 without reseed; 30 s tick vs 5 s cadence.

2026-10-23 Sergeant D (#alpha-testing msg 1563135315991072789, 10:21 UTC): 'the CS is following the BT correctly now! Great job' (the duty-cycle BT change, alpha.460). Same message raised CH=0 on PWM OFF: filed separately.

2026-10-25 mapping of every remaining difference (checked at HEAD by triage wf_8965d91a): fixed here: duty-cycle BT (10d9ffb0e, alpha.460). Done elsewhere: coefficient 2.0 (TASK-1347), overshoot-protection gating + flame-off offset (TASK-1268), MM/CH in PWM OFF (TASK-1316), CH rule/prevCS in PWM (TASK-1323), cold cutoff (TASK-1327). Own task: cycle-end OVERSHOOT/UNDERHEAT switch TASK-1331 (Q4); 30 s tick vs 5 s cadence TASK-1368 + TASK-1370; PM=25 after CS TASK-1371; continuous previous-CS term TASK-1381; PWM ON timing from phase start + ignition-timeout abort TASK-1382 (Q35); 180 s start gate TASK-1383 (Q38); PM/MI on runtime enable TASK-1384 (audit #44); range 5 reseed TASK-1385; PWM OFF anchor TASK-1380.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Parity review of the SAT startup sequence and PWM against thermo-nova (gruijter field run 2026-10-20). Fixed here: PWM duty-cycle boiler temperature from the real flow (10d9ffb0e, alpha.460, host test + bench red/green). Every other agreed difference has its own task (mapping note 2026-10-25): TASK-1268, 1316, 1323, 1327, 1347 Done; TASK-1331, 1368, 1370, 1371, 1380-1385 open, those changing boiler control blocked on Sergeant D's OK (Q4, Q35, Q38).
<!-- SECTION:FINAL_SUMMARY:END -->
