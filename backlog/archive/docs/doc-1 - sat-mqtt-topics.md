---
id: doc-1
title: sat-mqtt-topics
type: other
created_date: '2026-04-09 05:21'
---
# SAT MQTT Topics Reference

This document provides a complete inventory of all MQTT topics published and subscribed by the SAT (Smart Autotune Thermostat) module in OTGW-firmware.

All topics exist under the standard MQTT namespace:
- **Published**: `{TopTopic}/value/{UniqueId}/sat/<subtopic>`
- **Subscribed**: `{TopTopic}/set/{UniqueId}/sat/<subtopic>`

Where `{TopTopic}` defaults to `OTGW` and `{UniqueId}` defaults to `otgw-{MAC}`.

---

## Published Topics

Published every control loop interval (default 30 s) when SAT is enabled. Retain flag is noted per topic.

### Core Control State

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/mode` | `"off"` / `"continuous"` / `"pwm"` | yes | Current control mode |
| `sat/active` | `"true"` / `"false"` | yes | Whether SAT controller is actively controlling |
| `sat/setpoint` | `"43.1"` | yes | Final flow temperature setpoint sent to boiler (°C) |
| `sat/target` | `"21.0"` | yes | Target room temperature (°C) |
| `sat/heating_curve` | `"42.3"` | yes | Heating curve calculated value (°C) |
| `sat/pid_output` | `"43.1"` | yes | PID output = curve + P + I + D (°C) |
| `sat/overshoot_margin` | `"2.0"` | yes | Configured overshoot margin (°C) |

### PID Components

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/error` | `"0.50"` | no | PID error = target - room temp (°C) |
| `sat/pid_p` | `"0.82"` | no | Proportional term (°C) |
| `sat/pid_i` | `"0.03"` | no | Integral term (°C) |
| `sat/pid_d` | `"-0.04"` | no | Derivative term (°C) |
| `sat/pid_attributes` | JSON | no | Extended PID attributes (JSON object, see below) |
| `sat/raw_derivative` | `"-0.04"` | no | Raw (unfiltered) derivative term |
| `sat/kp` | `"5.0"` | no | Current Kp gain |
| `sat/ki` | `"0.5"` | no | Current Ki gain |
| `sat/kd` | `"0.05"` | no | Current Kd gain |
| `sat/error_mean` | `"0.12"` | no | Mean PID error over ring buffer (when error monitoring enabled) |
| `sat/error_stddev` | `"0.34"` | no | Std deviation of PID error over ring buffer |

### Temperature Sensors

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/room_temp` | `"20.8"` | no | Room temperature used by PID (°C) |
| `sat/outside_temp` | `"8.0"` | no | Outside temperature used by heating curve (°C) |
| `sat/estimated_room` | `"20.5"` | no | Estimated room temp during sensor fallback (°C) |
| `sat/humidity` | `"58.0"` | no | Indoor humidity % (when humidity sensor active) |
| `sat/humidity_valid` | `"true"` / `"false"` | no | Whether humidity reading is valid |
| `sat/comfort_offset` | `"0.5"` | no | Comfort adjustment applied to target (°C) |

### Boiler and Cycle Status

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/boiler_status` | `"3"` | no | Boiler status code (0-14, see code table below) |
| `sat/flame_status` | `"healthy"` | no | Flame health status string |
| `sat/flame_health` | `"ON"` / `"OFF"` | yes | Binary: ON = flame healthy |
| `sat/cycle_class` | `"good"` | no | Last cycle classification string |
| `sat/cycle_attributes` | JSON | no | Extended cycle attributes (JSON, see below) |
| `sat/cycle_phase` | `"startup"` / `"steady"` / `"cooldown"` / `"idle"` | no | Current cycle phase |
| `sat/pwm_duty` | `"0.65"` | no | PWM duty cycle (0.00-1.00) |
| `sat/duty_ratio` | `"0.70"` | no | EMA duty ratio (flame-on fraction) |
| `sat/overshoot_fraction` | `"0.12"` | no | EMA fraction of cycles with overshoot |

