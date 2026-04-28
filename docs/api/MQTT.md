# OTGW-firmware MQTT Topic Documentation

This document describes all MQTT topics published and subscribed to by the OTGW-firmware.

> **Breaking change in 2.0.0 (MQTT consumers please read):** three OT-bus presence values (`boiler_connected`, `thermostat_connected`, `otgw_connected`) have moved out of the hardware-specific `otgw-pic/` and `otgw-otdirect/` subtrees into the generic `OTGW/value/<uniqueId>/` namespace. The `otgw-otdirect/ot_online` topic has been retired; the same concept now lives under `otgw_connected`. The firmware self-heals retained payloads on the deprecated topics at first MQTT reconnect. Home Assistant users do not need to do anything; custom consumers should update their topic patterns. See [OT-bus state (generic, since 2.0.0)](#ot-bus-state-generic-since-200) and [Migration from 1.4.x / pre-release 2.0.0 (OT-bus state topics)](#migration-from-14x--pre-release-200-ot-bus-state-topics). Background: ADR-084.

## Overview

The OTGW-firmware uses MQTT as its primary integration method with Home Assistant and other home automation systems. It publishes OpenTherm data, device status, and sensor readings, and subscribes to command topics for controlling the OpenTherm Gateway.

## Topic Structure

### Namespace Convention

All MQTT topics follow a namespace convention based on user-configurable settings:

- **Publish namespace**: `{TopTopic}/value/{UniqueId}/`
- **Subscribe namespace**: `{TopTopic}/set/{UniqueId}/`
- **Last Will and Testament**: published to the publish namespace root

Where:

- `{TopTopic}` = `settings.mqtt.sTopTopic` (default: `OTGW`)
- `{UniqueId}` = `settings.mqtt.sUniqueid` (default: `otgw-{MAC_ADDRESS}`)

**Example** with defaults:

- Publish: `OTGW/value/otgw-AABBCCDDEEFF/boilertemperature`
- Subscribe: `OTGW/set/otgw-AABBCCDDEEFF/setpoint`

### Connection Lifecycle

On MQTT connect:

1. **Birth message**: Publishes `"online"` to the publish namespace root (retained)
2. **Last Will**: Configured to publish `"offline"` to the publish namespace root (retained) when the connection drops
3. **Discovery reset**: Clears all HA discovery state so JIT discovery re-publishes
4. **Subscribes** to `{TopTopic}/set/{UniqueId}/#` for incoming commands
5. **Subscribes** to `homeassistant/status` for Home Assistant lifecycle detection
6. **Publishes** version info, state information, and cached PIC settings

---

## Published Topics

All published topics are under the publish namespace `{TopTopic}/value/{UniqueId}/` unless otherwise noted. Topics are published with `retain = true` by default.

### Firmware & Device Information

Published at startup, on MQTT (re)connect, and every 5 minutes.

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `otgw-firmware/version` | `"1.3.0"` | Firmware version string |
| `otgw-firmware/reboot_count` | `"42"` | Number of reboots since first boot |
| `otgw-firmware/reboot_reason` | `"Software/System restart"` | Last reboot reason |
| `otgw-firmware/uptime` | `"12345"` | Uptime in seconds (not retained) |
| `otgw-firmware/board` | `"esp8266"` / `"esp32s3"` | Hardware board identifier |
| `otgw-firmware/hardware_mode` | `"pic"` / `"otdirect"` | Active hardware mode |
| `otgw-firmware/network_mode` | `"wifi"` / `"ethernet"` / `"ap"` | Active network mode |
| `otgw-firmware/error` | `"LittleFS mount failed..."` | Error messages (not retained, only when applicable) |

### OT-bus state (generic, since 2.0.0)

Published at startup, on MQTT (re)connect, and whenever the values change. These three topics describe what the OTGW-firmware observes on the OpenTherm bus, independent of whether the hardware uses the PIC coprocessor or the native OTDirect driver. Before 2.0.0 these values lived under `otgw-pic/` or `otgw-otdirect/`; see the [migration section](#migration-from-14x--pre-release-200-ot-bus-state-topics) for details.

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `boiler_connected` | `"ON"` / `"OFF"` | A boiler is visible on the OpenTherm bus (frames received from the slave side). |
| `thermostat_connected` | `"ON"` / `"OFF"` | A thermostat is visible on the OpenTherm bus (frames received from the master side). |
| `otgw_connected` | `"ON"` / `"OFF"` | The OpenTherm bus is active (the firmware is exchanging frames). This topic replaces the former `otgw-otdirect/ot_online` and mirrors the former `otgw-pic/otgw_connected`. |

Payloads are retained. Published as `OTGW/value/<uniqueId>/<topic>` using the configured `TopTopic` and `UniqueId` (see [Namespace Convention](#namespace-convention)).

### PIC Gateway Information

Published at startup, on MQTT (re)connect, and every 5 minutes.

Note: the OT-bus presence values (`boiler_connected`, `thermostat_connected`, `otgw_connected`) were previously listed here. Since 2.0.0 they are published under the generic namespace; see [OT-bus state (generic, since 2.0.0)](#ot-bus-state-generic-since-200) and the [migration section](#migration-from-14x--pre-release-200-ot-bus-state-topics).

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `otgw-pic/version` | `"5.4"` | PIC firmware version |
| `otgw-pic/deviceid` | `"gateway"` | PIC device ID |
| `otgw-pic/firmwaretype` | `"gateway"` | PIC firmware type (gateway/interface/diagnose) |
| `otgw-pic/picavailable` | `"ON"` / `"OFF"` | Whether PIC is detected |
| `otgw-pic/gateway_mode` | `"ON"` / `"OFF"` | Gateway mode (ON) vs monitor mode (OFF) |

### PIC Settings

Published at startup, on MQTT (re)connect, every 5 minutes, and when settings are queried. Only fields that have been queried (non-empty) are published.

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `otgw-pic/settings/setpoint_override` | `"20.00"` | Current setpoint override |
| `otgw-pic/settings/setback` | `"15.00"` | Setback temperature |
| `otgw-pic/settings/dhw_override` | `""` | DHW override state |
| `otgw-pic/settings/gpio` | `"0/1"` | GPIO pin configuration |
| `otgw-pic/settings/gpio_states` | `"0/0"` | GPIO pin states |
| `otgw-pic/settings/led` | `"F/X/O/M/P/C"` | LED function assignments |
| `otgw-pic/settings/tweaks` | `"1/1/1/1"` | Tweak flags |
| `otgw-pic/settings/temp_sensor` | `"O=19.50"` | Temperature sensor reading |
| `otgw-pic/settings/smart_power` | `"Low"` | Smart power mode |
| `otgw-pic/settings/thermostat_detect` | `"I"` | Thermostat detection mode |
| `otgw-pic/settings/builddate` | `"2023-01-01"` | PIC firmware build date |
| `otgw-pic/settings/clock_mhz` | `"4"` | PIC clock speed |
| `otgw-pic/settings/reset_cause` | `"Power-on"` | Last PIC reset cause |
| `otgw-pic/settings/standalone_interval` | `"0"` | Standalone mode interval |
| `otgw-pic/settings/voltage_ref` | `"3.3"` | Voltage reference |

### OT Direct (OTGW32)

Published at startup, on MQTT (re)connect, and every 5 minutes. Only present in OTGW32 builds (`HAS_DIRECT_OT=1`). On standard ESP8266+PIC hardware, only `otgw-otdirect/available` is published with value `"OFF"`.

Note: the OT-bus presence values (`boiler_connected`, `thermostat_connected`) and the former `ot_online` topic were previously listed here. Since 2.0.0 these values are published under the generic namespace; `ot_online` has been retired in favour of `otgw_connected`. See [OT-bus state (generic, since 2.0.0)](#ot-bus-state-generic-since-200) and the [migration section](#migration-from-14x--pre-release-200-ot-bus-state-topics).

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `otgw-otdirect/available` | `"ON"` / `"OFF"` | Whether OT-direct hardware is present and enabled |
| `otgw-otdirect/mode` | `"gateway"` / `"monitor"` / `"bypass"` / `"master"` / `"loopback"` | Current operating mode of the OT-direct engine |
| `otgw-otdirect/bypass` | `"ON"` / `"OFF"` | Whether the bypass relay is active |
| `otgw-otdirect/monitor_mode` | `"ON"` / `"OFF"` | Whether transparent pass-through mode is active |
| `otgw-otdirect/master_mode` | `"ON"` / `"OFF"` | Whether OTGW32 is acting as sole OT master |
| `otgw-otdirect/stepup` | `"ON"` / `"OFF"` | Whether the 24V step-up converter is active |
| `otgw-otdirect/setback_active` | `"ON"` / `"OFF"` | Whether setback override is active (thermostat disconnected) |
| `otgw-otdirect/schedule_active` | `"11"` | Number of OT polling schedule entries currently active |
| `otgw-otdirect/schedule_disabled` | `"1"` | Number of schedule entries disabled because boiler returned UNKNOWN_DATA_ID |
| `otgw-otdirect/overrides_active` | `"2"` | Number of active write-override slots |

These topics are only published when `isOTDirectEnabled()` returns true. Topics prefixed `otgw-otdirect/` are skipped during MQTT discovery replay when OT-direct is not enabled.

### OpenTherm Status Flags (Message ID 0)

Published when status flag values change. Individual bits are published as `"ON"` / `"OFF"`.

#### Master Status (Thermostat to Boiler)

| Topic | Description |
| ----- | ----------- |
| `status_master` | Full master status as text string |
| `chenable` | Central heating enable |
| `dhwenable` | DHW enable |
| `coolingenable` | Cooling enable |
| `otc_active` | Outside temperature compensation active |
| `ch2enable` | Central heating 2 enable |
| `summerwintermode` | Summer/winter mode |
| `dhwblockingenable` | DHW blocking enable |

#### Slave Status (Boiler to Thermostat)

| Topic | Description |
| ----- | ----------- |
| `status_slave` | Full slave status as text string |
| `faultindicator` | Fault indicator |
| `chmodus` | Central heating active |
| `dhwmode` | DHW active |
| `flamestatus` | Flame status |
| `coolingactive` | Cooling active |
| `ch2modus` | CH2 active |
| `diagnosticindicator` | Diagnostic indicator |

### OpenTherm ASF Flags (Message ID 5)

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `ASF_flags` | `"00000000"` | Application-specific fault flags (binary) |
| `OEMFaultCode` | `"0"` | OEM fault code |
| `service_request` | `"ON"` / `"OFF"` | Service request flag |
| `lockout_reset` | `"ON"` / `"OFF"` | Lockout reset flag |
| `low_water_pressure` | `"ON"` / `"OFF"` | Low water pressure flag |
| `gas_flame_fault` | `"ON"` / `"OFF"` | Gas/flame fault flag |
| `air_pressure_fault` | `"ON"` / `"OFF"` | Air pressure fault flag |
| `water_over_temperature` | `"ON"` / `"OFF"` | Water over-temperature flag |

### OpenTherm Configuration (Message IDs 3, 70, 74)

#### Slave Configuration (Boiler, ID 3)

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `slave_configuration` | `"00000001"` | Slave configuration flags (binary) |
| `slave_memberid_code` | `"0"` | Slave member ID code |
| `dhw_present` | `"ON"` / `"OFF"` | DHW present |
| `control_type_modulation` | `"ON"` / `"OFF"` | Modulating control type |
| `cooling_config` | `"ON"` / `"OFF"` | Cooling configuration |
| `dhw_config` | `"ON"` / `"OFF"` | DHW configuration |
| `master_low_off_pump_control_function` | `"ON"` / `"OFF"` | Pump control |
| `ch2_present` | `"ON"` / `"OFF"` | CH2 present |
| `remote_water_filling_function` | `"ON"` / `"OFF"` | Remote water filling |
| `heat_cool_mode_control` | `"ON"` / `"OFF"` | Heat/cool mode control |

#### Master Configuration (Thermostat, ID 70)

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `master_configuration` | `"00000000"` | Master configuration flags (binary) |
| `master_configuration_smart_power` | `"ON"` / `"OFF"` | Smart power |
| `master_memberid_code` | `"0"` | Master member ID code |

#### VH Configuration (Ventilation/Heat-recovery, ID 74)

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `vh_configuration` | `"00000000"` | VH configuration flags (binary) |
| `vh_configuration_system_type` | `"ON"` / `"OFF"` | System type |
| `vh_configuration_bypass` | `"ON"` / `"OFF"` | Bypass |
| `vh_configuration_speed_control` | `"ON"` / `"OFF"` | Speed control |
| `vh_memberid_code` | `"0"` | VH member ID code |

### OpenTherm Numeric Values

Published when OpenTherm messages are received. The topic name matches the OpenTherm message label. Common topics include:

| Topic | Unit | Description |
| ----- | ---- | ----------- |
| `controlsetpoint` | C | Control setpoint (CH water temp target) |
| `roomsetpoint` | C | Room setpoint |
| `roomtemperature` | C | Current room temperature |
| `relmodlvl` | % | Relative modulation level |
| `maxrelmodlvl` | % | Max relative modulation level setting |
| `boilertemperature` | C | Boiler water temperature |
| `returnwatertemperature` | C | Return water temperature |
| `dhwtemperature` | C | DHW temperature |
| `dhwsetpoint` | C | DHW setpoint |
| `maxchwatersetpoint` | C | Max CH water setpoint |
| `outsidetemperature` | C | Outside temperature |
| `chwaterpressure` | bar | CH water pressure |
| `oemdiagnosticcode` | - | OEM diagnostic code |
| `controlsetpoint2` | C | Control setpoint for CH2 |
| `roomsetpoint2` | C | Room setpoint for CH2 |
| `dhw_flowrate` | l/min | DHW flow rate |
| `exhaust_temperature` | C | Exhaust temperature |
| `boiler_fan_speed` | rpm | Boiler fan speed |
| `electrical_current_burner_flame` | uA | Burner flame current |
| `dhwboundaries` | C | DHW boundaries |
| `maxchboundaries` | C | Max CH boundaries |
| `otc_hc_ratio_ub_lb` | - | OTC HC ratio upper/lower bound |

The exact set of published topics depends on which OpenTherm message IDs the thermostat and boiler exchange. Any valid OT message ID (0-127) with a defined label will generate a corresponding MQTT topic.

### Remote Boiler Parameters (Message ID 6)

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `RBP_flags_transfer_enable` | `"00000000"` | Transfer enable flags (binary) |
| `RBP_flags_read_write` | `"00000000"` | Read/write flags (binary) |
| `rbp_dhw_setpoint` | `"ON"` / `"OFF"` | DHW setpoint transfer enabled |
| `rbp_max_ch_setpoint` | `"ON"` / `"OFF"` | Max CH setpoint transfer enabled |
| `rbp_rw_dhw_setpoint` | `"ON"` / `"OFF"` | DHW setpoint read/write |
| `rbp_rw_max_ch_setpoint` | `"ON"` / `"OFF"` | Max CH setpoint read/write |

### Remote Override (Message ID 100)

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `remote_override_function` | `"00000000"` | Remote override flags (binary) |
| `remote_override_manual_change_priority` | `"ON"` / `"OFF"` | Manual change priority |
| `remote_override_program_change_priority` | `"ON"` / `"OFF"` | Program change priority |

### VH Status Flags (Message IDs 70, 71)

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `status_vh_master` | text | VH master status text |
| `status_vh_slave` | text | VH slave status text |

### VH Transfer/RW Flags (Message ID 73)

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `vh_transfer_enable_nominal_ventilation_value` | `"ON"` / `"OFF"` | Nominal ventilation transfer enabled |
| `vh_rw_nominal_ventilation_value` | `"ON"` / `"OFF"` | Nominal ventilation read/write |

### Solar Storage (Message IDs 101, 102, 103)

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `solar_storage_slave_configuration` | `"00000000"` | Solar configuration (binary) |
| `solar_storage_slave_memberid_code` | `"0"` | Solar member ID |
| `solar_storage_system_type` | `"ON"` / `"OFF"` | System type |
| `solar_storage_master_mode` | `"0"` | Master solar mode |
| `solar_storage_slave_fault_indicator` | `"ON"` / `"OFF"` | Solar fault indicator |
| `solar_storage_mode_status` | `"0"` | Solar mode status |
| `solar_storage_slave_status` | `"0"` | Solar slave status |

### SAT (Smart Autotune Thermostat)

Published every control loop interval (default 30 s) when SAT is enabled. Topics are under the standard publish namespace. The complete topic inventory is in [backlog/docs/doc-1 - sat-mqtt-topics.md](../../backlog/docs/doc-1%20-%20sat-mqtt-topics.md).

#### Core Control State

| Topic | Value | Retained | Description |
| ----- | ----- | -------- | ----------- |
| `sat/mode` | `"off"` / `"continuous"` / `"pwm"` | yes | Current control mode |
| `sat/active` | `"true"` / `"false"` | yes | Whether SAT controller is actively controlling |
| `sat/setpoint` | `"43.1"` | yes | Final flow temperature setpoint sent to boiler (°C) |
| `sat/target` | `"21.0"` | yes | Target room temperature (°C) |
| `sat/heating_curve` | `"42.3"` | yes | Heating curve calculated value (°C) |
| `sat/pid_output` | `"43.1"` | yes | PID output = curve + P + I + D (°C) |
| `sat/overshoot_margin` | `"2.0"` | yes | Configured overshoot margin (°C) |

#### PID Components

| Topic | Value | Retained | Description |
| ----- | ----- | -------- | ----------- |
| `sat/error` | `"0.50"` | no | PID error = target - room temp (°C) |
| `sat/pid_p` | `"0.82"` | no | Proportional term (°C) |
| `sat/pid_i` | `"0.03"` | no | Integral term (°C) |
| `sat/pid_d` | `"-0.04"` | no | Derivative term (°C) |
| `sat/pid_attributes` | JSON | no | Extended PID attributes (`kp`, `ki`, `kd`, `p_term`, `i_term`, `d_term`, `raw_derivative`) |
| `sat/raw_derivative` | `"-0.04"` | no | Raw (unfiltered) derivative term |
| `sat/kp` | `"5.0"` | no | Current Kp gain |
| `sat/ki` | `"0.5"` | no | Current Ki gain |
| `sat/kd` | `"0.05"` | no | Current Kd gain |
| `sat/error_mean` | `"0.12"` | no | Mean PID error over ring buffer (when error monitoring enabled) |
| `sat/error_stddev` | `"0.34"` | no | Std deviation of PID error over ring buffer |

#### Temperature Sensors

| Topic | Value | Retained | Description |
| ----- | ----- | -------- | ----------- |
| `sat/room_temp` | `"20.8"` | no | Room temperature used by PID (°C) |
| `sat/outside_temp` | `"8.0"` | no | Outside temperature used by heating curve (°C) |
| `sat/estimated_room` | `"20.5"` | no | Estimated room temp during sensor fallback (°C) |
| `sat/humidity` | `"58.0"` | no | Indoor humidity % (when humidity sensor active) |
| `sat/humidity_valid` | `"true"` / `"false"` | no | Whether humidity reading is valid |
| `sat/comfort_offset` | `"0.5"` | no | Comfort adjustment applied to target (°C) |

#### Boiler and Cycle Status

| Topic | Value | Retained | Description |
| ----- | ----- | -------- | ----------- |
| `sat/boiler_status` | `"3"` | no | Boiler status code (see table below) |
| `sat/flame_status` | `"healthy"` | no | Flame health status string (see table below) |
| `sat/flame_health` | `"ON"` / `"OFF"` | yes | Binary: ON = flame healthy |
| `sat/cycle_class` | `"good"` | no | Last cycle classification string (see table below) |
| `sat/cycle_attributes` | JSON | no | Extended cycle attributes (`class`, `duration_sec`, `max_flow`, `overshoot_sec`, `kind`, `duty_ratio`) |
| `sat/cycle_phase` | `"startup"` / `"steady"` / `"cooldown"` / `"idle"` | no | Current cycle phase |
| `sat/pwm_duty` | `"0.65"` | no | PWM duty cycle (0.00-1.00) |
| `sat/duty_ratio` | `"0.70"` | no | EMA duty ratio (flame-on fraction) |
| `sat/overshoot_fraction` | `"0.12"` | no | EMA fraction of cycles with overshoot |

#### System Health and Safety

| Topic | Value | Retained | Description |
| ----- | ----- | -------- | ----------- |
| `sat/safety_tripped` | `"false"` | no | Safety shutdown active |
| `sat/valves_open` | `"true"` / `"false"` | no | Whether TRV valves are reported open |
| `sat/window_open` | `"false"` | no | Window-open detection state |
| `sat/ch_sync` | `"ON"` / `"OFF"` | yes | CH sync health (ON = OK) |
| `sat/modulation_reliable` | `"true"` | no | Whether boiler modulation feedback is reliable |
| `sat/setpoint_mismatch` | `"false"` | no | Setpoint mismatch between SAT and OT bus detected |

#### Pressure Monitoring

| Topic | Value | Retained | Description |
| ----- | ----- | -------- | ----------- |
| `sat/pressure` | `"1.5"` | no | Smoothed system pressure (bar) |
| `sat/pressure_drop_rate` | `"-0.02"` | no | Pressure drop rate (bar/hour, negative = dropping) |
| `sat/pressure_alarm` | `"false"` | no | Pressure alarm active |
| `sat/pressure_health` | `"ON"` / `"OFF"` | yes | Binary: ON = pressure healthy |
| `sat/pressure_health_attr` | JSON | no | Extended pressure health attributes |

#### Power and Energy

| Topic | Value | Retained | Description |
| ----- | ----- | -------- | ----------- |
| `sat/power` | `"12.5"` | no | Current boiler power (kW, modulation × capacity) |
| `sat/energy_total` | `"234.5"` | yes | Cumulative energy (kWh, retained for HA energy dashboard) |

#### Heating Curve Recommendation

| Topic | Value | Retained | Description |
| ----- | ----- | -------- | ----------- |
| `sat/curve_recommendation` | `"increase"` / `"decrease"` / `"hold"` / `"insufficient"` | no | Automatic heating curve recommendation |
| `sat/curve_recommendation_attributes` | JSON | no | Extended recommendation attributes (error stats, sample count) |

#### Presets

| Topic | Value | Retained | Description |
| ----- | ----- | -------- | ----------- |
| `sat/preset_comfort` | `"21.0"` | yes | Comfort preset target temperature (°C) |
| `sat/preset_eco` | `"18.0"` | yes | Eco preset target temperature (°C) |
| `sat/preset_away` | `"15.0"` | yes | Away preset target temperature (°C) |
| `sat/preset_sleep` | `"16.0"` | yes | Sleep preset target temperature (°C) |
| `sat/preset_activity` | `"10.0"` | yes | Activity preset target temperature (°C) |
| `sat/preset_home` | `"18.0"` | yes | Home preset target temperature (°C) |

#### Settings Echo Topics

The following topics are published (retained) when SAT settings change. They echo the current configured values.

| Topic | Example | Description |
| ----- | ------- | ----------- |
| `sat/heating_curve_coeff` | `"1.5"` | Heating curve coefficient |
| `sat/deadband` | `"0.25"` | PID deadband (°C) |
| `sat/control_interval` | `"30"` | Control loop interval (seconds) |
| `sat/max_modulation` | `"100"` | Max relative modulation % |
| `sat/flame_off_offset` | `"0.0"` | Setpoint offset when flame is off (°C) |
| `sat/flow_offset` | `"2.0"` | Continuous mode max setpoint drop (°C) |
| `sat/boiler_capacity` | `"24.0"` | Boiler capacity (kW) |
| `sat/summer_threshold` | `"18.0"` | Summer simmer outdoor temperature threshold (°C) |
| `sat/ovp_value` | `"52.5"` | OPV calibrated maximum flow temperature (°C) |
| `sat/ovp_enabled` | `"false"` | Whether OPV is enabled |
| `sat/heating_system` | `"1"` | Heating system type (0=auto, 1=radiators, 2=heat pump, 3=underfloor) |
| `sat/simulation` | `"ON"` / `"OFF"` | Simulation mode active |
| `sat/auto_tune` | `"ON"` / `"OFF"` | Auto-tune analysis active |
| `sat/manufacturer` | `"Remeha"` | Detected/configured boiler manufacturer name |

Settings echo topics are retained. More settings echo topics exist; the full list is in the [SAT MQTT topics reference](../../backlog/docs/doc-1%20-%20sat-mqtt-topics.md).

#### Thermal Model

| Topic | Value | Retained | Description |
| ----- | ----- | -------- | ----------- |
| `sat/thermal_coeff` | `"0.05"` | yes | Learned thermal drop coefficient (°C/hr per °C delta) |
| `sat/thermal_drop_rate` | `"0.03"` | no | Current thermal drop rate sample |
| `sat/thermal_model_valid` | `"true"` | yes | Whether thermal model has sufficient data |

#### Solar Gain

| Topic | Value | Retained | Description |
| ----- | ----- | -------- | ----------- |
| `sat/solar_gain` | `"true"` / `"false"` | no | Solar gain compensation currently active |
| `sat/indoor_rise_rate` | `"0.8"` | no | Measured indoor temperature rise rate (°C/hr) |
| `sat/solar_gain_sun_elevation` | `"32.5"` | no | Current sun elevation (degrees) |

#### Summer Simmer

| Topic | Value | Retained | Description |
| ----- | ----- | -------- | ----------- |
| `sat/summer_active` | `"false"` | no | Summer simmer mode currently suppressing heating |
| `sat/summer_hours_above` | `"4.5"` | no | Hours outdoor temp has been above threshold |

#### SAT Switch State Topics (TASK-284)

State topics for the 13 SAT boolean switches. Payloads are `"true"` / `"false"`. All retained.

| Topic | Description |
| ----- | ----------- |
| `sat/solar_gain_enable` | Solar gain compensation enable state |
| `sat/summer_simmer_enable` | Summer simmer enable state |
| `sat/comfort_adjust_enable` | Humidity-based comfort adjust enable state |
| `sat/multi_area_enable` | Multi-area averaging enable state |
| `sat/auto_tune_enable` | Auto-tune analysis enable state |
| `sat/simulation_enable` | Simulation mode enable state |
| `sat/window_detection_enable` | Window detection enable state |
| `sat/force_pwm_enable` | Force-PWM override enable state |
| `sat/push_setpoint_enable` | Push-setpoint enable state |
| `sat/ovp_enabled` | OPV (overshoot-protection value) enabled state |
| `sat/preset_sync_enable` | Preset-sync enable state |
| `sat/dhw_enabled` | DHW heating enable state |
| `sat/pwm_auto_switch_enable` | Auto-switch between PWM and continuous mode |

#### Climate Entity Attributes

| Topic | Value | Retained | Description |
| ----- | ----- | -------- | ----------- |
| `sat/climate_attributes` | JSON | no | HA climate entity JSON attributes (mode, setpoint, heating_curve, pid_output, error, boiler_status, cycle_class, safety_tripped) |

#### BLE Temperature Sensor (ESP32 only)

These topics are only available when BLE is enabled on ESP32 hardware.

| Topic | Value | Retained | Description |
| ----- | ----- | -------- | ----------- |
| `sat/ble_temp` | `"20.5"` | no | BLE sensor temperature (°C) |
| `sat/ble_humidity` | `"55.0"` | no | BLE sensor humidity (%) |
| `sat/ble_sensor_rssi` | `"-72"` | no | BLE sensor RSSI (dBm) |
| `sat/ble_battery` | `"85"` | no | BLE sensor battery level (%) |
| `sat/ble_sensor_count` | `"1"` | no | Number of active BLE sensors seen |
| `sat/ble_temp_valid` | `"true"` | no | BLE temperature reading is valid and non-stale |

#### Value Tables

**Boiler status codes** (`sat/boiler_status`): 0=Off, 1=Idle, 2=Preheating, 3=At Setpoint, 4=Modulating Up, 5=Modulating Down, 6=Ignition Surge, 7=Stalled Ignition, 8=Anti-Cycling, 9=Pump Starting, 10=Waiting Flame, 11=Overshoot Cooling, 12=Post-Cycle, 13=Heating, 14=Cooling

**Cycle class** (`sat/cycle_class`): `none`, `good`, `overshoot`, `underheat`, `short`, `uncertain`

**Flame status** (`sat/flame_status`): `insufficient_data`, `healthy`, `idle_ok`, `stuck_on`, `stuck_off`, `pwm_short`, `short_cycling`

### Raw OpenTherm Message (Optional)

When `settings.mqtt.bOTmessage` is enabled:

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `otmessage` | `"T80000200"` | Raw OpenTherm message string |

### Event Reports

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `event_report` | text | OTGW event notification messages |

### Error Reports

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `Error 01` | text | Error: line too short |
| `Error 02` | text | Error: line too long |
| `Error 03` | text | Error: parity check failed |
| `Error 04` | text | Error: invalid response type |
| `Error_BufferOverflow` | count | Serial buffer overflow counter |

### S0 Pulse Counter

Published when S0 counter is enabled (`settings.s0.bEnabled`):

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `s0pulsecount` | `"123"` | Pulse count in current interval |
| `s0pulsecounttot` | `"456789"` | Total pulse count since boot |
| `s0pulsetime` | `"500"` | Last pulse duration (ms) |
| `s0powerkw` | `"1.234"` | Calculated power in kW |

### Dallas Temperature Sensors

Published when GPIO sensors are enabled (`settings.sensors.bEnabled`):

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `{sensor_address}` | `"21.5"` | Temperature in Celsius |

Where `{sensor_address}` is the Dallas 1-Wire address (e.g., `28FF64D1841703F1`). The address format depends on the `gpiosensorslegacyformat` setting.

### Source-Separated Topics (Optional)

When `settings.mqtt.bSeparateSources` is enabled, OpenTherm data is additionally published to source-specific sub-topics:

```
{TopTopic}/value/{UniqueId}/{label}/thermostat
{TopTopic}/value/{UniqueId}/{label}/boiler
{TopTopic}/value/{UniqueId}/{label}/gateway
```

This allows distinguishing whether a value was sent by the thermostat (T-request), boiler (B-response), or injected/modified by the gateway.

#### Publish gating per topic class (2.0.0+, ADR-066)

The base topic `{TopTopic}/value/{UniqueId}/{label}` publishes only the
thermostat-side intent: Read-Ack for read-supported messages and Write-Data
for write-supported messages. Slave Write-Ack values are NOT routed to the
base topic. This avoids flapping between thermostat-written values and
slave-acked protocol-zero values for non-echo MsgIDs.

The `/boiler` subtopic is gated by a per-MsgID `bSlaveEchoesValue` flag in
the OTlookup table. For MsgIDs where the OpenTherm v4.2 specification defines
the slave's Write-Ack data field as undefined (typically `Tr` 24, `TrSet` 16,
`MaxRelModLevelSetting` 14, `TrSetCH2` 23, `TRoomCH2` 37, `RFstrengthbatterylevel` 98),
the `/boiler` subtopic is NOT updated for Write-Ack messages. The slave's
acknowledgement carries no measurement; suppressing it avoids polluting
the per-source observability surface with fake-zero readings.

For MsgIDs where the slave does store and echo the value (most R/W
parameters, Class 5 remote boiler parameters such as `MaxTSet` 57 and
`TdhwSet` 56, Class 6 transparent slave parameters, R/W counters), the
`/boiler` subtopic continues to publish the slave's stored value, including
clamped or modified variants distinct from the master's request.

See `docs/api/MQTT-message-id-echo-audit.md` for the full per-MsgID
classification with spec-citation rationale.

---

## Subscribed Topics

### Command Topics

The firmware subscribes to `{TopTopic}/set/{UniqueId}/#` and processes commands published to sub-topics.

#### Direct Commands

| Topic Suffix | Payload | OT Command | Description |
| ------------ | ------- | ---------- | ----------- |
| `command` | `"TT=20.5"` | (raw) | Raw OTGW command string |
| `setpoint` | `"20.5"` | `TT=20.5` | Temporary temperature setpoint |
| `constant` | `"20.5"` | `TC=20.5` | Constant temperature setpoint |
| `outside` | `"12.0"` | `OT=12.0` | Outside temperature |
| `hotwater` | `"1"` | `HW=1` | Hot water on/off/push (`0`, `1`, `P`, other=auto) |
| `gatewaymode` | `"1"` / `"0"` / `"2"` / `"S"` / `"M"` / `"L"` / `"R"` | `GW=…` | Operating mode (see table below) |
| `setback` | `"15.0"` | `SB=15.0` | Setback temperature |
| `maxchsetpt` | `"80"` | `SH=80` | Max CH water setpoint (°C) |
| `maxdhwsetpt` | `"60"` | `SW=60` | Max DHW setpoint |
| `maxmodulation` | `"100"` | `MM=100` | Max modulation level |
| `ctrlsetpt` | `"55"` | `CS=55` | Control setpoint |
| `ctrlsetpt2` | `"0"` | `C2=0` | Control setpoint 2 |
| `chenable` | `"1"` | `CH=1` | Central heating enable |
| `chenable2` | `"0"` | `H2=0` | Central heating 2 enable |
| `ventsetpt` | `"50"` | `VS=50` | Ventilation setpoint |
| `coolingenable` | `"0"` / `"1"` | `CE=0` / `CE=1` | Cooling enable (bit2 of OT master status). Not persisted. |
| `summermode` | `"0"` / `"1"` | `SM=0` / `SM=1` | Summer mode (bit5 of OT master status). Persisted to flash. |
| `dhwblocking` | `"0"` / `"1"` | `BW=0` / `BW=1` | DHW blocking (bit6 of OT master status). Not persisted. |
| `coolinglevel` | `"50.0"` | `CC=50.0` | Cooling control signal (MsgID 7, 0-100%). Alias: `coolingcontrol`. |
| `coolingcontrol` | `"50.0"` | `CC=50.0` | Cooling control signal (MsgID 7, 0-100%). Alias for `coolinglevel`. |
| `ch1opermode` | `"0"`–`"15"` | `MH=x` | CH1 operating mode (lower nibble of MsgID 99 byte 4). Values 0-15 per OT spec. |
| `dhwopermode` | `"0"`–`"15"` | `MW=x` | DHW operating mode (lower nibble of MsgID 99 byte 3). Values 0-15 per OT spec. |
| `ch2opermode` | `"0"`–`"15"` | `M2=x` | CH2 operating mode (upper nibble of MsgID 99 byte 4). Values 0-15 per OT spec. |
| `remoterequest` | `"0"` | `RR=x` | Remote request (MsgID 4, one-shot WRITE_DATA). Value is the request code byte. |
| `timesync` | `"HH:MM/DOW"` | `SC=HH:MM/DOW` | Time/day sync (MsgID 20). DOW is day-of-week 1-7. No response synthesized. |
| `printsummary` | `"0"` / `"1"` | `PS=0` / `PS=1` | Print summary mode. 1=suppress frame-by-frame output; emit periodic CSV summary. 0=resume normal output. |
| `msginterval` | `"500"` | `MI=500` | Minimum OT message interval in ms (100-1275). OTGW32 only; persisted. Acknowledge value is in centiseconds (divide by 10). |
| `failsafe` | `"0"` / `"1"` | `FS=0` / `FS=1` | Fail-safety on thermostat disconnect. 1=activate setback when thermostat is silent; 0=disable. Persisted. |
| `gpioa` | `"0"`-`"9"` | `GA=x` | GPIO-A function code. On OTGW32: stored locally and returned by PR=G query, but has no hardware effect. |
| `gpiob` | `"0"`-`"9"` | `GB=x` | GPIO-B function code. On OTGW32: stored locally and returned by PR=G query, but has no hardware effect. |

**Gateway mode values (`gatewaymode` topic)**

| Payload | OT Command | Mode | Description |
|---------|-----------|------|-------------|
| `"1"` | `GW=1` | Gateway | Full gateway: scheduler + thermostat forwarding + overrides (default) |
| `"0"` | `GW=0` | Bypass | Thermostat direct to boiler via relay; OT-direct inactive. Requires bypass relay hardware. On boards without relay: no-op (`NG` response). |
| `"2"` or `"S"` | `GW=2` / `GW=S` | Master/Standalone | OTGW32 is the sole OT master; scheduler only, no thermostat expected. OTGW32 extension. |
| `"M"` | `GW=M` | Monitor | Transparent pass-through; all frames forwarded unmodified. OTGW32 extension. |
| `"L"` | `GW=L` | Loopback | Internal loopback test mode with simulated boiler responses; no hardware needed. OTGW32 extension. |
| `"R"` | `GW=R` | Reset | Full gateway reset: clears all transient state and restarts OT interfaces. OTGW32 extension. |

On standard PIC firmware, only `0` (monitor) and `1` (gateway) are recognized.

#### Advanced Commands

| Topic Suffix | Payload | OT Command | Description |
| ------------ | ------- | ---------- | ----------- |
| `temperaturesensor` | `"O=21.5"` | `TS=O=21.5` | Temperature sensor function (`O`=outside, `R`=return) |
| `addalternative` | `"12"` | `AA=12` | Re-enable a previously disabled schedule entry (MsgID 1-127) |
| `delalternative` | `"12"` | `DA=12` | Disable a schedule entry (MsgID 1-127) |
| `unknownid` | `"12"` | `UI=12` | Mark MsgID as unknown: gateway responds UNKNOWN_DATAID to thermostat |
| `knownid` | `"12"` | `KI=12` | Mark MsgID as known again; restore normal forwarding |
| `priomsg` | `"12"` | `PM=12` | Send MsgID on next cycle with priority (force immediate poll) |
| `setresponse` | `"12:0000"` | `SR=12:0000` | Set stored response override: gateway answers thermostat for MsgID with given 4-hex-digit data word |
| `clearrespons` | `"12"` | `CR=12` | Clear stored response override for MsgID |
| `setresponsemod` | `"12:0000"` | `RM=12:0000` | Set response modifier: modify boiler→thermostat response data for MsgID (4-hex-digit mask applied) |
| `clearresponsemod` | `"12"` | `CM=12` | Clear response modifier for MsgID |
| `resetcounter` | `"HBS"` | `RS=HBS` | Reset a boiler energy/runtime counter (see table below) |
| `ignoretransitations` | `"0"` / `"1"` | `IT=0` / `IT=1` | Ignore transitions in OT signal (0=off, 1=on) |
| `overridehb` | `"0"` / `"1"` | `OH=0` / `OH=1` | Override high byte of OT frames (0=off, 1=on) |
| `forcethermostat` | `"I"` | `FT=I` | Force thermostat detection mode (mirrors PIC FT= command) |
| `voltageref` | `"3"` | `VR=3` | Voltage reference setting (stored locally, returned by PR=V) |
| `debugptr` | `"0"` | `DP=0` | Debug pointer (no-op; acknowledged but has no effect on OTGW32) |
| `thermostatslaveparam` | `"11:0"` | `TP=11:0` | Read TSP/FHB thermostat slave parameter. Format: `MsgID:Index` (read) or `MsgID:Index=Value` (write). Valid MsgIDs: 11/13 (HC), 89/91 (DHW), 106/108 (solar). FHB entries (13, 91, 108) are read-only. |

**Reset counter codes (`resetcounter` topic / `RS=` command)**

OTGW32 maps counter names to OT MsgIDs via WRITE_DATA with value 0:

| Payload | OT MsgID | Counter |
|---------|----------|---------|
| `HBS` | 116 | Heat burner starts |
| `HPS` | 117 | Heat pump starts |
| `WPS` | 118 | Warm water pump starts |
| `WBS` | 119 | Warm water burner starts |
| `HBH` | 120 | Heat burner operating hours |
| `HPH` | 121 | Heat pump operating hours |
| `WPH` | 122 | Warm water pump operating hours |
| `WBH` | 123 | Warm water boiler operating hours |

**PR= query commands**

Sending `PR=X` returns the current value of register `X`. On OTGW32 these are synthesized from internal state; the responses are compatible with the PIC firmware format.

| Topic suffix | Payload | Returns | Description |
|--------------|---------|---------|-------------|
| `command` | `"PR=A"` | `PR: A=OpenTherm Gateway OTGW32` | Device banner (tools pattern-match on this) |
| `command` | `"PR=M"` | `PR: M=G` / `M=M` / `M=P` / `M=S` / `M=L` | Current gateway mode: G=gateway, M=monitor, P=passthru (bypass), S=standalone, L=loopback |
| `command` | `"PR=O"` | `PR: O=T20.50` / `O=C20.50` / `O=N` | Setpoint override: T=temporary (TT=), C=constant (TC=), N=none |
| `command` | `"PR=S"` | `PR: S=15.00` | Setback temperature (SB= value) |
| `command` | `"PR=W"` | `PR: W=0` / `W=1` / `W=A` | DHW override: 0=off, 1=on, A=auto |
| `command` | `"PR=G"` | `PR: G=00` | GPIO A+B function codes (stored locally, no hardware effect on OTGW32) |
| `command` | `"PR=I"` | `PR: I=00` | GPIO A+B input states (always `00` on OTGW32, no GPIO inputs) |
| `command` | `"PR=L"` | `PR: L=RFFTTT` | LED A–F function chars (stored locally, no hardware effect on OTGW32) |
| `command` | `"PR=T"` | `PR: T=0/0` | Tweaks: ignore_transitions/override_high_byte |
| `command` | `"PR=D"` | `PR: D=O` | Temp sensor function (O=outside, R=return) |
| `command` | `"PR=P"` | `PR: P=M` | Smart power mode (always M=medium on OTGW32) |
| `command` | `"PR=R"` | `PR: R=I` | Thermostat detection mode (always I=internal on OTGW32) |
| `command` | `"PR=B"` | `PR: B=Apr  8 2026` | Firmware build date |
| `command` | `"PR=C"` | `PR: C=240` | CPU clock in MHz (ESP32-S3 at 240 MHz) |
| `command` | `"PR=Q"` | `PR: Q=P` / `Q=C` / `Q=W` / `Q=B` / `Q=E` | Reset cause: P=power-on, C=software(cold), W=watchdog, B=brownout, E=external |
| `command` | `"PR=N"` | `PR: N=10` | Message interval in centiseconds (100ms → N=10) |
| `command` | `"PR=V"` | `PR: V=3` | Voltage reference value |

**LED function codes (`LA=` through `LF=` commands)**

| Topic suffix | Payload | OT Command | Description |
|--------------|---------|------------|-------------|
| `command` | `"LA=F"` | `LA=F` | LED A function. On OTGW32: stored locally; returned by `PR=L`; no hardware LED effect. |
| `command` | `"LB=X"` | `LB=X` | LED B function. Same no-op note as LA=. |
| `command` | `"LC=O"` | `LC=O` | LED C function. Same no-op note as LA=. |
| `command` | `"LD=M"` | `LD=M` | LED D function. Same no-op note as LA=. |
| `command` | `"LE=P"` | `LE=P` | LED E function. Same no-op note as LA=. |
| `command` | `"LF=C"` | `LF=C` | LED F function. Same no-op note as LA=. |

All `LA=` through `LF=` commands are accepted and echoed back as a valid response on both PIC and OTGW32 builds. On OTGW32 they only update the internal state reported by `PR=L`; there is no physical LED hardware driven by these codes.

#### SAT Commands

SAT-specific commands are nested under the `sat/` sub-topic. All commands are subscribed under `{TopTopic}/set/{UniqueId}/sat/<subtopic>`.

**Core Control Commands:**

| Topic Suffix | Payload | Description |
| ------------ | ------- | ----------- |
| `sat/target` | `"21.5"` | Set SAT target room temperature (5.0-30.0 °C, persisted to flash) |
| `sat/indoor_temp` | `"20.8"` | Push external indoor temperature (expires after sensor max age) |
| `sat/outdoor_temp` | `"8.0"` | Push external outdoor temperature (expires after sensor max age) |
| `sat/enabled` | `"true"` / `"false"` | Enable or disable SAT controller |
| `sat/control_mode` | `"continuous"` / `"pwm"` / `"auto"` or `"0"` / `"1"` / `"2"` | Set control mode |
| `sat/preset` | `"away"` / `"eco"` / `"comfort"` / `"sleep"` / `"activity"` / `"home"` | Activate a named preset |

**Window and Humidity Commands:**

| Topic Suffix | Payload | Description |
| ------------ | ------- | ----------- |
| `sat/window` | `"open"` / `"closed"` / `"1"` / `"0"` / `"ON"` | Window open/closed detection |
| `sat/humidity` | `"62.5"` | Push indoor humidity (0-100%) |
| `sat/valves_open` | `"true"` / `"false"` / `"1"` / `"open"` | Report TRV valves open state |

**Multi-Area Temperature:**

| Topic Suffix | Payload | Description |
| ------------ | ------- | ----------- |
| `sat/area/0` | `"21.3"` | Push area 0 temperature (°C) |
| `sat/area/1` | `"20.1"` | Push area 1 temperature (°C) |
| `sat/area/2` | `"19.8"` | Push area 2 temperature (°C) |
| `sat/area/3` | `"22.0"` | Push area 3 temperature (°C) |

**Multi-Zone Temperature Control:**

| Topic Suffix | Payload | Description |
| ------------ | ------- | ----------- |
| `sat/zone/<n>/room_temp` | `"20.5"` | Push room temperature for zone n (1-based index) |
| `sat/zone/<n>/setpoint` | `"21.0"` | Push setpoint for zone n (1-based index) |
| `sat/zone_count` | `"2"` | Set number of active zones (1-4) |
| `sat/zone_timeout_s` | `"600"` | Set zone inactivity timeout in seconds |

**Solar and Sun Elevation:**

| Topic Suffix | Payload | Description |
| ------------ | ------- | ----------- |
| `sat/sun_elevation` | `"32.5"` | Push sun elevation in degrees (from HA sun entity) |
| `sat/solar_min_elevation` | `"12.0"` | Set minimum sun elevation for solar gain activation |

**OPV Calibration:**

| Topic Suffix | Payload | Description |
| ------------ | ------- | ----------- |
| `sat/ovp_start` | (any) | Start OPV calibration sequence |
| `sat/ovp_stop` | (any) | Cancel OPV calibration |
| `sat/ovp_value` | `"52.5"` | Manually set OPV value (°C) |
| `sat/ovp_enabled` | `"true"` / `"false"` | Enable or disable OPV |

**PID Control and Maintenance:**

| Topic Suffix | Payload | Description |
| ------------ | ------- | ----------- |
| `sat/reset_integral` | (any) | Reset PID integral accumulator to zero |
| `sat/flush` | (any) | Flush short-lived data (PID integral + cycle statistics window) |
| `sat/flush_threshold_h` | `"4"` | Configure auto-flush threshold in hours |

**Feature Switches (TASK-284, bound to HA switch entities):**

Payloads: `"true"` / `"false"`. Each of these corresponds to one of the 13 auto-discovered SAT switches.

| Topic Suffix | Bound SAT setting | Description |
| ------------ | ----------------- | ----------- |
| `sat/solar_gain` | `SATsolargain` | Enable solar gain compensation |
| `sat/summer_simmer` | `SATsummersimmer` | Enable summer simmer suppression |
| `sat/comfort_adjust` | `SATcomfortadjust` | Enable humidity-based comfort adjust |
| `sat/multi_area` | `SATmultiarea` | Enable multi-area temperature averaging |
| `sat/auto_tune` | `SATautotune` | Enable auto-tune analysis |
| `sat/simulation` | `SATsimulation` | Enable simulation mode |
| `sat/window_detection` | `SATwindowdetect` | Enable automatic window detection |
| `sat/force_pwm` | `SATforcepwm` | Force PWM mode override |
| `sat/push_setpoint` | `SATpushsetpoint` | Enable pushing setpoint changes to boiler |
| `sat/ovp_enabled` | `SATovpenabled` | Enable OPV (overshoot-protection value) |
| `sat/preset_sync` | `SATpresetsync` | Enable HA preset-sync subscription |
| `sat/dhw_enabled` | `SATdhwenabled` | Enable DHW heating |
| `sat/pwm_auto_switch` | `SATpwmautoswitch` | Auto-switch between PWM and continuous |

**Heating System Select (TASK-284):**

| Topic Suffix | Payload | Description |
| ------------ | ------- | ----------- |
| `sat/heating_system` | `"0"` / `"1"` / `"2"` / `"3"` | Heating system type (0=auto, 1=radiators, 2=heat pump, 3=underfloor). Bound to `SATsystem`. |

**Other Settings Commands (tuning and thresholds):**

These topics update the matching SAT setting directly. Payloads are numeric strings unless noted.

| Topic Suffix | Bound SAT setting | Notes |
| ------------ | ----------------- | ----- |
| `sat/heating_curve` | `SATcoefficient` | Heating curve coefficient |
| `sat/deadband` | `SATdeadband` | PID deadband (°C) |
| `sat/interval` | `SATinterval` | Control loop interval (seconds) |
| `sat/overshoot_margin` | `SATovershootmargin` | Overshoot margin (°C) |
| `sat/manufacturer` | `SATmanufacturer` | Boiler manufacturer name |
| `sat/max_modulation` | `SATmaxmodulation` | Max relative modulation % |
| `sat/dhw_setpoint` | `SATdhwsetpoint` | DHW setpoint (°C) |
| `sat/ovp_value` | `SATovpvalue` | OPV calibrated value (°C) |
| `sat/flame_off_offset` | `SATflameoffset` | Setpoint offset when flame is off |
| `sat/flow_offset` | `SATflowoffset` | Continuous mode max setpoint drop (°C) |
| `sat/summer_threshold` | `SATsummerthreshold` | Summer simmer outdoor threshold (°C) |
| `sat/summer_min_hours` | `SATsummerminhours` | Hours above threshold to activate summer mode |
| `sat/thermal_comfort` | `SATthermalcomfort` | Thermal comfort tuning |
| `sat/humidity_timeout_s` | `SAThumiditytimeout` | Humidity reading timeout (s) |
| `sat/comfort_humidity` | `SATcomforthumidity` | Reference humidity for comfort (%) |
| `sat/comfort_max_offset` | `SATcomfortmaxoffset` | Max comfort offset (°C) |
| `sat/ble_enable` | `SATbleenable` | Enable BLE sensor (ESP32 only) |
| `sat/ble_mac` | `SATblemac` | BLE sensor MAC address |
| `sat/ble_interval` | `SATbleinterval` | BLE poll interval (s) |
| `sat/preset_sync_topic` | `SATpresetsynctopic` | Preset-sync MQTT topic to subscribe to |
| `sat/auto_tune_rate` | `SATautotunerate` | Auto-tune learning rate |
| `sat/multi_area_count` | `SATmultiareacount` | Number of multi-area inputs |
| `sat/mod_sup_delay` | `SATmodsupdelay` | Modulation suppression delay |
| `sat/mod_sup_offset` | `SATmodsupoffset` | Modulation suppression offset |
| `sat/boiler_capacity` | `SATboilercapacity` | Rated boiler capacity (kW) |
| `sat/target_temp_step` | `SATtempstep` | Target temperature step (°C) |
| `sat/min_pressure` | `SATminpressure` | Min pressure alarm threshold (bar) |
| `sat/max_pressure` | `SATmaxpressure` | Max pressure alarm threshold (bar) |
| `sat/max_pressure_drop` | `SATmaxpressdrop` | Max acceptable pressure drop rate |
| `sat/preset_comfort` | `SATpresetcomfort` | Comfort preset temperature |
| `sat/preset_eco` | `SATpreseteco` | Eco preset temperature |
| `sat/preset_away` | `SATpresetaway` | Away preset temperature |
| `sat/preset_sleep` | `SATpresetsleep` | Sleep preset temperature |
| `sat/preset_activity` | `SATpresetactivity` | Activity preset temperature |
| `sat/preset_home` | `SATpresethome` | Home preset temperature |
| `sat/sensor_max_age` | `SATsensormaxage` | Max age for external sensor readings (s) |
| `sat/error_monitoring` | `SATerrormon` | Enable PID error ring-buffer monitoring |
| `sat/auto_gains_value` | `SATautogains` | Auto-gains multiplier |
| `sat/heating_mode` | `SATheatingmode` | Heating mode: `"eco"`, `"comfort"`, or numeric |
| `sat/cycles_per_hour` | `SATcyclesperhour` | Target PWM cycles per hour |
| `sat/valve_offset` | `SATvalveoffset` | Valve offset compensation |
| `sat/solar_freeze_integral` | `SATsolarfreezeint` | Freeze PID integral during solar gain |

The full reference with payload ranges and defaults is in the [SAT MQTT topics document](../../backlog/docs/doc-1%20-%20sat-mqtt-topics.md).

**Example**  - Set SAT target to 21.5°C:
```bash
mosquitto_pub -h mqtt-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/target" -m "21.5"
```

**Example**  - Push room temperature from external sensor:
```bash
mosquitto_pub -h mqtt-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/indoor_temp" -m "20.8"
```

**Example**  - Activate the Away preset:
```bash
mosquitto_pub -h mqtt-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/preset" -m "away"
```

**Example**  - Disable SAT:
```bash
mosquitto_pub -h mqtt-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/enabled" -m "false"
```

#### OTGW32 Commands (OT-direct)

OTGW32-specific commands are nested under the `otgw32/` sub-topic. All commands are subscribed under `{TopTopic}/set/{UniqueId}/otgw32/<subtopic>`. These are only processed on OTGW32 builds (`HAS_DIRECT_OT=1`); on standard PIC firmware, they are ignored.

| Topic Suffix | Payload | Description |
| ------------ | ------- | ----------- |
| `otgw32/room_temp` | `"20.8"` | Push room temperature to OT-direct engine |
| `otgw32/room_setpoint` | `"21.0"` | Push room setpoint to OT-direct engine |

**Example** -- Push room temperature to OTGW32:
```bash
mosquitto_pub -h mqtt-broker -t "OTGW/set/otgw-AABBCCDDEEFF/otgw32/room_temp" -m "20.8"
```

#### Alternative Topic Names

Commands can also be sent using the two-letter OTGW command codes directly as topic suffixes. For example, `TT`, `TC`, `OT`, etc.

### Home Assistant Status

| Topic | Description |
| ----- | ----------- |
| `homeassistant/status` | Monitors HA lifecycle (`online`/`offline`). On `offline` then `online`, re-publishes all HA discovery configs. |

---

## Home Assistant Auto-Discovery

The firmware supports Home Assistant MQTT auto-discovery, publishing discovery configuration messages to the `{haprefix}/` topic tree (default: `homeassistant/`).

### Architecture: Streaming Discovery

Discovery configs are no longer built from a filesystem template (`data/mqttha.cfg`, now archived under `docs/archive/`). The source of truth is the streaming compose functions in `src/OTGW-firmware/MQTTHaDiscovery.cpp`, which build each config directly into MQTT publish frames via a two-pass (MEASURE then WRITE) `MqttJsonWriter`. This eliminates the historical `sLine[1200]` staging buffer and avoids file I/O during discovery.

Each discovery publish uses `client.beginPublish()` with `retain = true`, so HA receives a full retained config per entity.

### Discovery Configuration

Discovery topics follow the pattern:

```
{haprefix}/{component}/{node_id}/{object_id}/config
```

Where:

- `{haprefix}` = `settings.mqtt.sHaprefix` (default: `homeassistant`)
- `{component}` = HA component type (`sensor`, `binary_sensor`, `switch`, `select`, `climate`, `number`)
- `{node_id}` = `settings.mqtt.sUniqueid` (default: `otgw-{MAC}`)
- `{object_id}` = unique identifier for the entity

### Entity Inventory

The firmware emits the following entity categories (verified against `MQTTHaDiscovery.cpp` on `feature-dev-2.0.0-otgw32-esp32-sat-support`):

| Component | Count | Source | Notes |
|-----------|-------|--------|-------|
| `sensor` | 289 | PROGMEM table `mqttHaSensorCfg[]` (indexed via `mqttHaSensorIndex[256]`) | Covers all OT numeric fields, status decoding, RBP, VH, solar, SAT sensor fan-out |
| `binary_sensor` | 53 | PROGMEM table `mqttHaBinSensorCfg[]` (indexed via `mqttHaBinSensorIndex[256]`) | Master/slave status bits, ASF flags, SAT health binaries |
| `climate` | 2 | `streamClimateDiscovery(idx=0/1)` | 0 = Thermostat, 1 = DHW Control |
| `number` | 1 | `streamNumberDiscovery()` | `Toutside_override` slider |
| `switch` (SAT, TASK-284) | 13 | `streamSatSwitchDiscovery(idx=0..12)` | See list below |
| `select` (SAT, TASK-284) | 1 | `streamSatSelectDiscovery(idx=0)` | `sat_heating_system` (options `"0"`..`"3"`) |
| `sensor` (Dallas) | runtime | `streamDallasSensorDiscovery()` via `configSensors()` | Published per detected 1-Wire address |

The SAT switch and select entries piggyback on OT pseudo-ID 0 in the discovery bitmap (they are streamed alongside the climate entities and marked done together with ID 0). The number entity is marked done with OT ID 27.

### Discovery Modes

The firmware uses three discovery paths:

1. **Drip publisher (background)**: After boot, MQTT connect, or Home Assistant restart, all known OT message IDs are queued in a bitmap. The `loopMQTTDiscovery()` function in the main loop drip-publishes one discovery config every 3 seconds (slowed to 30 seconds under heap pressure). This avoids MQTT burst storms that previously caused heap exhaustion on the ESP8266.

2. **JIT discovery (on OT message)**: When an OpenTherm message ID is observed for the first time and its discovery config has not yet been published, the config is sent immediately. This ensures discovery is available even before the drip publisher reaches that ID.

3. **Manual force**: Triggered via REST API (`POST /api/v2/otgw/discovery`) or the `F` serial command. Marks all IDs as pending and the drip publisher re-publishes them over the following minutes.

### Source-Separated Discovery

When `settings.mqtt.bSeparateSources` is enabled, discovery entries with source-template placeholders are expanded into three variants:

- `{entity}_thermostat` - values from thermostat requests
- `{entity}_boiler` - values from boiler responses
- `{entity}_gateway` - values injected/modified by the gateway

### SAT Discovery Entities

When SAT is enabled, the following HA entities are auto-discovered. Entities marked **ESP32 only** require BLE to be enabled and are published only on ESP32 builds.

**Climate Entity:**

| Entity Type | Entity ID | Description |
|-------------|-----------|-------------|
| `climate` | `sat_climate` | Climate entity: target temp control, mode (off/heat), current room temp from OT bus. `json_attributes_topic` provides extended state. `action_topic` maps off/pwm/continuous to off/heating. |

**Sensor Entities:**

| Entity Type | Entity ID | Unit | Notes |
|-------------|-----------|------|-------|
| `sensor` | `sat_setpoint` | °C | Flow temperature setpoint sent to boiler |
| `sensor` | `sat_heating_curve` | °C | Heating curve calculated value |
| `sensor` | `sat_pid_output` | °C | PID output; `json_attributes_topic` = `sat/pid_attributes` |
| `sensor` | `sat_error` | °C | PID error (target - room temp) |
| `sensor` | `sat_mode` | - | Control mode string (off/continuous/pwm) |
| `sensor` | `sat_boiler_status` | - | Boiler status code (0-14) |
| `sensor` | `sat_pwm_duty` | % | PWM duty cycle (value_template multiplies raw 0.00-1.00 by 100) |
| `sensor` | `sat_room_temp` | °C | Room temperature used by PID |
| `sensor` | `sat_outside_temp` | °C | Outside temperature used by heating curve |
| `sensor` | `sat_estimated_room` | °C | Estimated room temp during sensor fallback |
| `sensor` | `sat_humidity` | % | Indoor humidity |
| `sensor` | `sat_comfort_offset` | °C | Comfort adjustment applied to target |
| `sensor` | `sat_current_modulation` | % | Current boiler modulation (from `sat/current_modulation`) |
| `sensor` | `sat_pressure` | bar | Smoothed system pressure |
| `sensor` | `sat_pressure_drop_rate` | bar/h | Pressure drop rate |
| `sensor` | `sat_power` | kW | Current boiler power |
| `sensor` | `sat_energy_total` | kWh | Cumulative energy (total_increasing state class for HA energy dashboard) |
| `sensor` | `sat_duty_ratio` | - | EMA duty ratio (flame-on fraction) |
| `sensor` | `sat_manufacturer` | - | Detected/configured boiler manufacturer |
| `sensor` | `sat_pid_p` | °C | Proportional PID term |
| `sensor` | `sat_pid_i` | °C | Integral PID term |
| `sensor` | `sat_pid_d` | °C | Derivative PID term |
| `sensor` | `sat_kp` | - | Current Kp gain |
| `sensor` | `sat_ki` | - | Current Ki gain |
| `sensor` | `sat_kd` | - | Current Kd gain |
| `sensor` | `sat_raw_derivative` | - | Raw (unfiltered) derivative term |
| `sensor` | `sat_error_mean` | °C | Mean PID error over ring buffer |
| `sensor` | `sat_error_stddev` | °C | Std deviation of PID error |
| `sensor` | `sat_thermal_coeff` | - | Learned thermal drop coefficient |
| `sensor` | `sat_thermal_drop_rate` | °C/h | Current thermal drop rate sample |
| `sensor` | `sat_indoor_rise_rate` | °C/h | Measured indoor temperature rise rate |
| `sensor` | `sat_overshoot_fraction` | - | EMA fraction of cycles with overshoot |
| `sensor` | `sat_cycle_class` | - | Cycle classification string; `json_attributes_topic` = `sat/cycle_attributes` |
| `sensor` | `sat_curve_recommendation` | - | Heating curve recommendation; `json_attributes_topic` = `sat/curve_recommendation_attributes` |
| `sensor` | `sat_flame_status` | - | Flame health status string |
| `sensor` | `sat_summer_hours_above` | h | Hours outdoor temp above summer threshold |
| `sensor` | `sat_auto_tune_score` | - | Auto-tune PID score |
| `sensor` | `sat_consumption` | m³/h | Gas consumption |
| `sensor` | `sat_ble_temp` | °C | BLE sensor temperature (ESP32 only) |
| `sensor` | `sat_ble_humidity` | % | BLE sensor humidity (ESP32 only) |
| `sensor` | `sat_ble_rssi` | dBm | BLE sensor RSSI, diagnostic category (ESP32 only) |

**Binary Sensor Entities:**

| Entity Type | Entity ID | Device Class | Notes |
|-------------|-----------|--------------|-------|
| `binary_sensor` | `sat_flame_health` | problem | ON = flame healthy |
| `binary_sensor` | `sat_active` | - | Whether SAT is actively controlling |
| `binary_sensor` | `sat_safety_tripped` | problem | Safety shutdown active |
| `binary_sensor` | `sat_valves_open` | - | TRV valves reported open |
| `binary_sensor` | `sat_window_open` | window | Window open detection |
| `binary_sensor` | `sat_pressure_alarm` | problem | Pressure alarm active |
| `binary_sensor` | `sat_pressure_health` | problem | Pressure healthy; `json_attributes_topic` = `sat/pressure_health_attr` |
| `binary_sensor` | `sat_modulation_reliable` | - | Boiler modulation feedback reliable |
| `binary_sensor` | `sat_setpoint_mismatch` | problem | Setpoint mismatch detected on OT bus |
| `binary_sensor` | `sat_thermal_model_valid` | - | Thermal model has sufficient data |
| `binary_sensor` | `sat_solar_gain` | - | Solar gain compensation active |
| `binary_sensor` | `sat_summer_active` | - | Summer simmer suppressing heating |
| `binary_sensor` | `sat_humidity_valid` | - | Humidity reading is valid |
| `binary_sensor` | `sat_auto_tune_active` | - | Auto-tune analysis running |
| `binary_sensor` | `sat_setpoint_sync` | problem | Setpoint sync health |
| `binary_sensor` | `sat_modulation_sync` | problem | Modulation sync health |
| `binary_sensor` | `sat_ch_sync` | problem | CH sync health |

**Number Entity:**

| Entity Type | Entity ID | Unit | Description |
|-------------|-----------|------|-------------|
| `number` | `sat_dhw_setpoint` | °C | DHW setpoint slider (30-60 °C, 0.5 step) |

**Switch Entities (TASK-284, 13 switches):**

Each switch publishes its state to `%mqtt_pub_topic%/sat/<name>_enable` and takes commands on `%mqtt_sub_topic%/sat/<command>`. Payloads are literal `"true"` / `"false"` (`pl_on` / `pl_off`).

| Entity ID | Command topic suffix | State topic suffix | Icon |
|-----------|----------------------|--------------------|------|
| `sat_solar_gain_enable` | `sat/solar_gain` | `sat/solar_gain_enable` | `mdi:white-balance-sunny` |
| `sat_summer_simmer_enable` | `sat/summer_simmer` | `sat/summer_simmer_enable` | `mdi:weather-sunny-alert` |
| `sat_comfort_adjust_enable` | `sat/comfort_adjust` | `sat/comfort_adjust_enable` | `mdi:water-thermometer` |
| `sat_multi_area_enable` | `sat/multi_area` | `sat/multi_area_enable` | `mdi:home-group` |
| `sat_auto_tune_enable` | `sat/auto_tune` | `sat/auto_tune_enable` | `mdi:auto-fix` |
| `sat_simulation_enable` | `sat/simulation` | `sat/simulation_enable` | `mdi:flask` |
| `sat_window_detection_enable` | `sat/window_detection` | `sat/window_detection_enable` | `mdi:window-open-variant` |
| `sat_force_pwm_enable` | `sat/force_pwm` | `sat/force_pwm_enable` | `mdi:pulse` |
| `sat_push_setpoint_enable` | `sat/push_setpoint` | `sat/push_setpoint_enable` | `mdi:upload` |
| `sat_ovp_enabled` | `sat/ovp_enabled` | `sat/ovp_enabled` | `mdi:shield-check` |
| `sat_preset_sync_enable` | `sat/preset_sync` | `sat/preset_sync_enable` | `mdi:sync` |
| `sat_dhw_enabled` | `sat/dhw_enabled` | `sat/dhw_enabled` | `mdi:water-boiler` |
| `sat_pwm_auto_switch_enable` | `sat/pwm_auto_switch` | `sat/pwm_auto_switch_enable` | `mdi:swap-horizontal` |

**Select Entity (TASK-284):**

| Entity ID | Command topic suffix | State topic suffix | Options |
|-----------|----------------------|--------------------|---------|
| `sat_heating_system` | `sat/heating_system` | `sat/heating_system` | `"0"`, `"1"`, `"2"`, `"3"` |

### Retained discovery verification (v1.4.1+)

Since 1.4.1 the firmware can actively verify that its retained Home Assistant discovery configs are still present on the broker. This closes the gap where the broker loses retained state while Home Assistant stays connected, such as a `mosquitto` restart without `persistence true`, a volatile-filesystem crash or a manual `mosquitto_pub -r -n` deletion. None of those events fire the `homeassistant/status` offline → online transition, so the legacy reconnect-driven republish paths cannot recover from them. See [ADR-062](../adr/ADR-062-retained-discovery-verification.md) for the mechanism and the memory trade-offs.

**Mechanism**. The firmware subscribes to the node-scoped wildcard `<haprefix>/+/<nodeId>/#` for a 15-second window, counts retained discovery messages that arrive, and compares the total against `state.discovery.iPublishedTopicCount`. If fewer than expected arrive, it calls `markAllMQTTConfigPending()` and the drip re-announces every config. Foreign-nodeId retained configs that happen to pass through the wildcard are counted separately as "orphans" for diagnostics.

**Triggers**. A verify run can start in three ways:

1. **Automatic daily** — when `settings.mqtt.bDiscoveryAutoVerify` is true (default), the ADR-086 time dispatcher triggers a verify at the day-flip boundary. Disable this if your broker is noisy on wildcard subscriptions or if multiple OTGW nodes share a prefix and you want to spread the load manually.
2. **REST** — `POST /api/v2/discovery/verify`. See `docs/api/README.md` and `openapi.yaml` for the full contract, including the `409` / `503` error cases.
3. **Telnet debug key** — pressing `V` on the debug console starts an immediate verify window, provided the same preconditions are met (MQTT connected, free heap above the start threshold, no verify or drip already active).

**Why OTGW does not delete orphans**. The `nodeId` in the subscribe wildcard is user-configurable. Two OTGW devices, or an OTGW plus a test-bench instance, can legitimately share the same `<haprefix>`. Deleting everything under another node's path would silently wipe a neighbour's entities. OTGW therefore only *counts* orphans and publishes the number in `disc_last_orphan`; cleanup is always a manual broker operation.

**Disabling**. Set `settings.mqtt.bDiscoveryAutoVerify = false` via the Web UI (Settings → MQTT) or the REST settings API if the daily verify is undesirable in your environment. On-demand verify via the REST endpoint or the telnet `V` key remains available regardless of this setting.

**Diagnostic interpretation**.

- `disc_last_missing > 0` immediately after a run means a republish was just triggered. Wait for the drip to finish (observable via `pending_ids` on `GET /api/v2/discovery`), then start a second verify. If `last_missing` is still non-zero after two or three passes, investigate the broker: retained-message settings, persistence configuration, backup/restore gaps.
- `disc_last_orphan > 0` is purely informational. On a shared broker it is expected and does not require action.
- If `verify_runs` increases but `disc_last_verify_epoch` does not, the verify is aborting early because the heap dropped below the abort threshold during the window. This is harmless but indicates the device is under memory pressure from another subsystem.

### Discovery Lifecycle

- On firmware boot (`startMQTT()`): all discovery IDs are marked pending for drip publishing
- On MQTT connect: OT value-change tracking is reset (all values re-published), but discovery state is NOT reset (retained messages survive a reconnect on the broker)
- On Home Assistant restart (detected via `homeassistant/status` transitioning from `offline` to `online`): all discovery IDs are re-queued for drip publishing
- On manual force (`POST /api/v2/otgw/discovery`): all IDs are re-queued
- Discovery configs are published with `retain = true`
- The drip publisher runs at 3-second intervals (or 30-second intervals when free heap is below 8KB)

### Discovery Composition

Discovery configs are composed in flash by streaming functions in `MQTTHaDiscovery.cpp`:

- `streamSensorDiscovery()`, `streamBinarySensorDiscovery()` iterate hand-maintained PROGMEM config arrays (`mqttHaSensors[]`, `mqttHaBinSensors[]`) in `MQTTHaDiscovery.cpp`.
- `streamClimateDiscovery(idx)`, `streamNumberDiscovery()`, `streamSatSwitchDiscovery(idx)`, `streamSatSelectDiscovery(idx)` are hardcoded per index.
- `streamDallasSensorDiscovery(addr)` handles the runtime-addressed Dallas sensors.

Each function uses a two-pass `MqttJsonWriter` (first pass measures bytes, second writes via `PubSubClient::beginPublish()` + chunked payload writes) so no full-message RAM buffer is needed.

Runtime values interpolated into configs (instead of template placeholders) come from a `HaDiscoveryContext`:

| Context field | Used for |
| ------------- | -------- |
| `nodeId` | `{MQTT unique id}` (default `otgw-{MAC}`) |
| `hostname` | Entity `name` prefix |
| `haPrefix` | Discovery topic prefix |
| `mqttPubTopic` | Publish/state topic base |
| `mqttSubTopic` | Subscribe/command topic base |
| `version` | `device.sw_version` and `origin.sw` |

Source-separated discovery (when `settings.mqtt.bSeparateSources = true`) is handled at stream time by `expandAndStreamSensorSources()`, which emits three variants (`_thermostat`, `_boiler`, `_gateway`) for sensors marked with `MQTT_HA_FLAG_ANY_SOURCE`.

---

## Heap diagnostic telemetry

The firmware publishes heap-pressure and discovery counters as 17 individual retained topics under `{TopTopic}/value/{UniqueId}/otgw-firmware/stats/*`. Each metric lives on its own topic (no JSON bundling) so consumers can subscribe to a single counter, expose it as a Home Assistant sensor without JSON path templating, or graph it directly in Grafana.

**Topic prefix**: `{TopTopic}/value/{UniqueId}/otgw-firmware/stats/<metric>` (all retained)

**Cadence**: once per hour, on the wall-clock hour boundary. Publishing is dispatched by the unified time handler introduced in ADR-086, which also drives the daily discovery-verify trigger. No publish happens while MQTT is disconnected.

**Device identity**: to map `{UniqueId}` (e.g. `otgw-a1b2c3`) back to a human-readable device name, subscribe to `{TopTopic}/value/{UniqueId}/otgw-firmware/hostname` (retained, published on every MQTT (re)connect).

**Metrics**: most topics carry *session counters* that reset to zero on reboot; a few are *live samples* measured at publish time; three are *last-known* values captured at the end of the previous discovery verify run. Payloads are plain ASCII decimal numbers.

| Metric topic suffix | Type | Kind | Meaning |
| ------------------- | ---- | ---- | ------- |
| `ws_drops` | uint32 | session counter | WebSocket messages dropped due to heap pressure since boot. |
| `mqtt_drops` | uint32 | session counter | MQTT messages dropped due to heap pressure since boot. |
| `enter_low` | uint32 | session counter | Transitions into the `HEAP_LOW` tier (from `HEALTHY`). |
| `enter_warning` | uint32 | session counter | Transitions into the `HEAP_WARNING` tier. |
| `enter_critical` | uint32 | session counter | Transitions into the `HEAP_CRITICAL` tier. |
| `drip_burst_skip` | uint32 | session counter | Discovery drip ticks skipped while a Status-frame burst was active (TASK-342). |
| `drip_cooldown_skip` | uint32 | session counter | Discovery drip ticks skipped in the post-burst cooldown window (TASK-347). |
| `drip_slowmode` | uint32 | session counter | Transitions into the 10-second slow-mode cadence caused by heap pressure. |
| `free_heap` | uint32 | live sample | `ESP.getFreeHeap()` at publish time, in bytes. |
| `max_block` | uint32 | live sample | `ESP.getMaxFreeBlockSize()` at publish time, in bytes. |
| `frag_pct` | uint8 | live sample | Heap fragmentation percentage at publish time (0 – 100). |
| `disc_verify_runs` | uint32 | session counter | Lifetime count of retained-discovery verify windows started since boot. |
| `disc_republish_triggered` | uint32 | session counter | Lifetime count of verify runs that ended with missing configs and triggered a republish. |
| `disc_last_missing` | uint16 | last known | Retained configs missing at the end of the previous verify run. |
| `disc_last_orphan` | uint16 | last known | Foreign-nodeId retained configs observed during the previous verify run (informational). |
| `disc_published_topics` | uint32 | live-ish | Running count of discovery topics successfully published since boot. Incremented inside the streaming helpers after a successful `endPublish`. |
| `disc_last_verify_epoch` | uint32 | last known | Unix-epoch timestamp of the last completed verify run (0 = none since boot). |

**Counter reset semantics**

- All `session counter` topics reset to zero on reboot and increase monotonically while the firmware runs. They are *cumulative* within a session.
- `live sample` topics reflect the state at the moment of publish; do not use them to infer trends without sampling.
- `last known` topics hold the result of the *previous* verify run. During an active verify window they are not updated until `endVerify` runs.

Subscribing to `{TopTopic}/value/{UniqueId}/otgw-firmware/stats/+` gives you all 17 counters as individual messages. A matching REST surface is available at `GET /api/v2/discovery` for the discovery-specific subset of these fields (see `docs/api/README.md`).

---

## MQTT Publish Gating

The firmware implements several safeguards to prevent MQTT message storms:

### Interval Gate

`mqttPublishAllowed` is a global flag managed by the `OTPublishGate` system. It controls whether `sendMQTTData()` calls are permitted. This prevents publishing faster than the configured interval (`settings.mqtt.iInterval`).

### Value-Change Deduplication

For most OpenTherm message IDs, the firmware tracks the last published value and only re-publishes when the value changes. This significantly reduces MQTT traffic for stable readings.

### Status Bit Throttling

Individual status flag bits (master/slave status, Message ID 0) have per-bit publish timers to prevent rapid toggling from flooding MQTT.

### Heap Health Backpressure

The `canPublishMQTT()` function checks heap health before each publish. When free heap drops below critical thresholds, MQTT publishing is throttled or suspended to prevent crashes.

### Republish on Reconnect

On MQTT (re)connect, the firmware calls `requestMQTTRepublishAll()` to reset all value-change tracking, ensuring the next observed value for each message ID is published regardless of whether it matches the previously published value.

### MQTT Gate Debug Flag

The `bMQTTGate` flag (`state.debug.bMQTTGate`) enables verbose telnet debug output for the MQTT interval gating and value-change deduplication logic. When enabled, the firmware logs each publish decision including whether a message was suppressed by the gate, the interval timer state, and the value-change check result.

Toggle via telnet debug console: press `g` to enable/disable MQTT gate logging.

This flag is runtime-only and is not persisted across reboots.

---

## Configuration Settings

These MQTT-related settings are configurable via the REST API (`/api/v2/settings`) or the Web UI:

| Setting | Default | Description |
| ------- | ------- | ----------- |
| `mqttenable` | `false` | Enable/disable MQTT |
| `mqttbroker` | `""` | MQTT broker hostname or IP |
| `mqttbrokerport` | `1883` | MQTT broker port |
| `mqttuser` | `""` | MQTT username (empty for anonymous) |
| `mqttpasswd` | `""` | MQTT password |
| `mqtttoptopic` | `"OTGW"` | Top-level topic prefix |
| `mqtthaprefix` | `"homeassistant"` | HA discovery prefix |
| `mqttuniqueid` | `"otgw-{MAC}"` | Unique device ID |
| `mqttharebootdetection` | `true` | Re-publish discovery on HA restart |
| `mqttotmessage` | `false` | Publish raw OT messages |
| `mqttinterval` | `0` | Minimum publish interval (seconds, 0 = no throttle) |
| `mqttseparatesources` | `false` | Publish to source-separated sub-topics |

---

## Example MQTT Messages

### Subscribing to All Data

```bash
mosquitto_sub -h mqtt-broker -t "OTGW/value/otgw-AABBCCDDEEFF/#" -v
```

### Setting Room Temperature

```bash
mosquitto_pub -h mqtt-broker -t "OTGW/set/otgw-AABBCCDDEEFF/setpoint" -m "21.5"
```

### Sending Raw OTGW Command

```bash
mosquitto_pub -h mqtt-broker -t "OTGW/set/otgw-AABBCCDDEEFF/command" -m "TT=21.5"
```

### Enabling Hot Water

```bash
mosquitto_pub -h mqtt-broker -t "OTGW/set/otgw-AABBCCDDEEFF/hotwater" -m "1"
```

### Setting Gateway Mode

```bash
mosquitto_pub -h mqtt-broker -t "OTGW/set/otgw-AABBCCDDEEFF/gatewaymode" -m "1"
```

### Home Assistant YAML Example (Manual)

If not using auto-discovery, you can manually configure MQTT sensors:

```yaml
mqtt:
  sensor:
    - name: "Boiler Temperature"
      state_topic: "OTGW/value/otgw-AABBCCDDEEFF/boilertemperature"
      unit_of_measurement: "\u00b0C"
      device_class: temperature

    - name: "Room Temperature"
      state_topic: "OTGW/value/otgw-AABBCCDDEEFF/roomtemperature"
      unit_of_measurement: "\u00b0C"
      device_class: temperature

    - name: "CH Water Pressure"
      state_topic: "OTGW/value/otgw-AABBCCDDEEFF/chwaterpressure"
      unit_of_measurement: "bar"
      device_class: pressure

  binary_sensor:
    - name: "Flame Status"
      state_topic: "OTGW/value/otgw-AABBCCDDEEFF/flamestatus"
      payload_on: "ON"
      payload_off: "OFF"

    - name: "CH Mode"
      state_topic: "OTGW/value/otgw-AABBCCDDEEFF/chmodus"
      payload_on: "ON"
      payload_off: "OFF"
```

---

## Migration from 1.4.x / pre-release 2.0.0 (OT-bus state topics)

Starting with 2.0.0, three OT-bus presence values have been promoted from hardware-specific subtrees to the generic value namespace. They describe what the OTGW-firmware observes on the OpenTherm bus, not properties of the PIC coprocessor or the OT-direct driver, so they no longer belong under a hardware-specific prefix. Background and rationale: ADR-084 (amends ADR-065).

### Before / after topic map

| Old topic (1.4.x / pre-release 2.0.0)                               | New topic (2.0.0 and later)                         |
|---------------------------------------------------------------------|-----------------------------------------------------|
| `OTGW/value/<uniqueId>/otgw-pic/boiler_connected`                   | `OTGW/value/<uniqueId>/boiler_connected`            |
| `OTGW/value/<uniqueId>/otgw-pic/thermostat_connected`               | `OTGW/value/<uniqueId>/thermostat_connected`        |
| `OTGW/value/<uniqueId>/otgw-pic/otgw_connected`                     | `OTGW/value/<uniqueId>/otgw_connected`              |
| `OTGW/value/<uniqueId>/otgw-otdirect/boiler_connected`              | `OTGW/value/<uniqueId>/boiler_connected`            |
| `OTGW/value/<uniqueId>/otgw-otdirect/thermostat_connected`          | `OTGW/value/<uniqueId>/thermostat_connected`        |
| `OTGW/value/<uniqueId>/otgw-otdirect/ot_online` (renamed)           | `OTGW/value/<uniqueId>/otgw_connected`              |

Six old topic paths, three new canonical topics. Payload semantics are unchanged: `"ON"` / `"OFF"`, retained.

### Automatic cleanup (no action required)

The 2.0.0 firmware self-heals. On every MQTT reconnect it briefly subscribes to the six deprecated topics above and clears any retained payload it finds by republishing an empty retained message. After the first successful reconnect following the upgrade, your broker no longer serves stale retained values on the old topics. This is idempotent: on brokers that never saw the pre-2.0.0 firmware, the cleanup is a silent no-op.

### Manual cleanup (advanced users)

If you prefer to clean up manually (for example, you do not boot the device after the upgrade, or you manage broker state with external tooling), publish an empty retained message on each deprecated topic:

```bash
# Replace <broker> and <uniqueId>.
# The default uniqueId is otgw-<MAC>, where <MAC> is the last three octets
# of the device MAC address, uppercased.
BROKER=192.168.1.10
UID=otgw-AABBCC

for leaf in \
    otgw-pic/boiler_connected \
    otgw-pic/thermostat_connected \
    otgw-pic/otgw_connected \
    otgw-otdirect/boiler_connected \
    otgw-otdirect/thermostat_connected \
    otgw-otdirect/ot_online ; do
  mosquitto_pub -h "$BROKER" -t "OTGW/value/$UID/$leaf" -r -n
done
```

Flags explained:

- `-r`: set the retain flag on the publish.
- `-n`: send a null (zero-length) payload. Combined with `-r`, this instructs the broker to delete the retained entry for that topic (MQTT 3.1.1, section 3.3.1.3).

### Listing which retained topics still exist on your broker

Before or after the cleanup, inspect retained state with:

```bash
mosquitto_sub -h "$BROKER" -t 'OTGW/value/+/otgw-pic/#'      -v --retained-only -W 2
mosquitto_sub -h "$BROKER" -t 'OTGW/value/+/otgw-otdirect/#' -v --retained-only -W 2
```

`-W 2` exits after 2 seconds of silence, which is enough for any retained delivery on a local broker.

### Home Assistant consumers

Home Assistant discovery is republished automatically by the firmware on reconnect. The `Boiler connected` and `Thermostat connected` binary_sensor entities keep their `unique_id`, so entity history and automations are preserved across the upgrade. After the first reconnect, their `state_topic` shows the new generic path. On OTGW32 builds without a PIC, these two entities now appear for the first time (they were previously gated behind the PIC flag).

---

## Related Documentation

- **REST API**: See [README.md](README.md) for the REST API reference
- **OTGW Commands**: See the [OTGW firmware documentation](https://otgw.tclcode.com/firmware.html) for the full PIC command reference
- **OpenTherm Protocol**: See `docs/opentherm specification/` in the repository
