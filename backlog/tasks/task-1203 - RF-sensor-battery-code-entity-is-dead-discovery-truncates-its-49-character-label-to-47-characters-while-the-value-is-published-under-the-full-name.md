---
id: TASK-1203
title: >-
  RF sensor battery-code entity is dead: discovery truncates its 49-character
  label to 47 characters while the value is published under the full name
status: To Do
assignee: []
created_date: '2026-10-03 17:54'
labels:
  - ha-discovery
  - bug
  - mqtt
dependencies: []
priority: medium
ordinal: 325000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by a TASK-1201 verifier, confirmed 2026-10-03 on the rig broker (alpha.404). Table row MQTTHaDiscovery.cpp:1113 (label RFSensorStatusInformation_battery_indication_code, 49 characters) is copied into char label[48] in composeSensorPayload (:2386-2389) and into labelBuf[48] in buildSensorDiscoveryTopic (:2634-2635). Retained config: stat_t = OTGW/value/<node>/RFSensorStatusInformation_battery_indication_co. The value is published by publish_mqtt_u8_code_and_text_topics(F("RFSensorStatusInformation_battery_indication_code")) at OTGW-Core.ino:3471, under the full 49-character name. So the Home Assistant entity never receives a value.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The discovery stat_t of the battery-code sensor equals the topic its value is published on (host test that fails on the old code)
- [ ] #2 No other discovery label is truncated: a host check over the whole sensor and binary-sensor tables compares every label length with the buffers
- [ ] #3 The uniq_id consequence is decided and recorded: whether the fixed label changes the uniq_id, and what happens to the existing dead entity in Home Assistant
- [ ] #4 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->
