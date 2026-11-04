---
id: TASK-32
title: Flame-off setpoint offset
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
Apply a specific setpoint offset when the flame is off to prevent unnecessary boiler restarts. When flame goes off, adjust the control setpoint by a configurable offset to create a hysteresis band. This reduces short-cycling by requiring a larger temperature drop before the boiler restarts.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py (CONF_FLAME_OFF_SETPOINT_OFFSET_CELSIUS)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Configurable flame-off setpoint offset (default e.g. 0.5C)
- [x] #2 When flame off: control_setpoint += offset (raises threshold for restart)
- [x] #3 When flame on: use normal setpoint calculation
- [x] #4 MQTT/REST/WebUI: flame-off offset setting
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Flame-off setpoint offset for anti-cycling hysteresis.\n\nChanges:\n- New setting fFlameOffOffset (default 0, range 0-5C)\n- When flame is off and offset > 0, adds offset to CS= command\n- Creates hysteresis band preventing unnecessary boiler restarts\n- Settings persistence, MQTT subscribe, REST API status field\n\nFiles: OTGW-firmware.h, SATcontrol.ino, settingStuff.ino, MQTTstuff.ino
<!-- SECTION:FINAL_SUMMARY:END -->
