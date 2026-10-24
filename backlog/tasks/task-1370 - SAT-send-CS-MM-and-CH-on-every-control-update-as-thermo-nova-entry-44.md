---
id: TASK-1370
title: 'SAT: send CS, MM and CH on every control update as thermo-nova (entry 44)'
status: To Do
assignee: []
created_date: '2026-10-24T21:44:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 505000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 (Q13, entry 44): follow thermo-nova, send CS/MM/CH every update instead of on change + 300 s (audit #24). TC= stays on change only (entry 16). Check PIC queue load and OT-Direct write cadence. Blocks ADR-199.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 CS/MM/CH are sent on every control update (host test red/green); PIC queue and OT-Direct load checked on the bench
<!-- AC:END -->
