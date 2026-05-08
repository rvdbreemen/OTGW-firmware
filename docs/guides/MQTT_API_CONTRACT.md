# MQTT API Contract Reference
## OT-Thing-OTGW32 vs SAT thermo-nova vs OTGW-firmware 2.0.0

**Document date:** 2026-05-08  
**Scope:** Full MQTT topic/payload contract mapping for all three projects, with compatibility analysis.

---

## How to read this document

Three projects interact over MQTT. Understanding which "dialect" each speaks is essential for integration:

| Project | Role | MQTT dialect |
|---------|------|-------------|
| **OT-Thing-OTGW32** (Seegel Systeme) | Firmware / gateway | Publisher of nested JSON `/state`; subscriber to `/+/set` commands |
| **SAT thermo-nova** (`MQTT_OTTHING` mode) | HA coordinator / consumer | Subscriber to `/state`; publisher of individual command topics |
| **SAT thermo-nova** (`MQTT_OPENTHERM` mode) | HA coordinator / consumer | Subscriber to flat scalar topics; publisher of PIC-command strings |
| **OTGW-firmware 2.0.0** | Firmware / gateway | Publisher of flat scalar topics + HA auto-discovery; speaks `MQTT_OPENTHERM` dialect |

**Key design decision (ADR-101):** OTGW-firmware 2.0.0 uses **flat per-value topics only**. It does not publish nested JSON state blobs. SAT users must use `MQTT_OPENTHERM` coordinator mode, not `MQTT_OTTHING`.

---

## Part 1: OT-Thing-OTGW32 MQTT Contract

### 1.1 Base topic

OT-Thing auto-generates its base topic from the device MAC address:

```
baseTopic = "otthing/" + last_6_chars_of_MAC_uppercase
```

**Example:** MAC `AA:BB:CC:DD:EE:FF` → base topic `otthing/DDEEFF`

This is hardcoded in `Firmware/src/mqtt.cpp:72-74`. Users cannot configure a custom prefix without editing firmware source.

### 1.2 Availability / LWT

| Topic | Payload | QoS | Retained | Direction |
|-------|---------|-----|----------|-----------|
| `{baseTopic}/status` | `"online"` | 1 | yes | firmware → broker |
| `{baseTopic}/status` | `"offline"` | 1 | yes | broker LWT (on disconnect) |

Source: `mqtt.cpp:80`

### 1.3 State publish — the `/state` topic

**Topic:** `{baseTopic}/state`  
**Interval:** Every 5 seconds  
**Payload:** JSON document — the single authoritative state blob  
**Source:** `mqtt.cpp:159`, `devstatus.cpp:34-95`, `otcontrol.cpp:1034-1104`

#### Full JSON schema

```json
{
  "runtime": <int — uptime in seconds>,
  "freeHeap": <int — bytes>,
  "largestBlock": <int — bytes>,
  "resetInfo": <int>,
  "fw_version": "<string>",
  "USB_connected": <bool>,
  "reset_reason0": <int>,
  "numWifiDisc": <int>,
  "new_fw": "<version string>" | absent,
  "dateTime": "<dd.mm.yyyy HH:MM:SS>" | absent,

  "wifi": {
    "status": <int>,
    "mode": <int>,
    "ipsta": "<ip address>",
    "mac": "<mac address>",
    "hostname": "<string>",
    "sta_ssid": "<string>",
    "rssi": <int — dBm>
  },

  "mqtt": {
    "connected": <bool>,
    "basetopic": "<string>",
    "numDisc": <int — reconnect count>
  },

  "slave": {
    "connected": <bool>,
    "txCount": <int>,
    "rxCount": <int>,
    "timeouts": <int> | absent,
    "flameRatio": <int — 0..100 percent>,
    "flameFreq": <float>,
    "<OT_field_name>": <value>,
    ...
    "status": {
      "flame": <bool>,
      "ch_mode": <bool>,
      "dhw_mode": <bool>,
      "fault": <bool>,
      "cooling": <bool>,
      "ch2_active": <bool>,
      "diag_ind": <bool>
    }
  },

  "thermostat": {
    "txCount": <int>,
    "rxCount": <int>,
    "invalidCount": <int>,
    "smartPower": "low" | "medium" | "high",
    "<OT_field_name>": <value>,
    ...
    "status": {
      "ch_enable": <bool>,
      "dhw_enable": <bool>,
      "cooling_enable": <bool>,
      "otc_active": <bool>,
      "ch2_enable": <bool>
    }
  },

  "heatercircuit": [
    {
      "roomsetpoint": <float — °C>,
      "roomTempFilt": <float — °C>,
      "roomtemp": <float — °C>,
      "ovrdFlow": <bool>,
      "mode": <int>,
      "integState": <float>,
      "suspended": <bool> | absent
    }
  ],

  "outsideTemp": <float — °C> | absent,
  "owResult": "<string>" | absent,
  "1wire": { "<sensor_id>": <float — °C>, ... },
  "BLE": { "<sensor_name>": { "temp": <float>, "hum": <float>, ... }, ... }
}
```

