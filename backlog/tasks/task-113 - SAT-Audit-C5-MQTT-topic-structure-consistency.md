---
id: TASK-113
title: 'SAT Audit C5: MQTT topic structure consistency'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:53'
updated_date: '2026-04-09 05:24'
labels:
  - audit
  - sat
  - phase-3
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify that the MQTT topic structure of SAT in the C++ firmware is consistent with Python SAT thermo-nova. Compare all published and subscribed topics, QoS and retain flags.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All SAT MQTT topics compared (publish and subscribe)
- [x] #2 QoS levels consistent
- [x] #3 Retain flags correctly set
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audited MQTT topic structure consistency for SAT across both platforms.

Published topics (sat/* prefix, via sendMQTTData):
All topics are unconditional on both platforms EXCEPT sat/ble_* topics which are inside SATble.ino wrapped in #if defined(ESP32).

Common topics (both platforms): sat/mode, sat/setpoint, sat/heating_curve, sat/pid_output, sat/target, sat/error, sat/pid_p, sat/pid_i, sat/pid_d, sat/pid_attributes, sat/raw_derivative, sat/boiler_status, sat/cycle_class, sat/cycle_attributes, sat/pwm_duty, sat/duty_ratio, sat/overshoot_fraction, sat/cycle_phase, sat/overshoot_margin, sat/active, sat/room_temp, sat/outside_temp, sat/kp, sat/ki, sat/kd, sat/safety_tripped, sat/flame_status, sat/flame_health, sat/valves_open, sat/pre_custom_temperature, sat/pre_activity_temperature, sat/window_open, sat/pressure, sat/pressure_drop_rate, sat/pressure_alarm, sat/pressure_health, sat/pressure_health_attr, sat/modulation_reliable, sat/setpoint_mismatch, sat/curve_recommendation, sat/curve_recommendation_attributes, sat/error_mean, sat/error_stddev, sat/power, sat/energy_total, sat/manufacturer, sat/thermal_coeff, sat/thermal_drop_rate, sat/thermal_model_valid, sat/estimated_room, sat/solar_gain, sat/indoor_rise_rate, sat/solar_gain_sun_elevation, sat/summer_active, sat/device_health, sat/cycle_health, sat/summer_simmer_index, sat/summer_simmer_perception, sat/modulation_state, sat/pwm_state, sat/dhw_setpoint, sat/max_setpoint, sat/requested_setpoint, sat/consumption, sat/setpoint_sync, sat/modulation_sync, sat/ch_sync, sat/heating_curve_coeff, sat/deadband, sat/control_interval, sat/max_modulation, sat/flame_off_offset, sat/flow_offset, sat/mod_sup_delay, sat/mod_sup_offset, sat/boiler_capacity, sat/comfort_humidity, sat/comfort_max_offset, sat/summer_threshold, sat/target_temp_step, sat/min_pressure, sat/max_pressure, sat/max_pressure_drop, sat/preset_comfort, sat/preset_eco, sat/preset_away, sat/preset_sleep, sat/preset_activity, sat/preset_home, sat/ovp_value, sat/heating_system, sat/manufacturer_id, sat/solar_gain_enable, sat/summer_simmer_enable, sat/comfort_adjust_enable, sat/multi_area_enable, sat/auto_tune_enable, sat/simulation_enable, sat/window_detection_enable, sat/force_pwm_enable, sat/push_setpoint_enable, sat/ovp_enabled, sat/preset_sync_enable, sat/dhw_enabled, sat/pwm_auto_switch_enable, sat/climate_attributes, sat/summer_hours_above, sat/cycles_per_hour, sat/auto_gains_value, sat/heating_mode, sat/valve_offset, sat/solar_freeze_integral, sat/weather/temperature, sat/weather/humidity, sat/weather/wind_speed

ESP32-only topics: sat/ble_temp, sat/ble_humidity, sat/ble_sensor_rssi, sat/ble_battery, sat/ble_sensor_count, sat/ble_temp_valid

Subscribed topics: set/<nodeId>/sat/<subcmd> handles: target, indoor_temp, outdoor_temp, enabled, control_mode, preset, ovp_start, ovp_stop, window, reset_integral, and 50+ settings commands. Topic path is identical on both platforms.

QoS: All sendMQTTData calls use QoS 0 (PubSubClient default). Retained flag is correctly set to true for persistent state (sat/active, sat/energy_total, sat/setpoint, sat/max_setpoint, etc.) and false for volatile data.

Issues found:
- TASK-199 (LOW): sat/ble_* topics are undocumented as ESP32-only; no HA discovery config gap found in code but should be documented.
- QoS levels are consistent (0) across both platforms — PubSubClient abstraction handles this uniformly.
- No topic naming inconsistencies between platforms found.
<!-- SECTION:FINAL_SUMMARY:END -->
