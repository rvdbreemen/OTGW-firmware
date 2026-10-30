---
id: TASK-1458
title: >-
  SAT: published Summer Simmer Index is computed from the SSI itself when
  Thermal comfort is on (double SSI)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T07:03:00Z'
updated_date: '2026-10-30T20:13:00Z'
labels:
  - sat
  - mqtt
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 618000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1441 (2026-10-29). The published Summer Simmer Index (sat/ssi, summer_simmer_index, summer_simmer_perception, SATcontrol.ino ~3607-3615) is computed from satGetRoomTemp(). With Thermal comfort on, satGetRoomTemp() already returns the SSI instead of the room temperature (~1700-1710), so the published index is the SSI of an SSI. Pre-existing on HEAD, reporting only (the control path uses the SSI once). Fix: compute the published index from the raw room temperature. Check thermo-nova climate.py:245-246 (summer_simmer_index attribute uses current_temperature, the raw value).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With Thermal comfort on, sat/ssi equals the index of the raw room temperature and humidity (host test red on HEAD, green on fix)
- [ ] #2 With Thermal comfort off the published index is unchanged
<!-- AC:END -->
