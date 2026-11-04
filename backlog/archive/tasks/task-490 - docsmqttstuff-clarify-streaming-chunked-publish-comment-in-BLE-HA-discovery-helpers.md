---
id: TASK-490
title: >-
  docs(mqttstuff): clarify "streaming chunked publish" comment in BLE
  HA-discovery helpers
status: Done
assignee: []
created_date: '2026-04-30 05:42'
updated_date: '2026-04-30 05:47'
labels:
  - esp32
  - ble
  - sat
  - mqtt
  - docs
  - code-review
dependencies:
  - TASK-488
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Problem

Code-review finding #2 on the TASK-488 BLE HA-discovery helpers.

`bleSensorPublishOneDiscovery()` in `src/OTGW-firmware/MQTTstuff.ino`
documents itself as "Streaming chunked publish (no full-message
buffer)", but the implementation builds the full payload in a single
`char payload[768]` stack buffer and writes it as one
`writeMqttChunk()` call. That is buffered-then-atomic, not chunked.

The behaviour itself is correct and ADR-077-conformant
(`canPublishMQTT()` + `MQTT_DISCOVERY_HEAP_MIN` gates bound heap
pressure, payload size is bounded at 768 bytes), but the comment
misleads future maintainers who might expect a true two-pass
measure-then-stream flow.

## Fix

Adjust the comment to reflect what the code actually does:

```
// Single-buffer publish via the streaming primitives. Payload is
// bounded at 768 bytes — same heap-safe shape ADR-077 prescribes,
// but without the two-pass measure dance because all four discovery
// configs fit comfortably inside the bound.
```

Optional follow-up (out of scope here): if the bound is ever
exceeded, switch to a true two-pass measure flow.

## Validation

- No code change beyond comment text.
- evaluate.py / build still clean.
- Comment renders accurately in code-review diff.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Comment block above `bleSensorPublishOneDiscovery()` no longer claims `chunked publish (no full-message buffer)` since the implementation does buffer the full payload
- [x] #2 Replacement comment notes the 768-byte bound and the ADR-077-conformance rationale
- [x] #3 Builds remain clean
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## TASK-490 — Comment cleanup for bleSensorPublishOneDiscovery

The comment block above `bleSensorPublishOneDiscovery()` claimed
"Streaming chunked publish (no full-message buffer)" while the
implementation actually buffers the full payload in a 768-byte stack
allocation and writes it as one `writeMqttChunk` call. Comment
rewritten to describe the actual shape (single-buffer publish via the
streaming primitives, payload bounded at 768 bytes) and to spell out
the ADR-077-conformance rationale (`canPublishMQTT()` and
`MQTT_DISCOVERY_HEAP_MIN` gates bound heap pressure even without a
two-pass measure).

### Change
Comment-only edit in `src/OTGW-firmware/MQTTstuff.ino`. No code
behaviour changed.

### Verification
ESP32 build SUCCESS (1m43s incremental, combined with TASK-489 +
TASK-491 in the same build cycle).

Pushed in commit `67ad53cf` on `feature-dev-2.0.0-otgw32-esp32-sat-support`.
<!-- SECTION:FINAL_SUMMARY:END -->
