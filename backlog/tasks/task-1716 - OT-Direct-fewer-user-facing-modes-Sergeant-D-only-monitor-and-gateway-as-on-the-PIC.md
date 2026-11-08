---
id: TASK-1716
title: >-
  OT-Direct: fewer user-facing modes (Sergeant D: only monitor and gateway, as
  on the PIC)
status: To Do
assignee: []
created_date: '2026-11-08T07:11:00Z'
labels:
  - otdirect
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 825000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-08 05:55 #alpha-testing (msg 1568866608133308471), to Robert: 'Do we really need all these modes? In original OTGW we have only monitoring and gateway modes. Shouldn't OT-direct have the same modes?' Today OT-Direct has 5 (OTDirecttypes.h:50-54): bypass 0 (relay, thermostat straight to the boiler), gateway 1 (default), monitor 2, master 3 (no thermostat, sole OT master; needed for SAT without a thermostat), loopback 4 (internal test, simulated boiler). Proposal (not decided, Robert): the settings UI offers gateway and monitor only; master is entered automatically when SAT is on and no thermostat is seen (TASK-1694 auto-detect), bypass and loopback stay as service/test commands. Touches OTDmode setting/REST/MQTT, TASK-1694, TASK-1702.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert decides which modes stay user-facing
- [ ] #2 Settings UI, REST and MQTT match that decision; existing stored modes migrate without a surprise for the user (test)
<!-- AC:END -->
