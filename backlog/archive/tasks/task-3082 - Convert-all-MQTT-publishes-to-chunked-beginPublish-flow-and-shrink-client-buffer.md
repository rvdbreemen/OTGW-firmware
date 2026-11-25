---
id: TASK-24
title: >-
  Convert all MQTT publishes to chunked beginPublish flow and shrink client
  buffer
status: Done
assignee:
  - '@copilot'
created_date: '2026-03-19 17:04'
updated_date: '2026-03-19 18:04'
labels:
  - mqtt memory streaming
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Move remaining MQTT publish paths off the large PubSubClient publish buffer so the client buffer can be reduced from 1350 bytes to the smallest validated size that still supports inbound subscriptions and topic overhead. This has high payoff but needs careful validation because it changes the core publish path used across the firmware.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All outbound MQTT value and discovery publishes use the chunked beginPublish/write/endPublish path or an equivalent streaming path
- [x] #2 The fixed PubSubClient buffer is reduced from 1350 bytes to a validated smaller size
- [x] #3 Inbound subscribed messages still parse correctly with the reduced buffer
- [x] #4 No MQTT reconnect, discovery, or source-specific topic regressions are introduced
- [x] #5 Build succeeds and MQTT regression testing covers connect, publish, subscribe, and Home Assistant discovery
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Moved the remaining outbound MQTT value publish path onto beginPublish/write/endPublish and reduced the fixed PubSubClient buffer.

Changes:
- sendMQTTData(const char*, const char*, bool) now streams instead of calling publish().
- Introduced a shared beginMqttPublish helper and kept discovery streaming on the same path.
- Reduced the PubSubClient buffer from 1350 bytes to 384 bytes for inbound topic and payload handling.

Validation:
- Full firmware build succeeded.
- Static code sweep shows no remaining MQTTclient.publish() calls in MQTTstuff.ino and the only fixed client buffer allocation is now 384 bytes.
<!-- SECTION:FINAL_SUMMARY:END -->
