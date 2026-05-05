---
id: TASK-543
title: >-
  feat-2.0.0: HA discovery for SAT user-facing topics (~55 sensors + dynamic
  zones)
status: To Do
assignee: []
created_date: '2026-05-05 07:47'
updated_date: '2026-05-05 07:47'
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
- [ ] #1 Discovery covers all 9 SAT control topics with sensible units/classes (target/setpoint/room/outside as °C with measurement; mode/active as text/binary)
- [ ] #2 Discovery covers all 8 PID tuning topics; pid_attributes/cycle_attributes use JSON value_templates
- [ ] #3 Discovery covers all 8 cycle/duty topics
- [ ] #4 Discovery covers all 10 statistics topics (cycles_this_hour as total_increasing; 4h_* as measurement)
- [ ] #5 Discovery covers all 4 safety/flame topics
- [ ] #6 Discovery covers ble_temp and ble_humidity as primary (temp/humidity device_class, measurement)
- [ ] #7 Discovery covers ch_pressure as primary (pressure device_class)
- [ ] #8 Discovery covers all 13 weather topics with HA-standard units (°C, %, m/s, hPa, mm, etc.)
- [ ] #9 Dynamic zone topics handled: SAT_MAX_ZONES configs published at boot (or runtime-conditional, decision documented)
- [ ] #10 Gating decision documented: either #if ENABLE_SAT around array entries, or unconditional with 'unavailable' fallback
- [ ] #11 Build (./build.sh) succeeds for ESP8266 + ESP32-S3
- [ ] #12 evaluate.py --quick shows no new failures
<!-- AC:END -->
