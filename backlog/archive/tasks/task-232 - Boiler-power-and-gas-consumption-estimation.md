---
id: TASK-232
title: Boiler power and gas consumption estimation
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:42'
updated_date: '2026-04-09 10:48'
labels:
  - sat
  - future-sprint
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python SAT estimates boiler gas consumption as modulation% x rated_power x efficiency x time. OT MsgID 17 provides modulation level but not rated power. Solution: add a user-configurable sat_boiler_rated_kw setting (default 0 = disabled). When set, accumulate estimated kWh into a dedicated LittleFS file (/sat_energy_estimate.json). Expose via sat/energy_estimated_kwh MQTT topic and HA energy dashboard entity. OT MsgIDs 25/26 (flow/return temp) + MsgID 74 (flow rate) could enable thermal power calculation but flow rate is rarely supported by boilers.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Add sat_boiler_rated_kw (float, 0=disabled) and sat_boiler_efficiency (float, default 0.92) settings
- [x] #2 Accumulate estimated kWh: modulation% / 100 * rated_kW * efficiency * dt each loop cycle
- [x] #3 Persist accumulated value to /sat_energy_estimate.json (save every 0.1 kWh)
- [x] #4 Publish sat/energy_estimated_kwh to MQTT as retained, expose as HA energy sensor
- [x] #5 If MsgID 74 (flow rate) is available from boiler: use thermal power calculation instead of modulation estimate
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add fBoilerRatedKW and fBoilerEfficiency to SATSection in OTGW-firmware.h
2. Add fEnergyEstimatedKWh and iEstEnergyLastMs to SATRuntimeSection in OTGW-firmware.h
3. Add persist/load in settingStuff.ino (SATboilerratedkw, SATboilerefficiency)
4. Extend satUpdatePowerEnergy() in SATcontrol.ino: accumulate estimated kWh using modulation or thermal power (DHWFlowRate)
5. Add satSaveEstimatedEnergy() / satLoadEstimatedEnergy() for /sat_energy_estimate.json
6. Call load on initSAT(), save on satDisable() and on 0.1 kWh boundary in control loop
7. Publish sat/energy_estimated_kwh as retained MQTT and add HA auto-discovery entry in mqttha.cfg
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added boiler gas consumption estimation to SAT (Task #232).

Changes:
- OTGW-firmware.h: added fBoilerRatedKW (float, 0=disabled) and fBoilerEfficiency (float, default 0.92) to SATSection; added fEnergyEstimatedKWh, fEstEnergyLastSavedKWh, iEstEnergyLastMs to SATRuntimeSection
- settingStuff.ino: persist/load SATboilerratedkw and SATboilerefficiency; constrained to [0, 200] kW and [0.5, 1.0] respectively
- SATcontrol.ino:
  - satSaveEstimatedEnergy() / satLoadEstimatedEnergy() for /sat_energy_estimate.json
  - satUpdatePowerEnergy() extended: when fBoilerRatedKW > 0 and flame on, accumulates kWh using either thermal power from DHW flow rate (P = flow_L/s * 4186 * dT / 1000) or modulation estimate as fallback
  - Load on initSAT(), save on satDisable() and on each 0.1 kWh boundary in the control loop
  - Publish sat/energy_estimated_kwh as retained MQTT (only when rated power is configured)
  - Expose boiler_rated_kw, boiler_efficiency, energy_estimated_kwh in status JSON
  - Broadcast sat/boiler_rated_kw and sat/boiler_efficiency as retained MQTT settings
- mqttha.cfg: added sat_energy_estimated_kwh HA energy sensor (device_class=energy, state_class=total_increasing, unit=kWh, icon=mdi:fire)

Behavior:
- Disabled by default (fBoilerRatedKW=0)
- When enabled: accumulates gas input energy in kWh using control-loop dt
- Prefers thermal calculation if DHWFlowRate > 0.1 L/min and flow/return temps valid and delta > 0.5 C
- Falls back to modulation * ratedKW * efficiency when flow rate unavailable
- LittleFS saves on 0.1 kWh increments to balance data safety vs write wear
<!-- SECTION:FINAL_SUMMARY:END -->
