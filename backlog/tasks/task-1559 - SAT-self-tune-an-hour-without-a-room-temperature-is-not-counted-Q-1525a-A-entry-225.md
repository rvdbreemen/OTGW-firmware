---
id: TASK-1559
title: >-
  SAT self-tune: an hour without a room temperature is not counted (Q-1525a A,
  entry 225)
status: To Do
assignee: []
created_date: '2026-11-02T05:45:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 711000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-02 answered Q-1525a with A (entry 225): when the room temperature is lost SAT runs on the curve only (signal 0); such an hour is excluded with reason 'no room temperature' instead of diluting the 3-day mean toward 0. ADR-205 / TASK-1525 built B (counted) until his answer. thermo-nova has no curve self-tune.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real code: an hour with the room temperature lost is logged SATTUNEH counted=0 reason=no_room and does not enter the daily mean (red on HEAD, green)
- [ ] #2 docs/sat/sat-log-format.md lists the new reason
<!-- AC:END -->