**OpenTherm field names in `slave` and `thermostat`:** These are the raw OpenTherm message ID aliases as defined by OT-Thing's `OTValue` registry, e.g.:
- `Tboiler` (flow temperature), `Tret` (return temperature)
- `rel_mod` (relative modulation level %)
- `TSet` (CH setpoint), `TdhwSet` (DHW setpoint)
- `MaxTSet`, `MaxRelModLevelSetting`, `MaxCapacityMinModLevel_hb_u8`, `MaxCapacityMinModLevel_lb_u8`
- `flow_t`, `return_t`, `ch_set_t`, `dhw_set_t`, `max_capacity`, `min_modulation`

### 1.4 HA auto-discovery

OT-Thing publishes HA auto-discovery config messages on `homeassistant/` topics. All config messages point their `state_topic` at `{baseTopic}/state` and use `value_template` expressions to extract the relevant field from the JSON blob.

Source: `mqtt.cpp:143`, `otcontrol.cpp:1115+`

### 1.5 Command subscribe contract

**Subscribe pattern:** `{baseTopic}/+/set`  
**Source:** `mqtt.cpp:82-83`

OT-Thing accepts commands on `{baseTopic}/{commandName}/set`. All payloads are plain strings (numbers as ASCII, modes as named strings).

| Command topic | Payload format | Effect |
|--------------|---------------|--------|
| `{base}/outsideTemp/set` | `"<float>"` — °C | Sets outside temperature compensation |
| `{base}/dwhSetTemp/set` | `"<float>"` — °C | Sets DHW setpoint |
| `{base}/chSetTemp1/set` | `"<float>"` — °C | Sets CH circuit 1 setpoint |
| `{base}/chSetTemp2/set` | `"<float>"` — °C | Sets CH circuit 2 setpoint |
| `{base}/dhwMode/set` | `"heat"` / `"auto"` / `"off"` | Sets DHW control mode |
| `{base}/chMode1/set` | `"heat"` / `"auto"` / `"off"` | Sets CH circuit 1 mode |
| `{base}/chMode2/set` | `"heat"` / `"auto"` / `"off"` | Sets CH circuit 2 mode |
| `{base}/roomTemp1/set` | `"<float>"` — °C | Injects measured room temperature |
| `{base}/roomTemp2/set` | `"<float>"` — °C | Injects circuit 2 room temperature |
| `{base}/roomSetpoint1/set` | `"<float>"` — °C | Sets room setpoint for circuit 1 |
| `{base}/roomSetpoint2/set` | `"<float>"` — °C | Sets room setpoint for circuit 2 |
| `{base}/roomComp1/set` | `"heat"` / `"auto"` / `"off"` | Sets room compensation mode for circuit 1 |
| `{base}/roomComp2/set` | `"heat"` / `"auto"` / `"off"` | Sets room compensation mode for circuit 2 |
| `{base}/overrideCh1/set` | `"ON"` / `"OFF"` | Force CH circuit 1 on/off |
| `{base}/overrideCh2/set` | `"ON"` / `"OFF"` | Force CH circuit 2 on/off |
| `{base}/overrideDhw/set` | `"ON"` / `"OFF"` | Force DHW on/off |
| `{base}/ventSetpoint/set` | `"<int>"` — 0..100 % | Sets ventilation setpoint |
| `{base}/ventEnable/set` | `"ON"` / `"OFF"` | Enables/disables ventilation |
| `{base}/maxModulation/set` | `"<int>"` — 0..100 % | Sets maximum modulation limit |
| `{base}/openBypass/set` | (not implemented) | Stub — no handler |
| `{base}/autoBypass/set` | (not implemented) | Stub — no handler |
| `{base}/freeVentEnable/set` | (not implemented) | Stub — no handler |

