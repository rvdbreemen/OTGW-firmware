# ADR-073: SAT MQTT Topic Structure

**Status:** Accepted
**Date:** 2026-04-09

## Context

SAT publishes a large number of telemetry values and accepts commands via MQTT. The topic
structure must integrate with the existing OTGW firmware MQTT conventions (ADR-006,
ADR-040) and must work with Home Assistant auto-discovery (ADR-041, `mqttha.cfg`).

The existing firmware topic pattern is:

```
<mqttPrefix>/value/<nodeId>/<key>         — state/telemetry (published by firmware)
<mqttPrefix>/set/<nodeId>/<key>           — command (subscribed by firmware)
```

Where `<mqttPrefix>` defaults to `OTGW` and `<nodeId>` is the chip ID. SAT publishes
approximately 50 distinct values and subscribes to approximately 12 command topics.

### Design constraints

- **Flat keys vs. nested JSON**: The existing firmware publishes each OT value as a
  separate topic (`value/<id>/boiler_temp`, `value/<id>/ch_mode`, etc.), not as a single
  JSON blob. This allows Home Assistant to subscribe only to the values it needs,
  avoids parsing JSON in HA automations, and matches the existing `mqttha.cfg` template
  structure.
- **HA auto-discovery**: Each SAT metric must be expressible as an HA `sensor` or `binary_sensor`
  entity with a single `stat_t` pointing to one topic. A single-blob JSON approach would
  require HA `value_template` on every entity — verbose and error-prone.
- **SAT namespace isolation**: SAT topics must not collide with existing OT-value topics
  (e.g., `boiler_temp` already exists for the raw OT boiler temperature; SAT's computed
  setpoint is a different value). A `sat/` prefix isolates all SAT topics cleanly.
- **Retained vs. non-retained**: Settings and slow-changing state should be retained
  (survive broker restart). Fast-changing telemetry (PID P/I/D per cycle) should not be
  retained to avoid stale values in HA after a firmware restart.
- **Command topics**: SAT accepts commands on the standard `set/<nodeId>/sat/<sub-command>`
  path, consistent with all other firmware command topics.

### Alternatives considered

1. **Single JSON blob for all SAT state** (`value/<id>/sat` = `{"setpoint":45.2,"pid_p":1.1,...}`):
   Rejected. HA `sensor` entities require a single scalar `stat_t` or a `value_template`
   JSON path. With 50 values, every HA entity definition would need a `value_template`,
   making `mqttha.cfg` significantly more complex. The flat-key approach also reduces
   MQTT payload size per update (only changed values are republished per cycle).

2. **Nested sub-namespaces** (`sat/pid/p`, `sat/pid/i`, `sat/pid/d`, `sat/curve/value`, etc.):
   Rejected. While semantically cleaner, the existing firmware uses flat keys for all OT
   values. Introducing nested namespaces for SAT only would be inconsistent. The `sat/`
   prefix already provides sufficient namespace isolation. Sub-grouping within `sat/` (e.g.,
   `sat/weather/temperature`) is used where it clearly belongs to a subsystem (weather,
   BLE), but not as a general hierarchical scheme.

3. **Separate command topic prefix** (`sat/cmd/<sub-command>` instead of `set/<id>/sat/<sub-command>`):
   Rejected. The existing `set/<nodeId>/` prefix enforces per-device addressing, which is
   essential when multiple OTGW devices are on the same broker. Departing from this for SAT
   commands would require per-device filtering in HA automations.

4. **MQTT 5 user properties or response topics**: Not applicable — the firmware targets
   MQTT 3.1.1 brokers (Mosquitto, Home Assistant Mosquitto add-on).

## Decision

SAT MQTT topics follow the existing firmware flat-key convention with a `sat/` prefix:

### State topics (published by firmware)

All state topics are published under `<mqttPrefix>/value/<nodeId>/sat/<key>`.

**Core control state** (retained where noted):

| Sub-key | Type | Retained | Description |
|---------|------|----------|-------------|
| `mode` | string | yes | `off`, `heating`, `pwm` |
| `setpoint` | float | yes | Final boiler flow setpoint sent (°C) |
| `heating_curve` | float | yes | Heating curve output before PID trim (°C) |
| `pid_output` | float | no | PID correction term (°C) |
| `target` | float | yes | Target indoor room temperature (°C) |
| `error` | float | no | Room temp error (target - actual, °C) |
| `pid_p` | float | no | PID proportional component |
| `pid_i` | float | no | PID integral component |
| `pid_d` | float | no | PID derivative component |
| `kp`, `ki`, `kd` | float | no | Current PID gains |
| `raw_derivative` | float | no | Unfiltered derivative before smoothing |
| `active` | bool | yes | SAT control loop actively issuing setpoints |
| `safety_tripped` | bool | no | Safety system engaged (manual reset needed) |

**Boiler and cycle diagnostics**:

| Sub-key | Retained | Description |
|---------|----------|-------------|
| `boiler_status` | no | Enum: `off`, `heating`, `at_setpoint`, `cooling`, ... |
| `cycle_class` | no | Enum: `good`, `overshoot`, `underheat`, `short`, `uncertain` |
| `pwm_duty` | no | PWM on-fraction (0.0-1.0) |
| `duty_ratio` | no | EMA of flame-on fraction |
| `overshoot_fraction` | no | EMA of cycles with overshoot |
| `cycle_phase` | no | Current PWM phase name |
| `overshoot_margin` | yes | Configured overshoot margin (°C) |

**Sensor inputs**:

