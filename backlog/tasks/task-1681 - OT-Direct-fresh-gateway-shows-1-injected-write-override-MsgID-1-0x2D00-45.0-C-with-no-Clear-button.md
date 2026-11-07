---
id: TASK-1681
title: >-
  OT-Direct: fresh gateway shows 1 injected write override MsgID 1 = 0x2D00
  (45.0 C) with no Clear button
status: To Do
assignee: []
created_date: '2026-11-07T09:03:00Z'
labels:
  - bug
  - otdirect
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 802000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by tjfs in #alpha-testing 2026-11-07 08:52 UTC (msg 1568548767014330449), board without PIC, beta.117. Write overrides (otOverrides, OTDirect.ino:397) come from CS= / setback (checkThermostatTimeout, OTDirect.ino:2662) / master-mode thermostat WRITE of MsgID 1 (OTDirect.ino:2718); the web UI renders write rows without a Clear action (v2.js renderOtdOvr, otdOvrRow(...,null)). Unknown which path set 45.0 C on a fresh unit (candidate: a retained MQTT command, or master mode with a thermostat). Asked tester for mode, thermostat, MQTT and a capture.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Source of the MsgID 1 write override on a fresh unit identified from tester data or a bench reproduction
- [ ] #2 Write overrides can be cleared from the web UI, or the UI says why they cannot (mode-driven)
<!-- AC:END -->
