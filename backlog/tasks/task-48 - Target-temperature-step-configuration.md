---
id: TASK-48
title: Target temperature step configuration
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 21:06'
updated_date: '2026-04-05 23:29'
labels:
  - sat
  - feature
dependencies: []
references:
  - 'other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py:81'
  - 'other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py:348'
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add CONF_TARGET_TEMPERATURE_STEP setting (default 0.5C) to control the UI stepping granularity for target temperature in WebUI. Some users prefer 0.1C precision, others prefer 1.0C steps.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 CONF_TARGET_TEMPERATURE_STEP setting added with default 0.5C
- [ ] #2 WebUI target temperature controls use configured step size
- [x] #3 Valid range enforced (0.1C to 1.0C)
- [x] #4 Setting persisted across reboots
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Target temperature step configuration.\n\nChanges:\n- New setting fTargetTempStep (default 0.5C, range 0.1-1.0)\n- REST status includes target_temp_step\n- Settings persistence\n- AC#2 (WebUI using step) left for WebUI polish phase\n\nFiles: OTGW-firmware.h, settingStuff.ino, SATcontrol.ino
<!-- SECTION:FINAL_SUMMARY:END -->
