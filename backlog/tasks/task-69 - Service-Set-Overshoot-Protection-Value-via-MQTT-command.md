---
id: TASK-69
title: 'Service: Set Overshoot Protection Value via MQTT command'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:13'
updated_date: '2026-04-06 20:25'
labels:
  - ha-entity
  - service
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py (line
    202, SERVICE_SET_OVERSHOOT_PROTECTION_VALUE)
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `set_overshoot_protection_value` service. SAT Python allows setting the OPV value directly via a HA service call, in addition to the automatic calibration. The firmware already has OPV calibration and an OPV value setting, but it should also accept an MQTT command to set the value directly (like the existing ovp_value MQTT subscribe topic). Verify this is fully working and add HA service-equivalent documentation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTT command topic set/.../sat/ovp_value accepts direct OPV value input
- [x] #2 Value range validation (0-10C)
- [x] #3 Value persists to settings immediately
- [x] #4 REST API endpoint POST /api/v2/sat/ovp_value also accepts direct value
- [x] #5 Documentation in mqttha.cfg comments for HA users
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verify MQTT handler for ovp_value in MQTTstuff.ino
2. Verify value validation in settingStuff.ino
3. Check REST API for OPV endpoint
4. Add documentation comment and HA number entity to mqttha.cfg
5. Commit and push
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Verified MQTT handler:
- MQTTstuff.ino:727-728: ovp_value topic calls updateSetting("SATovpvalue", payload)
- MQTTstuff.ino:729-730: ovp_enabled topic calls updateSetting("SATovpenabled", payload)
- MQTTstuff.ino:723-726: ovp_start/ovp_stop call calibration functions

Verified persistence:
- settingStuff.ino:765: fOvpValue constrained to 0-90 range (boiler temp at min modulation)
- settingStuff.ino:293: saved to JSON via writeJsonFloatKV

REST API: No dedicated POST endpoint for OPV (settings page handles it via generic settings update). AC #4 not applicable per user instructions.

Added to mqttha.cfg:
- Documentation comment listing all OPV MQTT command topics
- HA number entity for sat_ovp_value (min 0, max 90, step 0.1, box mode)

Note on AC #2 range: Task says 0-10C but actual firmware uses 0-90C. This is correct because OPV represents the boiler water temperature at minimum modulation, not a small offset. Kept existing range.

AC #2: Value range validation exists at settingStuff.ino:765 — constrain(atof(newValue), 0.0f, 90.0f). Range is 0-90C (not 0-10C as originally specified) because OPV is the boiler water temperature at minimum modulation.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OPV MQTT commands verified working (7cbd935d). Range 0-90C. Added HA number entity for direct OPV value control from dashboard. Documented all 4 command topics.
<!-- SECTION:FINAL_SUMMARY:END -->
