---
id: TASK-31
title: Push setpoint to thermostat display (TC= command)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 20:47'
updated_date: '2026-04-05 23:20'
labels:
  - sat
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sync SAT target temperature back to the physical thermostat display using the TC= (thermostat control) OTGW command. This way the thermostat shows what SAT is actually targeting, not its own stale setpoint. SAT Python has push_setpoint_to_thermostat config option.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py (CONF_PUSH_SETPOINT_TO_THERMOSTAT)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Send TC= command to update thermostat display with SAT target temp
- [x] #2 Configurable enable/disable (default off)
- [x] #3 Only active in gateway mode (thermostat present)
- [x] #4 MQTT/REST/WebUI: setting to enable/disable push
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Push SAT target to thermostat display via TC= command.\n\nChanges:\n- New setting bPushSetpoint (default off)\n- When enabled, sends TC=<target_temp> alongside CS= each control cycle\n- Settings persistence, MQTT subscribe, REST API status field\n\nFiles: OTGW-firmware.h, SATcontrol.ino, settingStuff.ino, MQTTstuff.ino
<!-- SECTION:FINAL_SUMMARY:END -->
