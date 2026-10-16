---
id: TASK-1240
title: >-
  Two small code findings from the C4 refresh: case-sensitive MQTT side effect,
  dead S0 autoconfigure call
status: To Do
assignee: []
created_date: '2026-10-16 04:18'
labels:
  - bug
  - settings
  - s0
dependencies: []
priority: low
ordinal: 378000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while refreshing docs/c4 (TASK-1239), verified by reading the source on alpha.427. (1) settingStuff.ino updateSetting(): the MQTT restart side effect is set by strstr_P(field, PSTR("mqtt")), which is case-sensitive, so a setting key written as e.g. 'MQTTbroker' (settings.ini spelling) through a path that does not lowercase it would change the value without restarting MQTT. The v2 UI sends lowercase names, so the impact is probably limited to other callers (MQTT set commands, scripts). Needs a reproduction before a fix. (2) s0PulseCount.ino calls sensorAutoConfigure(OTGWs0dataid, true, ""), which returns immediately on an empty sensor id (MQTTstuff.ino), so the call is dead; S0 discovery (id 245) already goes through the generic discovery drip. Remove the call or document why it stays.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Finding 1 reproduced (or shown unreachable) on the bench, with the callers that can pass a mixed-case key listed
- [ ] #2 If reachable: the side-effect match is case-insensitive and a mixed-case key restarts MQTT (old-vs-fix shown)
- [ ] #3 The dead S0 sensorAutoConfigure call is removed, S0 discovery still appears on the broker
<!-- AC:END -->
