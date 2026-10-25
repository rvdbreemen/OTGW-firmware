---
id: TASK-1387
title: >-
  SAT: make the end-of-cycle classifier match thermo-nova's classifier.py
  exactly (entry 55, Q4 A)
status: To Do
assignee: []
created_date: '2026-10-25T11:34:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
ordinal: 526000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-25 (Q4: A, entry 55). Remove the firmware-only rules (a continuous cycle labelled SHORT after the overshoot test; any cycle under 120 s always UNCERTAIN) and add classifier.py's UNCERTAIN results: kind UNKNOWN (cycles/classifier.py:22-23), PWM tail setpoint error p50/p90 missing (:42-43), continuous tail control-setpoint error p50/p90 missing (:71-72). The label switches PWM/continuous (entry on Q2), so it must follow classifier.py. Prerequisite for TASK-1331.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Classifier results equal classifier.py for every rule (host test table, red on HEAD)
- [ ] #2 No SHORT for continuous cycles and no blanket under-120-s UNCERTAIN
<!-- AC:END -->
