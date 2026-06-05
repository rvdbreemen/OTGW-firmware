---
id: TASK-827
title: >-
  fix(mqtt-ha): remove dead MqttHaSensorCfg in Dallas discovery (esp8266
  -Wunused warning)
status: Done
assignee: []
created_date: '2026-06-05 22:10'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
streamDallasSensorDiscovery() built a local MqttHaSensorCfg + dallasFriendlyName but the compose lambda emits the payload inline and never reads cfg -> dead code, -Wunused-but-set-variable on esp8266. Removed both; discovery output byte-identical.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MqttHaSensorCfg cfg + dallasFriendlyName removed from streamDallasSensorDiscovery; warning gone; output unchanged
<!-- AC:END -->
