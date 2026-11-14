---
id: TASK-1928
title: >-
  SAT research: improvements beyond thermo-nova (self-tune, boost detection,
  weather, heating curve)
status: To Do
assignee: []
created_date: '2026-11-14T08:32:00Z'
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
- [ ] #1 Each idea states: the mechanism, which inputs it needs (and whether the firmware has them today), what thermo-nova does in that area (file:line, or 'not found in the code'), the risk (comfort, gas, flame-outs, short cycling), and how to test it first (sim, replay of field captures, bench)
- [ ] #2 At least 7 ideas, ranked by expected benefit versus risk and effort
- [ ] #3 Model numbers are labelled MODEL RESULT; field numbers name their capture
- [ ] #4 Report to Robert; ideas he picks are put to Sergeant D one question per message, recorded in sergeant-d-corrections.md before any implementation task
<!-- AC:END -->
