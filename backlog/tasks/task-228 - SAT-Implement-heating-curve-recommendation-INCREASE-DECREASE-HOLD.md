---
id: TASK-228
title: 'SAT: Implement heating curve recommendation (INCREASE/DECREASE/HOLD)'
status: To Do
assignee: []
created_date: '2026-04-09 05:30'
labels:
  - audit-fix
  - sat
  - heating-curve
dependencies: []
references:
  - backlog/docs/sat-feature-completeness-matrix.md
  - backlog/docs/sat-python-cpp-mapping.md
  - src/OTGW-firmware/SATcontrol.ino
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python's SatHeatingCurveRecommendationSensor computes a daily rolling temperature error history and recommends whether the heating curve coefficient should be increased, decreased, or held. This is a self-diagnostic capability that helps users tune their system without guesswork.

Algorithm:
- Track daily median of (room_temp - target_temp) errors
- If median error > +0.5 C sustained for N days: recommend DECREASE (system running too hot)
- If median error < -0.5 C sustained for N days: recommend INCREASE (system running too cold)
- Otherwise: HOLD

The C++ firmware has no equivalent. Users currently have no feedback on whether their heating curve coefficient is correct.

Complexity: Medium. Requires storing a rolling daily temperature error summary (median per day, last 7 days = 7 floats = 28 bytes). The recommendation logic is simple threshold comparison.

Risk: Low. This is a read-only advisory sensor; it does not affect the control loop. The only risk is a misleading recommendation during edge cases (holiday mode, unusual outdoor temperature patterns), which is mitigated by the N-day sustain requirement.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Daily temperature error median (room_temp - target_temp) is computed and stored for the last 7 days
- [ ] #2 Recommendation is INCREASE if median error < -0.5 C for 3+ consecutive days
- [ ] #3 Recommendation is DECREASE if median error > +0.5 C for 3+ consecutive days
- [ ] #4 Recommendation is HOLD otherwise
- [ ] #5 Recommendation and daily error values are published to MQTT
- [ ] #6 Recommendation resets to HOLD when SAT is disabled or room sensor is unavailable
- [ ] #7 Daily error data persists across restarts (stored to LittleFS or settings.sat)
<!-- AC:END -->