Source: `mqtt.cpp:13-36` (topic name definitions), `mqtt.cpp:207-298` (handlers)

---

## Part 2: SAT thermo-nova MQTT Coordinator Modes

SAT supports 4 MQTT coordinator modes, each implementing a different gateway dialect:

| Mode enum | Class | Gateway target |
|-----------|-------|---------------|
| `MQTT_OTTHING` | `SatOtthingMqttCoordinator` | OT-Thing firmware |
| `MQTT_OPENTHERM` | `SatOpenThermMqttCoordinator` | OTGW (PIC), OTGW-firmware, OTGW-firmware 2.0.0 |
| `MQTT_EMS` | `SatEmsMqttCoordinator` | EMS-ESP / Buderus/Bosch EMS bus |
| `FAKE` | (coordinator.py) | Simulator — no MQTT |

Source: `coordinator/entry_data.py:22-30`

### 2.1 Mode: MQTT_OTTHING

**Subscribe:** `{mqtt_topic}/state`  
**Source:** `coordinator/mqtt/otthing.py:184`

SAT expects the following JSON structure in the payload (exact schema from test suite `tests/test_mqtt_otthing_coordinator.py:32-53`):

```json
{
  "slave": {
    "status": {
      "flame": <bool>,
      "ch_mode": <bool>,
      "dhw_mode": <bool>
    },
    "flow_t": <float — boiler flow temperature °C>,
    "return_t": <float — return temperature °C>,
    "rel_mod": <float — relative modulation 0..100 %>,
    "max_capacity": <float — max boiler capacity kW>,
    "min_modulation": <float — minimum modulation %>,
    "maxModulation": <float — maximum modulation limit %>,
    "ch_set_t": <float — CH setpoint °C>,
    "dhw_set_t": <float — DHW setpoint °C>,
    "memberId": <int — OpenTherm member ID>,
    "dhwMin": <float — min DHW setpoint °C>,
    "dhwMax": <float — max DHW setpoint °C>
  },
  "thermostat": {
    "status": {
      "ch_enable": <bool>,
      "dhw_enable": <bool>
    },
    "ch_set_t": <float — thermostat CH demand setpoint>,
    "dhw_set_t": <float — thermostat DHW demand setpoint>
  }
}
```

**Data keys extracted** (`otthing.py:19-39`):

| SAT internal constant | JSON path | Meaning |
|----------------------|-----------|---------|
| `DATA_BOILER_TEMPERATURE` | `slave.flow_t` | Boiler flow temperature |
| `DATA_RETURN_TEMPERATURE` | `slave.return_t` | Return temperature |
| `DATA_FLAME_ACTIVE` | `slave.status.flame` | Flame active |
| `DATA_CENTRAL_HEATING` | `slave.status.ch_mode` | CH active (slave) |
| `DATA_DHW_ENABLE` | `slave.status.dhw_mode` | DHW active |
| `DATA_CONTROL_SETPOINT` | `thermostat.ch_set_t` | Thermostat's demanded CH setpoint |
| `DATA_DHW_SETPOINT` | `thermostat.dhw_set_t` | Thermostat's demanded DHW setpoint |
| `DATA_REL_MOD_LEVEL` | `slave.rel_mod` | Current relative modulation |
| `DATA_BOILER_CAPACITY` | `slave.max_capacity` | Max boiler capacity |
| `DATA_REL_MIN_MOD_LEVEL` | `slave.maxModulation` | Max modulation setting (note: variable name is misleading — see §4.1) |
| `DATA_MAX_REL_MOD_LEVEL_SETTING` | `slave.min_modulation` | Min modulation setting (note: variable name is misleading — see §4.1) |
| `DATA_DHW_SETPOINT_MINIMUM` | `slave.dhwMin` | Min DHW setpoint limit |
| `DATA_DHW_SETPOINT_MAXIMUM` | `slave.dhwMax` | Max DHW setpoint limit |

**Publish commands** (`otthing.py:160-174`):

| SAT action | Topic | Payload |
|-----------|-------|---------|
| Set CH setpoint | `{mqtt_topic}/chSetTemp1` | `"<float:.1f>"` °C |
| Set DHW setpoint | `{mqtt_topic}/dwhSetTemp` | `"<float:.1f>"` °C |
| Enable CH | `{mqtt_topic}/chMode1` | `"heat"` |
| Disable CH | `{mqtt_topic}/chMode1` | `"off"` |

