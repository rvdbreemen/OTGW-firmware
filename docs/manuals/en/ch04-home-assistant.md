## Chapter 4: Home Assistant Integration

### Overview

OTGW-firmware is designed for tight Home Assistant integration. Once you have an MQTT broker running and the firmware connected to it, Home Assistant automatically discovers all OpenTherm sensors, the boiler climate entity, binary status sensors, and (when SAT is enabled) a set of SAT switches, a heating-system select, and a number entity, with no YAML required. This chapter covers how that works, what gets discovered, and how to send commands from Home Assistant to the gateway.

---

### 4.1 Prerequisites

Before Home Assistant can see the gateway, you need:

- A running MQTT broker (Mosquitto is the most common choice, covered below).
- The MQTT integration enabled in Home Assistant (Settings > Devices and Services > MQTT).
- The MQTT integration configured to use the same broker the firmware connects to.
- MQTT discovery enabled in Home Assistant (it is on by default).

The firmware does not require any YAML in Home Assistant when auto-discovery is used.

---

### 4.2 MQTT Broker Setup

#### Using the Mosquitto Add-on (Recommended)

The easiest path is the official Mosquitto broker add-on for Home Assistant OS:

1. In Home Assistant, go to Settings > Add-ons > Add-on Store.
2. Find "Mosquitto broker" and install it.
3. After installation, configure a username and password in the add-on configuration.
4. Start the add-on and enable "Start on boot."

The Mosquitto add-on automatically registers a broker connection with the HA MQTT integration.

#### Using an External Broker

If you already run Mosquitto (or another MQTT broker) separately:

1. Ensure the broker is reachable from both the ESP device and the Home Assistant host.
2. In Home Assistant, go to Settings > Devices and Services > Add Integration > MQTT.
3. Enter the broker address, port (default 1883), and credentials.

#### Broker Configuration Tips

- Use a dedicated MQTT user for the gateway with a strong password.
- Enable persistence in Mosquitto (`persistence true`) so retained messages survive broker restarts.
- Default port is 1883 (plain TCP). The firmware uses plain MQTT only, no TLS.
- If you use a firewall, open port 1883 between the ESP device and the broker host.

A minimal Mosquitto configuration for a local install:

```
listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd
persistence true
persistence_location /var/lib/mosquitto/
```

---

### 4.3 Configuring the Firmware for MQTT

Open the OTGW-firmware web UI and go to the Settings page. The MQTT section has the following fields:

| Setting | Default | Description |
|---|---|---|
| MQTT Broker | (empty) | IP address or hostname of the broker |
| MQTT Port | 1883 | Broker port |
| MQTT User | (empty) | Username (leave empty if anonymous) |
| MQTT Password | (empty) | Password |
| Top Topic | `OTGW` | Root namespace for all published topics |
| Unique ID | `otgw-{MAC}` | Node identifier, included in every topic path |
| HA Discovery Prefix | `homeassistant` | Must match the discovery prefix in HA MQTT integration |
| Device Manufacturer | `NodoShop` | Shown in the HA device info block. Editable. |
| Device Model | `OTGW` | Shown in the HA device info block. Editable. |
| Separate Sources | off | When on, publishes per-source sibling topics (`<topic>_thermostat` and `<topic>_boiler`) alongside the canonical topic (ADR-097). The canonical entity is always kept. |
| Use Legacy OT Topics | off | When off (default), publishes the new self-describing binary_sensor labels. When on, publishes the legacy OT-spec labels instead. Mutually exclusive: never both at the same time. See section 4.4. |

The manufacturer and model are now configurable so the same firmware can be labelled correctly when it runs on different hardware variants (classic NodoShop OTGW, ESP32 OT-Shield, etc.). These values appear in Home Assistant under Settings > Devices and Services > MQTT > OTGW. As a small nod to the origin of the project, a separate `otgw-pic/designer` topic is always published with the value `Schelte Bron`, to credit the original PIC firmware author.

After saving, the firmware connects to the broker within a few seconds. The MQTT status indicator on the web UI main page turns green when the connection is established.

---

### 4.4 MQTT Topic Structure

