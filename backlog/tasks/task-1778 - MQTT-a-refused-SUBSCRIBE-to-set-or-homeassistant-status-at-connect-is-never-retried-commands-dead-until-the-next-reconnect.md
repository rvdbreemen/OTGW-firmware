---
id: TASK-1778
title: >-
  MQTT: a refused SUBSCRIBE to set/# or homeassistant/status at connect is never
  retried (commands dead until the next reconnect)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-10T14:26:00Z'
updated_date: '2026-11-14T18:40:00Z'
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
- [x] #1 Reproduce: force a refused SUBSCRIBE at connect (host test on the real onMqttConnect path, or bench) and show that set/# commands are ignored afterwards on the old code
- [ ] #2 Fix: a refused command or homeassistant/status SUBSCRIBE is retried (bounded, e.g. next loop pass / when the largest block recovers); same reproduction shows commands work
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
PSRAM nuance (research 2026-11-14): espMqttClient compares max(ESP.getMaxAllocHeap(), ESP.getMaxAllocPsram()) (libraries/espMqttClient/src/Helpers.h:18) with EMC_MIN_FREE_MEMORY 16384 (Config.h:31-32, Packets/Packet.cpp:315). On a board with PSRAM the largest PSRAM block stays above 16 KB, so the refusal practically only happens on boards without PSRAM (the ADR-222 class; TASK-1757's 4.2 KB was such a unit). setCleanSession(true) (MQTTstuff.ino:1516): a refused SUBSCRIBE left commands dead until the next reconnect.

Fix (local, no build: coordinator memory freeze): TASK-1778 helper block next to onMqttConnect(): mqttSubscribeCommandTopic(), pending flags for the command topic and homeassistant/status set from the subscribe results at connect, mqttRetryPendingSubscribes() called from the MQTT_STATE_IS_CONNECTED branch (forward-declared), at most one attempt per second until both succeed. onMqttConnect() still queues birth + two subscriptions. Evidence: tests/test_mqtt_subscribe_retry.py (the real subscribe block + the real helper; MQTTclient stub refuses while the modelled heap is low): OLD (origin/dev) red: refused at connect, both topics stay unsubscribed after the heap is back; FIX green: both subscribed once the heap recovers, accepted-at-connect does 2 subscribes and nothing more, a 10 s refusal makes 22 attempts (1/s per topic). tests/test_mqtt_connect_staging.py now includes the real helper block (9/9 PASS), test_mqtt_connect_probe 5 PASS, static_integrity, ino_prototype_hoisting PASS; evaluate 85/0. AC1 ticked (host reproduction on the real path). AC2 after the build: bench on a unit WITHOUT PSRAM if one exists (09 knows from device/info), else host + a check on a PSRAM unit that the retry never fires (no extra SUBSCRIBEs, no log spam) and the no-PSRAM case as a field note.
<!-- SECTION:NOTES:END -->
