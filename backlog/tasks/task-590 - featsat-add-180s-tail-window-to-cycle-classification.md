---
id: TASK-590
title: 'feat(sat): add 180s tail window to cycle classification'
status: To Do
assignee: []
created_date: '2026-05-08 16:49'
labels: []
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The Python SAT cycle classifier uses only the last 180 seconds of each cycle (the tail window) for P90/P50 percentile classification, not the whole cycle. The C++ SATcycles.ino classifies on whole-cycle percentiles. The tail window makes classification more responsive to how the cycle ended: a cycle that started with overshoot but corrected itself is not classified as OVERSHOOT. The C++ simplification can produce false overshoot classifications for long cycles.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Tail ring buffer (last 180s of samples) maintained alongside the full-cycle buffer in SATcycles.ino
- [ ] #2 Tail P90 and P50 percentiles computed from the tail buffer at end-of-cycle
- [ ] #3 Cycle classifier updated to use tail percentiles instead of full-cycle percentiles
- [ ] #4 No regression in SHORT_CYCLING or GOOD cycle classification
- [ ] #5 Tail buffer size derived from 180s and the existing 1Hz sample rate
<!-- AC:END -->
