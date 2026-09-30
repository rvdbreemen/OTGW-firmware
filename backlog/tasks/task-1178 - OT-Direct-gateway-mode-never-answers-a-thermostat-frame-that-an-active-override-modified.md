---
id: TASK-1178
title: >-
  OT-Direct gateway mode never answers a thermostat frame that an active
  override modified
status: To Do
assignee: []
created_date: '2026-09-30 09:12'
labels:
  - bug
  - otdirect
  - gateway
dependencies: []
priority: high
ordinal: 301000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
DEFECT (dev / 2.0.0, OT-Direct gateway mode; verified by reading the code on 2026-09-30, not yet reproduced on hardware)

In gateway mode a thermostat frame goes through applyOverrides() (src/OTGW-firmware/OTDirect.ino ~:1406-1432). When an override for its MsgID is active, the data is replaced and modified = true. The frame is then sent to the boiler with origin OT_DIRECT_ORIGIN_GATEWAY instead of OT_DIRECT_ORIGIN_THERMOSTAT (:2002-2003: 'modified ? OT_DIRECT_ORIGIN_GATEWAY : OT_DIRECT_ORIGIN_THERMOSTAT').

handleMasterResponse() only sends the boiler's reply back to the thermostat when otLastRequestOrigin == OT_DIRECT_ORIGIN_THERMOSTAT (:1375, otSlave.sendResponse at :1387). There is no other reply path for GATEWAY-origin frames: the function ends by resetting the origin to GATEWAY (:1403). The five otSlave.sendResponse sites are :1243 (loopback), :1387 (forwarded THERMOSTAT origin), :1976 (UI table), :1989 (SR table) and :2558 (master-mode slave handler).

Consequence: while any override is active (TT/TC room-setpoint override on MsgID 16, CS= control setpoint on MsgID 1, or any other otOverrides entry), every thermostat request for that MsgID goes unanswered. OT spec v4.2 requires a slave to answer every valid request, and the PIC gateway always answers the thermostat. A thermostat that misses replies reports communication errors and may fall back. The thermostat overrides via Home Assistant climate setpoints are the core use of gateway mode.

Found as follow-up (b) of the TASK-1173 judge; this task records the code-level verification. 1.x: not applicable (no OT-Direct).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The required reply semantics for an overridden frame are established from primary sources and recorded in the notes with file:line: OT spec v4.2 (slave response rules) and the PIC gateway firmware (other-projects/otgw-6.6): which data the PIC returns to the thermostat for an overridden WRITE and READ (the boiler's reply as-is, or the thermostat's original value), and whether the PIC ever leaves a thermostat request unanswered
- [ ] #2 In gateway mode every thermostat request that is forwarded to the boiler, modified or not, produces exactly one reply to the thermostat (or the documented deliberate exception from AC#1), for both a boiler reply and a boiler timeout
- [ ] #3 Old-vs-fix proof on the same setup: OLD shows the thermostat's request for an overridden MsgID without any reply (no 'A'/reply frame to the thermostat, or thermostat error), FIX shows the reply. Real thermostat on the OTGW32 slave terminals preferred; a host harness is acceptable only if it compiles the real handleMasterResponse/gateway code (sliced, not re-implemented)
- [ ] #4 Monitor mode stays transparent and the UI/SR table paths are unchanged
- [ ] #5 The change ships in one commit with its own prerelease bump; build.bat (esp32 and esp32-combo) SUCCESS with fresh binaries; evaluate.py --quick shows no new FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Filed 2026-09-30. Verified statically by reading OTDirect.ino (the :2003 origin choice, the :1375 reply gate, the :1403 origin reset, applyOverrides' modified flag). Needs a thermostat on the OTGW32 slave terminals for the hardware proof; the bench currently has none.
<!-- SECTION:NOTES:END -->
