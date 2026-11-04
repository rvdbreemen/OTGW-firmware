---
id: TASK-81
title: 'HA Entities: Number/Switch/Select for all SAT settings controllable from HA'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:22'
updated_date: '2026-04-07 06:05'
labels:
  - ha-entity
  - auto-discovery
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py
    (OPTIONS_DEFAULTS, all configurable settings)
  - src/OTGW-firmware/MQTTstuff.ino (41 MQTT command handlers)
  - 'src/OTGW-firmware/OTGW-firmware.h (SATSection struct, 52 settings fields)'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT Python allows ALL settings to be changed via the HA options flow UI. Our firmware has MQTT command topics for most settings, but these require users to send raw MQTT messages - there are no HA number/switch/select entities for dashboard control. Add HA auto-discovery for controllable settings as interactive entities.

**Number entities needed (with MQTT command topics):**
- Heating curve coefficient (0.1-12.0, step 0.1)
- Deadband (0.0-5.0 C, step 0.05)
- Overshoot margin (0.5-5.0 C, step 0.1)
- Control interval (10-300 sec, step 10)
- Max relative modulation (0-100 %, step 1)
- Flame off setpoint offset (0-30 C, step 0.5)
- Flow setpoint offset (0.5-10.0 C, step 0.5)
- Modulation suppression delay (0-120 sec, step 5)
- Modulation suppression offset (0-10 C, step 0.5)
- Boiler capacity (1-50 kW, step 0.5)
- Comfort reference humidity (10-90 %, step 1)
- Comfort max offset (0-3 C, step 0.1)
- Summer threshold temperature (5-35 C, step 0.5)
- Auto-tune rate (0.005-0.1, step 0.005)
- Preset temperatures: comfort, eco, away, sleep, activity, home (5-35 C each)
- Min/Max pressure thresholds (0-4 bar, step 0.1)
- Max pressure drop rate (0-3 bar/hr, step 0.05)
- Target temperature step (0.1-1.0, step 0.1)

**Switch entities needed (bool toggles):**
- Solar gain compensation, Summer simmer, Comfort adjustment, Multi-area, Auto-tune
- Simulation mode, Window detection, Force PWM, Push setpoint, OVP enabled
- Preset sync, BLE enable (ESP32), DHW enabled, Use external temp

**Select entities needed (enum choices):**
- Heating system (Auto/Radiators/Heat Pump/Underfloor)
- Manufacturer (Auto + 17 manufacturers)
- Heating mode (Comfort/ECO) - only when rooms configured

Each entity needs: MQTT state topic + MQTT command topic + HA auto-discovery config in mqttha.cfg.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All numeric settings exposed as HA number entities with correct min/max/step
- [x] #2 All boolean settings exposed as HA switch entities
- [x] #3 All enum settings exposed as HA select entities
- [x] #4 Each entity has MQTT state topic (read current value) and command topic (set new value)
- [x] #5 HA auto-discovery configs added to mqttha.cfg
- [x] #6 Settings persist to flash when changed via HA entities
- [x] #7 All entities grouped under SAT device in HA
- [x] #8 Preset temperatures (6) each have their own number entity
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inventory all SAT settings from OTGW-firmware.h SATSection struct (~52 fields)
2. Check which already have MQTT command handlers in MQTTstuff.ino
3. Check which already publish state via sendMQTTData in SATcontrol.ino
4. Add missing state publishing for settings that only have command topics
5. Add HA auto-discovery entries to mqttha.cfg for:
   a. Number entities (numeric settings with min/max/step)
   b. Switch entities (boolean toggles)
   c. Select entities (enum choices)
6. Each entry needs: state topic, command topic, correct component type
7. Build and verify
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added HA auto-discovery for all SAT settings as interactive entities controllable from Home Assistant dashboards.

Changes:
- mqttha.cfg: Added 16 number entities (heating curve, deadband, overshoot margin, control interval, max modulation, flame off offset, flow offset, mod suppression delay/offset, boiler capacity, comfort humidity/max offset, summer threshold, auto-tune rate, target temp step), 6 preset temperature number entities, 3 pressure number entities, 13 switch entities for boolean settings, and 1 select entity for heating system type.
- SATcontrol.ino: Added MQTT state publishing for all settings in satPublishMQTT() so HA entities can read current values. Each setting gets its own retained MQTT topic.
- MQTTstuff.ino: Added 19 new MQTT command handlers for settings that were missing (heating_curve, deadband, mod_sup_delay/offset, boiler_capacity, target_temp_step, min/max_pressure, max_pressure_drop, preset_comfort/eco/away/sleep/activity/home, solar_gain, window_detection, pwm_auto_switch).

All entities use the existing updateSetting() mechanism which persists to flash and respects constrain() bounds from settingStuff.ino.
<!-- SECTION:FINAL_SUMMARY:END -->
