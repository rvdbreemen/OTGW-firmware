---
id: TASK-583
title: 'feat(otdirect): increase ventilation MsgID polling frequency from 60s to 10s'
status: To Do
assignee: []
created_date: '2026-05-08 16:00'
labels:
  - otdirect
  - otgw32
  - ventilation
  - performance
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OT-Thing-OTGW32 polls ventilation/heat-recovery MsgID 70 (V/H status) every 10 seconds when the slave application is SLAVEAPP_VENT. The 2.0.0 OTDirect schedule polls MsgID 70 every 60s (slow-poll tier). For users controlling ventilation via OpenTherm, a 60s lag between a setpoint command and the next status read is too slow for responsive control. Reclassify V/H MsgIDs 70-76 into the 10s fast-poll tier when vent mode is active, matching OT-Thing behavior.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 When otdMode includes vent control, MsgIDs 70 (V/H status) and 71 (Vset) are polled every 10 seconds
- [ ] #2 MsgIDs 72-76 (V/H config/diagnostics) remain in the 60s tier
- [ ] #3 The reclassification only activates when the boiler slave config (MsgID 3) indicates ventilation application support
- [ ] #4 No regression on builds without ventilation boilers (polling falls back to 60s if slave app != vent)
- [ ] #5 Change is documented in the OTDirect schedule constants (ADR-068 or successor)
<!-- AC:END -->
