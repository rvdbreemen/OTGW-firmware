---
id: TASK-1699
title: 'HA discovery drip: one discovery message per tick (~1 s), not one ID per tick'
status: To Do
assignee: []
created_date: '2026-11-07T16:56:00Z'
labels:
  - mqtt
  - ha
  - heap
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 813000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07: drip out the HA discovery on boot one MQTT discovery message at a time, paced ~1 s apart, as intended on 1.x. Today (2.x and 1.x alike) loopMQTTDiscovery publishes one pending ID per tick every DISCOVERY_INTERVAL_NORMAL = 2 s (10 s under heap pressure; MQTTstuff.ino:2364-2365, 'one per tick'), but one ID can carry many entity configs: OTid 0 streams the status-bit binary sensors, two climate entities and all SAT switch/select/number entities in one tick (MQTTstuff.ino:2958-2975). beta.123 diag on .61: MQTT outbox high-water 48-49 messages after every boot (TASK-1679), estimated 4-10 KB. Change the drip to a per-entity cursor: one config per tick at ~1 s, ADR-088 burst windows and cooldown unchanged, ADR-077 streaming kept. Related: TASK-1697 (MQTT pcb cap/outbox bound).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Boot on .61: hd_mqtt_queue_hwm during the discovery drip <= a small bound (e.g. 3) instead of 48-49, and every entity still appears in HA (count before/after equal)
- [ ] #2 Host test: a pending OTid 0 publishes one entity config per tick (red on old code: many per tick)
- [ ] #3 ADR-088 gates in evaluate.py green; full discovery duration measured and documented
<!-- AC:END -->