All topics follow a two-namespace convention.

#### Published topics (gateway to broker)

```
{TopTopic}/value/{UniqueId}/{topic-name}
```

Example with defaults:

```
OTGW/value/otgw-AABBCCDDEEFF/boilertemperature
OTGW/value/otgw-AABBCCDDEEFF/flamestatus
OTGW/value/otgw-AABBCCDDEEFF/sat/setpoint
```

Published topics carry `retain = true` by default, so Home Assistant receives the last known value immediately on startup.

#### Command topics (HA to gateway)

```
{TopTopic}/set/{UniqueId}/{command-name}
```

Example:

```
OTGW/set/otgw-AABBCCDDEEFF/setpoint
OTGW/set/otgw-AABBCCDDEEFF/hotwater
OTGW/set/otgw-AABBCCDDEEFF/sat/target
```

#### Connection status

The firmware publishes a birth/last-will message at the root of the publish namespace:

```
OTGW/value/otgw-AABBCCDDEEFF   →  "online"  (retained, published on connect)
OTGW/value/otgw-AABBCCDDEEFF   →  "offline" (retained, last will on disconnect)
```

Since 2.0.0 this topic is owned exclusively by the MQTT birth/LWT mechanism. HA entity availability tracks the ESP-to-broker link only; an idle OpenTherm bus no longer makes all HA entities flap to `unavailable`. The OT-bus liveness is exposed separately under `otgw_connected` (ADR-102).

#### Topic-naming mode (new in 2.0.0, breaking)

Per ADR-106, 37 binary_sensor labels were renamed to self-describing names by default. Examples:

| Legacy label (pre-2.0.0) | New default label |
|---|---|
| `dhw_present` | `supports_hot_water` |
| `cooling_config` | `supports_cooling` |
| `fault` | `fault_indication` |
| `centralheating` | `central_heating` |
| `domestichotwater` | `hot_water` |
| `vh_bypass_position` | `ventilation_bypass_position` |
| `solar_storage_slave_fault_indicator` | `solar_storage_fault` |

The full mapping is in ADR-105 (Categories A, B, C). The new names match Home Assistant core's `opentherm_gw` integration where possible, so automations carry over without rewriting.

To restore the legacy labels, enable `Use Legacy OT Topics` in the MQTT settings (`MQTTuseLegacyOtTopics` on disk, `mqttuselegacyottopics` over REST). The two modes are mutually exclusive: the firmware never publishes both the new and the legacy label for the same bit. When you flip the toggle, the firmware drains the retained discovery configs from the other mode in the background so the broker stays tidy.

On a fresh upgrade from a pre-ADR-106 build, 37 stale retained legacy discovery configs may linger on your broker until you either (a) toggle `Use Legacy OT Topics` once and back, or (b) manually purge them with `mosquitto_pub -t '<haPrefix>/binary_sensor/<gw>/<legacy_label>/config' -n -r`. HA entities for the old names show as `unavailable` until cleaned up.

The 21 binary_sensor labels without an alias (`flame`, `cooling`, `low_water_pressure`, `air_pressure_fault`, `electric_production`, master-side enable bits, etc.) are unaffected and always publish under their existing name.

---

### 4.5 MQTT Publish Gate (Interval Filtering)

By default, the firmware publishes an OpenTherm value every time it is received from the bus. For busy installations this can mean dozens of messages per second. The publish interval setting lets you reduce this traffic.

#### How it works

The `MQTT Interval` setting (Settings page, field `iInterval`) controls when a value is published:

| `iInterval` value | Publish behavior |
|---|---|
| `0` (default) | Publish on every received message, no filtering |
| `> 0` (seconds) | Publish only when: the value is seen for the first time (`firstSeen`), the value has changed since the last publish (`valueChanged`), or the configured interval has elapsed since the last publish (`intervalElapsed`) |

When the interval is set to, say, 60 seconds, a stable boiler temperature will be published at most once per minute. A value that changes mid-interval is still published immediately.

The interval gate applies to OpenTherm data messages only. SAT topics, connection status, version info, and other non-OT topics are always published when they change, regardless of the interval setting.

#### Debugging MQTT

