---
id: TASK-873
title: >-
  fix(mqtt): re-resolve broker DNS on reconnect (F2) — recover from boot-time
  name-resolution failure
status: To Do
assignee: []
created_date: '2026-06-15 14:28'
labels: []
dependencies: []
ordinal: 89000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MQTT review F2 (HIGH). WiFi.hostByName()+setServer() run ONLY in MQTT_STATE_INIT (MQTTstuff.ino:1085-1095); the state machine never returns to INIT on a normal reconnect (ERROR->WAIT_FOR_RECONNECT->TRY_TO_CONNECT). So if DNS fails at the single INIT pass (default broker homeassistant.local, mDNS not yet up at boot), setServer() is never called -> espMqttClient connects to _host=nullptr forever -> device permanently MQTT-unavailable in HA until a WiFi bounce or reboot. Likely root cause of George's 'MQTT unavailable' field report. Fix: on a failed/lost connection, re-run WiFi.hostByName()+setServer() (e.g. route back through INIT, or re-resolve in the reconnect path) so a transient boot-time DNS miss recovers; also picks up a DHCP-driven broker IP change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On reconnect after a DNS failure, hostByName+setServer re-run and the client connects once the name resolves
- [ ] #2 A device booted before its resolver is up self-recovers without WiFi bounce/reboot
- [ ] #3 Build green 3 targets; evaluate.py --quick no new failures
- [ ] #4 Field-validation: induce a boot-time DNS miss, confirm HA goes available after the resolver comes up
<!-- AC:END -->
