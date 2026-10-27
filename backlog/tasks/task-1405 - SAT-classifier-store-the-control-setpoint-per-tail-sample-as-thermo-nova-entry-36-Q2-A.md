---
id: TASK-1405
title: >-
  SAT classifier: store the control setpoint per tail sample, as thermo-nova
  (entry 36, Q2 A)
status: To Do
assignee: []
created_date: '2026-10-25T21:44:00Z'
updated_date: '2026-10-27T23:46:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.3
references:
  - TASK-1412
ordinal: 543000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 36 (Q2 A): the end-of-cycle mode switch waits until the classifier uses a per-sample control setpoint in the tail, as thermo-nova does, instead of the per-cycle _cycle_setpointAtStart (SATcycles.ino ~740/911/920/947 per the 2026-10-25 audit). Prerequisite for TASK-1331, alongside TASK-1387.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Tail control-setpoint error p50/p90 computed from per-sample setpoints (host test red on HEAD: a setpoint change mid-cycle changes the classification)
- [ ] #2 RAM cost of the per-sample buffer measured and within budget
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.
<!-- SECTION:NOTES:END -->
