---
id: TASK-45
title: Power and consumption sensors
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 21:06'
updated_date: '2026-04-06 11:32'
labels:
  - sat
  - feature
dependencies: []
references:
  - other-projects/SAT-releases-thermo-nova/custom_components/sat/sensor.py
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Calculate and expose power (kW) from modulation percentage * boiler capacity, and cumulative energy consumption (kWh) tracking with min/max bounds. Useful for energy dashboards and Home Assistant energy integration.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Power sensor (kW) calculated from modulation * capacity
- [x] #2 Cumulative energy consumption sensor (kWh) with running total
- [x] #3 Min/max bounds applied to prevent erroneous readings
- [x] #4 Sensors exposed via MQTT
- [x] #5 Compatible with Home Assistant energy dashboard
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add boiler capacity setting (kW, default 24)
2. Calculate power from modulation% * capacity
3. Track cumulative kWh (integrate power over time)
4. Add min/max bounds
5. MQTT publish with HA energy dashboard compatible device_class
6. REST API
7. Commit
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation in progress:
- Added fBoilerCapacity setting to SATSection (default 24 kW)
- Added fCurrentPower, fEnergyTotal, iEnergyLastMs to SATRuntimeSection
- Added satUpdatePowerEnergy() function in SATcontrol.ino
- Called from satControlLoop() after pressure monitoring
- Added JSON status output (power_kw, energy_kwh, boiler_capacity)
- Added MQTT publishing (sat/power, sat/energy_total with retained flag)
- Added persistence in settingStuff.ino (write, updateSetting handler)
- Added to restAPI.ino (sendDeviceSettings, knownSettings whitelist)
- Added translateFields and translateTooltips entries in index.js
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Power and energy sensors for HA energy dashboard.

Changes:
- Power (kW) = modulation% * boiler capacity (configurable, default 24kW)
- Cumulative energy (kWh) integration over time
- Min/max bounds (0 to 110% capacity)
- MQTT: sat/power, sat/energy_total (retained)
- REST/WebUI status display
- Setting: SATboilercapacity (1-100 kW)
<!-- SECTION:FINAL_SUMMARY:END -->
