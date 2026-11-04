---
id: TASK-209
title: 'SAT: Expand MQTT.md SAT section with all missing published topics'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:24'
updated_date: '2026-04-09 05:45'
labels:
  - audit-fix
  - sat
  - docs
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The SAT section in docs/api/MQTT.md documents only 8 published topics. The firmware actually publishes 80+ sat/* topics. The following categories are entirely missing from MQTT.md:\n\n- sat/active, sat/kp, sat/ki, sat/kd, sat/raw_derivative, sat/pid_attributes (JSON)\n- sat/cycle_class (string, not numeric), sat/cycle_attributes (JSON), sat/cycle_phase, sat/duty_ratio, sat/overshoot_fraction\n- sat/flame_status, sat/flame_health, sat/ch_sync\n- sat/room_temp, sat/outside_temp, sat/estimated_room, sat/humidity, sat/humidity_valid, sat/comfort_offset\n- sat/pressure, sat/pressure_drop_rate, sat/pressure_alarm, sat/pressure_health, sat/pressure_health_attr (JSON)\n- sat/power, sat/energy_total\n- sat/curve_recommendation, sat/curve_recommendation_attributes (JSON)\n- sat/preset_comfort/eco/away/sleep/activity/home (6 topics)\n- All settings-echo topics (30+ sat/* retained topics published on settings change)\n- sat/thermal_coeff, sat/thermal_drop_rate, sat/thermal_model_valid\n- sat/solar_gain, sat/indoor_rise_rate, sat/solar_gain_sun_elevation\n- sat/summer_active, sat/summer_hours_above\n- sat/simulation, sat/auto_tune\n- sat/climate_attributes (JSON)\n- sat/ble_temp, sat/ble_humidity, sat/ble_sensor_rssi, sat/ble_battery, sat/ble_sensor_count, sat/ble_temp_valid\n- sat/pre_custom_temperature, sat/pre_activity_temperature\n- sat/modulation_reliable, sat/setpoint_mismatch, sat/valves_open, sat/window_open\n\nAlso missing from the subscribed topics section:\n- sat/ovp_start, sat/ovp_stop, sat/ovp_value, sat/ovp_enabled\n- sat/reset_integral\n- sat/window, sat/valves_open\n- sat/sun_elevation, sat/solar_min_elevation\n- sat/area/0 through sat/area/3\n- All 40+ settings commands (sat/overshoot_margin, sat/heating_system, sat/manufacturer, etc.)\n\nThe complete inventory is in backlog/docs/sat-mqtt-topics.md. Update MQTT.md to reference or include this content.\n\nSource: docs/api/MQTT.md (SAT section lines 249-267), src/OTGW-firmware/SATcontrol.ino (lines 1271-1881), MQTTstuff.ino (lines 693-866)."
<!-- SECTION:DESCRIPTION:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Expanded the MQTT.md SAT section from 8 topics to 80+ topics. Organized into subsections: core control state, PID components, temperature sensors, boiler/cycle status, system health/safety, pressure monitoring, power/energy, heating curve recommendation, presets (6), settings echo topics (30+), thermal model, solar gain, summer simmer, climate attributes, and BLE topics (ESP32 only). Added value tables for boiler status codes (0-14), cycle class strings, and flame status strings. Linked to full sat-mqtt-topics.md reference doc.
<!-- SECTION:FINAL_SUMMARY:END -->
