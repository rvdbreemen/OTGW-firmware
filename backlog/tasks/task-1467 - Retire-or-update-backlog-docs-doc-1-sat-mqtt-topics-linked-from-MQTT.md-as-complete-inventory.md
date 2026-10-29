---
id: TASK-1467
title: >-
  Retire or update backlog/docs/doc-1 (sat-mqtt-topics), linked from MQTT.md as
  complete inventory
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T07:24:00Z'
updated_date: '2026-10-29T21:16:00Z'
labels:
  - docs
  - mqtt
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 627000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. MQTT.md links backlog/docs/doc-1 as the complete SAT topic inventory; after the audit it is probably stale. The non-SAT Entity Inventory (~MQTT.md:1208) cites an old branch name and old counts.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 doc-1 retired (link removed) or brought in line with MQTT.md; the non-SAT inventory paragraph corrected
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence (2026-10-29, read only, no device): doc-1 archived with 'backlog doc archive doc-1' (now backlog/archive/docs/). Seven links repointed to docs/api/MQTT.md#sat-smart-autotune-thermostat (heading MQTT.md:296): MQTT.md:298 and :1129, MANUAL.md, ch09 EN, h09 NL, openapi.yaml, README.md:141. grep for 'doc-1'/'sat-mqtt-topics' outside backlog/archive, backlog/tasks, docs/history: 0 hits. openapi.yaml no-value command list now only reset_integral (ovp_start/ovp_stop gone; restAPI.ino:1599). Entity Inventory: sensor 394 = MQTTHaDiscovery.cpp:835, binary_sensor 98 = MQTTHaDiscovery.cpp:1390, switch 13 = MQTTstuff.h:565; old branch name removed. Audit evidence: 549e93312.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Retired backlog doc-1 (archived via CLI) and pointed every link to the SAT section of docs/api/MQTT.md, which is the complete topic inventory since audit 549e93312. Fixed the non-SAT Entity Inventory counts (394/98/13, cited to source) and dropped the stale openapi ovp_start/ovp_stop no-value commands. Evidence: grep 0 remaining doc-1 refs; counts read from MQTTHaDiscovery.cpp:835/1390 and MQTTstuff.h:565. Docs only, not device-tested.
<!-- SECTION:FINAL_SUMMARY:END -->