Telnet into the firmware on port 23. Two separate debug toggles cover MQTT behaviour:

| Key | Flag | What it logs |
|---|---|---|
| `3` | `bMQTT` | MQTT module trace: connect, subscribe, publish, discovery progress, errors |
| `g` | `bMQTTGate` | Per-message gate decisions from the publish interval filter |

Enable `g` to see why a given message was published or suppressed by the interval filter:

```
MQTT gate id=25 src=S slot=25 prev=0x0000 curr=0x0510 first=true changed=true interval=false last=0 now=42 => publish [tracked update]
MQTT gate id=25 src=S slot=25 prev=0x0510 curr=0x0510 first=false changed=false interval=false last=42 now=71 => skip [suppressed by interval]
```

Enable `3` for the higher-level MQTT trace when diagnosing connection, subscription, or discovery issues. The two flags are independent: the gate log is high-volume, so you generally only enable it briefly while diagnosing a specific sensor.

---

### 4.6 Home Assistant Auto-Discovery

When the firmware connects to the MQTT broker, it automatically publishes discovery configuration messages to Home Assistant. You do not need to write any YAML.

#### How it works

Home Assistant listens on `homeassistant/#` for discovery messages. The firmware publishes a JSON configuration payload for each entity at:

```
homeassistant/{component}/{node_id}/{entity_id}/config
```

#### Streaming discovery architecture

Starting with v2.0.0 the firmware uses a streaming discovery pipeline (implemented in `MQTTHaDiscovery.cpp`). Every discovery payload is composed on the fly and written to the MQTT client in 128-byte chunks, similar to how ESPHome builds its discovery messages. Nothing is buffered as a full JSON string in RAM. This keeps heap pressure low (around 200 bytes of working memory per entity) and avoids the heap fragmentation problems that the older approach suffered from on the ESP8266.

The streaming functions (`streamSensorDiscovery`, `streamBinarySensorDiscovery`, `streamClimateDiscovery`, `streamNumberDiscovery`, `streamDallasSensorDiscovery`, `streamSatSwitchDiscovery`, `streamSatSelectDiscovery`) replace the older PROGMEM-template approach. Streaming is always on; there is no flag to turn it off.

The legacy `mqttha.cfg` file that earlier versions stored on LittleFS is no longer used at runtime. It has been archived to `docs/archive/mqttha.cfg` as a reference for the original template layout only.

#### Bitmap-driven drip publisher (JIT, ADR-100)

Discovery is incremental and just-in-time. A 256-bit "done" bitmap tracks which OpenTherm message IDs have already had their discovery entities published. Two entry points drive the pipeline:

- `doAutoConfigure()` is reserved for the explicit force path (telnet `F`, REST `POST /api/v2/otgw/discovery`). It marks every OT id pending and streams them all.
- `doAutoConfigureMsgid(OTid)` runs opportunistically: each time an OpenTherm message of a given id arrives on the bus, the corresponding discovery entries are streamed once, then the bitmap bit is set so the same id never re-publishes. This means the discovery set for a given boiler is built up naturally as messages are observed, rather than dumping all 256 possible ids at boot.

At boot the firmware queues only the non-OT entities (climate + DHW, outside-temperature override number, Dallas sensors, heap stats); the OT data sensors appear in HA as their MsgIDs are first seen.

Two pseudo-IDs are used to hang non-OT entities off the bitmap:

- ID `0` carries the two climate entities (thermostat and DHW control), the 13 SAT switches, and the SAT select.
- ID `27` carries the "outside temperature override" number entity.

Dallas sensors have their own path (`streamDallasSensorDiscovery`) because their discovery depends on the actual 1-Wire address read at runtime, and they are published via `configSensors()` once those addresses are known.

#### Heap safety

Every `stream*Discovery` function checks free heap (`STREAM_HEAP_MIN = 4000` bytes) before composing a payload and returns `false` if heap is too low. The drip loop then re-tries later when memory has recovered. This keeps discovery alive through transient heap pressure without ever crashing the device.

#### Re-discovery triggers

The firmware re-publishes discovery configurations in these situations:

