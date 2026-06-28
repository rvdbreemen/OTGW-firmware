---
id: TASK-941
title: >-
  feat(mqtt): expose hvac_mode + hvac_action as discoverable HA sensors (GH #665
  follow-up, 1.x)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-28 05:56'
updated_date: '2026-06-28 06:36'
labels:
  - feature
  - mqtt
  - ha-discovery
dependencies: []
priority: medium
ordinal: 154000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-938 (GH #665). The firmware already publishes <pub>/hvac_mode (off/heat/cool, from master enable bits) and <pub>/hvac_action (from slave actual bits) in OTGW-Core.ino (publishHvacMode/publishHvacAction), but those topics are ONLY wired into the HA climate entity (mode_stat_t + action_topic). There are NO standalone discoverable HA sensor entities for them, so a user cannot see hvac mode/action as plain sensors (reporter jelvank on #665 could not find hvac_action anywhere). Add two sensor discovery configs in mqtt_configuratie.cpp pointing at the EXISTING topics -- no new publish logic. Target line: otgw-1.x.x (ESP8266). Sibling 2.0.0 port tracked separately.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A discoverable HA sensor entity for hvac_mode is published (state from the existing <pub>/hvac_mode topic; values off/heat/cool)
- [ ] #2 A discoverable HA sensor entity for hvac_action is published (state from the existing <pub>/hvac_action topic)
- [ ] #3 No new publish logic added; both sensors reuse the topics already emitted by publishHvacMode/publishHvacAction; climate entity unchanged and still works
- [ ] #4 Build green esp8266; evaluate.py --quick no new failures; prerelease bumped
- [ ] #5 Field: both sensors appear in HA discovery and show correct values across off/heat/cool and idle/heating/cooling
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2.0.0 sibling: see the feat-2.0.0 port task (hvac_mode/hvac_action discoverable sensors). Keep the entity shape (sensor types, value sets off/heat/cool) coherent across both lines.

Implemented on otgw-1.x.x 2026-06-28: added streamHvacSensorDiscovery() in mqtt_configuratie.cpp (two retained sensor configs hvac_mode + hvac_action, reusing the existing <pub>/hvac_mode and <pub>/hvac_action topics from publishHvacMode/publishHvacAction; no new publish logic). Declared in MQTTstuff.h; called twice in the OTid==0 drip block (grouped with the climate entity). Build green esp8266 (sketch 72% flash, 64% RAM). Shipped under 1.7.1-beta.3.
<!-- SECTION:NOTES:END -->
