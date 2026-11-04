---
id: TASK-41
title: Force PWM mode override
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 21:05'
updated_date: '2026-04-05 23:24'
labels:
  - sat
  - feature
dependencies: []
references:
  - 'other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py:80'
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/entry_data.py:149
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add manual toggle CONF_FORCE_PULSE_WIDTH_MODULATION to force PWM operation even when boiler is in continuous modulation mode. User-facing setting in WebUI.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 CONF_FORCE_PULSE_WIDTH_MODULATION setting added to configuration
- [x] #2 Toggle accessible from WebUI settings page
- [x] #3 When enabled, PWM mode is forced regardless of boiler modulation mode
- [x] #4 Setting persisted across reboots
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Force PWM mode override.\n\nChanges:\n- New setting bForcePWM (default off)\n- When enabled, forces PWM mode regardless of boiler modulation\n- Auto-switch logic respects force flag\n- Settings persistence, MQTT subscribe, REST API\n\nFiles: OTGW-firmware.h, SATcontrol.ino, settingStuff.ino, MQTTstuff.ino
<!-- SECTION:FINAL_SUMMARY:END -->