### System Health and Safety

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/safety_tripped` | `"false"` | no | Safety shutdown active |
| `sat/valves_open` | `"true"` / `"false"` | no | Whether TRV valves are reported open |
| `sat/window_open` | `"false"` | no | Window-open detection state |
| `sat/ch_sync` | `"ON"` / `"OFF"` | yes | CH sync health (ON = OK) |
| `sat/modulation_reliable` | `"true"` | no | Whether boiler modulation feedback is reliable |
| `sat/setpoint_mismatch` | `"false"` | no | Setpoint mismatch between SAT and OT bus detected |

### Pressure Monitoring

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/pressure` | `"1.5"` | no | Smoothed system pressure (bar) |
| `sat/pressure_drop_rate` | `"-0.02"` | no | Pressure drop rate (bar/hour, negative = dropping) |
| `sat/pressure_alarm` | `"false"` | no | Pressure alarm active |
| `sat/pressure_health` | `"ON"` / `"OFF"` | yes | Binary: ON = pressure healthy |
| `sat/pressure_health_attr` | JSON | no | Extended pressure health attributes |

### Power and Energy

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/power` | `"12.5"` | no | Current boiler power (kW, modulation × capacity) |
| `sat/energy_total` | `"234.5"` | yes | Cumulative energy (kWh, retained for HA energy dashboard) |

### Heating Curve Recommendation

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/curve_recommendation` | `"increase"` / `"decrease"` / `"hold"` / `"insufficient"` | no | Automatic heating curve recommendation |
| `sat/curve_recommendation_attributes` | JSON | no | Extended recommendation attributes (error stats, sample count) |

### Presets

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/preset_comfort` | `"21.0"` | yes | Comfort preset target temperature (°C) |
| `sat/preset_eco` | `"18.0"` | yes | Eco preset target temperature (°C) |
| `sat/preset_away` | `"15.0"` | yes | Away preset target temperature (°C) |
| `sat/preset_sleep` | `"16.0"` | yes | Sleep preset target temperature (°C) |
| `sat/preset_activity` | `"10.0"` | yes | Activity preset target temperature (°C) |
| `sat/preset_home` | `"18.0"` | yes | Home preset target temperature (°C) |

### Boiler Configuration (Settings Echo)

Published when SAT settings change. Retained.

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/heating_curve_coeff` | `"1.5"` | yes | Heating curve coefficient |
| `sat/deadband` | `"0.25"` | yes | PID deadband (°C) |
| `sat/control_interval` | `"30"` | yes | Control loop interval (seconds) |
| `sat/max_modulation` | `"100"` | yes | Max relative modulation % |
| `sat/flame_off_offset` | `"0.0"` | yes | Setpoint offset when flame is off (°C) |
| `sat/flow_offset` | `"2.0"` | yes | Continuous mode max setpoint drop (°C) |
| `sat/mod_sup_delay` | `"20.0"` | yes | Modulation suppression delay (seconds) |
| `sat/mod_sup_offset` | `"1.0"` | yes | Modulation suppression offset (°C) |
| `sat/boiler_capacity` | `"24.0"` | yes | Boiler capacity in kW |
| `sat/comfort_humidity` | `"50.0"` | yes | Reference humidity for comfort adjustment |
| `sat/comfort_max_offset` | `"1.0"` | yes | Max comfort offset (°C) |
| `sat/summer_threshold` | `"18.0"` | yes | Summer simmer outdoor temperature threshold (°C) |
| `sat/target_temp_step` | `"0.5"` | yes | Target temperature UI step size |
| `sat/min_pressure` | `"0.8"` | yes | Minimum acceptable pressure (bar) |
| `sat/max_pressure` | `"2.5"` | yes | Maximum acceptable pressure (bar) |
| `sat/max_pressure_drop` | `"0.3"` | yes | Maximum acceptable drop rate (bar/hour) |
| `sat/ovp_value` | `"52.5"` | yes | OPV calibrated maximum flow temperature (°C) |
| `sat/ovp_enabled` | `"false"` | yes | Whether OPV is enabled |
| `sat/heating_system` | `"1"` | yes | Heating system type (0=auto, 1=radiators, 2=heat pump, 3=underfloor) |
| `sat/manufacturer_id` | `"0"` | yes | Manufacturer ID (0=auto) |
| `sat/solar_gain_enable` | `"false"` | yes | Solar gain compensation enabled |
| `sat/summer_simmer_enable` | `"false"` | yes | Summer simmer mode enabled |
| `sat/comfort_adjust_enable` | `"false"` | yes | Thermal comfort humidity adjustment enabled |
| `sat/multi_area_enable` | `"false"` | yes | Multi-area room temperature enabled |
| `sat/auto_tune_enable` | `"false"` | yes | Auto-tune PID gains enabled |
| `sat/simulation_enable` | `"false"` | yes | Simulation mode enabled |
| `sat/window_detection_enable` | `"false"` | yes | Window open detection enabled |
| `sat/force_pwm_enable` | `"false"` | yes | Force PWM mode enabled |
| `sat/push_setpoint_enable` | `"false"` | yes | Push setpoint to thermostat display enabled |
| `sat/preset_sync_enable` | `"false"` | yes | Preset sync to secondary entities enabled |
| `sat/dhw_enabled` | `"false"` | yes | DHW control in standalone/fallback mode enabled |
| `sat/pwm_auto_switch_enable` | `"false"` | yes | PWM auto-switch between modes enabled |