Note: SAT does **not** use the `/set` suffix when publishing commands in `MQTT_OTTHING` mode. The topic is `{base}/chMode1`, not `{base}/chMode1/set`.

> **Important:** OT-Thing subscribes to `{baseTopic}/+/set` (with `/set` suffix). SAT `MQTT_OTTHING` publishes to `{base}/{command}` (without `/set`). This is a **compatibility gap** — OT-Thing would not receive SAT's command messages without a MQTT topic bridge or firmware patch.

Source: `coordinator/mqtt/__init__.py:134-149` (publish helper, QoS 1)

### 2.2 Mode: MQTT_OPENTHERM

This is the mode compatible with OTGW-firmware 2.0.0 (and the classic ESP8266 OTGW-firmware).

**Subscribe pattern:** `{mqtt_topic}/value/{device_id}/{key}`  
**Source:** `coordinator/mqtt/opentherm.py:229-230`

Subscribed keys (flat scalar values):

| Topic key | Meaning | Type |
|-----------|---------|------|
| `flame` | Flame active | `"ON"` / `"OFF"` |
| `TdhwSet` | DHW setpoint | float string |
| `TSet` | CH setpoint | float string |
| `MaxTSet` | Max CH setpoint | float string |
| `RelModLevel` | Relative modulation % | float string |
| `Tboiler` | Boiler flow temperature | float string |
| `Tret` | Return temperature | float string |
| `domestichotwater` | DHW active | `"ON"` / `"OFF"` |
| `centralheating` | CH active | `"ON"` / `"OFF"` |
| `CHPressure` | CH circuit pressure | float string |
| `slave_memberid_code` | OpenTherm member ID | integer string |
| `MaxCapacityMinModLevel_hb_u8` | Max capacity (high byte) | integer string |
| `MaxCapacityMinModLevel_lb_u8` | Min modulation (low byte) | integer string |
| `MaxRelModLevelSetting` | Max relative modulation setting | float string |
| `TdhwSetUBTdhwSetLB_value_lb` | DHW setpoint lower bound | float string |
| `TdhwSetUBTdhwSetLB_value_hb` | DHW setpoint upper bound | float string |

Source: `coordinator/mqtt/opentherm.py:174-193`

**Publish commands** — PIC-protocol command strings:  
**Topic:** `{mqtt_topic}/set/{device_id}/command`  
**Source:** `opentherm.py:232-233`

| SAT action | Command payload | Notes |
|-----------|----------------|-------|
| Set CH setpoint | `CS={value}` + separate `PM=25` | Sets control setpoint + mode |
| Set DHW setpoint | `SW={value}` | Set DHW temperature |
| Set thermostat setpoint | `TC={value}` | Remote thermostat override |
| Enable/disable CH | `CH=1` / `CH=0` | Force CH mode |
| Set max relative modulation | `MM={value}` (most boilers) | Or `TP=11:12={value}` for Immergas |
| Set max setpoint | `SH={value}` | Sets maximum CH setpoint |

**Boot sequence** sent once on connect (`opentherm.py:166-172`):
- `PM=3`, `PM=15`, `PM=48` — request periodic OpenTherm data frames
- `MI=500` — for Ideal, Intergas, Nefit boilers only

### 2.3 Mode: MQTT_EMS

Targets EMS-bus gateways (Buderus/Bosch). Not relevant to OTGW32 hardware.

**Subscribe:** `{mqtt_topic}/boiler_data` and individual sensor keys  
**Publish:** `{mqtt_topic}/boiler` with JSON `{"cmd": "...", "value": ...}` payloads  
**Source:** `coordinator/mqtt/ems.py`

---

## Part 3: OTGW-firmware 2.0.0 MQTT Contract

### 3.1 Topic structure

OTGW-firmware 2.0.0 uses flat per-value topics (ADR-101). Each sensor or control value has its own dedicated topic carrying a plain scalar payload.

**Base topic:** Configurable (default: `OTGW`). Set via settings UI → MQTT → Topic prefix.

**Topic format:** `{base}/{source}/{sensorId}` or `{base}/{sensorId}`

Examples:
```
OTGW/boilertemp        → "64.50"
OTGW/flame             → "1"
OTGW/thermostat/TSet   → "20.00"
OTGW/boiler/Tboiler    → "64.50"
```

**Payload:** Always a plain scalar string — float (via `dtostrf`), integer (via `snprintf_P`), or named constant (`"ON"`, `"OFF"`, `"true"`, `"false"`, `"online"`, `"offline"`). Never JSON on value topics.

