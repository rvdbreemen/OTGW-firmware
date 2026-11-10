---
id: TASK-1778
title: >-
  MQTT: a refused SUBSCRIBE to set/# or homeassistant/status at connect is never
  retried (commands dead until the next reconnect)
status: To Do
assignee: []
created_date: '2026-11-10T14:26:00Z'
labels:
  - mqtt
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 875000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Side finding of the TASK-1757 MQTT connect-burst analysis (2026-11-10). onMqttConnect subscribes once: set/# at src/OTGW-firmware/MQTTstuff.ino:1610-1615 logs a failure (PrintMQTTError) but does not retry; homeassistant/status at :1616 ignores the return value. espMqttClient refuses every SUBSCRIBE when the largest free block is below EMC_MIN_FREE_MEMORY = 16384 B (libraries/espMqttClient/src/Config.h:31-32, Packet.cpp:315-318), which the bench reaches in the connect window (TASK-1757: 4.2 KB). With a clean session the gateway then ignores every MQTT command (and the ADR-174 HA-online republish) until the next reconnect. Not reproduced on hardware yet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce: force a refused SUBSCRIBE at connect (host test on the real onMqttConnect path, or bench) and show that set/# commands are ignored afterwards on the old code
- [ ] #2 Fix: a refused command or homeassistant/status SUBSCRIBE is retried (bounded, e.g. next loop pass / when the largest block recovers); same reproduction shows commands work
<!-- AC:END -->
