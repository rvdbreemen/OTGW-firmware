---
id: TASK-1423
title: >-
  SAT PID: round every PID value to 3 decimals and match thermo-nova's Ki-near-0
  clamp (entry 95, Q28a/Q28b)
status: To Do
assignee: []
created_date: '2026-10-28T07:51:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1368
references:
  - docs/sat/sergeant-d-corrections.md
priority: medium
ordinal: 587000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-28 (entry 95): Q28a 'Round all the values to 3 decimals not only the Output' (deviation: thermo-nova rounds gains to 6 decimals pid.py:71/:82/:93, P/I/D/raw derivative to 3 pid.py:101-119, output to 1 pid.py:132); Q28b Ki-near-0 integral clamp as thermo-nova (SATpid.ino ~187 skips the clamp). ADR-197.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: gains, P, I, D, raw derivative and output are rounded to 3 decimals (red on HEAD, green on fix)
- [ ] #2 Host test: integral clamp with Ki near 0 behaves as pid.py (red on HEAD where it differs)
- [ ] #3 ADR-197 updated with entry 95
- [ ] #4 build + evaluate green
<!-- AC:END -->
