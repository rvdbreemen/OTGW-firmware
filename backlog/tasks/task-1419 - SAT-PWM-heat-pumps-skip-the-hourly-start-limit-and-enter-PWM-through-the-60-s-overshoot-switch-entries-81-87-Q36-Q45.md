---
id: TASK-1419
title: >-
  SAT PWM: heat pumps skip the hourly start limit and enter PWM through the 60 s
  overshoot switch (entries 81, 87; Q36, Q45)
status: To Do
assignee: []
created_date: '2026-10-28T06:46:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1382
references:
  - docs/sat/sergeant-d-corrections.md
priority: medium
ordinal: 583000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-28: Q36 A (entry 81, no hourly start cap for heat pumps, narrows entry 59) and Q45 A (entry 87, remove the !satAlwaysMaxModulation() gate so heat pumps enter PWM via the 60 s switch, as thermo-nova). Keep recording ON starts so cycles_this_hour is still reported. Q45 only after TASK-1382 (Q35).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: heat pump with 3 PWM starts in an hour is not held (red on HEAD, green on fix); gas boiler regression keeps the entry 59 limit
- [ ] #2 Host test: heat pump in sustained overshoot enters PWM after 60 s (red on HEAD, green on fix)
- [ ] #3 build + evaluate green
<!-- AC:END -->
