---
id: TASK-1653
title: >-
  SAT on PIC: request MsgID 25 itself when the thermostat never reads it
  (maintainer decision)
status: To Do
assignee: []
created_date: '2026-11-05T07:00:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 777000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-05: on a PIC installation whose thermostat never reads MsgID 25, SAT has no flow value (the PIC only listens). SAT asks for MsgID 25 itself through a PIC command, only when 25 was not seen for N s. The safe PIC command must be confirmed in other-projects/otgw-6.6 first (no guessing).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The PIC command and its behaviour are cited from otgw-6.6 (file:line) and checked against otmonitor usage before any code
- [ ] #2 SAT requests MsgID 25 only after N s without a sample, never in parallel with the thermostat's own reads; N and rate documented
- [ ] #3 Host test red on old / green on fix; bench on the Classic (.63) with a thermostat that does not read 25, or a replay that omits it
<!-- AC:END -->
