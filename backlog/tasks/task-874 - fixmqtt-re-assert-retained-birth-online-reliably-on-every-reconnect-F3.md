---
id: TASK-874
title: >-
  fix(mqtt): re-assert retained birth 'online' reliably on every (re)connect
  (F3)
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-15 14:29'
updated_date: '2026-06-20 10:56'
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

Audit wp0vjoo5s: F3 birth-online re-assert code complete and live on HEAD (publishBirthOnline() bypasses heap gate at MQTTstuff.ino:1773-1780, called on CONNACK :1306, re-asserted every 300s :1063/:1187). AC#3 build receipt now satisfied: 3-target build green at alpha.224+cdc4ec7 (after SimpleTelnet submodule fix -> 7013fdc3): esp32/esp32-classic/esp32-combo all SUCCESS, evaluate.py --quick green (0 fail/1 warn/98.6%). Audit wp0vjoo5s 2026-06-20. Remaining: AC#4 field gate = live broker + HA + induced heap pressure to confirm avty_t flips offline->online and self-heals within 300s. Moving to In Review pending that field validation.
<!-- SECTION:NOTES:END -->
