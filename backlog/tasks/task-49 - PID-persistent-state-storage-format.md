---
id: TASK-49
title: PID persistent state storage format
status: To Do
assignee: []
created_date: '2026-04-05 21:06'
labels:
  - sat
  - feature
  - pid
dependencies: []
references:
  - 'other-projects/SAT-releases-thermo-nova/custom_components/sat/pid.py:141-157'
  - 'other-projects/SAT-releases-thermo-nova/custom_components/sat/pid.py:239-251'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Store PID controller state (integral, last_error, raw_derivative, last_temperature) to survive reboots. Uses specific store key format per entity. Extends task #6 (basic PID) with persistence details to prevent integral windup reset on reboot.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 PID state (integral, last_error, raw_derivative, last_temperature) saved to persistent storage
- [ ] #2 Specific store key format per entity (unique key per PID instance)
- [ ] #3 State restored on boot before first PID cycle
- [ ] #4 Graceful handling of missing/corrupt stored state (fall back to defaults)
- [ ] #5 Storage writes throttled to prevent flash wear (not every cycle)
<!-- AC:END -->
