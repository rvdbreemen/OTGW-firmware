---
id: TASK-81
title: 'HA Entities: Number/Switch/Select for all SAT settings controllable from HA'
status: To Do
assignee: []
created_date: '2026-04-06 19:22'
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
- [ ] #1 All numeric settings exposed as HA number entities with correct min/max/step
- [ ] #2 All boolean settings exposed as HA switch entities
- [ ] #3 All enum settings exposed as HA select entities
- [ ] #4 Each entity has MQTT state topic (read current value) and command topic (set new value)
- [ ] #5 HA auto-discovery configs added to mqttha.cfg
- [ ] #6 Settings persist to flash when changed via HA entities
- [ ] #7 All entities grouped under SAT device in HA
- [ ] #8 Preset temperatures (6) each have their own number entity
<!-- AC:END -->
