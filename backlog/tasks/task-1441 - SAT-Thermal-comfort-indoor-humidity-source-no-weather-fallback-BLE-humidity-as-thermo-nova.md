---
id: TASK-1441
title: >-
  SAT Thermal comfort: indoor humidity source (no weather fallback? BLE
  humidity?) as thermo-nova
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T05:11:00Z'
updated_date: '2026-10-29T05:16:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 602000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1282 (2026-10-29). Thermal comfort computes the Summer Simmer Index from humidity (climate.py:276-277). The firmware falls back to the outdoor weather humidity when no indoor value is present (SATcontrol.ino ~5486: state.sat.fHumidity = state.sat.weather.fHumidity), and BLE sensor humidity never feeds fHumidity. thermo-nova takes humidity from the configured humidity sensor only. Needs a question to Sergeant D (A: indoor only as thermo-nova, B: keep weather fallback, C: other) before any change; then implement with a host test red on HEAD.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Question asked and answer recorded in docs/sat/sergeant-d-corrections.md
- [ ] #2 Implemented per the answer with a host test red on HEAD, green on fix
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29 Q57 asked of Sergeant D in #alpha-testing (citations opened in session: thermo-nova climate.py:282-287, firmware SATcontrol.ino:5484-5488, SATble.ino:892). Waiting for his answer.

2026-10-29 Q57 answered A (entry 117, msg 1565232358192455792): indoor only (MQTT/REST or BLE room sensor humidity), no weather fallback; without indoor humidity Thermal comfort uses the room temperature. Our reading for push vs BLE priority: most recent valid reading wins.
<!-- SECTION:NOTES:END -->
