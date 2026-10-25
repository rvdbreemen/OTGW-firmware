---
id: TASK-1381
title: 'SAT continuous mode: add thermo-nova''s previous-CS term to the flow clamp'
status: To Do
assignee: []
created_date: '2026-10-25T10:38:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
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
