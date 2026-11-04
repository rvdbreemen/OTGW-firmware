---
id: TASK-181
title: 'Fix: suspend schedule polling in monitor mode'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:32'
updated_date: '2026-04-08 22:52'
labels:
  - audit-fix
  - otgw32
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
In monitor mode (GW=M), scheduleMasterRequest() is called unconditionally every 100ms from loopOTDirect(). This causes the gateway to inject its own scheduled OT master frames (MsgID 0 every 800ms, temperature reads every 10s, etc.) onto the OT bus alongside the real thermostat. OpenTherm is a single-master protocol — two masters collide, breaking transparent pass-through. Fix: add a mode guard in loopOTDirect() so scheduleMasterRequest() is skipped in monitor mode. The slave frame forwarding path (line 1185) is already correct; only the unsolicited schedule injection needs suppression.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 scheduleMasterRequest() is not called when IS_MONITOR_MODE() is true
- [x] #2 In monitor mode, only thermostat-originated frames are forwarded to the boiler (no gateway-originated frames)
- [x] #3 MsgID 0 status request from gateway is suppressed in monitor mode
- [x] #4 All other scheduled reads/writes are suppressed in monitor mode
- [ ] #5 Audit task 153 AC #4 passes after fix
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added IS_MONITOR_MODE() guard to the scheduleMasterRequest() call in loopOTDirect().

In monitor mode the gateway must not inject its own master frames onto the OT bus (single-master protocol). Only the thermostat-forward path (already correct at line 1185) is allowed. The guard is placed on the DUE(timerOTSchedule) branch so it also suppresses the MsgID 0 status request and all slow/temp poll entries.
<!-- SECTION:FINAL_SUMMARY:END -->
