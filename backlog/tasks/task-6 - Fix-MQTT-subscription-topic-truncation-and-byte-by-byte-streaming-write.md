---
id: TASK-6
title: Fix MQTT subscription topic truncation and byte-by-byte streaming write
status: Done
assignee:
  - '@claude'
created_date: '2026-03-12 20:51'
updated_date: '2026-03-12 21:47'
labels:
  - bug
  - mqtt
dependencies: []
references:
  - 'src/OTGW-firmware/MQTTstuff.ino:606'
  - 'src/OTGW-firmware/MQTTstuff.ino:798-804'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two related MQTT issues that can cause silent data loss:

1. **Subscription topic buffer too small** (MQTTstuff.ino:606): `char topic[100]` receives `MQTTSubNamespace` which can be up to `MQTT_NAMESPACE_MAX_LEN` (192 bytes). `strlcpy` silently truncates, causing subscription to wrong/partial topic. Device appears connected but silently misses incoming commands.

2. **Byte-by-byte streaming write** (MQTTstuff.ino:798-804): `sendMQTTStreaming()` writes one byte at a time in a loop via `MQTTclient.write(json[pos + i])`. PubSubClient's `write(const uint8_t*, size_t)` bulk overload exists and would be far more efficient. Current pattern causes unnecessary function call overhead and potential heap fragmentation from internal buffering.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Subscription topic buffer uses MQTT_TOPIC_MAX_LEN (200) instead of hardcoded 100
- [ ] #2 sendMQTTStreaming uses bulk write (write(buf, len)) instead of byte-by-byte loop
- [ ] #3 No functional change to MQTT behavior for topics that fit in 100 bytes
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed two MQTT issues:
1. Subscription topic buffer enlarged from char[100] to char[MQTT_TOPIC_MAX_LEN] (200 bytes) to prevent truncation with long topic prefixes.
2. Replaced byte-by-byte write loop in sendMQTTLargeJson with bulk MQTTclient.write(ptr, len) call — eliminates per-byte function call overhead for large JSON payloads.

Build passes.
<!-- SECTION:FINAL_SUMMARY:END -->
