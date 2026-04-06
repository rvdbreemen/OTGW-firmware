---
id: TASK-37
title: Simulation mode for testing without boiler
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 20:52'
updated_date: '2026-04-06 10:53'
labels:
  - sat
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Simulation mode that emulates boiler behavior for testing SAT control logic without a real boiler connected. SAT Python has simulated_heating, simulated_cooling, and simulated_warming_up configs that model thermal behavior.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py (CONF_SIMULATED_*)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Simulated heating: model temperature rise when flame on
- [x] #2 Simulated cooling: model temperature drop when flame off
- [x] #3 Simulated warming up: model initial heating phase
- [x] #4 Configurable enable/disable simulation mode
- [x] #5 WebUI: simulation mode toggle and simulated temp display
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add simulation settings (enable, heating rate, cooling rate, warmup time)
2. Add simulation state tracking (simulated room temp, flow temp)
3. Model thermal behavior: heating=rise toward setpoint, cooling=decay toward ambient, warmup=initial ramp
4. Override OT readings when simulation active
5. WebUI toggle + display
6. MQTT status
7. Commit
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation complete:
- OTGW-firmware.h: added bSimulation, fSimHeatRate, fSimCoolRate to SATSection; added fSimRoomTemp, fSimFlowTemp, fSimOutdoorTemp, iSimLastUpdateMs, bSimWarmupDone to SATRuntimeSection
- SATcontrol.ino: added satUpdateSimulation() with thermal model; modified satGetRoomTemp/satGetOutsideTemp to return simulated values; added simulation fields to JSON status and MQTT publish
- settingStuff.ino: persistence for SATsimulation, SATsimheatrate, SATsimcoolrate
- restAPI.ino: added to sendDeviceSettings and knownSettings whitelist
- MQTTstuff.ino: added sat/simulation MQTT command handler
- sat.js: simulation badge, toggle button, state tracking
- index.html: simulation badge and button in Controls section
- index.css/index_dark.css: sat-badge-sim styling
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Simulation mode for testing SAT without boiler hardware.

Changes:
- Thermal model: room temp heating/cooling, flow temp warmup/ramp
- Configurable rates (SATsimheatrate, SATsimcoolrate)
- Overrides satGetRoomTemp()/satGetOutsideTemp() when active
- Settings persistence + REST/MQTT/WebUI controls
- Purple simulation badge in dashboard
<!-- SECTION:FINAL_SUMMARY:END -->
