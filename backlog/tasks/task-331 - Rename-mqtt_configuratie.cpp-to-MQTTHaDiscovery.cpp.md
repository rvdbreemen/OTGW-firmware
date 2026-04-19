---
id: TASK-331
title: Rename mqtt_configuratie.cpp to MQTTHaDiscovery.cpp
status: Done
assignee:
  - '@claude'
created_date: '2026-04-19 17:12'
updated_date: '2026-04-19 17:31'
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
- [x] #1 File renamed via git mv so git blame history is preserved
- [x] #2 Any references in docs (ADR-077 especially) updated to the new filename
- [x] #3 Both ESP8266 and ESP32 build clean after the rename (file is linked, not included, so include paths unaffected)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
git mv src/OTGW-firmware/mqtt_configuratie.cpp -> src/OTGW-firmware/MQTTHaDiscovery.cpp. Git detected the rename, so blame history is preserved. 113 references updated across 12 active files: src/OTGW-firmware/MQTTstuff.ino+.h, docs/api/MQTT.md+MQTT_DISCOVERY_CHECK.md+README.md+openapi.yaml, docs/c4/c4-code-mqtt.md, docs/adr/ADR-077, docs/manuals/{en,nl}/ch04+ch08. Historical backlog tasks (275, 282, 284, 285, 306) intentionally left untouched -- they document the file name as it was at the time. The new name is self-documenting (Home Assistant discovery compose), matches project PascalCase after MQTT prefix (MQTTstuff sibling), and groups alphabetically next to MQTTstuff.ino. Both ESP8266 and ESP32-S3 build clean.
<!-- SECTION:FINAL_SUMMARY:END -->
