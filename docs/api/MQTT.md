# OTGW-firmware MQTT Topic Documentation

This document describes all MQTT topics published and subscribed to by the OTGW-firmware.

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
| `otgw-firmware/error` | `"LittleFS mount failed..."` | Error messages (not retained, only when applicable) |

### PIC Gateway Information

Published at startup, on MQTT (re)connect, and every 5 minutes.

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `otgw-pic/version` | `"5.4"` | PIC firmware version |
| `otgw-pic/deviceid` | `"gateway"` | PIC device ID |
| `otgw-pic/firmwaretype` | `"gateway"` | PIC firmware type (gateway/interface/diagnose) |
| `otgw-pic/picavailable` | `"ON"` / `"OFF"` | Whether PIC is detected |
| `otgw-pic/boiler_connected` | `"ON"` / `"OFF"` | Boiler communication status |
| `otgw-pic/thermostat_connected` | `"ON"` / `"OFF"` | Thermostat communication status |
| `otgw-pic/gateway_mode` | `"ON"` / `"OFF"` | Gateway mode (ON) vs monitor mode (OFF) |
| `otgw-pic/otgw_connected` | `"ON"` / `"OFF"` | OTGW serial connection status |

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

Published every control loop interval (default 30s) when SAT is enabled. Topics are under the standard publish namespace.

| Topic | Value | Retained | Description |
| ----- | ----- | -------- | ----------- |
| `sat/mode` | `"off"` / `"continuous"` / `"pwm"` | yes | Current control mode |
| `sat/setpoint` | `"43.1"` | yes | Final flow temperature setpoint sent to boiler (°C) |
| `sat/heating_curve` | `"42.3"` | yes | Heating curve calculated value (°C) |
| `sat/pid_output` | `"43.1"` | yes | PID output = curve + P + I + D (°C) |
| `sat/target` | `"21.0"` | yes | Target room temperature (°C) |
| `sat/error` | `"0.50"` | no | PID error = target - room temp (°C) |
| `sat/pid_p` | `"0.82"` | no | Proportional term |
| `sat/pid_i` | `"0.03"` | no | Integral term |
| `sat/pid_d` | `"-0.04"` | no | Derivative term |
| `sat/boiler_status` | `"3"` | no | Boiler status code (0-14) |
| `sat/cycle_class` | `"1"` | no | Last cycle classification (0-5) |
| `sat/pwm_duty` | `"0.65"` | no | PWM duty cycle (0.00-1.00) |
| `sat/safety_tripped` | `"false"` | no | Safety shutdown active |

**Boiler status codes**: 0=Off, 1=Idle, 2=Preheating, 3=At Setpoint, 4=Modulating Up, 5=Modulating Down, 6=Ignition Surge, 7=Stalled Ignition, 8=Anti-Cycling, 9=Pump Starting, 10=Waiting Flame, 11=Overshoot Cooling, 12=Post-Cycle, 13=Heating, 14=Cooling

**Cycle class codes**: 0=None, 1=Good, 2=Overshoot, 3=Underheat, 4=Short, 5=Uncertain

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
| `gatewaymode` | `"1"` | `GW=1` | Gateway mode (1=gateway, 0=monitor) |
| `setback` | `"15.0"` | `SB=15.0` | Setback temperature |
| `maxchsetpt` | `"80"` | `SH=80` | Max CH water setpoint |
| `maxdhwsetpt` | `"60"` | `SW=60` | Max DHW setpoint |
| `maxmodulation` | `"100"` | `MM=100` | Max modulation level |
| `ctrlsetpt` | `"55"` | `CS=55` | Control setpoint |
| `ctrlsetpt2` | `"0"` | `C2=0` | Control setpoint 2 |
| `chenable` | `"1"` | `CH=1` | Central heating enable |
| `chenable2` | `"0"` | `H2=0` | Central heating 2 enable |
| `ventsetpt` | `"50"` | `VS=50` | Ventilation setpoint |

#### Advanced Commands

| Topic Suffix | Payload | OT Command | Description |
| ------------ | ------- | ---------- | ----------- |
| `temperaturesensor` | `"O=21.5"` | `TS=O=21.5` | Temperature sensor function |
| `addalternative` | `"12"` | `AA=12` | Add alternative message ID |
| `delalternative` | `"12"` | `DA=12` | Delete alternative message ID |
| `unknownid` | `"12"` | `UI=12` | Add to unknown ID list |
| `knownid` | `"12"` | `KI=12` | Add to known ID list |
| `priomsg` | `"12"` | `PM=12` | Set priority message |
| `setresponse` | `"12,0000"` | `SR=12,0000` | Set response for message ID |
| `clearrespons` | `"12"` | `CR=12` | Clear response for message ID |
| `resetcounter` | `"0"` | `RS=0` | Reset counter |
| `ignoretransitations` | `"0"` | `IT=0` | Ignore transitions |
| `overridehb` | `"0"` | `OH=0` | Override high byte |
| `forcethermostat` | `"0"` | `FT=0` | Force thermostat detection |
| `voltageref` | `"3.3"` | `VR=3.3` | Set voltage reference |
| `debugptr` | `"0"` | `DP=0` | Debug pointer |

