---
id: TASK-1988
title: 'Capture tool: record only the MQTT topics this gateway publishes, never ''#'''
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T21:19:00Z'
updated_date: '2026-11-15T22:02:00Z'
labels:
  - capture
  - privacy
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1016000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-15 (CLI via the coordinator, 22:1x and 22:2x: 'Follow your advice. A.'). gruijter's beta.250 capture held his whole zigbee2mqtt household because otgw_capture.py subscribes to '#' by default (otgw_capture.py --topic default '#'). The capture must by default record only what this gateway publishes itself: its own topic prefix (value/state/status/LWT of this device, node id from device/info) and its own Home Assistant discovery configs (homeassistant/+/<its node id>/...). Not other devices, not other OTGWs on the same broker, not command topics others publish (set/...). An explicit opt-in --mqtt-all-topics subscribes to '#' for someone who wants it on purpose, with a clear warning on the console and in summary.txt that other devices are in the capture.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Default: the MQTT subscription and the written mqtt.log hold only this gateway's own published topics and its own HA discovery configs; a fake broker stream with zigbee2mqtt/... and a second OTGW's topics: red on the current tool, green after
- [x] #2 --mqtt-all-topics subscribes to '#' and prints and records a warning (console + summary.txt) that other devices are in the capture; test
- [x] #3 The node id / prefix comes from the gateway (device/info, settings); without it the tool records nothing from MQTT rather than '#', and says so; README updated
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Firmware topics checked against the source (2026-11-15): every publish goes through mqttPublishRaw() (MQTTstuff.ino:343; the only other MQTTclient.publish is the LWT 'offline' on the namespace, :582). Topics: <toptopic>/value/<uniqueid> itself (LWT) and below (sendMQTTData() prefixes MQTTPubNamespace, MQTTstuff.ino:606; SAT, SAT BLE state <ns>/sat/ble/<mac>/<kind> :3643, v2 migration clears under the namespace :2103); HA discovery <haprefix>/<component>/<nodeId>/.../config in MQTTHaDiscovery.cpp (sensor, binary_sensor, climate, number, switch, select, button, generic table :3943) and MQTTstuff.ino (sat_zone, sat_pv, binary_sensor); the one exception <haprefix>/sensor/<uniqueid>_ble_<mac>_<kind>/config (MQTTstuff.ino:3639, :3836), the unique id inside a level. docs/api/MQTT.md names no other root. Subscriptions: <top>/value/<uid>/#, <ha>/+/<uid>/#, <ha>/sensor/+/config; own_mqtt_topic() drops every other message before it is written (the summary counts them). Never <top>/set/<uid>/... (commands others publish).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: by default the capture subscribes to this gateway's own namespace and HA discovery configs and own_mqtt_topic() drops every other message before it is written. tests/test_capture_mqtt_own_topics.py (8 cases) fails on the origin tool (2 failures, 5 errors) and passes on the fix; its source-derived case builds 25 sample topics from every firmware topic format and a mutant without the BLE rule fails it. tests/test_capture_tool.py test_o: a fake broker sends the gateway's topic plus zigbee2mqtt, a second gateway and a set/ command: the origin tool writes all of them (zigbee2mqtt/Kitchen plug in mqtt.log, gruijter's case), the fix only the own topic, and the broker sees no '#' subscription. AC2: --mqtt-all-topics (-MqttAllTopics) subscribes to '#' (or --topic, only with it), writes everything, and the warning is on the console and in the summary (test_o2). AC3: prefix, HA prefix and unique id come from GET /api/v2/settings; without them (401, no answer, empty) no MQTT is recorded and the summary says why; the saved settings no longer keep Topic; README updated. Sweep -j 1: the 9 test files that import the tool, 17/17 pass.
<!-- SECTION:FINAL_SUMMARY:END -->
