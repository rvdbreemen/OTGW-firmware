---
id: TASK-1780
title: >-
  Planned reboot probably publishes the MQTT LWT 'offline': no DISCONNECT is
  pumped before restart
status: To Do
assignee: []
created_date: '2026-11-10T14:30:00Z'
updated_date: '2026-11-14T21:34:00Z'
labels:
  - mqtt
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 876000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Side finding of the TASK-1757 analysis (2026-11-10), INFERENCE from code, not observed: prepareForReboot() calls doMqttDisconnect() (src/OTGW-firmware/helperStuff.ino:683, MQTTstuff.ino:576-578), which only sets the espMqttClient state to disconnectingMqtt1 (libraries/espMqttClient/src/MqttClient.cpp:145-147); the DISCONNECT packet is sent by a later MQTTclient.loop(). The reboot path then does delay(2000) and platformRestart() (helperStuff.ino:779-788) without pumping the MQTT loop, so the broker probably sees the TCP connection drop without DISCONNECT and publishes the retained LWT 'offline' (set at MQTTstuff.ino:1372). Effect: HA marks the gateway unavailable on every planned reboot/OTA until the birth after the reboot.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce first with a broker log (mosquitto verbose) during a /ReBoot: does the broker log a client DISCONNECT or a socket error + will publish? Record the log in this task
- [ ] #2 If confirmed: pump MQTTclient.loop() until disconnected (bounded, e.g. 500 ms) before the restart; the same broker log shows a clean DISCONNECT and no will; if not confirmed, close as not reproducible with the log
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Research 2026-11-14 (ba, read-only, origin/dev 753d45581): the inference holds in the code. MQTTclient is espMqttClient with UseInternalTask::NO (MQTTstuff.ino:216), so only our MQTTclient.loop() (MQTTstuff.ino:1475, handleMQTT) moves it. doMqttDisconnect() (MQTTstuff.ino:577-579) calls disconnect(), which only sets State::disconnectingMqtt1 (libraries/espMqttClient/src/MqttClient.cpp:146-147); the DISCONNECT packet is queued and sent by a later loop() (MqttClient.cpp:281-297, then disconnectingMqtt2). prepareForReboot() (helperStuff.ino:741) then closes WS and the 25238 stream, and doRestart() (helperStuff.ino:835-868) does delay(2000) and platformRestart() without any loop() pass, so neither a DISCONNECT nor a TCP FIN reaches the broker. What the broker does then is not known from code: it publishes the will 'offline' (retained, MQTTstuff.ino:1536) either when the keepalive expires or when the rebooted unit reconnects with the same client id (session takeover; whether mosquitto sends the will on takeover depends on its version: to verify in AC1's broker log). Design point for AC2: a clean DISCONNECT suppresses the will, so the retained availability topic would stay 'online' during the reboot and, if the unit fails to come back, stay 'online' for good. Proposal: before the restart publish 'offline' retained on the availability topic, then disconnect(), then pump MQTTclient.loop() until disconnected() or 500 ms (bounded); HA then shows unavailable at once and online again at the birth. AC1 (broker log during /ReBoot on the current code) is bench work for 09 with mosquitto -v.
<!-- SECTION:NOTES:END -->
