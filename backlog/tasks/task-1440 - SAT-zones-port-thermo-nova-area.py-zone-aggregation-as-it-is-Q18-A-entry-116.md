---
id: TASK-1440
title: >-
  SAT zones: port thermo-nova area.py zone aggregation as it is (Q18 A, entry
  116)
status: To Do
assignee: []
created_date: '2026-10-29T05:02:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 601000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-29 Q18 A (entry 116): port thermo-nova's zone logic as it is. thermo-nova requested_setpoint (climate.py:372-392): setpoint = max(primary PID, areas output); areas output = int(0.75N) percentile capped at baseline + 5.0 (area.py:286-301); overshoot ceiling 22 + 10 - 4 x degrees over for areas more than 0.3 over target, min over areas, floored at 22, applied as min(setpoint, cap) (area.py:312-333, climate.py:388-389); an area counts only with output > 22 or valve >= 10 (area.py:166-175); ECO skips both (climate.py:377-379); aggregation also with one room. Firmware today (SATcontrol.ino:5516-5566) differs in eight ways (replaces primary, ceil(0.75N)-1, headroom added, overshoot subtracted, no 0.3 margin, no cap with one zone, counts zones above 10, runs only with 2+ zones and in ECO). Changes how SAT controls the boiler: decision recorded (entry 116), so no further OK needed for the logic itself.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Zone aggregation follows climate.py:372-392 and area.py:166-175, 286-301, 312-333 (host test red on HEAD, green on fix: N=4 picks the highest output, headroom caps, main PID never lowered, overshoot ceiling floored at 22, ECO ignores zones)
- [ ] #2 Settings and docs that only served the old aggregation (fZoneAggregationHeadroom) handled and documented; firmware-sat-spec and sat-comparison updated
<!-- AC:END -->
