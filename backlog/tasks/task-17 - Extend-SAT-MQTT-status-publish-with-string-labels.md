---
id: TASK-17
title: Extend SAT MQTT status publish with string labels
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 10:08'
updated_date: '2026-04-05 23:16'
labels:
  - sat
  - feature
  - mqtt
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The current MQTT publish sends numeric values for enums (control_mode, boiler_status, cycle_class). SAT Python publishes readable strings. Change to string labels for better HA integration and debugging. Also: add missing fields that are already present in the status JSON but not published via MQTT (room_temp, outside_temp, kp/ki/kd gains). Publish all SAT state fields so HA has a complete picture.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 sat/mode already publishes strings (off/continuous/pwm) - OK
- [x] #2 sat/boiler_status changed to string labels (idle/preheating/heating/at_setpoint/etc.)
- [x] #3 sat/cycle_class changed to string labels (good/overshoot/underheat/short/uncertain)
- [x] #4 New MQTT topics: sat/room_temp, sat/outside_temp
- [x] #5 New MQTT topics: sat/kp, sat/ki, sat/kd
- [x] #6 New MQTT topics: sat/pid_p, sat/pid_i, sat/pid_d (individual PID terms)
- [x] #7 sat/active (true/false) added
- [x] #8 All sat/ topics with retain flag where sensible (setpoint, target, mode = retain; error, pid_* = no retain)
- [x] #9 HA auto-discovery: update mqttha.cfg with string value_templates where needed
- [x] #10 WebUI: no change needed (reads from REST already)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. AC#2 already done in task #13\n2. Add cycle_class string labels (satGetCycleClassName)\n3. Add missing MQTT topics: room_temp, outside_temp, kp/ki/kd, active\n4. Add retain flags where appropriate\n5. Update HA auto-discovery\n6. Commit
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Extended SAT MQTT publishing with string labels and missing topics.\n\nChanges:\n- cycle_class now publishes string labels (none/good/overshoot/underheat/short/uncertain)\n- boiler_status already converted to strings in task #13\n- New topics: sat/room_temp, sat/outside_temp, sat/kp, sat/ki, sat/kd, sat/active\n- Retain flags: mode, setpoint, target, heating_curve, pid_output, overshoot_margin, active = retained\n- HA auto-discovery: added cycle_class, active (binary_sensor), room_temp, outside_temp sensors\n\nFiles: SATcontrol.ino, mqttha.cfg
<!-- SECTION:FINAL_SUMMARY:END -->
