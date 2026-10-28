---
id: TASK-1428
title: >-
  SAT sustained switches: no flow-validity check (thermo-nova resets on flow
  None)
status: To Do
assignee: []
created_date: '2026-10-28T18:15:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 592000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1420 review. satGetFlowTemp() returns raw Tboiler (SATcontrol.ino ~1756-1760). With flow 0/unknown, the PWM underheat exit counts (flow <= requested - 3) in PWM ON. thermo-nova resets the overshoot start and the underheat timer when flow_temperature is None (heating_control.py:284-286, :333-335). Pre-existing (old code had the same gap with <). Needs: what counts as 'no flow' in the firmware (Tboiler not received / stale), then reset like thermo-nova.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: no valid flow resets the overshoot and underheat starts, red on HEAD, green on fix
- [ ] #2 build + evaluate green
<!-- AC:END -->
