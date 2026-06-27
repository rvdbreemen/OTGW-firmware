---
id: TASK-939
title: >-
  feat-2.0.0: port TASK-938 — unified heat/cool/off HA climate entity (cooling,
  GH #665)
status: To Do
assignee: []
created_date: '2026-06-27 10:47'
labels:
  - feature
dependencies: []
ordinal: 152000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the otgw-1.x.x cooling fix (7b2d3cdd, GH #665) to the 2.0.0 dev line. Same design: new <pub>/hvac_mode (off/heat/cool) + <pub>/hvac_action (off/idle/heating/cooling) computed in OTGW-Core.ino from the OT status bits (mode=cool if master cooling_enable else heat when connected else off; action=cooling/heating/idle from slave bits, NOT flame; off on thermostat disconnect), and the climate discovery (MQTTHaDiscovery.cpp streamClimateDiscovery): modes [off,heat,cool], mode_stat_t/action_topic -> new topics, max_temp 28->30. 2.0.0 specifics: state.otBus.bThermostatState (not state.otgw); ESP32 build target. Design fully validated against real logs on the 1.x side (idle/DHW/heating/cooling, gas boiler + heatpump).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 hvac_mode/hvac_action published from OT status bits; climate entity modes [off,heat,cool] with mode/action from new topics
- [ ] #2 ESP32 build green + evaluate.py green (ESP abstraction boundary respected)
- [ ] #3 Behaviour parity with the 1.x implementation (7b2d3cdd)
<!-- AC:END -->
