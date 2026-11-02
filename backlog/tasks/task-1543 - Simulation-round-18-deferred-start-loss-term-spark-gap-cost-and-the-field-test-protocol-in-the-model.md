---
id: TASK-1543
title: >-
  Simulation round 18 (deferred): start-loss term, spark-gap cost and the
  field-test protocol in the model
status: To Do
assignee: []
created_date: '2026-11-01T09:39:00Z'
updated_date: '2026-11-02T04:39:00Z'
labels:
  - simulatie
dependencies: []
priority: low
ordinal: 699000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Deferred by Robert 2026-11-01, keep as a reminder. From TASK-1527 / entry 192: (1) add a cycling start-loss term (degradation coefficient 0.05 and 0.10 as sensitivity, NIST TN 1848 p.15, US air-to-air context) and report electricity with and without it; (2) cost at a chosen electricity/gas price ratio with SAT on a gas boiler in the same house as reference (Sergeant D's 'spark gap'); (3) run the field-test protocol (4-day blocks, ABBA, discard day 1; arms SAT PWM 30 min vs 90/120/180 min) in the model to size the number of blocks. Plan: docs/sat/sim-hp-lowload/field-test-plan.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Round 18 report in docs/sat/sim-hp-lowload/round18.md with charts, paired against today's control
- [ ] #2 Previous rounds reproduce bit-exact with the new flags off
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no round18. Deferred by Robert.
<!-- SECTION:NOTES:END -->
