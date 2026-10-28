---
id: TASK-1420
title: >-
  SAT: sustained switches as thermo-nova: overshoot >=, underheat/saturation
  timers, scheduled tick time, switch before CS (entries 86, 88; Q43, Q44, Q46,
  Q47)
status: To Do
assignee: []
created_date: '2026-10-28T06:47:00Z'
updated_date: '2026-10-28T07:52:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1412.3
references:
  - docs/sat/sergeant-d-corrections.md
priority: medium
ordinal: 584000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-28: Q43 A (>=), Q44 A (underheat/saturation timed from the first qualifying sample, no 60 s bail-out, <= for underheat), Q46 B (time the 60 s switch on the tick's scheduled time), Q47 A (run continuous->PWM switch before the CS of the same tick). All in satCycleCheckAutoSwitch(). Q46b (scope for other timers) open. Feeds TASK-1412.8.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host tests per rule, red on HEAD and green on the fix: equality counts; underheat fires at 180 s and saturation at 300 s at a 30 s and a 60 s tick; switch at the first tick >= 60 s scheduled; first PWM CS on the switch tick
- [ ] #2 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 Q46b (entry 95): scheduled tick time applies to the underheat and saturation timers too.
<!-- SECTION:NOTES:END -->
