---
id: TASK-1022
title: 'Phase 1: capacity-curve sweep (offered-N 1..8, bounded gate 8)'
status: To Do
assignee: []
created_date: '2026-07-05 22:22'
labels: []
dependencies: []
ordinal: 233000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1015. One instrumented build, bounded safety gate cap=8. Sweep offered-concurrency 1,2,3,4,6,8 in software (no reflash). Run S1-S5 per step. Produce curve: latency + incidents + firmware hwm vs offered-N. Identify device ceiling + candidate N*.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Capacity curve (latency+incidents+hwm vs offered-N) produced
- [ ] #2 Candidate N* = highest offered-N with zero incidents identified
<!-- AC:END -->
