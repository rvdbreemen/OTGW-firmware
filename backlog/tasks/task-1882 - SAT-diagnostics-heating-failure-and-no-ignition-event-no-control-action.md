---
id: TASK-1882
title: 'SAT diagnostics: heating-failure and no-ignition event (no control action)'
status: To Do
assignee: []
created_date: '2026-11-13T06:13:00Z'
labels:
  - sat
  - diagnostics
  - needs-maintainer-ok
dependencies:
  - TASK-1879
priority: low
ordinal: 955000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Lesson 3 of docs/sat/vtherm-comparison-2026-11-13.md (TASK-1879). VTherm raises a heating-failure event when on >= 90% for 15 min and the room rises < 0.5 C, without a control action (const.py:540-544, feature_heating_failure_detection_manager.py:224-264). SAT counterparts: (a) high requested setpoint for N min while the room temperature does not rise; (b) PWM ON phase or requested CS above the cold cutoff with no flame within X. Event + telemetry + mark the tuner hour as not counted. Thresholds N and X are a proposal for Robert; marking tuner hours changes tuner input, so that part needs Sergeant D's OK before implementation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Thresholds and the tuner-marking part decided (Robert; Sergeant D for the tuner part) and recorded
- [ ] #2 Host test reproduces both failure patterns: event absent on the old code, raised on the fix; no change in CS/MM/CH output
- [ ] #3 Event visible in REST, MQTT (flat topic) and the telnet log
<!-- AC:END -->
