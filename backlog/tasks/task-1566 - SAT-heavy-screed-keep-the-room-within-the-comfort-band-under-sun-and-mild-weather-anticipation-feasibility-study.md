---
id: TASK-1566
title: >-
  SAT heavy screed: keep the room within the comfort band under sun and mild
  weather (anticipation feasibility study)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T19:15:00Z'
updated_date: '2026-11-02T19:16:00Z'
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
- [ ] #1 Baseline measured over the season: hours above +0.3 C and below -0.3 C (all hours after warm-up), split by time since heating
- [ ] #2 Strategies simulated paired against the baseline: room cutoff, forecast-based curve (12 h, 24 h, perfect forecast as upper bound), combined, and a realistic forecast with error
- [ ] #3 Results over ten seasons and the round 21 units/houses; starts per day and energy reported alongside
- [ ] #4 Findings and a design question put to Sergeant D (one question, A/B/C, recommendation)
<!-- AC:END -->
