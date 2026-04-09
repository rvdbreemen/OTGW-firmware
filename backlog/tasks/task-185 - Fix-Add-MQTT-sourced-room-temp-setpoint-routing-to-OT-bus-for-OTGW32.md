---
id: TASK-185
title: 'Fix: Add MQTT-sourced room temp/setpoint routing to OT bus for OTGW32'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:33'
updated_date: '2026-04-08 23:08'
labels:
  - audit-fix
  - otgw32
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OT-Thing supports routing MQTT-published room temperature and room setpoint values into OT bus writes (MsgID 24 = Tr, MsgID 16 = TrSet) via Sensor::SOURCE_MQTT. OTDirect currently only sends these values when set via PIC-compatible commands (TT=, OT=). A dedicated MQTT subscription path that feeds into the write cache would enable HA-driven room temperature reporting to the boiler — useful for weather compensation and room compensation algorithms.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTT topic subscription routes room temperature into MsgID 24 write cache
- [x] #2 MQTT topic subscription routes room setpoint into MsgID 16 write cache
- [x] #3 Implementation uses existing MQTT infrastructure, not a new broker connection
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. In handleMQTTcallback() in MQTTstuff.ino, add otgw32 sub-topic handler (same pattern as sat)
2. room_temp -> validate range, convert to f8.8, call enqueueWriteCommandOTD(24, value) via a new extern-callable function
3. room_setpoint -> validate range, convert to f8.8, call enqueueWriteCommandOTD(16, value)
4. Add extern declaration in OTDirect.ino for the callable function
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added MQTT-sourced room temperature and setpoint routing to OT bus for OTGW32.

Changes:
- Added otdMqttSetRoomTemp(float) and otdMqttSetRoomSetpoint(float) in OTDirect.ino
- Both functions validate range (-40 to 127C), convert to f8.8, and call enqueueWriteCommand() for MsgID 24 (Tr) and MsgID 16 (TrSet) respectively
- Forward declarations added to OTGW-firmware.h
- In handleMQTTcallback() (MQTTstuff.ino): added "otgw32" sub-topic handler (same pattern as "sat")
  - set/<nodeId>/otgw32/room_temp -> otdMqttSetRoomTemp()
  - set/<nodeId>/otgw32/room_setpoint -> otdMqttSetRoomSetpoint()
- Handler guarded by HAS_DIRECT_OT compile flag; non-OTGW32 builds get a debug message
- Uses existing MQTT subscription (MQTTSubNamespace/#) and callback infrastructure; no new broker connection
<!-- SECTION:FINAL_SUMMARY:END -->
