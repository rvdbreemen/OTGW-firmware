---
id: TASK-17
title: Extend SAT MQTT status publish with string labels
status: To Do
assignee: []
created_date: '2026-04-05 10:08'
updated_date: '2026-04-05 10:24'
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
- [ ] #1 sat/mode already publishes strings (off/continuous/pwm) - OK
- [ ] #2 sat/boiler_status changed to string labels (idle/preheating/heating/at_setpoint/etc.)
- [ ] #3 sat/cycle_class changed to string labels (good/overshoot/underheat/short/uncertain)
- [ ] #4 New MQTT topics: sat/room_temp, sat/outside_temp
- [ ] #5 New MQTT topics: sat/kp, sat/ki, sat/kd
- [ ] #6 New MQTT topics: sat/pid_p, sat/pid_i, sat/pid_d (individual PID terms)
- [ ] #7 sat/active (true/false) added
- [ ] #8 All sat/ topics with retain flag where sensible (setpoint, target, mode = retain; error, pid_* = no retain)
- [ ] #9 HA auto-discovery: update mqttha.cfg with string value_templates where needed
- [ ] #10 WebUI: no change needed (reads from REST already)
<!-- AC:END -->
