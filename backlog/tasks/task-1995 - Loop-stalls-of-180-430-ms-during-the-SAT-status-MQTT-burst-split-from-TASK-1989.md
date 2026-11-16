---
id: TASK-1995
title: >-
  Loop stalls of 180-430 ms during the SAT status MQTT burst (split from
  TASK-1989)
status: To Do
assignee: []
created_date: '2026-11-16T06:28:00Z'
labels:
  - performance
dependencies: []
priority: low
ordinal: 1019000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1989 (out of its scope, coordinator 2026-11-16). In gruijter's beta.250 capture 34 of the 92 [loop-stall] lines (200-600 ms) sit in an MQTT burst: satSendStatus plus the sat/* settings topics at :x6:25 / :x6:55, with an internal gap of 180-400 ms between addCommandToQueue and satControlLoopRun inside the window. Not a file write, so the TASK-1989 file-write work does not touch it. Find what runs in that step with the slowest-step instrumentation of TASK-1989 (92c914047) on a capture, then decide fix or keep.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A capture on a build with the slowest-step stall line names the step of the burst stalls
- [ ] #2 Fix with old-vs-fix numbers, or a recorded reason why it stays (bounded, harmless)
<!-- AC:END -->