### 3.2 HA auto-discovery

OTGW-firmware 2.0.0 publishes rich HA auto-discovery payloads on `homeassistant/` topics separately from value topics:

- `homeassistant/sensor/{nodeId}/{sensorId}/config` — JSON, metadata only
- `homeassistant/binary_sensor/{nodeId}/{sensorId}/config` — JSON, metadata only
- `homeassistant/switch/{nodeId}/{sensorId}/config` — JSON, metadata only
- etc.

Each config payload points `state_topic` at the corresponding flat value topic. The value topic itself carries only the scalar.

Source: `MQTTHaDiscovery.cpp` (streaming discovery publisher), ADR-077, ADR-101

### 3.3 SAT `json_attributes_topic` topics (exceptions to ADR-101)

A small number of topics in `SATcontrol.ino` carry JSON payloads. These are `json_attributes_topic` entries — not value topics — used to expose PID internals as HA entity attributes. They are legitimate ADR-101 exceptions because:

1. They are registered in the HA discovery system as `json_attributes_topic` (not `state_topic`)
2. HA treats them as supplementary attribute blobs, not sensor state

Wired exceptions (discovery registered):
- `sat/pid_attributes` — PID error, P, I, D components (`SATcontrol.ino:1887-1890`)
- `sat/cycle_attributes` — Cycle classification metadata (`SATcontrol.ino:1917-1926`)

Unwired exceptions (see §4 — need attention):
- `sat/curve_recommendation_attributes` — `SATcontrol.ino:2083-2090`
- `sat/climate_attributes` — `SATcontrol.ino:2451-2526`
- `sat/pressure_health_attr` — `SATcontrol.ino:2042-2062`

### 3.4 Command subscribe contract

OTGW-firmware 2.0.0 subscribes to command topics for PIC-protocol pass-through and SAT control:

| Topic pattern | Payload | Effect |
|--------------|---------|--------|
| `{base}/set/{deviceId}/command` | `CS=<float>`, `SW=<float>`, `TC=<float>`, `MM=<int>`, etc. | PIC/OTDirect command forwarding (SAT `MQTT_OPENTHERM` mode) |
| `{base}/set/outsideTemp` | float string °C | Sets outside temperature |
| `{base}/set/chSetTemp` | float string °C | Sets CH setpoint |
| `{base}/set/dhwSetTemp` | float string °C | Sets DHW setpoint |
| `{base}/set/mode` | `"heating"` / `"off"` / ... | Sets operating mode |

Source: `MQTTstuff.ino` (MQTT subscribe handler)

---

## Part 4: Compatibility Matrix

### 4.1 SAT mode selection guide

| SAT mode | Compatible gateway | Works with OTGW-firmware 2.0.0? | Works with OT-Thing? |
|----------|-------------------|--------------------------------|---------------------|
| `MQTT_OTTHING` | OT-Thing firmware | **No** — 2.0.0 does not publish nested JSON state | **Yes*** (see gap below) |
| `MQTT_OPENTHERM` | OTGW, OTGW-firmware, OTGW-firmware 2.0.0 | **Yes** — full dialect match | **No** — OT-Thing does not speak flat scalar topics |
| `MQTT_EMS` | EMS-bus gateways | No | No |

*SAT `MQTT_OTTHING` vs OT-Thing topic suffix gap: SAT publishes commands to `{base}/chMode1` but OT-Thing subscribes to `{base}/chMode1/set`. The state subscription (`{base}/state`) matches. Status reads work; commands don't reach OT-Thing without a bridge or patch.

### 4.2 Payload schema match: SAT MQTT_OTTHING vs OT-Thing `/state`

| SAT expects | OT-Thing publishes | Match |
|------------|-------------------|-------|
| `slave.flow_t` | `slave.flow_t` or `slave.Tboiler` (alias) | Partial — depends on OT-Thing firmware version |
| `slave.return_t` | `slave.return_t` or `slave.Tret` | Partial |
| `slave.status.flame` | `slave.status.flame` | Match |
| `slave.status.ch_mode` | `slave.status.ch_mode` | Match |
| `slave.rel_mod` | `slave.rel_mod` | Match |
| `thermostat.status.ch_enable` | `thermostat.status.ch_enable` | Match |
| `thermostat.ch_set_t` | `thermostat.ch_set_t` | Match |
| `slave.maxModulation` | Field name varies | Partial |

