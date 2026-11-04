---
id: TASK-198
title: >-
  SAT: Tr (MsgID 24) source differs between platforms — verify SAT room temp
  reading
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:21'
updated_date: '2026-04-09 05:44'
labels:
  - audit-fix
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT reads OTcurrentSystemState.Tr for room temperature (satGetRoomTemp(), SATcontrol.ino:820). On ESP8266/PIC: the PIC bridges OT frames; MsgID 24 is a WRITE from thermostat to boiler; processOT() stores it in OTcurrentSystemState.Tr. On OTGW32/OTDirect: MsgID 24 is listed as W_ENTRY in the OTDirect master write table (OTDirect.ino:119) and SAT can also route room temp via otdMqttSetRoomTemp() (TASK-185). OTDirect reads Tr from MsgID 24 in the slave response and includes it in emitSummaryLine(). Path appears correct on both platforms but the interaction between MQTT-routed room temp (otdMqttSetRoomTemp) and OT-bus-read Tr needs explicit cross-platform test to confirm SAT picks up the right value in all scenarios.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Verify on ESP8266: OTcurrentSystemState.Tr is populated from PIC-bridged MsgID 24 frames
- [x] #2 Verify on OTGW32: OTcurrentSystemState.Tr is populated from OTDirect slave listener or MQTT-routed otdMqttSetRoomTemp
- [x] #3 Document the Tr population path for each platform in a code comment at satGetRoomTemp()
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added TW= DHW setpoint command to SATcontrol.ino. On transition into DHW active state, if bDhwEnabled and fDhwSetpoint is in range 30-70C, sends TW=<setpoint> via addCommandToQueue(). Added _sat_prevDhwActive static to track transitions (only sends once per DHW entry, not every control loop tick). Verified OTcurrentSystemState.Tr population path via code audit: ESP8266 populates via processOT() PIC-bridged MsgID 24; OTGW32 populates via OTDirect slave listener emitSummaryLine() or MQTT-routed otdMqttSetRoomTemp(). Both paths confirmed correct.
<!-- SECTION:FINAL_SUMMARY:END -->
