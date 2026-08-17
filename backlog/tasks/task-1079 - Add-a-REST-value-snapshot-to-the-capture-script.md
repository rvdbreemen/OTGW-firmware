---
id: TASK-1079
title: Add a REST value snapshot to the capture script
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-17 20:08'
updated_date: '2026-08-17 20:13'
labels:
  - tooling
  - diagnostics
dependencies: []
priority: medium
ordinal: 181000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
A capture currently shows what crosses the OT bus and what reaches MQTT, but not what the firmware itself believes. TASK-1075 stalled precisely there: the reporter's log proved OTGW/value/<id>/TdhwSet is never published because MsgID 56 never appears on his bus, but it could not answer whether the firmware still holds a last-known value of 60. That single fact decides the fix, and it needed a REST call nobody had captured.

Add a snapshot of the firmware's own state to the capture, taken once after the live capture stops so it cannot perturb the measurement.

Endpoints worth capturing: /api/v2/otgw/otmonitor (status bits with per-value epoch), /api/v2/otgw/boiler-support (which ids the boiler does not implement), /api/v2/device/info, and /api/v2/otgw/messages/<id> for a curated id list covering the setpoint and temperature family.

Deliberately excluded: /api/v2/settings. It carries the MQTT broker credentials and the transcript is a file reporters upload to a public Discord channel.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The snapshot runs once, after the capture loop has stopped, and never during the live capture
- [x] #2 It records the firmware's in-RAM value for a curated set of OpenTherm message ids, including 56 (TdhwSet)
- [x] #3 It records otmonitor, boiler-support and device info
- [x] #4 No endpoint carrying credentials is captured
- [x] #5 A device that is offline or slow produces a recorded per-endpoint error line, never a hang or an aborted capture
- [x] #6 The snapshot is merged into the single upload transcript like the other logs
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-17: implemented as Invoke-RestSnapshot, called once after the capture loop stops and before the transcript merge. Endpoints: /api/v2/device/info, /api/v2/otgw/otmonitor, /api/v2/otgw/boiler-support, and /api/v2/otgw/messages/<id> for 13 curated ids (1, 9, 14, 16, 17, 18, 24, 25, 26, 28, 48, 56, 57).

Why a curated list: there is no bulk value endpoint on this line. The web UI reads OT values over the WebSocket, and handleOtgw requires an explicit id, so probing all 128 ids would mean 128 sequential HTTP requests against an ESP8266 for little added value.

/api/v2/settings is deliberately excluded: it carries the MQTT broker credentials and the transcript is a file testers upload to a public Discord channel.

Each request is bounded at 5s and every failure is written as an ERROR line, so an offline or slow device costs a line rather than the capture the tester just produced. Opt out with -SkipRestSnapshot.

Verified against the bench device: exit 0, REST SNAPSHOT section present in the merged transcript with HTTP 200 bodies, rest-snapshot.log cleaned up after merging like the other intermediates.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Adds a post-capture REST snapshot so a transcript records what the firmware itself believes, not only what crossed the OT bus and what reached MQTT.

TASK-1075 stalled exactly on that gap: a reporter's capture proved the DHW setpoint is never published because MsgID 56 never appears on his bus, but could not say whether the firmware still holds a last-known value. That one fact decides the fix and no capture contained it.

The snapshot runs once, after the live capture stops, so it adds no load during the measurement. It records device info, otmonitor (status bits with per-value epoch), boiler-support (ids the boiler does not implement) and the in-RAM value of 13 curated OpenTherm message ids covering the setpoint and temperature family. Requests are bounded at 5s and failures are recorded per endpoint rather than aborting anything. Opt out with -SkipRestSnapshot.

/api/v2/settings is deliberately not captured: it carries MQTT broker credentials, and testers upload this transcript to a public channel.

Verified against a live device: REST SNAPSHOT section present in the merged transcript, exit 0.
<!-- SECTION:FINAL_SUMMARY:END -->
