---
id: TASK-1352
title: 'SAT: last cycle class never expires (thermo-nova drops it after 6 h)'
status: To Do
assignee: []
created_date: '2026-10-24T16:28:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 487000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md) finding #55: SATcycles.ino:588 keeps the last cycle class forever; thermo-nova cycles/history.py:48-57 drops it after 6 h, so HA keeps showing an old cycle problem. Display/state only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Last cycle class reported as none after 6 h without a new cycle (host test red on HEAD, green)
<!-- AC:END -->
