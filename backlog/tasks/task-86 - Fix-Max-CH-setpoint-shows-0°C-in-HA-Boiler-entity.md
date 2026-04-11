---
id: TASK-86
title: 'Fix: Max CH setpoint shows 0°C in HA Boiler entity'
status: Done
assignee: []
created_date: '2026-04-08 20:51'
updated_date: '2026-04-11 09:31'
labels:
  - bug
  - needs-info
  - mqtt
  - home-assistant
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/543'
  - 'Discord #beta-testing, user andrebrait, 2026-04-07/08'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The maximum CH water setpoint is correctly visible in the web UI and the HA Thermostat entity, but the HA Boiler entity always shows 0°C. Reported by andrebrait on Discord #beta-testing. Present since at least v1.0.0.

**Status: waiting for information — do not pick up yet.**

Needed before work can start:
- Telnet debug logs from andrebrait while issue is present (requested 2026-04-08)
- Confirmation of exact MQTT topics the Boiler entity subscribes to vs. what is published

Suspected cause: OT message ID mapping issue — value reaches firmware correctly but may be published to wrong MQTT source topic or Boiler entity HA auto-discovery references wrong topic.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 HA Boiler entity shows correct max CH setpoint (matching web UI value)
- [x] #2 HA Thermostat entity still shows correct value after fix
- [x] #3 Telnet logs confirm correct MQTT publish path
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fix already applied in commit e0660fb4 (publish OTGW Answer Thermostat messages to boiler MQTT source topic). Root cause: ID 57 (MaxTSet) arrives only as OTGW_ANSWER_THERMOSTAT (A-prefix); resolveSourceIndex() was mapping A-prefix to sourceIndex 2 (gateway) instead of sourceIndex 1 (boiler), so the boiler source MQTT topic never received the value. Fix: split OTGW_ANSWER_THERMOSTAT to map to boiler sourceIndex. HA Boiler entity now shows correct MaxTSet.
<!-- SECTION:FINAL_SUMMARY:END -->
