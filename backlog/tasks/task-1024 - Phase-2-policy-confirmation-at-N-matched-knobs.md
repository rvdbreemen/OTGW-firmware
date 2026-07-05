---
id: TASK-1024
title: 'Phase 2: policy confirmation at N* (matched knobs)'
status: To Do
assignee: []
created_date: '2026-07-05 22:22'
labels: []
dependencies: []
ordinal: 235000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1015. Set gate caps = N* AND client MAX_INFLIGHT = N*, flash, run S1-S5 incl. combined-stress + overload. Holds (zero incidents, heap-safe, WS+HTTP recover) -> confirm. Fails -> drop to N*-1 and re-confirm.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 N* confirmed under combined-stress + overload, or reduced until it confirms
<!-- AC:END -->
