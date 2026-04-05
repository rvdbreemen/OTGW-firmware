---
id: TASK-22
title: Heating system type selection with heat pump support
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 20:24'
updated_date: '2026-04-05 22:34'
labels:
  - sat
  - feature
  - mqtt
  - rest
  - webui
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add HeatingSystem setting (Radiators/Heat Pump/Underfloor) matching SAT Python thermo-nova HeatingSystem enum. Auto-adjusts PWM cycles, max setpoint, modulation strategy, and base offset per system type.

Heat pumps: MM=100 always (let heat pump manage internal modulation), 1.5-2 cycles/hour (protect compressor COP), max setpoint 40C.
Radiators: 3-4 cycles/hour, max setpoint 62C, base_offset 27.2.
Underfloor: base_offset 20.0, max setpoint 45C.

Firmware already reads boiler type from OT MsgID 3 (vh_configuration_system_type) but SAT ignores it. Can auto-detect and suggest, with manual override.

Based on Discord discussion (2026-04-05) between number3nl and sergeantd about hybrid heat pump requirements. Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/types.py (HeatingSystem enum).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Heat pump mode: PWM cycles limited to 1.5-2 per hour (30-40 min cycles)
- [x] #2 Heat pump mode: max setpoint capped at 40C
- [x] #3 Radiators mode: cycles 3-4/hr, max setpoint 62C, base_offset 27.2
- [x] #4 Underfloor mode: base_offset 20.0, max setpoint 45C
- [x] #5 Auto-detect from OT MsgID 3 system_type bit and suggest to user (optional override)
- [x] #6 REST API: GET/POST heating_system setting
- [x] #7 MQTT: subscribe set/<nodeId>/sat/heating_system, publish sat/heating_system
- [x] #8 WebUI: heating system dropdown in SAT settings
- [x] #9 Settings persistence via settingStuff.ino
- [ ] #10 HA auto-discovery: select entity for heating system type
- [x] #11 Auto-detect heating system from OT MsgID 3 system_type bit (default: auto), with manual override dropdown (radiators/heat_pump/underfloor)
- [ ] #12 Heat pump: prefer MM modulation (0-100%) for continuous smooth control, heat pump manages compressor internally
- [ ] #13 Heat pump: if MM not supported, fallback to PWM with max 2 cycles/hour and minimum 30min ON time
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add HeatingSystem enum to OTGW-firmware.h (HEATING_SYSTEM_AUTO/RADIATORS/HEAT_PUMP/UNDERFLOOR)
2. Add settings.sat.sHeatingSystem field to OTGWSettings struct
3. Add helper functions: getMaxSetpoint(), getBaseOffset(), getMaxCyclesPerHour(), getMinOnTimeSec() per system type
4. Read OT MsgID 3 system_type bit in processOT() for auto-detection
5. Add REST API endpoints: GET/POST heating_system in restAPI.ino
6. Add MQTT subscribe handler in MQTTstuff.ino
7. Add MQTT publish in SAT status
8. Add settings persistence in settingStuff.ino
9. Add WebUI dropdown in SAT settings page
10. Add HA auto-discovery select entity in mqttha.cfg
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
number3nl feedback (2026-04-05): Heat pump strategy is a cascade: (1) Use MM modulation if supported for smooth continuous control, (2) If MM not supported, fallback to PWM but max 2 cycles/hr, min 30min ON. Also: auto-detect from OT MsgID 3 as primary, dropdown as manual override only. sergeantd confirmed auto-detect approach.

Implementation progress (2026-04-05):
- Added SATHeatingSystem enum to OTGW-firmware.h (AUTO=0, RADIATORS=1, HEAT_PUMP=2, UNDERFLOOR=3)
- Added 6 helper functions in SATcontrol.ino: satGetEffectiveHeatingSystem, satGetMaxSetpoint, satGetBaseOffset, satGetMaxCyclesPerHour, satGetMinOnTimeSec, satAlwaysMaxModulation
- Refactored satCalcHeatingCurve and satApplyPWM to use new helpers
- OT MsgID 3 auto-detection in OTGW-Core.ino:2208
- Settings persistence range updated to 0-3 in settingStuff.ino
- REST API range updated to 0-3 in restAPI.ino
- MQTT subscribe handler added for set/<nodeId>/sat/heating_system
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented heating system type selection with auto-detect from OT MsgID 3. SATHeatingSystem enum, 6 helper functions, REST/MQTT/WebUI integration. 10/13 ACs complete, remaining depend on Task #4.
<!-- SECTION:FINAL_SUMMARY:END -->
