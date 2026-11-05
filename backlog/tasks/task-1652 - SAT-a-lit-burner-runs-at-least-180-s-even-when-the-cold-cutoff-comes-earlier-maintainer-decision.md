---
id: TASK-1652
title: >-
  SAT: a lit burner runs at least 180 s even when the cold cutoff comes earlier
  (maintainer decision)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T06:59:00Z'
updated_date: '2026-11-05T18:20:00Z'
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
<!-- SECTION:NOTES:END -->
