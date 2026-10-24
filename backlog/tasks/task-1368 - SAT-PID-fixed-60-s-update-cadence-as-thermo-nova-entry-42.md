---
id: TASK-1368
title: 'SAT PID: fixed 60 s update cadence as thermo-nova (entry 42)'
status: To Do
assignee: []
created_date: '2026-10-24T21:43:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 503000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 (Q10, entry 42): follow thermo-nova, PID updated every 60 s (climate.py:712-716) instead of on the 30 s control tick (audit #14). Blocks ADR-197.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 PID updates every 60 s independent of the control tick (host test red/green)
<!-- AC:END -->
