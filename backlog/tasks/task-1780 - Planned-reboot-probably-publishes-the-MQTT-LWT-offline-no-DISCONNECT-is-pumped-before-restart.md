---
id: TASK-1780
title: >-
  Planned reboot probably publishes the MQTT LWT 'offline': no DISCONNECT is
  pumped before restart
status: To Do
assignee: []
created_date: '2026-11-10T14:30:00Z'
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