| Sub-key | Retained | Description |
|---------|----------|-------------|
| `room_temp` | no | Current indoor temperature used by control loop (°C) |
| `outside_temp` | no | Current outdoor temperature used by control loop (°C) |
| `manufacturer` | yes | Detected or configured boiler manufacturer |

**Weather sub-namespace**:

| Sub-key | Retained | Description |
|---------|----------|-------------|
| `weather/temperature` | no | Open-Meteo current outdoor temperature |
| `weather/humidity` | no | Open-Meteo current humidity |
| `weather/wind_speed` | no | Open-Meteo current wind speed |

**BLE sub-namespace** (OTGW32 only, `#if HAS_BLE`):

| Sub-key | Retained | Description |
|---------|----------|-------------|
| `ble_temp` | no | BLE sensor temperature (°C) |
| `ble_humidity` | no | BLE sensor humidity (%) |
| `ble_sensor_rssi` | no | BLE sensor signal strength |
| `ble_battery` | no | BLE sensor battery level |
| `ble_sensor_count` | no | Number of active BLE sensors |
| `ble_temp_valid` | no | BLE temperature is fresh and valid |

**Settings reflected as retained state** (published on change):

| Sub-key | Description |
|---------|-------------|
| `simulation` | Simulation mode active (`ON`/`OFF`) |
| `auto_tune` | Auto-tune mode active (`ON`/`OFF`) |
| `heating_mode` | `comfort` or `eco` |
| `cycles_per_hour` | Configured PWM cycles per hour |
| `sensor_max_age` | Maximum acceptable sensor age (seconds) |
| `error_monitoring` | Error monitoring enabled |

Additional diagnostic and health topics: `flame_status`, `flame_health`, `valves_open`,
`window_open`, `pressure`, `pressure_alarm`, `pressure_health`, `modulation_reliable`,
`setpoint_mismatch`, `thermal_coeff`, `thermal_model_valid`, `power`, `energy_total`,
`solar_gain`, `summer_active`, `humidity`, `comfort_offset`, `curve_recommendation`,
`auto_gains_value`, `valve_offset`, and attribute/JSON supplementary topics (`*_attributes`).

### Command topics (subscribed by firmware)

All command topics are subscribed under `<mqttPrefix>/set/<nodeId>/sat/<sub-command>`.

| Sub-command | Payload | Effect |
|-------------|---------|--------|
| `target` | float string (e.g., `"21.0"`) | Set target indoor temperature |
| `enabled` | `"1"` / `"0"` or `"true"` / `"false"` | Enable or disable SAT |
| `dhw_enabled` | `"1"` / `"0"` | Enable/disable DHW control |
| `ovp_enabled` | `"1"` / `"0"` | Enable/disable overflow protection |
| `ovp_start` | (any) | Start OPV calibration |
| `ovp_stop` | (any) | Stop OPV calibration |
| `target_temp_step` | float | Set target temperature step size |

### HA auto-discovery

SAT entities are registered via `mqttha.cfg` using the JIT discovery pattern (ADR-041).
Each entity uses a single `stat_t` pointing to the flat `sat/<key>` topic. Climate entity
`cmd_t` points to `set/<nodeId>/sat/target`. No `value_template` JSON extraction is needed.

### Retained policy rationale

Topics are retained when they represent configuration, accumulated state, or values that
HA would otherwise show as unavailable after a firmware restart (e.g., `sat/mode`,
`sat/setpoint`, `sat/active`). Fast-cycling diagnostics (`sat/pid_p`, `sat/boiler_status`,
`sat/room_temp`) are not retained to avoid stale HA dashboard values after a SAT disable/enable
cycle.

## Consequences

### Benefits

- No `value_template` needed in HA: each entity subscribes to exactly one scalar topic
- Consistent with existing OT-value topic structure — no special-casing in HA configs
- `sat/` prefix makes all SAT topics trivially identifiable in MQTT broker logs
- `weather/` and `ble/` sub-namespaces group subsystem topics without breaking the flat-key
  convention for core control values
- Command topics reuse the existing `set/<nodeId>/` pattern, so multi-device HA setups
  continue to work without modification

### Trade-offs

- Approximately 50 individual MQTT publishes per control cycle rather than one JSON blob.
  At 30-second intervals this is negligible for the MQTT broker. At the minimum 30-second
  control interval and broker defaults, SAT publishes roughly 1-2 messages per second on
  average, well within Mosquitto capacity.
- New SAT topics require new `mqttha.cfg` entries for HA auto-discovery. This is manageable
  because the config file is templated and generated per-device.

### Risks

- The number of SAT topics (approximately 50) makes the `mqttha.cfg` SAT section long.
  Adding new metrics requires adding both a publish call in `SATcontrol.ino` and an entry
  in `mqttha.cfg`. The risk is that one is added without the other — mitigated by treating
  each new metric addition as a two-file change.

## Related

- ADR-006: MQTT Integration Pattern (existing topic convention)
- ADR-040: MQTT Source-Specific Topics (per-source topic approach)
- ADR-041: JIT HA Discovery (auto-discovery via `mqttha.cfg`)
- ADR-085: SAT Integration (overall SAT architecture and MQTT overview)
- `SATcontrol.ino`: `satPublishMQTT()` (~line 1265)
- `SATweather.ino`: `weatherPublishMQTT()`
- `SATble.ino`: `blePublishMQTT()`
- `MQTTstuff.ino`: SAT command handler (~line 692), `set/<nodeId>/sat/<sub-command>`
- `data/mqttha.cfg`: HA auto-discovery entries for SAT entities
