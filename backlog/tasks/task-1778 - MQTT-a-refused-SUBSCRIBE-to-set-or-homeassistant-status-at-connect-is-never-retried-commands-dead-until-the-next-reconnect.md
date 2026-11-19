---
id: TASK-1778
title: >-
  MQTT: a refused SUBSCRIBE to set/# or homeassistant/status at connect is never
  retried (commands dead until the next reconnect)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-10T14:26:00Z'
updated_date: '2026-11-16T09:35:00Z'
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
- [x] #2 Fix: a refused command or homeassistant/status SUBSCRIBE is retried (bounded, e.g. next loop pass / when the largest block recovers); same reproduction shows commands work
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
PSRAM nuance (research 2026-11-14): espMqttClient compares max(ESP.getMaxAllocHeap(), ESP.getMaxAllocPsram()) (libraries/espMqttClient/src/Helpers.h:18) with EMC_MIN_FREE_MEMORY 16384 (Config.h:31-32, Packets/Packet.cpp:315). On a board with PSRAM the largest PSRAM block stays above 16 KB, so the refusal practically only happens on boards without PSRAM (the ADR-222 class; TASK-1757's 4.2 KB was such a unit). setCleanSession(true) (MQTTstuff.ino:1516): a refused SUBSCRIBE left commands dead until the next reconnect.

Fix (local, no build: coordinator memory freeze): TASK-1778 helper block next to onMqttConnect(): mqttSubscribeCommandTopic(), pending flags for the command topic and homeassistant/status set from the subscribe results at connect, mqttRetryPendingSubscribes() called from the MQTT_STATE_IS_CONNECTED branch (forward-declared), at most one attempt per second until both succeed. onMqttConnect() still queues birth + two subscriptions. Evidence: tests/test_mqtt_subscribe_retry.py (the real subscribe block + the real helper; MQTTclient stub refuses while the modelled heap is low): OLD (origin/dev) red: refused at connect, both topics stay unsubscribed after the heap is back; FIX green: both subscribed once the heap recovers, accepted-at-connect does 2 subscribes and nothing more, a 10 s refusal makes 22 attempts (1/s per topic). tests/test_mqtt_connect_staging.py now includes the real helper block (9/9 PASS), test_mqtt_connect_probe 5 PASS, static_integrity, ino_prototype_hoisting PASS; evaluate 85/0. AC1 ticked (host reproduction on the real path). AC2 after the build: bench on a unit WITHOUT PSRAM if one exists (09 knows from device/info), else host + a check on a PSRAM unit that the retry never fires (no extra SUBSCRIBEs, no log spam) and the no-PSRAM case as a field note.

2026-11-16 10:25-10:33 bench 09 on .61 (OTGW32, no PSRAM, master mode, SAT off), 2.0.0-beta.341+3cb72c3 (contains 7241ad6a3): POST /ReBoot for a fresh MQTT connect (bootcount 844 -> 845, Software reset, mqttconnected true 18.5 s after the request). Commands over MQTT after that connect, published by mosquitto_pub to OTGW/set/otgw-1020BA21B4F8/command: BS=19 -> /api/v2/otdirect/overrides write {msgid 16, value 4864 = 0x1300}; TT=20.5 -> {16, 5248 = 0x1480}; TT=0 -> {16, 4864}; BS=0 -> write list empty. So the set/# subscription was live after the connect. No crash or panic, heap 56.5 KB, settings.ini byte-identical afterwards. Not observable on the bench: the SUBSCRIBE packets themselves and the 'FAILED, retried while connected' line are MQTTDebugTf output, off unless MQTT debug is on, so the absence of extra SUBSCRIBEs was not counted here; the forced refusal (largest block below 16 KB at connect) was not reproduced. Those paths rest on tests/test_mqtt_subscribe_retry.py (22 attempts in a 10 s refusal, 2 subscribes when accepted; red on the old code). Side observation: a telnet client that connects during the first seconds of boot received only the banner and no further log lines for 10 min; a client connecting after boot gets the normal stream. Evidence: 09 scratchpad closeout/mq/ (run.log, mq1778.py) and mq2/.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: reproduced on the real onMqttConnect path in tests/test_mqtt_subscribe_retry.py: a refused set/# SUBSCRIBE left commands dead on the old code. AC2: fix 7241ad6a3 retries a refused command or homeassistant/status SUBSCRIBE once per second while connected (mqttRetryPendingSubscribes, MQTTstuff.ino:1873); host test: 22 attempts during a 10 s refusal, 2 subscribes when accepted, red on the old code; bench on .61 beta.341: after a fresh connect MQTT set/<node>/command BS=19 / TT=20.5 / TT=0 / BS=0 all reached the gateway (override list follows), no regression. Not verified on hardware: the refusal itself (needs < 16 KB largest block at connect) and the SUBSCRIBE count (MQTT debug off).
<!-- SECTION:FINAL_SUMMARY:END -->
