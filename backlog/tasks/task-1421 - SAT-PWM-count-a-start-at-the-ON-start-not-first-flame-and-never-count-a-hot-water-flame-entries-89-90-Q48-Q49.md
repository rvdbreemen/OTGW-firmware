---
id: TASK-1421
title: >-
  SAT PWM: count a start at the ON start (not first flame) and never count a
  hot-water flame (entries 89, 90; Q48, Q49)
status: To Do
assignee: []
created_date: '2026-10-28T06:47:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1382
references:
  - docs/sat/sergeant-d-corrections.md
priority: high
ordinal: 585000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-28: Q49 B (count at the OFF->ON start, every start, as pwm.py:160; entry 90) and Q48 A with 'do not count flame when DHW enable is triggered' (entry 89). Move counting to satPwmBeginOn, drop the first-flame count, exclude a flame lit while the boiler DHW-active bit was set (Q48b may refine). Hard dependency: TASK-1382 (Q35) in the same build or earlier, else slow ignition burns the hourly limit. Feeds TASK-1412.9.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: an unlit ON start counts (red on HEAD, green on fix)
- [ ] #2 Host test: a hot-water flame carrying into a PWM ON phase is not counted (red on HEAD, green on fix)
- [ ] #3 build + evaluate green
<!-- AC:END -->