### Manufacturer and Thermal Model

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/manufacturer` | `"Remeha"` | yes | Detected/configured boiler manufacturer name |
| `sat/thermal_coeff` | `"0.05"` | yes | Learned thermal drop coefficient (C/hr per C delta) |
| `sat/thermal_drop_rate` | `"0.03"` | no | Current thermal drop rate sample |
| `sat/thermal_model_valid` | `"true"` | yes | Whether thermal model has sufficient data |

### Solar Gain

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/solar_gain` | `"true"` / `"false"` | no | Solar gain compensation currently active |
| `sat/indoor_rise_rate` | `"0.8"` | no | Measured indoor temperature rise rate (°C/hr) |
| `sat/solar_gain_sun_elevation` | `"32.5"` | no | Current sun elevation (degrees, from HA or internal) |

### Summer Simmer

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/summer_active` | `"false"` | no | Summer simmer mode currently suppressing heating |
| `sat/summer_hours_above` | `"4.5"` | no | Hours outdoor temp has been above threshold |

### Simulation and Auto-Tune

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/simulation` | `"ON"` / `"OFF"` | yes | Simulation mode active |
| `sat/auto_tune` | `"ON"` / `"OFF"` | yes | Auto-tune analysis active |

### Climate Entity Attributes

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/climate_attributes` | JSON | no | HA climate entity JSON attributes (see below) |

### BLE Temperature Sensor (ESP32 only)

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/ble_temp` | `"20.5"` | no | BLE sensor temperature (°C) |
| `sat/ble_humidity` | `"55.0"` | no | BLE sensor humidity (%) |
| `sat/ble_sensor_rssi` | `"-72"` | no | BLE sensor RSSI (dBm) |
| `sat/ble_battery` | `"85"` | no | BLE sensor battery level (%) |
| `sat/ble_sensor_count` | `"1"` | no | Number of active BLE sensors seen |
| `sat/ble_temp_valid` | `"true"` | no | BLE temperature reading is valid and non-stale |

### Pre-Setpoint Tracking

| Sub-topic | Example Value | Retained | Description |
|-----------|--------------|----------|-------------|
| `sat/pre_custom_temperature` | `"21.0"` | no | Target temp before last preset change (when applicable) |
| `sat/pre_activity_temperature` | `"19.0"` | no | Target temp before last window-open event |

---

## Subscribed Topics (Commands)

All subscribed under `{TopTopic}/set/{UniqueId}/sat/<subtopic>`.

### Core Control Commands

| Sub-topic | Example Payload | Description |
|-----------|----------------|-------------|
| `sat/target` | `"21.5"` | Set target room temperature (5.0–30.0 °C, persisted) |
| `sat/indoor_temp` | `"20.8"` | Push external indoor temperature (expires after sensor max age) |
| `sat/outdoor_temp` | `"8.0"` | Push external outdoor temperature (expires after sensor max age) |
| `sat/enabled` | `"true"` / `"false"` | Enable or disable SAT controller |
| `sat/control_mode` | `"continuous"` / `"pwm"` / `"auto"` or `"0"` / `"1"` / `"2"` | Set control mode |
| `sat/preset` | `"away"` / `"eco"` / `"comfort"` / `"sleep"` / `"activity"` / `"home"` | Activate a preset |

### Window and Humidity

| Sub-topic | Example Payload | Description |
|-----------|----------------|-------------|
| `sat/window` | `"open"` / `"closed"` / `"1"` / `"0"` / `"ON"` | Window open/closed detection |
| `sat/humidity` | `"62.5"` | Push indoor humidity (0–100%) |
| `sat/valves_open` | `"true"` / `"false"` / `"1"` / `"open"` | Report TRV valves open state |

### Multi-Area Temperature

