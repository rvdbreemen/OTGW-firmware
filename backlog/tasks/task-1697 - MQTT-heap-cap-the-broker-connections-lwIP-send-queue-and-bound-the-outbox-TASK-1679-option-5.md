---
id: TASK-1697
title: >-
  MQTT heap: cap the broker connection's lwIP send queue and bound the outbox
  (TASK-1679 option 5)
status: To Do
assignee: []
created_date: '2026-11-07T16:53:00Z'
labels:
  - mqtt
  - heap
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 812000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1679 (Robert 2026-11-07). Cause A/B on .61: with MQTT off the two-tab minimum is 42-47 KB, with MQTT on 13-26 KB. beta.123 diag: the espMqttClient outbox reaches 48-49 messages after every boot and holds 0-41 messages at the dip, plus up to 4.9 KB queued in lwIP on the broker pcb (uncapped, TCP_SND_BUF 5744). Apply platformTcpCapSendBuffer (2872 B) to the broker pcb and bound the outbox (count or bytes) so publishes beyond it are deferred or dropped by the existing ADR-088 burst machinery, not queued. Outbox byte size needs a library change in the vendored espMqttClient (ADR-186/187): ask Robert before touching it. A burst pause during file streams conflicts with ADR-088 (binding) and is out of scope.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Two-tab A/B on .61 with MQTT on (5 runs): minimum free heap higher than beta.123 (8.1-18.8 KB), no MQTT messages lost beyond the existing drop counters
- [ ] #2 hd_mqtt_queue_hwm bounded at the new limit; ADR-088 gates in evaluate.py green
<!-- AC:END -->