1. **Firmware boot.** The bitmap is cleared and the non-OT configs are queued immediately. OT configs publish JIT as MsgIDs arrive.
2. **Top Topic changed.** Same as boot.
3. **MQTT reconnect after > 5 minutes offline (broker-restart heuristic).** Treated as a probable broker restart: bitmaps are cleared and non-OT configs are queued again; OT configs publish JIT as MsgIDs arrive. A short reconnect (≤ 5 minutes) does nothing, because the broker has retained the discovery configs.
4. **Manual force.** Telnet `F` or `POST /api/v2/otgw/discovery` marks every OT id pending so the full set re-streams.

Home Assistant restart (`homeassistant/status → online`) no longer triggers a re-publish on its own. The broker retains the discovery messages, so HA picks them up directly from retained state.

#### Forcing a full rediscovery

From the telnet debug console, press `F` (uppercase). This clears the done bitmap and re-sends discovery for every entity. The same effect is available via the REST API:

```bash
curl -X POST http://otgw.local/api/v2/otgw/discovery
```

---

### 4.7 Auto-Discovered Entities

#### OpenTherm Sensors

| Sensor | Topic suffix | Unit |
|---|---|---|
| Boiler water temperature | `boilertemperature` | °C |
| Return water temperature | `returnwatertemperature` | °C |
| Room temperature | `roomtemperature` | °C |
| Room setpoint | `roomsetpoint` | °C |
| Control setpoint | `controlsetpoint` | °C |
| Relative modulation level | `relmodlvl` | % |
| DHW temperature | `dhwtemperature` | °C |
| DHW setpoint | `dhwsetpoint` | °C |
| Max CH water setpoint | `maxchwatersetpoint` | °C |
| Outside temperature | `outsidetemperature` | °C |
| CH water pressure | `chwaterpressure` | bar |

The exact set depends on which OpenTherm message IDs your thermostat and boiler exchange. Any defined message ID may generate one or more entities.

#### Binary Sensors

| Entity | Description |
|---|---|
| Flame status | Whether the boiler flame is on |
| Central heating active | CH mode active on boiler |
| DHW mode | Hot water mode active |
| Fault indicator | Boiler fault flag |
| CH enable | Thermostat requesting central heating |
| DHW enable | Thermostat requesting hot water |

#### Climate Entities

Two climate entities are published:

- Thermostat climate: shows the current room temperature, the active setpoint, and allows setting a new target temperature from the HA dashboard.
- DHW climate: exposes domestic hot water control as a climate entity.

If SAT is enabled, the thermostat climate is the SAT-driven one and supports mode and preset selection.

##### HVAC mode and action (off / heat / cool)

The thermostat climate entity supports the HVAC modes `off`, `heat`, and `cool`. The firmware computes two values from the OpenTherm status bits and publishes them on their own topics:

```
<pub>/hvac_mode     →  off | heat | cool
<pub>/hvac_action   →  off | idle | heating | cooling
```

- `hvac_mode` follows the master (thermostat) enable bits: it reflects the thermostat's standing intent. It is `off` when no thermostat is connected, `cool` when the master cooling-enable bit is set, and `heat` otherwise. A heating thermostat stays `heat` even while idle between calls.
- `hvac_action` follows the slave (boiler) actual bits: `cooling` when the cooling bit is set, `heating` when the central-heating bit is set, `idle` otherwise, and `off` when no thermostat is connected.

The mode is reflective: the firmware mirrors state, it does not command the thermostat. The thermostat owns heat/cool switching. Note that `hvac_action` derives from the central-heating and cooling status bits, not the flame, so a domestic-hot-water draw does not read as heating.

Both values are also exposed as two standalone, discoverable Home Assistant sensors (`hvac_mode` and `hvac_action`). They auto-discover on first boot alongside the other non-OT entities, so they appear even if you do not use the climate entity itself.

#### Number Entity

A number entity is published for the outside-temperature override, letting HA push a value as if it came from a wired outdoor sensor.

#### Button Entity (PIC reset)

