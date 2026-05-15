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
3. **Discovery reset**: Clears the discovery done/pending bitmaps. Non-OT pseudo-IDs (climate, number, Dallas, heap stats, firmware/PIC info) are immediately queued for drip publication. OT message ID discovery configs are **not** queued here; they publish JIT as each MsgID is first received on the bus (ADR-073).
4. **Subscribes** to `{TopTopic}/set/{UniqueId}/#` for incoming commands
5. **Subscribes** to `homeassistant/status` for Home Assistant lifecycle detection
6. **Publishes** version info, state information, and cached PIC settings
7. **Conditional OT republish**: Re-publishes all retained OT values only if the offline duration exceeded 5 minutes (the broker-loss threshold). Short outages are treated as network blips — the broker still holds retained topics. First boot and first-enable scenarios do not trigger a republish; the first-seen mechanism publishes each value naturally as it appears on the OT bus. On assumed broker restart (offline duration exceeded threshold), the discovery done/pending bitmaps are also reset and non-OT configs are re-queued, so JIT re-publishes OT configs as messages arrive.

---

## Published Topics

All published topics are under the publish namespace `{TopTopic}/value/{UniqueId}/` unless otherwise noted. Topics are published with `retain = true` by default.

### Firmware & Device Information

Published at startup, on MQTT (re)connect, and every 5 minutes.

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `otgw-firmware/hostname` | `"otgw-living"` | Device hostname (retained; maps UniqueId to a human-readable name) |
| `otgw-firmware/version` | `"1.5.0-beta.29"` | Firmware version string |
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

### SAT (Smart Autotune) Topics

Published when the SAT subsystem is active (`settings.sat.bEnabled`). Topics are published under the standard publish namespace, i.e. at `{TopTopic}/value/{UniqueId}/sat/<metric>`.

#### SAT Pressure Topics

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `sat/pressure` | `"1.45"` | Current CH water pressure in bar |
| `sat/pressure_drop_rate` | `"-0.002"` | Pressure drop rate in bar/hour |
| `sat/pressure_alarm` | `"true"` / `"false"` | Pressure alarm active |
| `sat/pressure_health` | `"ON"` / `"OFF"` | Pressure health status (retained) |

Note: the `sat/pressure_health_attr` JSON attributes topic was removed in v1.5.1-beta.3. The individual scalar topics (`sat/pressure`, `sat/pressure_drop_rate`, `sat/pressure_alarm`) remain and provide the same data without a JSON bundle.

#### SAT Climate Attributes Topic

| Topic | Value | Description |
| ----- | ----- | ----------- |
| `sat/climate_attributes` | JSON object | Extra state attributes for the HA thermostat climate entity; wired as `json_attributes_topic` in the discovery config |

The `sat/climate_attributes` payload is a JSON object containing PID and heating-curve state fields. Home Assistant reads this topic as `json_attributes_topic` on the SAT thermostat entity. Fields published:

| JSON key | Type | Description |
| -------- | ---- | ----------- |
| `optimal_coefficient` | float | Heating curve coefficient |
| `coefficient_derivative` | float | Coefficient derivative (0.0, not tracked) |
| `minimum_setpoint` | float | Minimum boiler setpoint (SAT_MIN_SETPOINT) |
| `boiler_flame_timing` | float | Duration of last completed flame cycle in seconds |
| `boiler_temperature_cold` | float | Boiler temperature when flame is off |
| `boiler_temperature_tracking` | bool | EMA tracking state (always false) |
| `boiler_temperature_derivative` | float | Temperature derivative (0.0, not tracked) |
| `error_source` | string | Error source zone (always `"main"`) |
| `error_pid` | float | Current PID error (target minus room) |
| `integral_enabled` | bool | Integral term active |
| `derivative_enabled` | bool | Derivative term active |
| `derivative_raw` | float | Raw filtered derivative before PID scaling |
| `current_kp` | float | Current proportional gain |
| `current_ki` | float | Current integral gain |
| `current_kd` | float | Current derivative gain |
| `relative_modulation_enabled` | bool | Relative modulation active (false when manufacturer quirk disables it) |

### Source-Separated Topics (Optional)

When `settings.mqtt.bSeparateSources` is enabled, OpenTherm data is published to two source-specific sibling topics alongside the canonical topic:

