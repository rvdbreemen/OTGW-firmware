---
id: TASK-1652
title: >-
  SAT: a lit burner runs at least 180 s even when the cold cutoff comes earlier
  (maintainer decision)
status: To Do
assignee: []
created_date: '2026-11-05T06:59:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 776000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-05: the 180 s minimum burn (entries 31/80) wins over the cold cutoff (entries 12/199), which today ends PWM ON phases with less than 180 s of flame (TASK-1412.19 simulation).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Rule implemented: after ignition the ON phase is not ended by the cold cutoff before 180 s of flame; other stop reasons (fault, DHW, SAT off, safety) still end it at once
- [ ] #2 Simulation (1412.19 harness) shows no ON phase below 180 s from the cold cutoff and no new overshoot class
- [ ] #3 Host test red on old / green on fix; ADR or spec note updated per the SAT rules
<!-- AC:END -->
