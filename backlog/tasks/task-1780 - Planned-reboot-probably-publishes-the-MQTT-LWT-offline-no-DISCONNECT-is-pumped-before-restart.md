---
id: TASK-1780
title: >-
  Planned reboot probably publishes the MQTT LWT 'offline': no DISCONNECT is
  pumped before restart
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-10T14:30:00Z'
updated_date: '2026-11-20T10:59:00Z'
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
- [x] #1 Reproduce first with a broker log (mosquitto verbose) during a /ReBoot: does the broker log a client DISCONNECT or a socket error + will publish? Record the log in this task
- [x] #2 If confirmed: before the restart publish 'offline' retained on the availability topic, then disconnect() and pump MQTTclient.loop() until disconnected() or 500 ms (bounded); the same broker log shows the retained 'offline' and a clean DISCONNECT, and HA shows the gateway unavailable at once and online again at the birth; if not confirmed, close as not reproducible with the log
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Research 2026-11-14 (ba, read-only, origin/dev 753d45581): the inference holds in the code. MQTTclient is espMqttClient with UseInternalTask::NO (MQTTstuff.ino:216), so only our MQTTclient.loop() (MQTTstuff.ino:1475, handleMQTT) moves it. doMqttDisconnect() (MQTTstuff.ino:577-579) calls disconnect(), which only sets State::disconnectingMqtt1 (libraries/espMqttClient/src/MqttClient.cpp:146-147); the DISCONNECT packet is queued and sent by a later loop() (MqttClient.cpp:281-297, then disconnectingMqtt2). prepareForReboot() (helperStuff.ino:741) then closes WS and the 25238 stream, and doRestart() (helperStuff.ino:835-868) does delay(2000) and platformRestart() without any loop() pass, so neither a DISCONNECT nor a TCP FIN reaches the broker. What the broker does then is not known from code: it publishes the will 'offline' (retained, MQTTstuff.ino:1536) either when the keepalive expires or when the rebooted unit reconnects with the same client id (session takeover; whether mosquitto sends the will on takeover depends on its version: to verify in AC1's broker log). Design point for AC2: a clean DISCONNECT suppresses the will, so the retained availability topic would stay 'online' during the reboot and, if the unit fails to come back, stay 'online' for good. Proposal: before the restart publish 'offline' retained on the availability topic, then disconnect(), then pump MQTTclient.loop() until disconnected() or 500 ms (bounded); HA then shows unavailable at once and online again at the birth. AC1 (broker log during /ReBoot on the current code) is bench work for 09 with mosquitto -v.

AC2 reworded 2026-11-14 (coordinator agreed, no contract change): a clean DISCONNECT suppresses the will, so pumping only the DISCONNECT would leave the retained availability 'online' during the reboot and for good if the unit does not come back. Publishing 'offline' retained first gives the same availability semantics as the will, at once and without the broker's keepalive or takeover timing.

Fix (ba 2026-11-14, lane wt-ba-1780 on top of the 1937 stack): doMqttDisconnect() (MQTTstuff.ino) publishes 'offline' retained on MQTTPubNamespace (the will topic), calls disconnect(), then pumps MQTTclient.loop() with delay(1) until disconnected() or 500 ms. Host red/green: tests/test_mqtt_reboot_offline.py slices the real function against a stub of espMqttClient's state machine (publish queues, disconnect() arms DISCONNECT, loop() sends one packet per call, DISCONNECT only after the outbox is empty). Old (OTGW_MQTT_REV=HEAD): nothing reaches the broker, FAIL. Fix: 'PUBLISH .. offline retain qos0;DISCONNECT' in 4 ms / 2 loops; a dead broker gives up after 500 ms; not connected: nothing, 0 loops. All tests/test_mqtt*.py OK; evaluate --quick 0 FAIL 0 WARN. AC1 (RED broker log) is item 6 of the stack bench; AC2 GREEN needs this build: same mosquitto -v log shows the retained offline + DISCONNECT and no will.

Bench 09 2026-11-16, .61 (OT-Direct), SAT off, satsimulation false. LAN broker mosquitto 192.168.88.32:1883 with file logging and connection_messages true (information level, not -v); subscriber on the availability topic OTGW/value/<node>. Same reproduction (/ReBoot) on old vs fix.

OLD 2.0.0-beta.257+47f0ea3 (before 79c6c7ce0; doMqttDisconnect() was only MQTTclient.disconnect()), /ReBoot 16:58:18: the broker logs NOTHING at the reboot (no DISCONNECT, no socket error). The old session stays half-open until the new boot connects with the same client id: '16:58:36: Client OTGW1020BA21B4F8 [192.168.88.61:56549] disconnected: session taken over.' The availability topic gets 'offline' only at 16:58:36, from the broker's will on the takeover, immediately followed by 'online' (~18 s after the reboot). Confirmed: a planned reboot sends no DISCONNECT, and offline is only signalled late, by the will.

FIX 2.0.0-beta.353+3a14423 (contains 79c6c7ce0), /ReBoot 16:53:29: '16:53:30: Client OTGW1020BA21B4F8 [192.168.88.61:50153] disconnected.' (clean DISCONNECT). The availability topic gets the retained 'offline' at 16:53:30 (the device's own publish, +1 s), then 'online' at 16:53:47 on the reconnect. AC2: broker part and topic part shown; the HA UI (unavailable at once, online at the birth) was NOT observed, so AC2 stays open for the HA leg. Side effect of the downgrade, restored: 257 rewrote settings.ini without the OTDohpicdefault marker, so 353's TASK-1961 migration set OH=1 once (settingStuff.ino:682-686); set back with OH=0, and settings.ini is byte-identical to before. Evidence: 09 scratchpad t1780/ (res-old257.json, res-fix353.json, sub-*.txt) and broker/mosq-lan.log.

SUPPORTING EVIDENCE for AC2(b), NOT the HA observation (09, 2026-11-17 ~01:00). The retained discovery configs of .61 (e.g. homeassistant/sensor/otgw-1020BA21B4F8/boiler_TSet/config, boiler_status_master, boiler_status_slave) set avty_t 'OTGW/value/otgw-1020BA21B4F8' with no custom payloads, so HA uses its MQTT defaults payload_available 'online' / payload_not_available 'offline'. On that exact topic the fix (beta.353+3a14423) published the retained 'offline' at 16:53:30, +1 s after /ReBoot, with the broker logging a clean 'disconnected.', and 'online' at the reconnect at 16:53:47. The old beta.257 got 'offline' only from the will at the session takeover, ~18 s later. Inference: HA marks every .61 entity unavailable at +1 s and available at the reconnect. NOT observed in HA: the HA-rig run is planned for 17 Nov after the 08:00 beta, once commit charge allows and the Docker start is OK'd (coordinator). AC2 stays open.

2026-11-20 02:08 bench 09 on .61 (beta.401), AC2 firmware side: REST /ReBoot 02:08:05.737 -> retained 'offline' on OTGW/value/otgw-1020BA21B4F8 at 02:08:06.823 (before the restart) -> boot 02:08:22 -> 'online' birth 02:08:24.483. HA side (entity unavailable/available + HA log) still open, after 08:00 on the HA rig. Evidence: OTGW-field-data/bench-otgw32/2026-11-20_task1780-ac2-broker/avail.log

2026-11-20 bench (09, otgw-bench-tester), AC2 evidence. Broker side, .61 401+47d08b1, mosquitto_sub on OTGW/value/otgw-1020BA21B4F8: REST /ReBoot 02:08:05.737 -> 'offline' 02:08:06.823 (1.09 s after the request, before the restart) -> boot 02:08:22 -> 'online' birth 02:08:24.483 (OTGW-field-data/bench-otgw32/2026-11-20_task1780-ac2-broker/avail.log). HA side, rig HA (setup_rig.py), .63 beta.402+b885f3d, entities scoped by unique_id otgw-AC276ECE45D8-*: /ReBoot 11:52:46.214 -> status_master/status_slave/outside_temperature/binary_sensor 'unavailable' at t+0.7 s, back at t+10.7 s (same in a first run at 11:48: t+0.7 s / t+10.8 s); HA error_log 0 mqtt lines (OTGW-field-data/bench-63/2026-11-20_ha-1780-1949/ha1780.log, ha_error_log.txt). No HA screenshots: REST/entity-registry + error_log are the evidence (coordinator: sufficient, no rerun).
<!-- SECTION:NOTES:END -->
