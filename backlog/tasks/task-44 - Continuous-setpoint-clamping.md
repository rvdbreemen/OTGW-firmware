---
id: TASK-44
title: Continuous setpoint clamping
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 21:06'
updated_date: '2026-04-05 23:24'
labels:
  - sat
  - feature
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/heating_control.py:450-509
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement asymmetric setpoint clamping for continuous modulation mode. Different behavior for rising vs falling demand. minimum_allowed = boiler_temp - flow_offset (default 2.0C). Prevents setpoint spikes when boiler reports overheat temperature, ensuring smooth control transitions.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Setpoint clamping applies different logic for rising vs falling demand
- [x] #2 minimum_allowed calculated as boiler_temp - flow_offset (default 2.0C)
- [x] #3 Prevents setpoint spikes on boiler overheat conditions
- [x] #4 flow_offset configurable (default 2.0C)
- [x] #5 Clamping logic only active in continuous modulation mode
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Continuous setpoint clamping with configurable flow offset.\n\nChanges:\n- Replaced hardcoded SAT_FLOW_OFFSET_CONTINUOUS (5.0C) with configurable fFlowOffset (default 2.0C)\n- minimum_allowed = boiler_temp - flow_offset per SAT Python\n- Settings persistence, MQTT subscribe\n\nFiles: OTGW-firmware.h, SATcontrol.ino, settingStuff.ino, MQTTstuff.ino
<!-- SECTION:FINAL_SUMMARY:END -->