#### SAT Commands

SAT-specific commands are nested under the `sat/` sub-topic:

| Topic Suffix | Payload | Description |
| ------------ | ------- | ----------- |
| `sat/target` | `"21.5"` | Set SAT target room temperature (5-30 °C, persisted to flash) |
| `sat/indoor_temp` | `"20.8"` | Push external indoor temperature (expires after 5 min) |
| `sat/outdoor_temp` | `"8.0"` | Push external outdoor temperature (expires after 10 min) |
| `sat/enabled` | `"true"` / `"false"` | Enable or disable SAT controller |
| `sat/control_mode` | `"continuous"` / `"pwm"` / `"auto"` | Set control mode (or `"0"`, `"1"`, `"2"`) |

**Example** — Set SAT target to 21.5°C:
```bash
mosquitto_pub -h mqtt-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/target" -m "21.5"
```

**Example** — Push room temperature from external sensor:
```bash
mosquitto_pub -h mqtt-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/indoor_temp" -m "20.8"
```

**Example** — Disable SAT:
```bash
mosquitto_pub -h mqtt-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/enabled" -m "false"
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

### Discovery Configuration

Discovery topics follow the pattern:

```
{haprefix}/{component}/{node_id}/{object_id}/config
```

Where:

- `{haprefix}` = `settings.mqtt.sHaprefix` (default: `homeassistant`)
- `{component}` = HA component type (sensor, binary_sensor, switch, climate, etc.)
- `{node_id}` = `settings.mqtt.sUniqueid` (default: `otgw-{MAC}`)
- `{object_id}` = unique identifier for the entity

### Discovery Modes

The firmware uses two discovery paths:

1. **Bulk discovery (Path A)**: Triggered manually via REST API (`POST /api/v2/otgw/discovery`) or serial command (`F`). Publishes all configs from the `mqttha.cfg` file.

2. **JIT discovery (Path B)**: Automatically publishes discovery configs the first time an OpenTherm message ID is observed. This avoids publishing configs for message IDs that the specific thermostat/boiler combination never uses.

### Source-Separated Discovery

When `settings.mqtt.bSeparateSources` is enabled, discovery entries with source-template placeholders are expanded into three variants:

- `{entity}_thermostat` - values from thermostat requests
- `{entity}_boiler` - values from boiler responses
- `{entity}_gateway` - values injected/modified by the gateway

### SAT Discovery Entities

When SAT is enabled, the following HA entities are auto-discovered:

| Entity Type | Entity ID | Description |
|-------------|-----------|-------------|
| `climate` | `sat_climate` | Climate entity with target temp control and mode display |
| `sensor` | `sat_setpoint` | Flow temperature setpoint (°C) |
| `sensor` | `sat_heating_curve` | Heating curve value (°C) |
| `sensor` | `sat_pid_output` | PID output (°C) |
| `sensor` | `sat_error` | PID error (°C) |
| `sensor` | `sat_mode` | Control mode (off/continuous/pwm) |
| `sensor` | `sat_boiler_status` | Boiler status code |
| `sensor` | `sat_pwm_duty` | PWM duty cycle (%) |

The climate entity publishes its target temperature to `sat/target` and reads current temperature from the room temp on the OT bus.

### Discovery Lifecycle

- On MQTT connect: discovery state is reset, JIT re-publishes as messages arrive
- On Home Assistant restart (detected via `homeassistant/status`): discovery state is reset
- Discovery configs are published with `retain = true`

### Configuration File

Discovery templates are stored in `/mqttha.cfg` on the LittleFS filesystem. The file format uses semicolon-delimited lines:

```
{msg_id};{topic_template};{message_template}
```

Template placeholders:

| Placeholder | Replaced With |
| ----------- | ------------- |
| `%node_id%` | MQTT unique ID |
| `%sensor_id%` | Sensor-specific ID (e.g., Dallas address) |
| `%hostname%` | Device hostname |
| `%version%` | Firmware version |
| `%mqtt_pub_topic%` | MQTT publish namespace |
| `%mqtt_sub_topic%` | MQTT subscribe namespace |
| `%homeassistant%` | HA discovery prefix |
| `%source_suffix%` | Source suffix (_thermostat, _boiler, _gateway) |
| `%source_name%` | Source display name (Thermostat, Boiler, Gateway) |
| `%source_topic_segment%` | Source topic segment (thermostat, boiler, gateway) |

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

## Related Documentation

- **REST API**: See [README.md](README.md) for the REST API reference
- **OTGW Commands**: See the [OTGW firmware documentation](https://otgw.tclcode.com/firmware.html) for the full PIC command reference
- **OpenTherm Protocol**: See `docs/opentherm specification/` in the repository
