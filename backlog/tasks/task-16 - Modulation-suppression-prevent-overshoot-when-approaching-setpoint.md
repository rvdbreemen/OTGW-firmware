---
id: TASK-16
title: 'Modulation suppression: prevent overshoot when approaching setpoint'
status: To Do
assignee: []
created_date: '2026-04-05 10:08'
updated_date: '2026-04-05 21:42'
labels:
  - sat
  - feature
  - mqtt
  - rest
  - webui
dependencies:
  - TASK-4
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT Python has modulation suppression logic: when the boiler temperature approaches the setpoint, modulation is temporarily suppressed (MM=0) to prevent overshoot. This is a refinement on top of basic modulation control (task 4). Parameters: suppression_delay (20s default), suppression_offset (1.0C default). When boiler temp >= (setpoint - offset) for delay seconds, send MM=0. When boiler temp drops below (setpoint - offset - hysteresis), restore MM to normal value. This is especially effective for boilers that modulate poorly at low demand.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New settings: fModSuppressionDelay (default 20, range 0-120), fModSuppressionOffset (default 1.0, range 0-5)
- [ ] #2 Suppression logic in SATcontrol.ino: track time that boiler temp >= setpoint - offset
- [ ] #3 On suppression: send MM=0 via addCommandToQueue
- [ ] #4 On recovery (temp drops below offset - 0.5C hysteresis): send MM=<normal value>
- [ ] #5 State tracking: state.sat.bModSuppressed, state.sat.iModSuppressionSinceMs
- [ ] #6 Suppression only active in continuous mode (not in PWM mode)
- [ ] #7 REST API: GET /api/v2/sat/status includes mod_suppressed field
- [ ] #8 MQTT publish: sat/mod_suppressed (true/false)
- [ ] #9 WebUI: modulation suppression indicator in Control Details
- [ ] #10 Settings persistence via settingStuff.ino
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SAT Python references (Modulation suppression):
- const.py:86-87 - CONF_MODULATION_SUPPRESSION_DELAY_SECONDS, CONF_MODULATION_SUPPRESSION_OFFSET_CELSIUS
- const.py:152-153 - defaults: delay=20s, offset=1.0C
- entry_data.py:178-183 - config properties: modulation_suppression_delay_seconds, modulation_suppression_offset_celsius
- heating_control.py:410-448 - full suppression logic in _compute_pwm_control_setpoint():
  - :414-415 - reads suppression_delay from config
  - :417-433 - if elapsed_since_flame_on < suppression_delay: holds flame-off setpoint or waits
  - :435-448 - after delay: reads flow_temperature, subtracts modulation_suppression_offset_celsius to get suppressed_setpoint; clears flame_off_hold
- config_flow.py:769,773 - UI fields for delay (int selector 0-120) and offset (float selector)
- translations/en.json:219,225-226,245,250-251 - user-facing labels: "Enable modulation suppression", "Modulation suppression delay", "Modulation suppression offset" with descriptions noting PWM-only applicability
<!-- SECTION:NOTES:END -->
