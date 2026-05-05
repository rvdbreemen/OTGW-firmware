---
id: TASK-543
title: >-
  feat-2.0.0: HA discovery for SAT user-facing topics (~55 sensors + dynamic
  zones)
status: Done
assignee:
  - '@copilot'
created_date: '2026-05-05 07:47'
updated_date: '2026-05-05 15:24'
labels:
  - mqtt
  - ha-discovery
  - sat
  - feat-2.0.0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add Home Assistant discovery for the ~55 SAT/OTDirect user-facing MQTT topics on feature-dev-2.0.0-otgw32-esp32-sat-support that are currently raw-published. Out-of-scope for TASK-541 (which only covers diagnostic topics) because these are mostly primary entities (climate-style values, statistics, zones) with non-trivial design choices.\n\nIn-scope topic groups:\n- SAT control: sat/{target,mode,setpoint,heating_curve,pid_output,error,active,room_temp,outside_temp} (9)\n- SAT PID tunings: sat/{pid_p,pid_i,pid_d,pid_attributes(JSON),raw_derivative,kp,ki,kd} (8)\n- SAT cycle/duty: sat/{boiler_status,cycle_class,cycle_attributes(JSON),pwm_duty,duty_ratio,overshoot_fraction,cycle_phase,overshoot_margin} (8)\n- SAT statistics: sat/{cycles_this_hour,4h_cycles,4h_avg_on_sec,4h_avg_off_sec,4h_avg_flow_temp,4h_duty_ratio,4h_overshoot_fraction,4h_underheat_fraction,4h_flow_ret_delta_p50,4h_flow_ret_delta_p90} (10)\n- SAT safety/flame: sat/{safety_tripped,flame_status,flame_health,valves_open} (4)\n- SAT BLE primaries: sat/{ble_temp,ble_humidity} (2)\n- SAT pressure measurement: sat/ch_pressure (1)\n- SAT weather: sat/weather/{temperature,apparent_temp,humidity,wind_speed,wind_direction,wind_gusts,cloud_cover,pressure_msl,precipitation,rain,snowfall,weather_code,is_day} (13)\n- SAT zones (dynamic): sat/zone/<n>/{active,output,error} per zone (n=1..SAT_MAX_ZONES)\n\nDesign considerations to settle before implementing:\n- Which entries are 'primary' vs 'diagnostic' entity_category? (Currently the audit assumes most are primary.)\n- Correct units / device_class / state_class for each (kp/ki/kd are unitless, weather follows HA standard units, etc.)\n- JSON-blob topics (pid_attributes, cycle_attributes) need value_template or per-attribute discovery — NOT plain stat_t.\n- Dynamic zone topics: pre-allocate SAT_MAX_ZONES discovery configs at boot, or only publish discovery for currently-active zones?\n- Gating: SAT is compile-time (#if defined(ENABLE_SAT)). Conditionally compile array entries, or always include them and let HA show 'unavailable' when no data?\n- Reuses existing MqttHaSensorCfg pipeline (TASK-540 pattern with new pseudo-IDs 251+).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Discovery covers all 9 SAT control topics with sensible units/classes (target/setpoint/room/outside as °C with measurement; mode/active as text/binary)
- [x] #2 Discovery covers all 8 PID tuning topics; pid_attributes/cycle_attributes use JSON value_templates
- [x] #3 Discovery covers all 8 cycle/duty topics
- [x] #4 Discovery covers all 10 statistics topics (cycles_this_hour as total_increasing; 4h_* as measurement)
- [x] #5 Discovery covers all 4 safety/flame topics
- [x] #6 Discovery covers ble_temp and ble_humidity as primary (temp/humidity device_class, measurement)
- [x] #7 Discovery covers ch_pressure as primary (pressure device_class)
- [x] #8 Discovery covers all 13 weather topics with HA-standard units (°C, %, m/s, hPa, mm, etc.)
- [x] #9 Dynamic zone topics handled: SAT_MAX_ZONES configs published at boot (or runtime-conditional, decision documented)
- [x] #10 Gating decision documented: either #if ENABLE_SAT around array entries, or unconditional with 'unavailable' fallback
- [x] #11 Build (./build.sh) succeeds for ESP8266 + ESP32-S3
- [x] #12 evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Started implementation in the 2.0.0 worktree.
- Reusing the archived SAT HA mappings as baseline, but scoping strictly to the TASK-543 topic groups.
- Planned approach: add user-facing SAT pseudo-IDs above 251, extend discovery metadata for JSON attributes / custom templates where needed, and keep zones runtime-conditional (configured or active only).

- Implemented TASK-543 in the 2.0.0 worktree by extending HA discovery metadata with custom value templates, JSON attribute topics, and binary payload conventions.
- Added new SAT pseudo-IDs 252..255 for user-facing SAT discovery and kept TASK-541 diagnostics isolated on 251.
- Zone discovery now publishes only for configured or recently active zones, using dedicated accessors from SATcontrol.ino to avoid coupling discovery logic to local SAT storage.
- Gating decision documented in OTGW-firmware.h: discovery is unconditional on the 2.0.0 dual-target branch, while runtime/platform publishers determine live availability (for example ESP32-only weather/BLE topics on ESP8266).
- Build succeeded for ESP8266 and ESP32-S3; evaluate.py --quick finished with no failures.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented Home Assistant discovery for the TASK-543 SAT user-facing MQTT surface on the 2.0.0 worktree.

Changes:
- Added user-facing SAT discovery entries under new pseudo-IDs 252..255, keeping existing TASK-541 diagnostics on pseudo-ID 251.
- Extended the discovery metadata/composer so SAT entries can define custom value templates, JSON attribute topics, and non-default binary payload conventions.
- Covered the SAT control, PID, cycle/duty, rolling statistics, BLE primary, CH pressure, weather, and safety/flame topic groups with Home Assistant-friendly units/classes.
- Converted ratio-style SAT values to percent where appropriate, converted weather wind topics from the published km/h stream to HA-standard m/s in discovery, and represented weather is_day as a 0/1 binary sensor.
- Wired pid_output and cycle_class to their JSON companion topics via json_attributes_topic instead of publishing opaque blob entities.
- Added dynamic zone discovery that only emits configs for zones that are currently configured or still active from recent SAT runtime data, and documented the unconditional gating decision for the dual-target branch.

Files:
- src/OTGW-firmware/MQTTHaDiscovery.cpp
- src/OTGW-firmware/MQTTstuff.h
- src/OTGW-firmware/MQTTstuff.ino
- src/OTGW-firmware/OTGW-firmware.h
- src/OTGW-firmware/SATcontrol.ino

Validation:
- ./build.sh
- python3 evaluate.py --quick
<!-- SECTION:FINAL_SUMMARY:END -->
