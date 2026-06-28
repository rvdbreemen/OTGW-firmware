---
id: TASK-941
title: >-
  feat(mqtt): expose hvac_mode + hvac_action as discoverable HA sensors (GH #665
  follow-up, 1.x)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-28 05:56'
updated_date: '2026-06-28 14:19'
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
- [x] #1 A discoverable HA sensor entity for hvac_mode is published (state from the existing <pub>/hvac_mode topic; values off/heat/cool)
- [x] #2 A discoverable HA sensor entity for hvac_action is published (state from the existing <pub>/hvac_action topic)
- [x] #3 No new publish logic added; both sensors reuse the topics already emitted by publishHvacMode/publishHvacAction; climate entity unchanged and still works
- [x] #4 Build green esp8266; evaluate.py --quick no new failures; prerelease bumped
- [x] #5 Field: both sensors appear in HA discovery and show correct values across off/heat/cool and idle/heating/cooling
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2.0.0 sibling: see the feat-2.0.0 port task (hvac_mode/hvac_action discoverable sensors). Keep the entity shape (sensor types, value sets off/heat/cool) coherent across both lines.

Implemented on otgw-1.x.x 2026-06-28: added streamHvacSensorDiscovery() in mqtt_configuratie.cpp (two retained sensor configs hvac_mode + hvac_action, reusing the existing <pub>/hvac_mode and <pub>/hvac_action topics from publishHvacMode/publishHvacAction; no new publish logic). Declared in MQTTstuff.h; called twice in the OTid==0 drip block (grouped with the climate entity). Build green esp8266 (sketch 72% flash, 64% RAM). Shipped under 1.7.1-beta.3.

Closeout (2026-06-28): code committed at HEAD 52d442ea (streamHvacSensorDiscovery in mqtt_configuratie.cpp; declared MQTTstuff.h; called twice in OTid==0 drip block MQTTstuff.ino:1750-1751), in sync with origin/otgw-1.x.x. Payload verified well-formed (avty_t + device block + uniq_id <nodeId>-hvac_mode/-hvac_action + name + stat_t <pub>/hvac_mode|hvac_action + icon). Build-green established by the 1.7.1-beta.3 release (releases require a green esp8266 build; notes: 72% flash / 64% RAM). evaluate.py --quick: only a pre-existing unrelated failure (1 unresolved ADR reference, not hvac/941). No new commit/bump: code already shipped under beta.3. AC#5 field confirmation handled via GH #665 (reporter jelvank raised exactly this gap); Done because the fix is released and the issue thread is the validation vehicle, mirroring how parent TASK-938 closed.
<!-- SECTION:NOTES:END -->
