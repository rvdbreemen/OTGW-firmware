---
id: TASK-583
title: 'feat(otdirect): increase ventilation MsgID polling frequency from 60s to 10s'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 16:00'
updated_date: '2026-05-08 16:52'
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
- [x] #1 When otdMode includes vent control, MsgIDs 70 (V/H status) and 71 (Vset) are polled every 10 seconds
- [x] #2 MsgIDs 72-76 (V/H config/diagnostics) remain in the 60s tier
- [x] #3 The reclassification only activates when the boiler slave config (MsgID 3) indicates ventilation application support
- [x] #4 No regression on builds without ventilation boilers (polling falls back to 60s if slave app != vent)
- [x] #5 Change is documented in the OTDirect schedule constants (ADR-068 or successor)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add OT_VENT_FAST_INTERVAL_MS = 10000 constant near other interval constants\n2. Add otIsVentSlave() inline helper checking MsgID 3 HB bits 6-7 == 0b11\n3. In scheduleMasterRequest() interval determination: override to 10s for MsgIDs 70+71 when otIsVentSlave() is true\n4. MsgIDs 72-76 remain at OT_SLOW_INTERVAL_MS (60s) unchanged\n5. Falls back to 60s automatically when slave app != vent (cache miss = false)
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Increased ventilation MsgID polling frequency from 60s to 10s when slave is ventilation/HRV type.

Changes:
- OTDirect.ino: added OT_VENT_FAST_INTERVAL_MS = 10000 constant
- OTDirect.ino: added otIsVentSlave() inline helper checking MsgID 3 HB bits 6-7 == 0b11 (OT spec: ventilation/HRV app type)
- OTDirect.ino: scheduler interval override applies 10s to MsgIDs 70+71 only when otIsVentSlave() returns true; MsgIDs 72-76 remain at 60s

Behavior: when slave config (MsgID 3) reports ventilation application type, V/H status (70) and control setpoint (71) are polled every 10s instead of 60s. Non-ventilation builds automatically fall back to 60s (otIsVentSlave() returns false when cache miss or non-vent bits set).
<!-- SECTION:FINAL_SUMMARY:END -->
