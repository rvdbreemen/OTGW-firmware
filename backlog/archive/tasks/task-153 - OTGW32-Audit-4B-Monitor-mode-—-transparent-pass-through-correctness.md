---
id: TASK-153
title: 'OTGW32-Audit-4B: Monitor mode — transparent pass-through correctness'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:20'
updated_date: '2026-04-08 22:33'
labels:
  - audit
  - otgw32
  - phase-4
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit monitor mode (GW=M): all frames from thermostat and boiler must pass through completely unmodified. No overrides applied, no data injection, no schedule polling. Verify that the slave ISR still captures frames for logging via processOT() but does not alter them.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Thermostat frames forwarded to boiler without any modification
- [x] #2 Boiler responses forwarded to thermostat without any modification
- [x] #3 No override entries are applied in monitor mode
- [ ] #4 Schedule polling is suspended in monitor mode
- [x] #5 Frames are still logged via processOT() for MQTT/REST/WebSocket
- [x] #6 Any unintended modification results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Audit findings

- Monitor mode slave frame handling (line 1185): passes otSlaveFrame directly to sendMasterRequestAsync with OT_DIRECT_ORIGIN_THERMOSTAT — no overrides, no SR/UI table lookups — correct
- Response path (line 967-973): applyResponseModifiers() only called when otCurrentMode == OTD_MODE_GATEWAY — not in monitor mode — correct
- processOT() logging: bridgeFrameToParser() called inside sendMasterRequestAsync() for T-frame and handleMasterResponse() for B-frame — frames ARE logged

## GAP FOUND: scheduleMasterRequest() not guarded by mode
- Line 1237-1240: scheduleMasterRequest() called unconditionally every 100ms regardless of mode
- In monitor mode, the gateway still injects its own scheduled frames (MsgID 0 every 800ms, temperature reads every 10s, etc.) as OT master
- This violates transparent pass-through: the gateway is inserting frames the thermostat never sent
- An OT bus is single-master: two masters (real thermostat + gateway scheduler) will collide
- Spec requires: "No overrides, no schedule polling" in monitor mode
- AC #4 FAILS: schedule polling is NOT suspended in monitor mode
- Audit-fix task required
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Monitor mode transparent pass-through audit: one real gap found.

- Slave frame forwarding (line 1185): correct — no UI/SR/override processing, passes frame directly
- Response-path modifiers: correctly skipped in monitor mode (only applied when mode==GATEWAY)
- processOT() logging: frames ARE logged via bridgeFrameToParser() — correct

GAP: scheduleMasterRequest() is called unconditionally every 100ms (lines 1237-1240), including in monitor mode. The gateway injects its own MsgID 0 status frames (every 800ms) and all scheduled read entries onto the OT bus. OpenTherm is single-master — this causes bus collisions and breaks transparent pass-through. AC #4 (schedule polling suspended) fails.

Audit-fix raised: TASK-181 "Fix: suspend schedule polling in monitor mode".
<!-- SECTION:FINAL_SUMMARY:END -->