SAT uses `dict.get()` with `None` fallbacks throughout, so missing fields degrade gracefully to `None` rather than raising exceptions.

### 4.3 Naming anomaly in SAT `MQTT_OTTHING` coordinator

The constants in `otthing.py:30-31` have their names swapped relative to their semantic meaning:

```python
DATA_REL_MIN_MOD_LEVEL = "maxModulation"  # reads maxModulation → used as minimum
DATA_MAX_REL_MOD_LEVEL_SETTING = "min_modulation"  # reads min_modulation → used as maximum
```

The logic is internally self-consistent (tests pass) but the constant names are backwards. This is a bug in variable naming, not in behavior. OT-Thing compatibility is unaffected.

---

## Part 5: Integration How-To

### Using SAT with OTGW-firmware 2.0.0 (recommended)

SAT must be configured in **OpenThermGateway-MQTT mode** (`MQTT_OPENTHERM`):

1. Ensure OTGW-firmware 2.0.0 MQTT is connected to your broker
2. In SAT HA configuration:
   - **Gateway type:** OpenThermGateway (via MQTT)
   - **MQTT topic:** `OTGW` (or your configured base topic)
   - **Device ID:** your node ID (e.g., `otgw32-abc123`)
3. SAT will subscribe to `OTGW/value/{deviceId}/{key}` for each sensor key
4. SAT will publish commands to `OTGW/set/{deviceId}/command`
5. OTGW-firmware 2.0.0 handles all 385+ sensors, 58 binary sensors, and SAT control via this flat topic tree

### Using SAT with OT-Thing-OTGW32

SAT must be configured in **OT-Thing mode** (`MQTT_OTTHING`):

1. Find the OT-Thing base topic: `otthing/{last-6-MAC}` (check OT-Thing web portal)
2. In SAT HA configuration:
   - **Gateway type:** OTthing (via MQTT)
   - **MQTT topic:** `otthing/DDEEFF` (replace with your device's suffix)
3. SAT subscribes to `otthing/DDEEFF/state` for all state updates
4. SAT publishes commands to `otthing/DDEEFF/chMode1`, `/chSetTemp1`, `/dwhSetTemp`
5. **Known gap:** OT-Thing subscribes to `/chMode1/set` (with `/set` suffix); SAT publishes to `/chMode1` (without suffix). Command delivery may fail depending on OT-Thing firmware version.

### Migrating from OT-Thing to OTGW-firmware 2.0.0

See `docs/guides/MIGRATION_FROM_OTTHING.md` for the full migration procedure.

The critical step: change SAT coordinator mode from `MQTT_OTTHING` to `MQTT_OPENTHERM`. The topic tree and payload schema are completely different between the two modes.

---

## Part 6: Known issues and open tasks

| Issue | Severity | Status |
|-------|---------|--------|
| `sat/curve_recommendation_attributes` published but no HA discovery entry | Medium | Open — orphaned MQTT traffic |
| `sat/climate_attributes` 512-byte static buffer, no HA discovery entry | High | Open — static RAM consumption + orphaned traffic |
| `sat/pressure_health_attr` published but no HA discovery entry | Medium | Open — orphaned MQTT traffic |
| SAT `MQTT_OTTHING` command topic suffix mismatch vs OT-Thing `/set` convention | Medium | Not a 2.0.0 concern (different dialect) |
| SAT `DATA_REL_MIN_MOD_LEVEL` / `DATA_MAX_REL_MOD_LEVEL_SETTING` name swap | Low | SAT-internal bug; behavior correct |

---

## References

- ADR-101: `docs/adr/ADR-101-flat-per-value-mqtt-topics-over-aggregated-json-payloads.md` — binding decision: OTGW-firmware uses flat topics only
- ADR-077: Streaming MQTT HA Discovery Architecture
- ADR-096: MQTT Source-Subtopic Worldview Semantics
- OT-Thing firmware source: `other-projects/OT-Thing-OTGW32/Firmware/src/mqtt.cpp`
- SAT coordinator source: `other-projects/SAT-releases-thermo-nova/custom_components/sat/coordinator/mqtt/`
- OT-Thing feature parity: `docs/guides/OT_THING_FEATURE_PARITY.md`
- OTGW-firmware MQTT publish: `src/OTGW-firmware/MQTTstuff.ino`
- OTGW-firmware HA discovery: `src/OTGW-firmware/MQTTHaDiscovery.cpp`
