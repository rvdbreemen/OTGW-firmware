---
id: TASK-1566
title: >-
  SAT heavy screed: keep the room within the comfort band under sun and mild
  weather (anticipation feasibility study)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T19:15:00Z'
updated_date: '2026-11-02T22:44:00Z'
labels:
  - sat
dependencies: []
priority: high
ordinal: 716000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-02: the room temperature MUST stay within the band, solar gain included. Winter 2024-25 trace (NIBE S2125-8, 2015 house, heavy screed, SAT continuous): room above 20.3 C in 20.4 % of season hours (PWM 24.4 %); 19 % fall within 24 h after heating (heat stored in the screed, released when sun or milder weather arrives), 1.2 % with no heating for over 24 h (passive, needs shading or cooling). Design and simulation only; any SAT behaviour change goes to Sergeant D first.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Baseline measured over the season: hours above +0.3 C and below -0.3 C (all hours after warm-up), split by time since heating
- [x] #2 Strategies simulated paired against the baseline: room cutoff, forecast-based curve (12 h, 24 h, perfect forecast as upper bound), combined, and a realistic forecast with error
- [x] #3 Results over ten seasons and the round 21 units/houses; starts per day and energy reported alongside
- [ ] #4 Findings and a design question put to Sergeant D (one question, A/B/C, recommendation)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-02 round 25 done (docs/sat/sim-hp-lowload/round25.md, round25_runs.json, scripts/sat_comfort_band_study25.py). AC1: 19 % of season minutes above the band within 24 h after heating, 1.2 % after >24 h; no-heating counterfactual max 18.7 C (stored heat, not passive sun). AC2: quick sweep 2024-25 NIBE 2015 house: base 79.0 %, cut 0.1/-0.1, cut0, f12, f24, f24cut, f24err(+cut), f12s2/s3, f6s3, combos; f6 best 87.7 %. AC3: 600 paired runs, ten winters, 3 units x 4 houses (incl. light BENG screed): base cont 85.8 % within / 14.1 % above, f6 90.2 % / 9.6 % (118/120 better, -3.3 % electricity, +0.35 starts/day), f6err 89.4 %, PWM base 82.5 % -> f6 88.4 %. Solar split sensitivity (0.2/0.5/0.8 to air) moves results < 1 point. Light screed BENG 87.8 % -> 89.6 %: the cause is mainly insulation plus storage, no strategy reaches 100 %. AC4 open: Q-1566a queued in open-questions.md (Q-1537b of the lead is still unanswered, Sergeant D quiet).
<!-- SECTION:NOTES:END -->
