---
id: TASK-1928
title: >-
  SAT research: improvements beyond thermo-nova (self-tune, boost detection,
  weather, heating curve)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T08:32:00Z'
updated_date: '2026-11-14T12:15:00Z'
labels:
  - sat
  - research
milestone: 2.0.0
dependencies: []
ordinal: 986000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer order (Robert, session 2026-11-14): research what SAT can improve compared with the thermo-nova branch (7beaaf5), especially auto-tuning, detecting moments when a boost is needed to heat up faster, using weather data (including forecasts), detecting that the house heats faster or slower than usual, and reacting when the outside temperature is lower than on the previous days; the heating-curve approach is a good basis. At least 7 ideas, they may go beyond thermo-nova. Research only: no firmware change. Every claim about thermo-nova cites file:line at 7beaaf5; every behaviour change is a proposal for Sergeant D (SAT ground truth rules). Related: TASK-1927 (tune-signal replay), Q-TUNE1, entries 161/174/334-337.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each idea states: the mechanism, which inputs it needs (and whether the firmware has them today), what thermo-nova does in that area (file:line, or 'not found in the code'), the risk (comfort, gas, flame-outs, short cycling), and how to test it first (sim, replay of field captures, bench)
- [x] #2 At least 7 ideas, ranked by expected benefit versus risk and effort
- [x] #3 Model numbers are labelled MODEL RESULT; field numbers name their capture
- [ ] #4 Report to Robert; ideas he picks are put to Sergeant D one question per message, recorded in sergeant-d-corrections.md before any implementation task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Picked up by comms (92) on coordinator assignment 2026-11-14. Coordinator's 9 seed ideas: weighted outside temperature 12-24 h, forecast feed-forward, boost detection on a learned warm-up rate, deviation detection, cool-down phase/excitation for the self-tune, RLS with forgetting factor, degree-hour budget, wind correction, solar detector instead of the 12 deg gate. Research via the tune-replay agent (light, serial), report to Robert; nothing to Sergeant D before Robert picks.

Report published 2026-11-14: https://claude.ai/artifact/RW714gjLDmnHUSSS5CEC8s (private), summary #dev-sat-mqtt 1571127034690936964. 11 ideas ranked: 1 setback-recovery guard (A), 2 curve on 6-12 h weighted outside temperature, 3 boost after a target step, 4 RLS learner (diagnostics), 5 deviation detection, 6 forecast feed-forward (conflicts entry 247), 7 warm-up rate as tune side signal, 8 excitation (replaced by passive night cool-down), 9 degree-hour budget, 10 wind correction, 11 solar detector (conflicts 160/161). All numbers FIELD (gruijter r1/r2/g167/g214/g68); no MODEL RESULT. Spot-checked: forecast and wind have no consumer in SAT control (only REST/MQTT/debug), entries 62/168/247 as cited. Evidence: scratchpad tune-replay/r1928/field_numbers.txt. Waiting for Robert's pick (AC4).

Q-1928a asked to Sergeant D by the coordinator on Robert's order (DM 1571135435906875443): #dev-sat-mqtt 1571136222062186529, A guard only / B guard then weighted curve after a replay / C A+1+3 / D none; advice B. Registered in open-questions.md. AC4 waits for his answer, recorded as an entry before any task.
<!-- SECTION:NOTES:END -->
