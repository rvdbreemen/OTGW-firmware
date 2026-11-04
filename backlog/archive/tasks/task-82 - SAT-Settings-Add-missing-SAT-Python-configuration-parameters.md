---
id: TASK-82
title: 'SAT Settings: Add missing SAT Python configuration parameters'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:22'
updated_date: '2026-04-07 16:31'
labels:
  - settings
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py
    (OPTIONS_DEFAULTS)
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/entry_data.py
    (SatConfig class)
  - src/OTGW-firmware/OTGW-firmware.h (SATSection struct)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Several SAT Python configuration parameters are not yet implemented in the firmware. These need to be added as settings (SATSection fields), with MQTT command topics, REST API visibility, and WebUI controls.

**Missing settings from SAT Python:**
1. `sensor_max_value_age` (time, default 6 hours) - Max age before sensor reading considered stale. Currently firmware has no staleness check on external temperature inputs.
2. `error_monitoring` (bool, default false) - Enable detailed error tracking/monitoring for diagnostics.
3. `automatic_gains_value` (float, default 2.0) - Multiplier for automatic PID gain calculation. Firmware has auto-tune but this specific multiplier is missing.
4. `heating_mode` (enum: COMFORT/ECO, default COMFORT) - ECO mode follows only primary PID, COMFORT uses secondary rooms too. Only relevant when rooms/multi-area configured.
5. `cycles_per_hour` (int, default 3-4) - Currently firmware PWM uses fixed duty cycle timing. SAT Python derives duty cycle from cycles_per_hour setting.
6. `minimum_consumption` / `maximum_consumption` (float, default 0) - Already in backlog task #52 for gas consumption sensor, but the settings themselves need to be added to SATSection.
7. `climate_valve_offset` (float, default 0) - Offset for TRV valve position detection. Related to task #67 (valves open).
8. `solar_gain_freeze_integral` (bool, default true) - Whether to freeze PID integral during solar gain. Firmware may already do this but needs explicit setting.
9. `dynamic_minimum_setpoint` (bool) - Dynamically adjust minimum setpoint based on boiler behavior.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 sensor_max_value_age setting added (uint32_t seconds, default 21600)
- [x] #2 error_monitoring bool setting added
- [x] #3 automatic_gains_value float setting added (default 2.0)
- [x] #4 heating_mode enum setting added (COMFORT/ECO)
- [x] #5 cycles_per_hour int setting added (default 3, range 2-6)
- [x] #6 climate_valve_offset float setting added (default 0, range -1 to 1)
- [x] #7 solar_gain_freeze_integral bool setting added (default true)
- [x] #8 All new settings persisted to flash via settingStuff.ino
- [x] #9 All new settings have MQTT command topics
- [x] #10 All new settings visible in REST API status JSON
- [ ] #11 All new settings in WebUI settings page
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add 7 new fields to SATSection struct in OTGW-firmware.h
2. Add persistence (save) in settingStuff.ino writeSettingsFile()
3. Add loading in settingStuff.ino updateSetting()
4. Add MQTT command handlers in MQTTstuff.ino SAT block
5. Publish new settings in satPublishMQTT() in SATcontrol.ino
6. Add to REST API JSON in restAPI.ino sendDeviceSettings()
7. Update satGetMaxCyclesPerHour() to use settings.sat.iCyclesPerHour
8. Run evaluate.py --quick to check for issues
9. Commit
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added 7 missing SAT Python config parameters to firmware.\n\nNew SATSection fields: iSensorMaxAgeS (default 21600s), bErrorMonitoring, fAutoGainsValue (default 2.0), iHeatingMode (0=COMFORT/1=ECO), iCyclesPerHour (default 3), fValveOffset (default 0.0), bSolarFreezeIntegral (default true).\n\nChanges across 5 files:\n- OTGW-firmware.h: 7 new fields with defaults\n- settingStuff.ino: save/load with range constraints\n- MQTTstuff.ino: 7 new command handlers under sat/ namespace\n- SATcontrol.ino: satGetMaxCyclesPerHour() uses settings.sat.iCyclesPerHour; all 7 fields in REST status JSON and satPublishMQTT()\n- restAPI.ino: all 7 fields in sendDeviceSettings()\n\nAC #11 (WebUI) left for Task #84. Commit: 4578fa5b"
<!-- SECTION:FINAL_SUMMARY:END -->
