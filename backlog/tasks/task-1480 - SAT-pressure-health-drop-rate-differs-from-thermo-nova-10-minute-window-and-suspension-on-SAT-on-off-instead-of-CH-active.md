---
id: TASK-1480
title: >-
  SAT pressure-health drop rate differs from thermo-nova: 10-minute window and
  suspension on SAT on/off instead of CH active
status: To Do
assignee: []
created_date: '2026-10-29T14:38:00Z'
labels:
  - sat
  - pressure
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 640000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in gruijter's field capture (TASK-1474, alpha.527): sat/pressure_alarm fired 10 times in 4.5 h (09:15 ... 13:27), each cleared within minutes. The boiler CH pressure (MsgID 18, read ~every 5 min) is ~2.0 bar while the slave CH-mode bit (pump incl. overrun) is set and 1.70-1.74 bar when it clears, so every pump stop is a 0.3 bar step that the regression reads as ~1.7 bar/h. Firmware satUpdatePressure() (SATcontrol.ino ~3864-4022 at fd491ff): 20-slot ring x 30 s, so the real window is ~10 min (the 3600 s eviction is dead code), and the 600 s suspension/clear follows state.sat.bActive (SAT on/off). thermo-nova binary_sensor.py:335-345 and 357-388 (constants :25-28): window max(age, 3600 s), suspension/clear on coordinator.active (= CH active; serial coordinator master CH enable :54, MQTT OpenTherm coordinator central-heating state opentherm.py:60). Second-reader simulation on the real series: firmware model reproduces 9/10 logged alarms; thermo-nova would also alarm (9 with master CH enable, 4-5 with the MQTT CH-mode signal). The alarm is display/MQTT only (no control effect). Whether a pump-head step should count at all is a question for Sergeant D (via the coordinator).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 satUpdatePressure follows thermo-nova binary_sensor.py:335-388 (window max(age,3600 s), suspension and sample clear on the CH-active transition), host test on the real function with the captured 2.0/1.7 bar series old vs new
- [ ] #2 Sergeant D's answer on pump-head steps is recorded in sergeant-d-corrections.md before any behaviour beyond parity is changed
<!-- AC:END -->
