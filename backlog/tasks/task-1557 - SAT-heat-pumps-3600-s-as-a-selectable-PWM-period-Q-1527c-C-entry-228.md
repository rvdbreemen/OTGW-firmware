---
id: TASK-1557
title: 'SAT heat pumps: 3600 s as a selectable PWM period (Q-1527c C, entry 228)'
status: To Do
assignee: []
created_date: '2026-11-02T05:38:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 707000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D answered Q-1527c with C (entry 228, 2026-11-02): add 60 min as a new heat-pump PWM period choice next to 1800 s, 2400 s and the experimental 10800 s. Today the SAThpcycle setter snaps to 1800/2400/10800 (dc566f8c6, TASK-1542). Default stays 1800 s (our interpretation of C, to confirm with Sergeant D when this is planned; entry 194 proposed 60 min as default). Round 23: 3600 s gives 9.9 starts/day vs 17.0 at 1800 s, comfort 0.163 vs 0.162 C (docs/sat/sim-hp-lowload/round23.md).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The heat-pump period setter accepts 3600 s and stores it; 1800/2400/10800 unchanged; a hybrid never gets 3600 s unless entry 188 allows it (check first)
- [ ] #2 The UI offers 60 min for heat pumps; host test red on the old setter, green on the fix
- [ ] #3 Bench: a heat pump set to 3600 s runs PWM periods of 60 min (log or capture)
- [ ] #4 Default stays 1800 s unless Sergeant D confirms otherwise; his confirmation recorded in sergeant-d-corrections.md
<!-- AC:END -->
