---
id: TASK-1681
title: >-
  OT-Direct: fresh gateway shows 1 injected write override MsgID 1 = 0x2D00
  (45.0 C) with no Clear button
status: To Do
assignee: []
created_date: '2026-11-07T09:03:00Z'
updated_date: '2026-11-07T09:11:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 likely cause found in code: in master mode with CH mode fixed (defaults iCHMode=1, fFlowTemp=45.0, OTDirecttypes.h:120-121), loopOTDirect calls enqueueWriteCommand(1, floatToF88(getFlowTemp()), "heating-curve") every 60 s (OTDirect.ino:2258-2263), which sets the MsgID 1 write override to 0x2D00 = 45.0 C. Bench .61 (master mode, no thermostat, SAT enabled) shows write overrides MsgID 1=9420 (0x24CC, 36.8 C from SAT) and 14=25600 (0x6400). So on a master-mode unit the entry is the gateway's own control, not a stray injection; a Clear button would be undone within 60 s. Fix direction: label master-mode/SAT writes as the gateway's own control in the UI instead of 'injected', no Clear for them. Waiting for tjfs to confirm the mode.
<!-- SECTION:NOTES:END -->