A `button` entity called `resetgateway` is published under pseudo-ID 244 with `entity_category: config` and `payload_press: "1"`. Pressing it from the HA dashboard hardware-resets the PIC over its reset pin. To avoid storms from misconfigured automations the firmware now requires the exact payload `"1"` and applies a 5-second cooldown between successive resets; any other payload is silently ignored.

#### BLE Sensor Entities (ESP32 only, new in 2.0.0)

When BLE is enabled on ESP32 hardware, each first-seen sensor is published as its own HA device with four sensor entities (temperature, humidity, battery, RSSI). Per-sensor state topics are published under:

```
OTGW/value/<uniqueId>/sat/ble/<mac>/{temp,rh,bat,rssi}
```

The MAC is rendered as 12 lowercase hex characters with no separators (e.g. `a4c138123456`). Discovery is one-shot per MAC per session. The four entities are grouped under a single HA device with `model: "BLE Sensor"` and `via_device: <uniqueId>`.

Each BLE probe appears as a separate Home Assistant child-device, linked back to the main OTGW device through `via_device` (ADR-148). This is the one exception to the single-device topology (ADR-140): every non-BLE entity stays inside the single OTGW device, and only BLE probes are broken out into their own child-devices so each physical sensor shows as its own device in Home Assistant.

#### SAT Switches and Select (new in 2.0.0)

When SAT is enabled, the streaming pipeline publishes 13 boolean switches and 1 select, giving Home Assistant direct control over the SAT feature flags. All entity names are prefixed with the hostname.

Switches:

| Suffix | Purpose |
|---|---|
| `SAT_Solar_Gain` | Enable solar gain compensation |
| `SAT_Summer_Simmer` | Enable summer simmer index auto-disable |
| `SAT_Comfort_Adjust` | Enable comfort adjust |
| `SAT_Multi_Area` | Enable multi-area weighted room temperature |
| `SAT_Auto_Tune` | Enable automatic PID gain tuning |
| `SAT_Simulation` | Run SAT in simulation mode (no real boiler control) |
| `SAT_Window_Detection` | Enable window-open detection |
| `SAT_Force_PWM` | Force PWM mode regardless of boiler modulation |
| `SAT_Push_Setpoint` | Push control setpoint to boiler via TC= |
| `SAT_OVP_Enabled` | Enable overshoot prevention calibration |
| `SAT_Preset_Sync` | Sync climate preset to secondary entities |
| `SAT_DHW_Enabled` | Enable DHW control interaction with SAT |
| `SAT_PWM_Auto_Switch` | Auto-switch between continuous and PWM mode |

Select:

| Suffix | Options | Purpose |
|---|---|---|
| `SAT_Heating_System` | `0`, `1`, `2`, `3` | Heating system type (radiator, underfloor, low-temp, heat pump) |

Each switch uses `true`/`false` payloads and is retained; the select publishes and accepts the numeric strings above.

#### SAT Sensor Entities

When SAT is enabled, the firmware also auto-discovers diagnostic sensor entities. These are published under the `sat/` topic prefix.

**Core SAT topics (always published when SAT is active):**

| Topic suffix | Description | Unit |
|---|---|---|
| `sat/setpoint` | Final flow temperature sent to boiler | °C |
| `sat/target` | Target room temperature | °C |
| `sat/mode` | Control mode: `off`, `continuous`, or `pwm` | - |
| `sat/heating_curve` | Heating curve base value | °C |
| `sat/pid_output` | PID corrected output | °C |
| `sat/error` | PID error (target minus room temperature) | °C |
| `sat/room_temp` | Room temperature used by PID | °C |
| `sat/outside_temp` | Outdoor temperature used by heating curve | °C |
| `sat/boiler_status` | Current boiler status (text label) | - |
| `sat/active` | Whether SAT is actively controlling | bool |
| `sat/safety_tripped` | Whether a safety layer has tripped | bool |
| `sat/simulation` | Simulation mode on/off | - |

**SAT command topics (gateway subscribes to these):**

