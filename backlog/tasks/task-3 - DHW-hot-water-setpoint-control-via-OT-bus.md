---
id: TASK-3
title: DHW (hot water) setpoint control via OT bus
status: To Do
assignee: []
created_date: '2026-04-05 10:03'
updated_date: '2026-04-05 10:58'
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
- [ ] #1 New setting settings.sat.fDhwSetpoint (default 0.0 = inactive, range 30-60)
- [ ] #2 New setting settings.sat.bDhwEnabled (default false)
- [ ] #3 When DHW enabled: send SW=<setpoint> command via addCommandToQueue at initSAT and on change
- [ ] #4 State tracking: state.sat.bDhwActive read from OTcurrentSystemState.Statusflags bit 2
- [ ] #5 SAT control loop: skip CS= command when DHW is active (boiler manages itself)
- [ ] #6 REST API: GET /api/v2/sat/status includes dhw_active and dhw_setpoint fields
- [ ] #7 REST API: POST /api/v2/sat/dhw with body for setpoint value
- [ ] #8 MQTT subscribe: set/<nodeId>/sat/dhw_setpoint
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
