---
id: TASK-3000
title: >-
  MQTT: publish the per-MsgID OT support table (boiler and thermostat,
  read/write) retained, like /api/v2/otgw/ot-support
status: To Do
assignee: []
created_date: '2026-11-21T10:20:00Z'
updated_date: '2026-11-21T10:23:00Z'
labels:
  - mqtt
  - feature
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1125000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter, #alpha-testing 2026-11-21 (msg 1573643085097738272): 'could the per-MsgID support info from GET /api/v2/otgw/ot-support also be published retained on MQTT, for both boiler and thermostat (read/write: ack, unknown, never seen)? Right now MQTT only has otgw-firmware/boiler/unsupported_msgids. My Homey app is MQTT-only, and with the full table it could offer only the controls a setup supports (e.g. SH=/SW=/MW=, TT= via MsgID 9). Something like otgw-firmware/boiler/supported_msgids and otgw-firmware/thermostat/sent_msgids, in the same CSV format.' Robert released and prioritised it (msg 1573643541412839531): 'prioritize this feature request. Publish on MQTT requested information.' Existing: otgw-firmware/boiler/unsupported_msgids, retained CSV (MQTTstuff.ino:2374; TASK-1781 empty-at-connect fix). Check ADR-101 (flat per-value topics) and ADR-088 (burst windowing) for the topic shape; a CSV list per category matches the existing topic. Data source is the same as /api/v2/otgw/ot-support.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Retained MQTT topics publish, for the boiler, the MsgIDs with read ack, write ack and unknown (CSV, same format as unsupported_msgids), and for the thermostat the MsgIDs it sent as read and as write; the values match GET /api/v2/otgw/ot-support on the same unit
- [ ] #2 Topics update on change and after an MQTT reconnect without overwriting a retained value with an empty one before the data is known (TASK-1781 lesson)
- [ ] #3 Topic names and payloads documented in the MQTT docs; gruijter told the topic names in #alpha-testing
- [ ] #4 Host test on the real publish code red before / green after; evaluate --quick no new FAIL; bench check on a unit via the NAS broker
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 (92): answered gruijter's follow-up (msg 1573644409688432691 -> 1573644897041391718). Facts from OTGW-Core.ino:6217-6290: the boiler bitmaps take every genuine B answer, whoever sent the request (T from the thermostat or R from the gateway, so SAT's own requests count too); bLocalAnswer and bReplayed frames do not count. The thermostat bitmaps take only real T frames (TASK-1612/1895). The bitmaps are persisted (/ot-boiler.json, /ot-thermo.json), so the acked lists only grow; an unsupported verdict can be retracted by a later genuine ack (TASK-1084).
<!-- SECTION:NOTES:END -->
