---
id: TASK-110
title: 'SAT Audit C2: OpenTherm message handling differences'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:52'
updated_date: '2026-04-09 05:24'
labels:
  - audit
  - sat
  - phase-3
  - otgw32
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify that OT message handling (MsgIDs relevant for SAT) works consistently on both ESP8266 and OTGW32. Check message timing, retry logic and error handling.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All SAT-relevant OT MsgIDs listed and verified
- [x] #2 Message timing and retry logic compared per platform
- [x] #3 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audited OT message handling differences between ESP8266/PIC and OTGW32/OTDirect platforms.

All SAT-relevant OT MsgIDs confirmed in OTcurrentSystemState struct and populated on both platforms:
- MsgID 0: Statusflags (MasterStatus + SlaveStatus) — populated by processOT() on both platforms
- MsgID 1: TSet — populated on both
- MsgID 14: MaxRelModLevelSetting — populated on both
- MsgID 17: RelModLevel — populated on both
- MsgID 18: CHPressure — populated on both
- MsgID 24: Tr (room temperature) — on ESP8266 from PIC-bridged thermostat WRITE; on OTGW32 from OTDirect slave listener + MQTT-routed via otdMqttSetRoomTemp (TASK-185)
- MsgID 25: Tboiler — populated on both
- MsgID 27: Toutside — populated on both
- MsgID 28: Tret — populated on both
- MsgID 49: MaxTSetUBMaxTSetLB / MaxTSet (MsgID 57) — populated on both

Command path: SAT calls addCommandToQueue() which routes to PIC queue (ESP8266) or handleOTDirectCommand() (OTGW32). Both ultimately call processOT() on the response, so MQTT/REST/WebSocket stacks are identical. No timing or retry logic differences found in SAT code itself.

Issue found:
- TASK-198: MsgID 24 (Tr) source differs by platform; the MQTT-routed room temp path on OTGW32 needs explicit test confirmation and documentation at satGetRoomTemp().
<!-- SECTION:FINAL_SUMMARY:END -->