| Sub-topic | Example Payload | Description |
|-----------|----------------|-------------|
| `sat/area/0` | `"21.3"` | Push area 0 temperature (°C) |
| `sat/area/1` | `"20.1"` | Push area 1 temperature (°C) |
| `sat/area/2` | `"19.8"` | Push area 2 temperature (°C) |
| `sat/area/3` | `"22.0"` | Push area 3 temperature (°C) |

### Solar and Sun Elevation

| Sub-topic | Example Payload | Description |
|-----------|----------------|-------------|
| `sat/sun_elevation` | `"32.5"` | Push sun elevation in degrees (from HA sun entity) |
| `sat/solar_min_elevation` | `"12.0"` | Set minimum sun elevation for solar gain activation |

### OPV Calibration

| Sub-topic | Example Payload | Description |
|-----------|----------------|-------------|
| `sat/ovp_start` | (any) | Start OPV calibration sequence |
| `sat/ovp_stop` | (any) | Cancel OPV calibration |
| `sat/ovp_value` | `"52.5"` | Manually set OPV value (°C) |
| `sat/ovp_enabled` | `"true"` / `"false"` | Enable or disable OPV |

### PID Control

| Sub-topic | Example Payload | Description |
|-----------|----------------|-------------|
| `sat/reset_integral` | (any) | Reset PID integral accumulator to zero |

### Settings Commands (write any SAT setting)

| Sub-topic | Example Payload | Description |
|-----------|----------------|-------------|
| `sat/overshoot_margin` | `"2.0"` | Overshoot margin °C |
| `sat/heating_system` | `"1"` | Heating system type (0=auto,1=radiators,2=heat pump,3=underfloor) |
| `sat/manufacturer` | `"remeha"` | Boiler manufacturer name or index |
| `sat/max_modulation` | `"100"` | Max relative modulation % |
| `sat/dhw_setpoint` | `"55.0"` | DHW setpoint °C (0 = inactive) |
| `sat/dhw_enabled` | `"true"` | DHW control in fallback mode |
| `sat/interval` | `"30"` | Control loop interval (seconds) |
| `sat/push_setpoint` | `"false"` | Push SAT target to thermostat display |
| `sat/flame_off_offset` | `"0.0"` | Anti-cycling offset when flame off (°C) |
| `sat/force_pwm` | `"false"` | Force PWM mode |
| `sat/flow_offset` | `"2.0"` | Continuous mode max setpoint drop (°C) |
| `sat/summer_simmer` | `"false"` | Enable summer simmer mode |
| `sat/summer_threshold` | `"18.0"` | Summer outdoor threshold (°C) |
| `sat/summer_min_hours` | `"6"` | Consecutive hours above threshold |
| `sat/comfort_adjust` | `"false"` | Enable humidity-based comfort adjustment |
| `sat/comfort_humidity` | `"50.0"` | Reference humidity % for comfort adjustment |
| `sat/comfort_max_offset` | `"1.0"` | Max comfort target adjustment (°C) |
| `sat/simulation` | `"false"` | Enable simulation mode |
| `sat/ble_enable` | `"false"` | Enable BLE temperature sensor (ESP32 only) |
| `sat/ble_mac` | `"AA:BB:CC:DD:EE:FF"` | Bind to specific BLE sensor MAC |
| `sat/ble_interval` | `"30"` | BLE scan interval (seconds) |
| `sat/preset_sync` | `"false"` | Enable preset sync to secondary entities |
| `sat/preset_sync_topic` | `"home/preset"` | MQTT topic for preset sync |
| `sat/multi_area` | `"false"` | Enable multi-area temperature |
| `sat/multi_area_count` | `"2"` | Number of configured areas (0-4) |
| `sat/auto_tune` | `"false"` | Enable auto-tune PID gains |
| `sat/auto_tune_rate` | `"0.02"` | Auto-tune adjustment rate per cycle |
| `sat/heating_curve` | `"1.5"` | Heating curve coefficient |
| `sat/deadband` | `"0.25"` | PID deadband (°C) |
| `sat/mod_sup_delay` | `"20.0"` | Modulation suppression delay (seconds) |
| `sat/mod_sup_offset` | `"1.0"` | Modulation suppression offset (°C) |
| `sat/boiler_capacity` | `"24.0"` | Boiler capacity (kW) |
| `sat/target_temp_step` | `"0.5"` | Target temp step size |
| `sat/min_pressure` | `"0.8"` | Minimum pressure (bar) |
| `sat/max_pressure` | `"2.5"` | Maximum pressure (bar) |
| `sat/max_pressure_drop` | `"0.3"` | Max pressure drop rate (bar/hour) |
| `sat/preset_comfort` | `"21.0"` | Comfort preset value (°C) |
| `sat/preset_eco` | `"18.0"` | Eco preset value (°C) |
| `sat/preset_away` | `"15.0"` | Away preset value (°C) |
| `sat/preset_sleep` | `"16.0"` | Sleep preset value (°C) |
| `sat/preset_activity` | `"10.0"` | Activity preset value (°C) |
| `sat/preset_home` | `"18.0"` | Home preset value (°C) |
| `sat/solar_gain` | `"false"` | Enable solar gain compensation |
| `sat/window_detection` | `"false"` | Enable window open detection |
| `sat/pwm_auto_switch` | `"true"` | Enable PWM auto-switch between modes |
| `sat/sensor_max_age` | `"21600"` | Max age of sensor reading before stale (seconds) |
| `sat/error_monitoring` | `"false"` | Enable detailed error statistics |
| `sat/auto_gains_value` | `"2.0"` | Auto-tune PID gain multiplier |
| `sat/heating_mode` | `"comfort"` / `"eco"` or `"0"` / `"1"` | Heating mode |
| `sat/cycles_per_hour` | `"3"` | Target cycles per hour (2-6) |
| `sat/valve_offset` | `"0.0"` | TRV valve position detection offset |
| `sat/solar_freeze_integral` | `"true"` | Freeze PID integral during solar gain |

