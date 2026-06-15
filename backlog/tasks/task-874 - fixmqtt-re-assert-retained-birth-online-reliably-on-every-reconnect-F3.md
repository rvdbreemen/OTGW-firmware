---
id: TASK-874
title: >-
  fix(mqtt): re-assert retained birth 'online' reliably on every (re)connect
  (F3)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-15 14:29'
updated_date: '2026-06-15 20:07'
labels: []
dependencies: []
ordinal: 90000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MQTT review F3 (HIGH). The retained availability birth sendMQTT(MQTTPubNamespace,'online') (MQTTstuff.ino:1292/1151) fires once on connect through the heap/throttle-gated sendMQTT with no retry, and nothing re-asserts it. The base namespace is the HA avty_t for every entity; LWT 'offline' is retained. If the one birth publish is dropped (HEAD_CRITICAL or async outbox full), retained availability stays 'offline' -> whole device 'unavailable' with no recovery while the link stays up. Fix: publish the birth via a path that bypasses the heap/throttle one-shot (or retries until queued), and/or periodically re-assert 'online' while connected.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Birth 'online' is published reliably on every (re)connect (retry-until-queued or gate-bypass)
- [x] #2 A dropped birth cannot strand HA availability at retained LWT offline (periodic re-assert OR guaranteed delivery)
- [ ] #3 Build green 3 targets; evaluate.py --quick no new failures
- [ ] #4 Field-validation: confirm HA availability flips to available promptly after reconnect under heap pressure
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
F3: added publishBirthOnline() that bypasses canPublishMQTT() heap gate; called from onMqttConnect + re-asserted every 300s in IS_CONNECTED.
Implemented on branch claude/mqtt-reliability-phase3 (off feature-2.0.0-esp32s3-async). evaluate.py --quick green (0 failures). ESP32 build NOT verifiable in this container (network policy blocks PlatformIO framework-arduinoespressif32 download); build + field ACs left for maintainer verification.
<!-- SECTION:NOTES:END -->
