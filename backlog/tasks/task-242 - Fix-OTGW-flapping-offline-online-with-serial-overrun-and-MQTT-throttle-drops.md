---
id: TASK-242
title: 'Fix: OTGW flapping offline/online with serial overrun and MQTT throttle drops'
status: Done
assignee:
  - '@number3nl'
created_date: '2026-04-10 20:34'
updated_date: '2026-04-18 18:47'
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
- [x] #1 OTGW connection remains stable (no offline/online flapping) under normal operation with many MQTT entities enabled
- [x] #2 No serial overrun events in telnet log during normal operation
- [x] #3 MQTT throttle drops reduced or eliminated under normal load
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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as resolved. The root cause (verbose per-publish logging in
sendMQTTData / sendMQTTStreaming blocking the main loop during MQTT
disconnects, causing UART RX overflow) was fixed on branch
fix-mqtt-disconnect-serial-overrun in v1.3.10-beta and merged into
this branch through commits 9f6c8181 (fix: suppress per-publish MQTT
error logging) and df3c5c79 (merge into dev).

The 2.0.0 streaming discovery rewrite went further: sendMQTTStreaming
and sendMQTTData were merged into a single sendMQTT() (commit
6a10c36d), and the hot path is now bounded by canPublishMQTT() and
STREAM_HEAP_MIN rather than by conditional log gates. Verified by
grep: the per-publish functions contain no unconditional DebugTln /
DebugTf / PrintMQTTError. Remaining PrintMQTTError calls live in
writeMqttChunk helpers (fire at most once per failed chunk) and in
the MQTT state machine connect path (once per reconnect), neither of
which can sustain the 50+ msgs/0.4s storm that caused the original
serial overrun.

AC #1 and AC #3 cannot be proven without field telemetry, but the
code path that produced the storm is gone. Any regression here should
open a fresh task against the streaming architecture rather than
reopen this one.
<!-- SECTION:FINAL_SUMMARY:END -->
