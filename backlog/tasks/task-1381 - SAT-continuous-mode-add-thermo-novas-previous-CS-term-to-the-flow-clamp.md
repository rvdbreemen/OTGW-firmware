---
id: TASK-1381
title: 'SAT continuous mode: add thermo-nova''s previous-CS term to the flow clamp'
status: To Do
assignee: []
created_date: '2026-10-25T10:38:00Z'
updated_date: '2026-10-28T20:21:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1270
references:
  - TASK-1412
ordinal: 520000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From TASK-1273 (gruijter field run 2026-10-20). thermo-nova clamps the continuous setpoint with the previous control setpoint: rising request -> max(requested, flow - offset); falling -> max(requested, min(flow - offset, previous)) (heating_control.py:484-488). Firmware satApplyContinuous (SATcontrol.ino ~1353-1392 at HEAD) is flow-offset only, so CS follows a rising flow. Changes how SAT controls the boiler: needs a queued question + Sergeant D's OK.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Continuous CS follows heating_control.py:484-488 incl. the previous-CS term (host test red on HEAD, green on fix)
- [ ] #2 Sergeant D's OK recorded in docs/sat/sergeant-d-corrections.md before implementation (CLAUDE.md SAT rules)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-28 dependency reconciliation (Robert's go: reconcile the graph alongside the wave-3 work; scout+verifier report): TASK-1412.3 dependency removed (own red/green AC#1). Real blocker not in the graph: AC#2 needs Sergeant D's recorded OK; no question was queued -> queued in open-questions.md (Q53).

2026-10-28 Q53 answered A (entry 112): follow thermo-nova heating_control.py:485-489; previous = last CS SAT sent (10 after heater-off/cold cutoff, 0 after enable/boot), no restore after reboot; thermal-safe and simmer write their CS as previous (our sub-choice). AC#2 has his OK.
<!-- SECTION:NOTES:END -->
