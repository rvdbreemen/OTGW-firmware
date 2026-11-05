---
id: TASK-1651
title: >-
  SAT: let the PID integral build outside the deadband, with a clamp (maintainer
  decision, deviation from thermo-nova)
status: To Do
assignee: []
created_date: '2026-11-05T06:59:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 775000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-05 chose option B for Q-1412.16a (see docs/sat/sergeant-d-corrections.md, maintainer entry): today the integral is reset to 0 outside the 0.1 C deadband on every update (thermo-nova pid.py:187-189, firmware equal), so the last 0.3 C of a warm-up takes ~2 h (gruijter 2026-11-04; simulation 224 min vs 22-155 min for a bounded PI). Deviation from thermo-nova: own ADR (Proposed) and simulation proof first.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Proposed ADR via adr-kit naming the deviation, the clamp and its bound, citing pid.py:187-189 and the maintainer entry
- [ ] #2 TASK-1412.16/1412.19 simulation rerun: warm-up time to within 0.3 C improves and overshoot/starts do not regress versus today (pre-registered thresholds)
- [ ] #3 Host test of the PID update red on old / green on fix; bench run and a field run named before Done
<!-- AC:END -->
