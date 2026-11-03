---
id: TASK-1566
title: >-
  SAT heavy screed: keep the room within the comfort band under sun and mild
  weather (anticipation feasibility study)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-02T19:15:00Z'
updated_date: '2026-11-03T07:08:00Z'
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
- [x] #4 Findings and a design question put to Sergeant D (one question, A/B/C, recommendation)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-02 round 25 done (docs/sat/sim-hp-lowload/round25.md, round25_runs.json, scripts/sat_comfort_band_study25.py). AC1: 19 % of season minutes above the band within 24 h after heating, 1.2 % after >24 h; no-heating counterfactual max 18.7 C (stored heat, not passive sun). AC2: quick sweep 2024-25 NIBE 2015 house: base 79.0 %, cut 0.1/-0.1, cut0, f12, f24, f24cut, f24err(+cut), f12s2/s3, f6s3, combos; f6 best 87.7 %. AC3: 600 paired runs, ten winters, 3 units x 4 houses (incl. light BENG screed): base cont 85.8 % within / 14.1 % above, f6 90.2 % / 9.6 % (118/120 better, -3.3 % electricity, +0.35 starts/day), f6err 89.4 %, PWM base 82.5 % -> f6 88.4 %. Solar split sensitivity (0.2/0.5/0.8 to air) moves results < 1 point. Light screed BENG 87.8 % -> 89.6 %: the cause is mainly insulation plus storage, no strategy reaches 100 %. AC4 open: Q-1566a queued in open-questions.md (Q-1537b of the lead is still unanswered, Sergeant D quiet).

TASK-1510 (gruijter, msg 1565717631510646795: weather forecast for pre-emptive CH control, only at high confidence) merged into this task by Robert 2026-11-03: evaluate a forecast as one input of the anticipation.

2026-11-03 06:53 UTC: Q-1566a posted to Sergeant D by the lead session (#alpha-testing msg 1567069177263558667), text from the open-questions row, advice A opt-in. Not in the post: SATweather fetches temperature + cloud cover, not irradiance, so option A needs irradiance added to the request or a cloud-cover solar estimate (round25.md finding 3); carry this into the follow-up task if he picks A. Answer to be recorded by the lead (entry 247 or next).

2026-11-03: Sergeant D answered Q-1566a 'B for now' (06:55 UTC, msg 1567069605912911913), recorded by the lead as entry 247: the curve keeps the current outdoor temperature, no forecast term for now (parked, not rejected).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Feasibility study (round 25, docs/sat/sim-hp-lowload/round25.md, scripts/sat_comfort_band_study25.py): can SAT keep the room inside +-0.3 C under sun and mild weather? Answer: no heating strategy can at all times. Over 600 paired runs (ten winters, 3 heat pumps x 4 houses incl. a light BENG screed) today's SAT is within the band 85.8 % of all hours (14.1 % above); the best strategy, a 6 h forecast term in the curve, reaches 90.2 % (118/120 runs better, -3 % electricity, +0.35 starts/day; 89.4 % with a realistic forecast error); the best single run 93 %. Cause: heat stored by morning heating in well-insulated houses (no-heating counterfactual never exceeds 18.7 C); insulation matters more than screed mass. Solar-split sensitivity < 1 point. Design question Q-1566a put to Sergeant D: answered B for now (entry 247), so the curve stays on the current outdoor temperature; the forecast option is parked with its data. Holding the band at all times needs shading, cooling or a lower spring setpoint, outside SAT's reach. Evidence per AC: AC1 season split + no-heating counterfactual (round25.md step 1); AC2 quick sweep (step 2); AC3 600-run table (step 3); AC4 msg 1567069177263558667 / answer 1567069605912911913, entry 247.
<!-- SECTION:FINAL_SUMMARY:END -->
