---
id: TASK-490
title: >-
  docs(mqttstuff): clarify "streaming chunked publish" comment in BLE
  HA-discovery helpers
status: To Do
assignee: []
created_date: '2026-04-30 05:42'
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
- [ ] #1 Comment block above `bleSensorPublishOneDiscovery()` no longer claims `chunked publish (no full-message buffer)` since the implementation does buffer the full payload
- [ ] #2 Replacement comment notes the 768-byte bound and the ADR-077-conformance rationale
- [ ] #3 Builds remain clean
<!-- AC:END -->
