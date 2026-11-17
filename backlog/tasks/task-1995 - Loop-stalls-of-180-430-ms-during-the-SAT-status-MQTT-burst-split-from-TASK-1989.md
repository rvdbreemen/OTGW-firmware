---
id: TASK-1995
title: >-
  Loop stalls of 180-430 ms during the SAT status MQTT burst (split from
  TASK-1989)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-16T06:28:00Z'
updated_date: '2026-11-17T00:26:00Z'
labels:
  - performance
dependencies: []
priority: low
ordinal: 1019000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1989 (out of its scope, coordinator 2026-11-16). In gruijter's beta.250 capture 34 of the 92 [loop-stall] lines (200-600 ms) sit in an MQTT burst: satSendStatus plus the sat/* settings topics at :x6:25 / :x6:55, with an internal gap of 180-400 ms between addCommandToQueue and satControlLoopRun inside the window. Not a file write, so the TASK-1989 file-write work does not touch it. Find what runs in that step with the slowest-step instrumentation of TASK-1989 (92c914047) on a capture, then decide fix or keep.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A capture on a build with the slowest-step stall line names the step of the burst stalls
- [ ] #2 Fix with old-vs-fix numbers, or a recorded reason why it stays (bounded, harmless)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-17 d3 (analysis + instrumentation, no behaviour change). The slowest-step stall line already exists on origin: OTGW-firmware.ino:1185-1190 '[loop-stall] %lu ms gap, slowest step %u->%u %lu ms' (TASK-1989, 92c914047), with probes at loop/doBackgroundTasks/handleMQTT/satControlLoop granularity (MQTTstuff.h CPROBE_* enum). Gap: the whole of satControlLoopRun() was one step (CPROBE_DISCOVERY or the previous probe -> CPROBE_SAT_RUN). Added (local, goes into d3's next build): CPROBE_SAT_PUB_BEGIN / CPROBE_SAT_PUB_END (MQTTstuff.h, appended, numbered 18/19) around satPublishMQTT() (SATcontrol.ino, after its gates; END from a scope guard on every return), only under #ifdef OTGW_CONN_PROBE. All six satPublishMQTT() callers sit in satControlLoopRun() (loop task). Read of the burst path, per step, what can block: (1) publishIfChanged*() (SATmqttPublish.cpp:64-81): compare, satOutboxFull() gate (depth vs SAT_MQTT_OUTBOX_LIMIT, connect pace hold), dtostrf into 16 B: no blocking. (2) sendMQTTData() (MQTTstuff.ino:1979-2014): snprintf_P topic, MQTTDebugTf (a telnet write only with the MQTT debug flag on; a slow telnet client could then block), diagRec append (only when recording), mqttPublishRaw(). (3) mqttPublishRaw() (MQTTstuff.ino:344-348): MQTTclient.publish() = espMqttClient 1.7.2 vendored, UseInternalTask::NO (MQTTstuff.ino:217): EMC_SEMAPHORE_TAKE (xSemaphoreTake portMAX_DELAY, Helpers.h:16) + _addPacket (heap alloc + memcpy into the Outbox), no socket I/O; the semaphore can only wait if another task publishes at the same time (not checked which tasks do). (4) No flash write and no String in this path. (5) The socket writes happen later in MQTTclient.loop() -> _sendPacket() (MqttClient.cpp:249-250) through the sync transport, which can block on a full TCP send window: that is step CPROBE_MQTT_PRE_LOOP -> CPROBE_MQTT_POST_LOOP, not the SAT step. Hypothesis to test with the capture (not verified): the burst stall lands in the MQTT loop step (flushing ~50 queued packets) or in loopMQTTDiscovery (step TIMERS -> DISCOVERY), rather than in satPublishMQTT itself. AC1 needs a capture on the build that carries these probes.
<!-- SECTION:NOTES:END -->
