---
id: TASK-16
title: 'Modulation suppression: prevent overshoot when approaching setpoint'
status: To Do
assignee: []
created_date: '2026-04-05 10:08'
updated_date: '2026-04-05 10:24'
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
