---
id: TASK-4
title: Relative modulation control (MM= command)
status: To Do
assignee: []
created_date: '2026-04-05 10:03'
updated_date: '2026-04-05 21:41'
labels:
  - sat
  - feature
  - mqtt
  - rest
  - webui
dependencies:
  - TASK-1
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT Python sends MM= (max relative modulation) command to the boiler to limit modulation. This is essential for low-load control and is a prerequisite for Overshoot Protection Value (OPV, task 5). On the ESP we send MM=<0-100> via addCommandToQueue. When SAT is in PWM mode and flame should be OFF, send MM=0 to suppress modulation. In continuous mode, send MM=100 (or configured max). SAT Python also uses modulation suppression: when boiler temp approaches setpoint, temporarily reduce modulation to prevent overshoot.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New setting settings.sat.iMaxRelModulation (default 100, range 0-100)
- [ ] #2 SAT control loop: send MM=<value> command alongside CS= command
- [ ] #3 PWM OFF phase: send MM=0 to suppress modulation
- [ ] #4 PWM ON phase: send MM=<configured max>
- [ ] #5 Continuous mode: send MM=<configured max>
- [ ] #6 Modulation suppression: when boiler temp within 1.0C of setpoint, send MM=0 after 20s delay
- [ ] #7 State tracking: state.sat.iCurrentModulation (current sent MM value)
- [ ] #8 REST API: GET /api/v2/sat/status includes max_rel_modulation and current_modulation fields
- [ ] #9 REST API: POST /api/v2/sat/modulation with body for max modulation value
- [ ] #10 MQTT subscribe: set/<nodeId>/sat/max_modulation
- [ ] #11 MQTT publish: sat/max_modulation and sat/current_modulation
- [ ] #12 WebUI: max modulation slider/field in SAT dashboard
- [ ] #13 WebUI: current modulation value visible in Control Details
- [ ] #14 Settings persistence via settingStuff.ino
- [ ] #15 HA auto-discovery: number entity for max modulation in mqttha.cfg
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SAT Python references (Relative Modulation MM=):
- heating_control.py:510-524 - _compute_relative_modulation_value(): if coordinator does not support rel mod management, sets None and skips; OFF state sets MINIMUM_RELATIVE_MODULATION, else uses config.pwm.maximum_relative_modulation; Geminox special case floors at 10
- heating_control.py:237 - async_control() calls coordinator.async_set_control_max_relative_modulation(value)
- coordinator/__init__.py:372 - base async_set_control_max_relative_modulation() abstract method
- coordinator/mqtt/opentherm.py:216-222 - OpenTherm MQTT impl publishes max_relative_modulation via MQTT
- coordinator/esphome/__init__.py:278-283 - ESPHome impl sends number command for max relative modulation
- coordinator/serial/__init__.py:216-220 - Serial impl sends CS= command with max relative modulation
- climate.py:50 - ATTR_RELATIVE_MODULATION_VALUE exposed as extra state attribute
- climate.py:257-258 - exposes relative_modulation_value and relative_modulation_state in state attrs
- climate.py:492 - passes relative_modulation to heating demand
- binary_sensor.py:62,143-144,151-152 - modulation sensor checks supports_relative_modulation_management, compares climate vs maximum modulation
<!-- SECTION:NOTES:END -->
