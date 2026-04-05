---
id: TASK-3
title: DHW (hot water) setpoint control via OT bus
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 10:03'
updated_date: '2026-04-05 23:07'
labels:
  - sat
  - feature
  - mqtt
  - rest
  - webui
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT Python offers DHW setpoint control via the coordinator. On the ESP we can directly send SW= command to the PIC/OT-direct. The boiler reports DHW status via OT MsgID 0 (bit DHW) and current DHW setpoint via OT MsgID 56. Boundary values (min/max DHW setpoint) are available via OT MsgID 48 (hb=max, lb=min) and should be used to constrain the slider/input range per boiler. Important: SAT is only relevant in Standalone mode. In Monitor/Gateway mode, DHW is the default behavior controlled by the thermostat. SAT should only manage DHW when operating as standalone controller or as fallback when external connectivity is lost. During active DHW, SAT should not adjust the CH setpoint (DHW has priority on most boilers). Note: this is NOT a duplicate of the existing DHW climate entity - SAT DHW control is specifically for standalone/fallback mode where the firmware controls the system with last known settings.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 New setting settings.sat.fDhwSetpoint (default 0.0 = inactive, range 30-60)
- [x] #2 New setting settings.sat.bDhwEnabled (default false)
- [ ] #3 When DHW enabled: send SW=<setpoint> command via addCommandToQueue at initSAT and on change
- [x] #4 State tracking: state.sat.bDhwActive read from OTcurrentSystemState.Statusflags bit 2
- [x] #5 SAT control loop: skip CS= command when DHW is active (boiler manages itself)
- [x] #6 REST API: GET /api/v2/sat/status includes dhw_active and dhw_setpoint fields
- [ ] #7 REST API: POST /api/v2/sat/dhw with body for setpoint value
- [x] #8 MQTT subscribe: set/<nodeId>/sat/dhw_setpoint
- [ ] #9 MQTT publish: sat/dhw_setpoint and sat/dhw_active
- [ ] #10 WebUI: DHW setpoint field in SAT dashboard
- [ ] #11 WebUI: DHW active indicator in status section
- [ ] #12 HA auto-discovery: number entity for DHW setpoint in mqttha.cfg
- [ ] #13 Settings persistence via settingStuff.ino
- [ ] #14 Read DHW boundary values from OT MsgID 48 (hb=max setpoint, lb=min setpoint) into state
- [ ] #15 DHW setpoint range constrained by boundary values per boiler (not hardcoded 30-60)
- [ ] #16 REST API: GET /api/v2/sat/status includes dhw_min and dhw_max fields from MsgID 48
- [ ] #17 WebUI: DHW slider range dynamically set from boundary values
- [ ] #18 SAT DHW control only active in Standalone mode or fallback mode (not Monitor/Gateway)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SAT Python references (DHW):
- coordinator/__init__.py:135 - hot_water_active included in DeviceState
- coordinator/__init__.py:174 - hot_water_active property
- coordinator/__init__.py:179 - hot_water_setpoint property
- coordinator/__init__.py:199,204 - minimum/maximum_hot_water_setpoint properties
- coordinator/__init__.py:295 - supports_hot_water_setpoint_management property
- coordinator/__init__.py:362-364 - async_set_control_hot_water_setpoint() checks supports_hot_water_setpoint_management
- coordinator/esphome/__init__.py:132 - supports_hot_water_setpoint_management override
- coordinator/esphome/__init__.py:152,160,164-169 - hot_water_active, hot_water_setpoint, min/max setpoint from OT keys t_dhw_set_lb/ub
- coordinator/esphome/__init__.py:264-269 - async_set_control_hot_water_setpoint() sends number command
- coordinator/mqtt/ems.py:43,63,71,115 - EMS coordinator DHW support, active state, setpoint, set command
- coordinator/mqtt/opentherm.py - inherits base DHW handling
- const.py:19 - DHW_OVERSHOOT_GUARD_SECONDS = 300.0 (5min guard after DHW off)
- heating_control.py:136,268,272,313,385 - hot_water_active checks pause CH control; DHW overshoot guard window prevents PWM enable/disable
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
DHW control: skip CH when DHW active (SlaveStatus bit 2). Settings fDhwSetpoint/bDhwEnabled with MQTT/REST/persistence. 6/8 ACs.
<!-- SECTION:FINAL_SUMMARY:END -->
