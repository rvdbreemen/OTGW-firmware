---
id: TASK-1987
title: >-
  analyze-capture skill: 'demand without flame' follows the master CH enable,
  not the boiler CH-mode bit
status: To Do
assignee: []
created_date: '2026-11-15T21:15:00Z'
updated_date: '2026-11-15T21:28:00Z'
labels:
  - capture
  - skill
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1015000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The analyze-capture skill's capture_summary.py counts 'demand without flame' from the boiler's CH-mode status bit, which includes the boiler's pump overrun. On gruijter's beta.250 capture (10 min overrun) that gave ~600 s 'demand without flame' per run while SAT had ch=0 (no CH enable), and the overview chart shades the overrun as 'Heating demand, no flame'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 capture_summary.py and capture_plot.py take demand from the master CH enable (MsgID 0 master status bit 0), and show the boiler CH-mode-only time (overrun) separately
- [ ] #2 Test on a small frame fixture with a pump overrun: old count wrong, new right
<!-- AC:END -->
