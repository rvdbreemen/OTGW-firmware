---
id: TASK-1346
title: >-
  OT-Direct: SAT's last CH=0 must not stay in the gateway's MsgID 0 after a SAT
  disable or trip
status: To Do
assignee: []
created_date: '2026-10-24T10:11:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 469000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md), finding #3: OTDirect.ino:2755-2760 / 756-764 keep SAT's CH=0; afterwards the board's own curve and the thermostat-timeout setback run with CH disabled, the house cools silently.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After disable/trip on OT-Direct the gateway MsgID 0 CH bit returns to the thermostat's / default value (host or bench test red on HEAD, green)
<!-- AC:END -->