| Topic suffix | Accepted values | Description |
|---|---|---|
| `sat/target` | Float, e.g. `21.0` | Set target room temperature |
| `sat/control_mode` | `off`, `continuous`, `pwm`, `auto` | Set control mode |
| `sat/enabled` | `true` / `false` | Enable or disable SAT |
| `sat/indoor_temp` | Float, e.g. `20.5` | Push indoor temperature from external sensor |
| `sat/outdoor_temp` | Float, e.g. `5.2` | Push outdoor temperature from external source |

---

### 4.8 Sending Commands from Home Assistant

#### Setting the Boiler CH Setpoint

```yaml
action:
  - service: mqtt.publish
    data:
      topic: "OTGW/set/otgw-AABBCCDDEEFF/setpoint"
      payload: "20.5"
```

#### Controlling Hot Water (DHW)

```
Topic:   OTGW/set/otgw-AABBCCDDEEFF/hotwater
Payload: 1      (force on)
         0      (force off)
         P      (push: one-shot hot water cycle)
         A      (automatic)
```

#### Sending Raw OTGW Commands

```yaml
action:
  - service: mqtt.publish
    data:
      topic: "OTGW/set/otgw-AABBCCDDEEFF/command"
      payload: "TT=21.5"
```

#### Hardware-Resetting the PIC

```yaml
action:
  - service: mqtt.publish
    data:
      topic: "OTGW/set/otgw-AABBCCDDEEFF/resetgateway"
      payload: "1"
```

The payload must be exactly `"1"`; any other value is ignored. A 5-second cooldown is applied so repeated triggers are rate-limited to one PIC reset per 5 seconds. This is a no-op on OTGW32 hardware (no PIC). The HA `Reset Gateway` button entity publishes this exact payload automatically.

---

### 4.9 Example Home Assistant Automations

#### Turn down heating when everyone leaves

```yaml
automation:
  - alias: "Away: reduce heating setpoint"
    trigger:
      - platform: state
        entity_id: group.family
        to: "not_home"
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw-AABBCCDDEEFF/setpoint"
          payload: "16.0"
```

#### Push outdoor temperature to the gateway for weather compensation

```yaml
automation:
  - alias: "Push outdoor temp to OTGW"
    trigger:
      - platform: state
        entity_id: sensor.openweathermap_temperature
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw-AABBCCDDEEFF/outside"
          payload: "{{ states('sensor.openweathermap_temperature') }}"
```

---

### 4.10 Manual Home Assistant Configuration (Without Auto-Discovery)

Example manual sensor configuration:

```yaml
mqtt:
  sensor:
    - name: "Boiler Temperature"
      state_topic: "OTGW/value/otgw-AABBCCDDEEFF/boilertemperature"
      unit_of_measurement: "°C"
      device_class: temperature
      state_class: measurement

    - name: "Room Temperature"
      state_topic: "OTGW/value/otgw-AABBCCDDEEFF/roomtemperature"
      unit_of_measurement: "°C"
      device_class: temperature
      state_class: measurement

  binary_sensor:
    - name: "Boiler Flame"
      state_topic: "OTGW/value/otgw-AABBCCDDEEFF/flamestatus"
      payload_on: "ON"
      payload_off: "OFF"
      device_class: heat

  climate:
    - name: "Heating Setpoint"
      modes:
        - "heat"
        - "off"
      current_temperature_topic: "OTGW/value/otgw-AABBCCDDEEFF/roomtemperature"
      temperature_state_topic: "OTGW/value/otgw-AABBCCDDEEFF/roomsetpoint"
      temperature_command_topic: "OTGW/set/otgw-AABBCCDDEEFF/setpoint"
      min_temp: 5
      max_temp: 30
      temp_step: 0.5
```

---

### 4.11 Nightly Restart

The firmware supports an optional scheduled nightly restart to recover heap memory. This is useful for long-running installations where gradual heap fragmentation can eventually cause instability.

| Setting | Default | Description |
|---|---|---|
| Nightly Restart | off | Enable or disable the scheduled daily restart |
| Nightly Restart Hour | 4 | Hour (0-23) when the restart occurs. Only active when NTP is enabled and synced. |

These settings are available in the Web UI (Settings page), via the REST API (`/api/v2/settings`), and via the MQTT settings topic. The restart causes a brief interruption of approximately 30 seconds.

---

