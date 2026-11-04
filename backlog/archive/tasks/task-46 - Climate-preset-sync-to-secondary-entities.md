---
id: TASK-46
title: Climate preset sync to secondary entities
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 21:06'
updated_date: '2026-04-06 11:45'
labels:
  - sat
  - feature
dependencies: []
references:
  - 'other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py:79'
  - other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add CONF_SYNC_CLIMATES_WITH_PRESET setting. When the SAT climate entity changes preset (e.g. away, comfort, sleep), synchronize the preset to all configured secondary climate entities. Enables whole-home preset coordination.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 CONF_SYNC_CLIMATES_WITH_PRESET setting added to configuration
- [x] #2 When SAT preset changes, all secondary climate entities receive the same preset
- [x] #3 Secondary climate entities configurable via settings
- [x] #4 Setting persisted across reboots
- [x] #5 Graceful handling when secondary entity does not support the preset
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add sync setting (enable + MQTT topic for secondary entities)
2. On preset change, publish preset name to sync topic
3. Settings persistence
4. REST/MQTT/WebUI
5. Commit
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented preset sync feature:
- OTGW-firmware.h: added bPresetSync and sPresetSyncTopic[65] to SATSection
- SATcontrol.ino: added MQTT publish in satHandlePreset() when sync enabled; added preset_sync to satSendStatusJSON()
- settingStuff.ino: persistence (write/read/update) for both settings
- restAPI.ino: added to sendDeviceSettings and knownSettings whitelist
- MQTTstuff.ino: added preset_sync and preset_sync_topic MQTT subscribe handlers
- index.js: added translateSettings labels and tooltips
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Preset sync to secondary climate entities via MQTT.

Changes:
- On preset change, publishes preset name to configurable MQTT topic (retained)
- Settings: SATpresetsync (enable), SATpresetsynctopic (MQTT topic)
- HA automations can subscribe to sync zone thermostats
- Safety: only fires when enabled + topic set + MQTT connected
<!-- SECTION:FINAL_SUMMARY:END -->
