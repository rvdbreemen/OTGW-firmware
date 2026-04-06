---
id: TASK-80
title: 'HA Auto-Discovery: bulk sensor entities for all published MQTT topics'
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
  - src/OTGW-firmware/data/mqttha.cfg (current 12 entries)
  - 'src/OTGW-firmware/SATcontrol.ino (satPublishMQTT function, 78 topics)'
  - src/OTGW-firmware/MQTTstuff.ino (doAutoConfigure)
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The firmware publishes 78 MQTT topics but only 12 have HA auto-discovery configured in mqttha.cfg. This means most SAT data is invisible to HA users. Add HA auto-discovery entries for all published sensor data that doesn't already have one.

Missing HA discovery entries for existing MQTT topics (grouped):

**PID diagnostics (6 sensors):** sat/pid_p, sat/pid_i, sat/pid_d, sat/kp, sat/ki, sat/kd - all device_class=None, unit=None
**Control state (5 sensors):** sat/active (binary), sat/room_temp (temperature C), sat/outside_temp (temperature C), sat/current_modulation (%), sat/raw_derivative
**Cycle tracking (3 sensors):** sat/cycle_class, sat/duty_ratio, sat/overshoot_fraction
**Pressure (3 sensors):** sat/pressure (pressure bar), sat/pressure_drop_rate (bar/hr), sat/pressure_alarm (binary)
**Energy (2 sensors):** sat/power (power kW), sat/energy_total (energy kWh, state_class=total_increasing)
**Thermal learning (4 sensors):** sat/thermal_coeff, sat/thermal_drop_rate, sat/thermal_model_valid (binary), sat/estimated_room (temperature C)
**Solar/Summer (4 sensors):** sat/solar_gain (binary), sat/indoor_rise_rate, sat/summer_active (binary), sat/summer_hours_above
**Comfort (3 sensors):** sat/humidity (humidity %), sat/humidity_valid (binary), sat/comfort_offset (temperature C)
**Diagnostics (4 sensors):** sat/safety_tripped (binary), sat/window_open (binary), sat/modulation_reliable (binary), sat/setpoint_mismatch (binary)
**Auto-tune (2 sensors):** sat/auto_tune_score, sat/auto_tune_active (binary)
**Other (2 sensors):** sat/manufacturer, sat/curve_recommendation

Total: ~38 new HA discovery entries needed in mqttha.cfg. Use proper device_class, unit_of_measurement, and state_class where applicable. Energy sensor must use state_class=total_increasing for HA energy dashboard integration.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All 78 published MQTT topics have corresponding HA auto-discovery entries in mqttha.cfg
- [ ] #2 Each sensor has correct device_class (temperature, pressure, power, energy, humidity, etc.)
- [ ] #3 Each sensor has correct unit_of_measurement
- [ ] #4 Energy sensor (sat/energy_total) has state_class=total_increasing for HA energy dashboard
- [ ] #5 Binary topics use binary_sensor discovery (not sensor)
- [ ] #6 All entities grouped under the SAT device in HA
- [ ] #7 Existing 12 discovery entries preserved and unchanged
<!-- AC:END -->
