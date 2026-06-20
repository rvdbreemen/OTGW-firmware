---
id: TASK-873
title: >-
  fix(mqtt): re-resolve broker DNS on reconnect (F2) — recover from boot-time
  name-resolution failure
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-15 14:28'
updated_date: '2026-06-20 10:56'
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
- [x] #1 On reconnect after a DNS failure, hostByName+setServer re-run and the client connects once the name resolves
- [x] #2 A device booted before its resolver is up self-recovers without WiFi bounce/reboot
- [ ] #3 Build green 3 targets; evaluate.py --quick no new failures
- [ ] #4 Field-validation: induce a boot-time DNS miss, confirm HA goes available after the resolver comes up
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
F2: MQTT_STATE_WAIT_FOR_RECONNECT now re-enters MQTT_STATE_INIT so WiFi.hostByName()+setServer() re-run each reconnect cycle.
Implemented on branch claude/mqtt-reliability-phase3 (off feature-2.0.0-esp32s3-async). evaluate.py --quick green (0 failures). ESP32 build NOT verifiable in this container (network policy blocks PlatformIO framework-arduinoespressif32 download); build + field ACs left for maintainer verification.

Audit wp0vjoo5s: F2 DNS re-resolve-on-reconnect code is complete and live on HEAD (alpha.224). AC#3 build receipt now satisfied: 3-target build green at alpha.224+cdc4ec7 (after SimpleTelnet submodule fix -> 7013fdc3): esp32/esp32-classic/esp32-combo all SUCCESS, evaluate.py --quick green (0 fail/1 warn/98.6%). Audit wp0vjoo5s 2026-06-20. (note: AC#3 wording 'esp8266/esp32/esp32-classic' is stale -> branch dropped esp8266; current 3 targets are esp32/esp32-classic/esp32-combo). Remaining: AC#4 field gate = live broker + induced boot-time DNS miss + HA observes available-after-resolver-up. Moving to In Review pending that field validation.
<!-- SECTION:NOTES:END -->
