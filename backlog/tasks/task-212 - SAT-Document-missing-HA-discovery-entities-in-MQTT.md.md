---
id: TASK-212
title: 'SAT: Document missing HA discovery entities in MQTT.md'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:25'
updated_date: '2026-04-09 05:45'
labels:
  - audit-fix
  - sat
  - ha
  - docs
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The MQTT.md SAT Discovery Entities section lists only 8 entities. mqttha.cfg actually defines 20+ SAT entities. The following entities are missing from the documentation in MQTT.md:\n\n- sat_room_temp (sensor, temperature)\n- sat_outside_temp (sensor, temperature)\n- sat_estimated_room (sensor, temperature)\n- sat_humidity (sensor, humidity)\n- sat_comfort_offset (sensor, no device class)\n- sat_current_modulation (sensor, no device class, fire icon)\n- sat_pressure (sensor, pressure)\n- sat_pressure_drop_rate (sensor, no device class)\n- sat_power (sensor, power)\n- sat_ble_temp (sensor, temperature, ESP32 only)\n- sat_ble_humidity (sensor, humidity, ESP32 only)\n- sat_ble_rssi (sensor, signal_strength, diagnostic, ESP32 only)\n\nAlso not documented:\n- Which topics serve as json_attributes_topic (sat/pid_attributes for sat_pid_output, sat/climate_attributes for sat_climate)\n- The action_topic and action_template on the climate entity\n- The value_template on sat_pwm_duty (multiplies by 100 to get %)\n- The entity_category: diagnostic on sat_ble_rssi\n\nNote: mqttha.cfg line 521 references sat/current_modulation but this topic is not found in SATcontrol.ino sendMQTTData calls. This may be a documentation discrepancy in mqttha.cfg itself — the topic may be unpublished or published under a different name. This needs investigation.\n\nSource: src/OTGW-firmware/data/mqttha.cfg (lines 502-525), docs/api/MQTT.md (SAT Discovery section ~line 534)."
<!-- SECTION:DESCRIPTION:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Expanded the SAT Discovery Entities section in MQTT.md from 8 entities to 45+ entities. Organized into three tables: Climate entity (with action_topic, action_template, and json_attributes_topic details), Sensor entities (38 sensors including room_temp, outside_temp, estimated_room, humidity, comfort_offset, current_modulation, pressure, pressure_drop_rate, power, energy_total, duty_ratio, manufacturer, PID terms, gains, thermal, overshoot, cycle class, curve recommendation, flame status, summer hours, auto-tune score, consumption, BLE sensors), Binary sensor entities (17 binary sensors covering health, safety, sync, and environmental states), and Number entity (sat_dhw_setpoint slider). Noted ESP32-only BLE entities. Documented key metadata: json_attributes_topic for sat_pid_output/sat_cycle_class/sat_pressure_health/sat_curve_recommendation, value_template on sat_pwm_duty, action_topic/action_template on climate, entity_category diagnostic on sat_ble_rssi.
<!-- SECTION:FINAL_SUMMARY:END -->
