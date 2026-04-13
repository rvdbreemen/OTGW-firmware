---
id: TASK-242
title: 'Fix: OTGW flapping offline/online with serial overrun and MQTT throttle drops'
status: In Progress
assignee:
  - '@number3nl'
created_date: '2026-04-10 20:34'
updated_date: '2026-04-10 21:37'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'Discord #beta-testing, user crashevans, 2026-04-10'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by crashevans in #beta-testing (2026-04-10). When many MQTT entities are enabled, the OTGW firmware appears to get into a processing backlog causing: repeated OTGW availability flaps (offline/online), MQTT throttle drops (40, 9, 7 msgs at a time), and at least one Serial Overrun event. The pattern suggests a throughput/buffering issue when MQTT output and debug logging get busy simultaneously. Reporter is on HA OS 2026.4.1, using built-in MQTT integration, OTGW beta firmware + PIC 6.6, with many OTGW MQTT entities enabled. Log attached to Discord message.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OTGW connection remains stable (no offline/online flapping) under normal operation with many MQTT entities enabled
- [x] #2 No serial overrun events in telnet log during normal operation
- [ ] #3 MQTT throttle drops reduced or eliminated under normal load
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Root cause identified from crashevans logs (malformed_packets.txt + otgw.mqtt.log):
- When MQTT disconnects, every OT frame (5-10/sec) triggered sendMQTTData() calls
- Each call generated unconditional DebugTln() + PrintMQTTError() output
- Result: 50+ telnet debug messages in 0.4 seconds
- This blocked the main loop, causing UART 64-byte RX buffer to overflow
- Serial Overrun -> OTGW offline/online flapping

Fix implemented in v1.3.10-beta on branch fix-mqtt-disconnect-serial-overrun:
- Removed verbose per-publish error logging from sendMQTTData() and sendMQTTStreaming()
- Three hot-path functions fixed (lines 939, 978, 1064 in MQTTstuff.ino)
- Build successful
<!-- SECTION:NOTES:END -->
