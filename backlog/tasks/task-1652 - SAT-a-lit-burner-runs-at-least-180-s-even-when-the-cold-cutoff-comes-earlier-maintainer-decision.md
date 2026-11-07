---
id: TASK-1652
title: >-
  SAT: a lit burner runs at least 180 s even when the cold cutoff comes earlier
  (maintainer decision)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-05T06:59:00Z'
updated_date: '2026-11-05T20:22:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 776000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-05: the 180 s minimum burn (entries 31/80) wins over the cold cutoff (entries 12/199), which today ends PWM ON phases with less than 180 s of flame (TASK-1412.19 simulation).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Rule implemented: after ignition the ON phase is not ended by the cold cutoff before 180 s of flame; other stop reasons (fault, DHW, SAT off, safety) still end it at once
- [x] #2 Simulation (1412.19 harness) shows no ON phase below 180 s from the cold cutoff and no new overshoot class
- [x] #3 Host test red on old / green on fix; ADR or spec note updated per the SAT rules
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 (session Geheugengebruik beperken, took over 6b's wt-1652, rebased onto origin/dev, scripts now tools/research/sat): AC1 SATcontrol.ino satPwmColdCutWaitsForMinBurn()/_sat_coldCutDeferred/satColdCutActive(); other stop paths unchanged. AC2 1412.19 harness rule vs baseline (docs/sat/sim-1412-19/task-1652/compare-ruleB.txt, 1440 runs): cold-cutoff short phases 309 -> 32, 0 runs newly failing a hard check or S2; all 26 remaining in very_low_load are phases whose flame went out and relit (anchor = first ignition, as firmware and pwm.py:144-146), so no lit phase is cut before 180 s since its ignition. AC3 tests/test_sat_cold_cut_min_burn.py: old SATcontrol.ino 4 fail / 2 skip of 8, fix 8 OK; spec note in docs/sat/firmware-sat-spec.md step 18; Sergeant D entries 281/282 (A confirmed, link corrected to pin pwm.py:246 low-duty rule, read as A stands). Gates: evaluate --quick 83 pass 0 fail; tests sweep: all green when run alone (sim_port_equivalence 4/4, solar_gain 4/4, lowload_contract 35 OK skip1 xfail2, refresh_storm 39 OK skip2); the four sweep failures were load/timeouts. Not built, not bench-tested: handed to the lead for bump/build/bench.

2026-11-05 bench .61 beta.101+5781def (lead session, evidence scratchpad/bench-101): satsimulation + built-in OTGW frame replay (for a valid MsgID 3). Run 1: PWM ON 20:47:25.17 (flame lit); target lowered at 20:48:47; 20:49:05.194 'SAT: Heating off waits for the 180 s minimum burn: requested 11.7 ...'; SATTICK cs=33.5 ch=1 while requested 11.4-11.7; 20:50:25.213 'Heating off: requested 11.4 is at or below the cold setpoint 28.2', cs=10.0 ch=0: cutoff 180.04 s after PWM ON. Control run 3: PWM ON 21:15:05.65, condition reached after 180 s of flame -> 21:19:05.71 'Heating off' in the same tick, no 'waits' line. Run 2 was interrupted by a replayed DHW draw (hold resumed with 60 s left; not verified against the code). Not benched: a trigger within the first minute (82 s), the heat-pump branch, the DHW/fault/safety stop paths (host tests only). Side finding: the telnet narrate line is truncated ('cold setpoint 2'): narrate buffer too short for this message. Settings diff {}.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: SATcontrol.ino defers the latched cold cutoff while a lit PWM ON phase is younger than 180 s since ignition; other stop reasons act at once (host tests). AC2: 1412.19 simulation (docs/sat/sim-1412-19/task-1652) shows no ON phase below 180 s from the cold cutoff and no new overshoot class. AC3: tests/test_sat_cold_cut_min_burn.py 8 OK on the fix, 4 fail on the old code; spec note firmware-sat-spec.md step 18; Sergeant D entries 275, 281-283 (his answer A). Bench .61 beta.101+5781def: cutoff deferred to 180.04 s after PWM ON with the 'waits for the 180 s minimum burn' narration; control run cut at once after 180 s. Not verified: a real boiler, the heat-pump branch, DHW/fault stop paths on hardware.
<!-- SECTION:FINAL_SUMMARY:END -->