---

## JSON Payload Formats

### `sat/pid_attributes`
```json
{
  "kp": 5.0,
  "ki": 0.5,
  "kd": 0.05,
  "p_term": 0.82,
  "i_term": 0.03,
  "d_term": -0.04,
  "raw_derivative": -0.04
}
```

### `sat/cycle_attributes`
```json
{
  "class": "good",
  "duration_sec": 312.0,
  "max_flow": 48.5,
  "overshoot_sec": 0.0,
  "kind": "CH",
  "duty_ratio": 0.70
}
```

### `sat/climate_attributes`
Published to the HA climate entity's `json_attributes_topic`. Contains extended status:
```json
{
  "mode": "continuous",
  "setpoint": 43.1,
  "heating_curve": 42.3,
  "pid_output": 43.1,
  "error": 0.5,
  "boiler_status": "at_setpoint",
  "cycle_class": "good",
  "safety_tripped": false
}
```

---

## Value Tables

### Boiler Status Codes (`sat/boiler_status`)
| Value | Meaning |
|-------|---------|
| 0 | Off |
| 1 | Idle |
| 2 | Preheating |
| 3 | At Setpoint |
| 4 | Modulating Up |
| 5 | Modulating Down |
| 6 | Ignition Surge |
| 7 | Stalled Ignition |
| 8 | Anti-Cycling |
| 9 | Pump Starting |
| 10 | Waiting Flame |
| 11 | Overshoot Cooling |
| 12 | Post-Cycle |
| 13 | Heating |
| 14 | Cooling |

### Cycle Classifications (`sat/cycle_class`)
| Value | Meaning |
|-------|---------|
| `none` | No completed cycle yet |
| `good` | Normal cycle |
| `overshoot` | Flow temp exceeded setpoint + margin |
| `underheat` | Flow temp never reached setpoint |
| `short` | Cycle shorter than 60 s |
| `uncertain` | Insufficient data to classify |

### Flame Status (`sat/flame_status`)
| Value | Meaning |
|-------|---------|
| `insufficient_data` | Not enough history |
| `healthy` | Normal operation |
| `idle_ok` | Flame off but expected |
| `stuck_on` | Flame stays on unexpectedly |
| `stuck_off` | Flame stays off unexpectedly |
| `pwm_short` | PWM cycle too short |
| `short_cycling` | Repeated short cycles detected |

---

## QoS

All SAT topics use QoS 0 (fire-and-forget). The broker's retain flag is used instead of QoS 1/2 for persistence. This matches the behavior of all other OTGW-firmware MQTT topics.

---

## Source

- Publish topics: `src/OTGW-firmware/SATcontrol.ino` (lines 1271–1881), `src/OTGW-firmware/SATble.ino` (lines 361–375)
- Subscribe topics: `src/OTGW-firmware/MQTTstuff.ino` (lines 693–866)
