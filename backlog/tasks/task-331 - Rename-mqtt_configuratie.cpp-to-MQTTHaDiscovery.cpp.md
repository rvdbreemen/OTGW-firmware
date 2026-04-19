---
id: TASK-331
title: Rename mqtt_configuratie.cpp to MQTTHaDiscovery.cpp
status: To Do
assignee: []
created_date: '2026-04-19 17:12'
labels:
  - refactor
  - naming
  - review-2026-04-18-followup
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
mqtt_configuratie.cpp contains 2739 lines of Home Assistant MQTT auto-discovery compose + publish code (streamSensorDiscovery, composeSensorPayload, streamDallasSensorDiscovery, PROGMEM labels). Current name is misleading because 'configuratie' reads as MQTT broker configuration (which actually lives in settings.mqtt.*). It is also off-project: Dutch name + snake_case in a directory where every sibling uses English + PascalCase/camelCase (MQTTstuff.ino, SATcontrol.ino, OTGW-Core.ino, restAPI.ino, FSexplorer.ino). New name MQTTHaDiscovery.cpp is self-documenting, matches project conventions, and groups alphabetically next to MQTTstuff.ino.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 File renamed via git mv so git blame history is preserved
- [ ] #2 Any references in docs (ADR-077 especially) updated to the new filename
- [ ] #3 Both ESP8266 and ESP32 build clean after the rename (file is linked, not included, so include paths unaffected)
<!-- AC:END -->
