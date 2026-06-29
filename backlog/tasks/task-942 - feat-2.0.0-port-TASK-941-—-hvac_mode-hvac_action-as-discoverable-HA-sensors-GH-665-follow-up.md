---
id: TASK-942
title: >-
  feat-2.0.0: port TASK-941 — hvac_mode + hvac_action as discoverable HA sensors
  (GH #665 follow-up)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-28 05:57'
updated_date: '2026-06-29 04:40'
labels:
  - feature
  - mqtt
  - ha-discovery
dependencies: []
priority: medium
ordinal: 155000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2.0.0 sibling of TASK-941. The 2.0.0 line already carries the unified heat/cool/off climate entity (TASK-939, ADR-156) and publishes hvac_mode/hvac_action topics consumed by the climate entity. Add two standalone discoverable HA SENSOR entities for hvac_mode and hvac_action in MQTTHaDiscovery.cpp (the 2.0.0 equivalent of 1.x mqtt_configuratie.cpp), pointing at the existing published topics -- no new publish logic. Must respect ADR-140 single-device topology (the two new sensors attach to the single OTGW device, shared identifiers=nodeId, bare ids block on non-first entities, no via_device). Target line: dev (2.0.0 ESP32-S3).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A discoverable HA sensor entity for hvac_mode is published (state from the existing hvac_mode topic; values off/heat/cool)
- [x] #2 A discoverable HA sensor entity for hvac_action is published (state from the existing hvac_action topic)
- [x] #3 Both sensors follow ADR-140 single-device topology (shared nodeId identifiers, no via_device, full device block only on the first entity of the cycle)
- [x] #4 No new publish logic; climate entity unchanged and still works
- [x] #5 Build green esp32/esp32-classic/esp32-combo; evaluate.py --quick no new failures; prerelease bumped
- [ ] #6 Field: both sensors appear in HA discovery on a real OTGW32 and show correct values across off/heat/cool
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Build green: esp32 SUCCESS after fixing invalid HaIcon members (thermostat->thermostat_icon, heating->radiator; bug-138). evaluate.py --quick clean. prerelease alpha.282. Field AC#6 deferred (hardware: confirm both sensors in HA discovery + values across off/heat/cool).

ON-DEVICE BUG FOUND 2026-06-29 (OTGW32 @192.168.88.39, alpha.285, broker homeassistant.local): the hvac_mode/hvac_action companion sensors do NOT publish on a normal boot/reconnect. Broker has the climate config (modes [off,heat,cool], TASK-939) but NONE of the 109 retained configs for this device include sensor/.../hvac_mode|hvac_action/config. Root cause: id 242 (OTGWhvacid) is a FAUX sensor id (never seen on the OT bus), and publishNonOTDiscoveryConfigs() — the boot/reconnect discovery list — marks 0/27/dallas/heap/fw/pic* but NOT 242. So the companion sensors only appear after a manual full markAllMQTTConfigPending (F-key/discovery republish), not out of the box. readSensorIndex[242]=385 is valid, so the full-scan path works; only the boot list omits it. Fix: add setMQTTConfigPending(OTGWhvacid) to publishNonOTDiscoveryConfigs.
<!-- SECTION:NOTES:END -->
