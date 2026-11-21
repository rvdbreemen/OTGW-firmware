---
id: TASK-2994
title: >-
  SAT docs: firmware-sat-spec and sat-comparison still describe the 180 s held
  BT and MM=T across range 5 (entries 437/438/440)
status: To Do
assignee: []
created_date: '2026-11-21T04:26:00Z'
labels:
  - docs
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1121000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After TASK-2993/2992 the PWM duty uses the live flow after a 30 s start-up EMA (entries 437/438) and MM=T needs a raw duty above 1.03 (entries 436/440). Stale text found by the TASK-2993 review: docs/sat/firmware-sat-spec.md:1636, :1655, :1711, :1900; docs/sat/sat-comparison.md:515, :547, :568.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 firmware-sat-spec.md describes the 30 s EMA then live flow, and MM=T only above raw duty 1.03, with entry citations
- [ ] #2 sat-comparison.md lists both as deliberate deviations from thermo-nova 7beaaf5 (pwm.py:144-149, 229-239) with entries 437/438/440
<!-- AC:END -->
