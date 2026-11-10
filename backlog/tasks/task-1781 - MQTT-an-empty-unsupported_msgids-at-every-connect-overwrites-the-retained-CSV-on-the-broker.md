---
id: TASK-1781
title: >-
  MQTT: an empty unsupported_msgids at every connect overwrites the retained CSV
  on the broker
status: To Do
assignee: []
created_date: '2026-11-10T14:32:00Z'
labels:
  - mqtt
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 877000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Side finding of the TASK-1757 analysis (2026-11-10). onMqttConnect calls publishBoilerUnsupportedMsgids() (src/OTGW-firmware/MQTTstuff.ino, post-subscribe block), which publishes the CSV of boiler Unknown-Data-Id msgIDs RETAINED to otgw-firmware/boiler/unsupported_msgids (publishBoilerUnsupportedMsgids). Right after boot the bitmap is still empty, so every connect publishes '' retained and replaces the CSV learned before the reboot; the value is rebuilt only as the boiler answers Unknown-Data-Id again. Question for Robert: is that intended (fresh state per boot) or should the empty map not be published at connect? Priority low: diagnostic topic only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decide with Robert whether an empty map at connect should overwrite the retained CSV
- [ ] #2 If not: publish only once the map is non-empty (or on change); a host test on the real function shows no retained '' at connect with an empty map
<!-- AC:END -->
