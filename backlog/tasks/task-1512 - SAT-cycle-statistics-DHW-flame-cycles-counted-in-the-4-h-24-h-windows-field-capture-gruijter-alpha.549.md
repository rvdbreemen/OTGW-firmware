---
id: TASK-1512
title: >-
  SAT cycle statistics: DHW flame cycles counted in the 4 h / 24 h windows
  (field capture gruijter, alpha.549)
status: To Do
assignee: []
created_date: '2026-10-30T14:13:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 668000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field evidence: gruijter's capture 20261030-092718 (alpha.549). 14 DHW flame cycles (CS 10, flow ~62 C, 3-52 s, cycle_attributes kind DOMESTIC_HOT_WATER, class 5 UNCERTAIN / 7 INSUFFICIENT) were logged as SAT cycles #7-#20 and counted in 'SAT 4h': n=7 avgOn 489 s duty 0.34 flow 42.0 (12:33) -> n=20 avgOn 181 s duty 0.29 flow 54.6 (13:23). Question: does thermo-nova exclude DHW cycles from its cycle windows / statistics? Must be checked at the pinned commit (cycles/ or classifier.py) with file:line before any change; if thermo-nova also counts them, the choice goes to Sergeant D. Related: TASK-1387 (classifier parity).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 thermo-nova behaviour for DHW cycles in the cycle windows cited (file:line, second reader)
- [ ] #2 Firmware matches the decided behaviour; host test with a DHW cycle shows the window counts
<!-- AC:END -->