```
{TopTopic}/value/{UniqueId}/{label}             <- canonical (always published)
{TopTopic}/value/{UniqueId}/{label}_thermostat  <- when bSeparateSources=true
{TopTopic}/value/{UniqueId}/{label}_boiler      <- when bSeparateSources=true
```

All three are sibling leaves. Each is a normal MQTT topic — the canonical has no children, which makes topic-browser UX (mosquitto_sub, MQTT Explorer) straightforward and removes the structural ambiguity that an earlier nested shape would create.

There is no `_gateway` topic. Gateway override is observable by comparing `_thermostat` and `_boiler` — divergence means the gateway is intervening.

#### Worldview semantics (ADR-069)

Each per-source topic shows what *that device* sees on the OpenTherm bus, regardless of which side put the frame on the wire.

- **`{label}_thermostat`** = the value the thermostat sent (write requests) or received (read responses, including any gateway-faked answer via `A` frame).
- **`{label}_boiler`** = the value the boiler received (write requests, including any gateway override via `R` frame) or sent (read responses).
- **`{label}` (canonical)** = boiler-side worldview, identical to `{label}_boiler` for writes. For reads without answer-override, same as `{label}_boiler`. During answer-override, canonical carries `B` (the boiler's actual response) rather than the gateway-faked `A` value.

**Example: `TSet` with `CS=27.37` setpoint override active, thermostat asking 23 °C:**

| Topic | Value | Meaning |
|---|---|---|
| `…/value/<id>/TSet_thermostat` | `23.00` | What the thermostat asked for |
| `…/value/<id>/TSet_boiler` | `27.37` | What the boiler actually received |
| `…/value/<id>/TSet` (canonical) | `27.37` | Boiler-side worldview (= what reached the boiler) |

Without override, all three publish the same value. `_thermostat` and `_boiler` always update independently regardless of override state.

#### Frame-to-topic routing reference

| OT frame | Direction | Routes to `_thermostat` | Routes to `_boiler` | Routes to canonical |
|---|---|---|---|---|
| `T` (thermostat-write) | M→S | yes | yes (when no R follows) | yes (when no R follows) |
| `R` (gateway-substituted write) | M→S | no | yes | yes |
| `B` (boiler-response) | S→M | yes (when no A follows) | yes | yes |
| `A` (gateway-faked answer) | S→M | yes | no | no |

#### Canonical-topic publish gating (ADR-066, preserved)

The canonical topic `{TopTopic}/value/{UniqueId}/{label}` does not receive Write-Ack frames. The `_boiler` topic is additionally gated by a per-MsgID `bSlaveEchoesValue` flag in the OTlookup table. For MsgIDs where the OpenTherm v4.2 specification defines the slave's Write-Ack data field as undefined (typically `Tr` 24, `TrSet` 16, `MaxRelModLevelSetting` 14, `TrSetCH2` 23, `TRoomCH2` 37, `RFstrengthbatterylevel` 98), the `_boiler` topic is NOT updated for Write-Ack messages. The slave's acknowledgement carries no measurement; suppressing it avoids polluting the per-source observability surface with fake-zero readings.

For MsgIDs where the slave does store and echo the value (most R/W parameters, Class 5 remote boiler parameters such as `MaxTSet` 57 and `TdhwSet` 56, Class 6 transparent slave parameters, R/W counters), the `_boiler` topic continues to publish the slave's stored value, including clamped or modified variants distinct from the master's request.

See `docs/api/MQTT-message-id-echo-audit.md` for the full per-MsgID classification with spec-citation rationale.

#### Migration note (1.5.x topic-shape transition)

Two changes shipped in successive 1.5.0-beta builds. Migration guidance:

1. **Worldview routing (ADR-069, beta.20).** Earlier builds routed `A` (gateway-faked answer) frames to `/boiler` and dropped `T` frames during gateway override. ADR-069 corrected both: `A` now routes to the thermostat topic (where the value actually arrives), and `T` is preserved on the thermostat topic and canonical even when the gateway substitutes a different value to the boiler.
2. **Sibling-suffix shape (ADR-070, beta.21+).** Earlier builds used nested children topics (`{label}/thermostat`, `{label}/boiler`). ADR-070 replaces these with sibling siblings (`{label}_thermostat`, `{label}_boiler`) so the canonical topic is a clean leaf without children. Discovery configs are auto-updated on boot — Home Assistant unsubscribes from the old topic and subscribes to the new one in place (verified against `homeassistant/components/mqtt/subscription.py`). The mutual-exclusion rule from ADR-068 is dropped; the canonical entity now stays advertised alongside the two source variants.

**Cleanup of stale retained values:** old retained values at the previous nested topics (`{label}/thermostat`, `{label}/boiler`) linger on the broker as orphans because the firmware no longer publishes there. Home Assistant ignores them after discovery refresh (it has unsubscribed). Users with broker access can clear them with:

```bash
mosquitto_pub -h <broker> -t '<base>/value/<id>/<label>/thermostat' -r -n
mosquitto_pub -h <broker> -t '<base>/value/<id>/<label>/boiler' -r -n
```

HA users with `bSeparateSources = true` who built **manual** YAML sensor configs against the older routing or topic shape should re-point them at `{label}_thermostat` / `{label}_boiler`. Auto-discovered users need no action.

#### Migration note (zombie discovery configs from beta.21, ADR-071)

ADR-070 originally kept the **discovery** topic nested (`homeassistant/sensor/<id>/<entity>/thermostat/config`) while flattening the **state** topic to a sibling suffix. Empirical testing against `homeassistant/components/mqtt/discovery.py` showed HA's `TOPIC_MATCHER` regex requires `[a-zA-Z0-9_-]+` for `object_id` — slashes after the entity name fail the match and HA logs `illegal discovery topic` and discards the config. ADR-071 (beta.22+) supersedes that carve-out: discovery topics are now sibling-suffix too, e.g. `homeassistant/sensor/<id>/<entity>_thermostat/config`. The state-topic decision from ADR-070 is unchanged.

**Cleanup of zombie discovery configs:** retained nested-discovery configs published by beta.21 builds are zombies — HA never registered them, but they sit retained on the broker. Enumerate and clear them with:

```bash
# enumerate zombies (Ctrl-C after a second)
mosquitto_sub -h <broker> -v -t 'homeassistant/sensor/+/+/thermostat/config' -t 'homeassistant/sensor/+/+/boiler/config'
# clear each one (substitute the actual topic from the listing above)
mosquitto_pub -h <broker> -t 'homeassistant/sensor/<id>/<entity>/thermostat/config' -r -n
mosquitto_pub -h <broker> -t 'homeassistant/sensor/<id>/<entity>/boiler/config'    -r -n
```

Auto-discovered users on beta.22+ get fresh sibling-suffix configs on the next boot; the zombies are cosmetic broker state, not active HA entities.

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
| `gpioa` | `"0"` | `GA=0` | GPIO A function (0–7, see PIC firmware docs) |
| `gpiob` | `"0"` | `GB=0` | GPIO B function (0–7, see PIC firmware docs) |
| `leda` | `"F"` | `LA=F` | LED A function code (B/C/E/F/H/M/O/P/R/T/W/X) |
| `ledb` | `"F"` | `LB=F` | LED B function code |
| `ledc` | `"F"` | `LC=F` | LED C function code |
| `ledd` | `"F"` | `LD=F` | LED D function code |
| `lede` | `"F"` | `LE=F` | LED E function code |
| `ledf` | `"F"` | `LF=F` | LED F function code |
| `setclock` | `"3/14:30"` | `SC=3/14:30` | Set PIC clock (day 1=Mon…7=Sun, `day/HH:MM`) |
| `resetgateway` | *(any)* | *(hardware reset)* | Reset the OTGW PIC via hardware reset pin; payload ignored |

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

#### Alternative Topic Names

Commands can also be sent using the two-letter OTGW command codes directly as topic suffixes. For example, `TT`, `TC`, `OT`, etc.

### Home Assistant Status

| Topic | Description |
| ----- | ----------- |
| `homeassistant/status` | Monitors HA lifecycle (`online`/`offline`). On HA restart, HA re-reads retained discovery configs from the broker via its `homeassistant/#` subscription. The firmware does not republish configs on this event; retained configs are already on the broker (ADR-073). |

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

2. **JIT discovery (Path B, ADR-073)**: OT message ID discovery configs are published the first time that MsgID is received on the OpenTherm bus. This is now the sole automatic mechanism for OT IDs. Non-OT pseudo-IDs (climate thermostat/DHW control, outside temperature number, Dallas sensors, heap stats, firmware/PIC info) are queued at boot and published via the normal drip pipeline — they do not wait for a bus message.

   On assumed broker restart (offline duration exceeded 5 minutes), the discovery state resets and the same split applies: non-OT configs are re-queued immediately, OT configs re-publish as each MsgID re-appears on the bus.

### Source-Separated Discovery

When `settings.mqtt.bSeparateSources` is enabled, source-templated discovery entries are expanded into two variants matching the worldview routing model (ADR-069):

- `{entity}_thermostat` — value the thermostat sees (its sent write or its received read response, including any gateway-faked answer)
- `{entity}_boiler` — value the boiler sees (its received write, including any gateway override, or its sent read response)

The base `{entity}` is also advertised in discovery alongside both source variants (ADR-070 dropped the earlier mutual-exclusion rule from ADR-068). This means existing HA dashboards referencing the canonical entity continue to work when a user enables `bSeparateSources` — the option is purely additive.

For non-source-templated MsgIDs the base entity continues to publish in both modes.

There is no `{entity}_gateway` variant; gateway override is observable via divergence between the two source-variant topics.

#### Discovery topic shape (ADR-071)

Discovery topics for source variants use sibling-suffix shape, matching the state topic shape. This is required because Home Assistant's discovery dispatcher (`discovery.py:TOPIC_MATCHER`) only accepts `[a-zA-Z0-9_-]+` for the `object_id` segment — slashes in the object ID cause the payload to be silently discarded.

| Layer | Discovery topic |
|---|---|
| canonical | `{haprefix}/sensor/{nodeId}/{label}/config` |
| thermostat view | `{haprefix}/sensor/{nodeId}/{label}_thermostat/config` |
| boiler view | `{haprefix}/sensor/{nodeId}/{label}_boiler/config` |

### Retained discovery verification (v1.4.1+)

Since 1.4.1 the firmware can actively verify that its retained Home Assistant discovery configs are still present on the broker. This closes the gap where the broker loses retained state while Home Assistant stays connected, such as a `mosquitto` restart without `persistence true`, a volatile-filesystem crash or a manual `mosquitto_pub -r -n` deletion. None of those events fire the `homeassistant/status` offline → online transition, so the legacy reconnect-driven republish paths cannot recover from them. See [ADR-062](../adr/ADR-062-retained-discovery-verification.md) for the mechanism and the memory trade-offs.

**Mechanism**. The firmware subscribes to the node-scoped wildcard `<haprefix>/+/<nodeId>/#` for a 15-second window, counts retained discovery messages that arrive, and compares the total against `state.discovery.iPublishedTopicCount`. If fewer than expected arrive, it resets the discovery bitmaps and queues non-OT configs for drip re-publication; OT ID configs re-publish JIT as messages arrive (ADR-073). Foreign-nodeId retained configs that happen to pass through the wildcard are counted separately as "orphans" for diagnostics.

**Triggers**. A verify run can start in three ways:

1. **Automatic daily** — when `settings.mqtt.bDiscoveryAutoVerify` is true (default), the ADR-064 time dispatcher triggers a verify at the day-flip boundary. Disable this if your broker is noisy on wildcard subscriptions or if multiple OTGW nodes share a prefix and you want to spread the load manually.
2. **REST** — `POST /api/v2/discovery/verify`. See `docs/api/README.md` and `openapi.yaml` for the full contract, including the `409` / `503` error cases.
3. **Telnet debug key** — pressing `V` on the debug console starts an immediate verify window, provided the same preconditions are met (MQTT connected, free heap above the start threshold, no verify or drip already active).

**Why OTGW does not delete orphans**. The `nodeId` in the subscribe wildcard is user-configurable. Two OTGW devices, or an OTGW plus a test-bench instance, can legitimately share the same `<haprefix>`. Deleting everything under another node's path would silently wipe a neighbour's entities. OTGW therefore only *counts* orphans and publishes the number in `disc_last_orphan`; cleanup is always a manual broker operation.

**Disabling**. Set `settings.mqtt.bDiscoveryAutoVerify = false` via the Web UI (Settings → MQTT) or the REST settings API if the daily verify is undesirable in your environment. On-demand verify via the REST endpoint or the telnet `V` key remains available regardless of this setting.

**Diagnostic interpretation**.

- `disc_last_missing > 0` immediately after a run means a republish was just triggered. Wait for the drip to finish (observable via `pending_ids` on `GET /api/v2/discovery`), then start a second verify. If `last_missing` is still non-zero after two or three passes, investigate the broker: retained-message settings, persistence configuration, backup/restore gaps.
- `disc_last_orphan > 0` is purely informational. On a shared broker it is expected and does not require action.
- If `verify_runs` increases but `disc_last_verify_epoch` does not, the verify is aborting early because the heap dropped below the abort threshold during the window. This is harmless but indicates the device is under memory pressure from another subsystem.

### Entity Friendly Names (ADR-072)

The `name` field in every HA discovery payload follows a uniform format (shipping from beta.29 onward):

- Underscores replaced with spaces.
- First letter of each word capitalised (Title Case).
- Existing capitals preserved, so recognised acronyms render consistently: `DHW`, `CH`, `VH`, `OEM`, `ASF`, `RBP`, `GPIO`, `LED`, `MQTT`, `RF`, `OTC`, etc.
- No hostname prefix. The device-card title already shows the gateway hostname once; repeating it on every entity name is redundant.

**Examples:**

| Internal PROGMEM string | Rendered in HA |
|---|---|
| `DHW_Setpoint` | `DHW Setpoint` |
| `Burner_Unsuccessful_Starts` | `Burner Unsuccessful Starts` |
| `CH_Water_Pressure` | `CH Water Pressure` |
| `Status_Master_MemberID_Code` | `Status Master MemberID Code` |

Slug strings (used in `state_topic` paths and `unique_id` fields) are unaffected by this change and remain stable across upgrades.

### Discovery Lifecycle

- **On MQTT connect**: discovery bitmaps are reset; non-OT configs are queued for drip publication; OT ID configs publish JIT as each MsgID arrives on the bus (ADR-073).
- **On assumed broker restart** (offline duration exceeded 5 minutes): same as on connect -- bitmaps reset, non-OT configs queued, OT ID configs publish JIT.
- **On Home Assistant restart** (detected via `homeassistant/status`): HA reads retained discovery configs from the broker automatically via its `homeassistant/#` subscription. The firmware does not republish on this event (ADR-073). Retained configs are already on the broker from the original publication.
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
| `%source_suffix%` | Source suffix (`_thermostat`, `_boiler`, or empty for the canonical entity) |
| `%source_name%` | Source display name (`Thermostat`, `Boiler`, or empty for the canonical variant) |
| `%source_topic_segment%` | Source topic segment (`thermostat`, `boiler`, or empty for the canonical topic). No `gateway` segment exists; gateway override is observable via divergence between the two source-variant subtopics (ADR-069). |

---

## Heap diagnostic telemetry

The firmware publishes heap-pressure and discovery counters as 17 individual retained topics under `{TopTopic}/value/{UniqueId}/otgw-firmware/stats/*`. Each metric lives on its own topic (no JSON bundling) so consumers can subscribe to a single counter, expose it as a Home Assistant sensor without JSON path templating, or graph it directly in Grafana. These topics are announced via HA discovery so they appear automatically as diagnostic entities under the OTGW device card.

**Topic prefix**: `{TopTopic}/value/{UniqueId}/otgw-firmware/stats/<metric>` (all retained)

**Cadence**: once per hour, on the wall-clock hour boundary. Publishing is dispatched by the unified time handler introduced in ADR-064, which also drives the daily discovery-verify trigger. No publish happens while MQTT is disconnected.

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

On MQTT (re)connect, the firmware checks how long the connection was offline. If the offline duration exceeded 5 minutes (`MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS`), a full republish is triggered on the assumption that the broker may have lost its retained state (for example, mosquitto restarted without persistence). Short outages (under 5 minutes) are treated as network blips — the broker still holds all retained topics, so no republish is performed.

First boot and first-enable scenarios are treated as zero offline duration, so no republish is triggered. Instead, the first-seen mechanism publishes each OT value naturally the first time it appears on the OT bus.

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
| `mqttharebootdetection` | `true` | Detect HA offline/online cycle before acting on `homeassistant/status`. When enabled (default), requires HA to go offline first. When disabled, any `online` message triggers the cycle. Since ADR-073 the online event no longer republishes discovery configs; this setting is retained for compatibility. |
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
