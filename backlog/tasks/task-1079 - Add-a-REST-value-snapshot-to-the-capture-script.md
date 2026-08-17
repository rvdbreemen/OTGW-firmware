---
id: TASK-1079
title: Add a REST value snapshot to the capture script
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-17 20:08'
updated_date: '2026-08-17 20:10'
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
- [ ] #1 The snapshot runs once, after the capture loop has stopped, and never during the live capture
- [ ] #2 It records the firmware's in-RAM value for a curated set of OpenTherm message ids, including 56 (TdhwSet)
- [ ] #3 It records otmonitor, boiler-support and device info
- [ ] #4 No endpoint carrying credentials is captured
- [ ] #5 A device that is offline or slow produces a recorded per-endpoint error line, never a hang or an aborted capture
- [ ] #6 The snapshot is merged into the single upload transcript like the other logs
<!-- AC:END -->
