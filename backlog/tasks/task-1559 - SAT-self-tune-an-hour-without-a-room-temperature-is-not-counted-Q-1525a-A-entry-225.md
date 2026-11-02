---
id: TASK-1559
title: >-
  SAT self-tune: an hour without a room temperature is not counted (Q-1525a A,
  entry 225)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T05:45:00Z'
updated_date: '2026-11-02T23:43:00Z'
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
- [x] #1 Host test on the real code: an hour with the room temperature lost is logged SATTUNEH counted=0 reason=no_room and does not enter the daily mean (red on HEAD, green)
- [x] #2 docs/sat/sat-log-format.md lists the new reason
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03: removed the SAT_CT_EXCLUDE_ROOM_LOST build switch (pending flag for Q-1525a, now entry 225 A): a room-lost hour returns SATT_NO_ROOM unconditionally. tests/test_sat_curve_selftune.py 55/55; on the old SATcontrol.ino the two no_room tests fail (+1 source check that mixes old .ino and new header, not behaviour). docs/sat/sat-log-format.md updated; CHANGELOG. evaluate --quick 0 failed. Bench: SATTUNEH needs a full hour without room temperature, host test is the evidence.
<!-- SECTION:NOTES:END -->
