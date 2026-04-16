## Chapter 4: Home Assistant Integration

### Overview

OTGW-firmware is designed for tight Home Assistant integration. Once you have an MQTT broker running and the firmware connected to it, Home Assistant automatically discovers all OpenTherm sensors, the boiler climate entity, and binary status sensors, with no YAML required. This chapter covers how that works, what gets discovered, and how to send commands from Home Assistant to the gateway.

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

#### Debugging the gate

If you connect to the firmware telnet debug port (port 23) and press `g`, the MQTT gate debug flag toggles. With it enabled, the firmware logs every gate decision to the debug console:

```
MQTT gate id=25 src=S slot=25 prev=0x0000 curr=0x0510 first=true changed=true interval=false last=0 now=42 => publish [tracked update]
MQTT gate id=25 src=S slot=25 prev=0x0510 curr=0x0510 first=false changed=false interval=false last=42 now=71 => skip [suppressed by interval]
```

This is useful when diagnosing why a sensor is or is not appearing in Home Assistant.

---

### 4.6 Home Assistant Auto-Discovery

When the firmware connects to the MQTT broker, it automatically publishes discovery configuration messages to Home Assistant. You do not need to write any YAML.

#### How it works

Home Assistant listens on `homeassistant/#` for discovery messages. The firmware publishes a JSON configuration payload for each entity at:

```
homeassistant/{component}/{node_id}/{entity_id}/config
```

#### Async drip publisher

Starting with v2.0.0, discovery uses an async "drip" publisher. Instead of sending all 345 discovery messages in a single burst at startup, the firmware publishes one entity every 3 seconds in the background. This means a full discovery cycle takes approximately 17 minutes, but the approach is far gentler on the ESP8266's limited heap and on the MQTT broker.

The drip publisher includes two protective mechanisms:

- **Heap guard.** If free heap drops below 8 KB, the discovery publish is skipped until memory recovers. This prevents out-of-memory crashes during periods of high network activity.
- **Adaptive interval.** Under heap pressure the interval automatically widens from 3 seconds to 30 seconds. When heap recovers, the interval returns to 3 seconds. This ensures the system stays stable while still making progress on discovery.

During the initial 17 minutes, entities appear gradually in Home Assistant. Sensors that have not yet been discovered will show up as they are published. Once all entities are discovered, the state is remembered and no re-discovery is needed unless you explicitly trigger one.

#### Discovery templates in PROGMEM

Discovery templates are compiled into flash memory (PROGMEM) rather than loaded from the LittleFS filesystem at runtime. This is transparent to the user, but it eliminates filesystem I/O during discovery and slightly reduces RAM usage. The `mqttha.cfg` file is no longer read at runtime; it serves only as the source for the build-time code generator.

#### Re-discovery triggers

The firmware re-publishes all discovery configurations in two situations:

1. **Firmware boot or MQTT settings change.** All discovery IDs are queued for drip publishing.
2. **Home Assistant restart.** The firmware monitors `homeassistant/status`. When HA comes back online, all IDs are re-queued.

On a simple MQTT reconnect (e.g. a brief network interruption), discovery is *not* re-run. The broker retains the discovery messages, so re-publishing them on every reconnect would be unnecessary.

#### Triggering a full rediscovery manually

```bash
curl -X POST http://otgw.local/api/v2/otgw/discovery
```

This queues all discovery IDs for drip publishing. The entities will appear over the following ~17 minutes.

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

#### Binary Sensors

| Entity | Description |
|---|---|
| Flame status | Whether the boiler flame is on |
| Central heating active | CH mode active on boiler |
| DHW mode | Hot water mode active |
| Fault indicator | Boiler fault flag |
| CH enable | Thermostat requesting central heating |
| DHW enable | Thermostat requesting hot water |

#### Climate Entity

The firmware publishes a climate entity showing the current room temperature, the active setpoint, and allows setting a new target temperature from the HA dashboard. If SAT is enabled, a dedicated SAT climate entity (`sat_climate`) appears with full mode and preset support.

#### SAT Sensor Entities

When SAT is enabled, the firmware auto-discovers additional diagnostic entities. These are published under the `sat/` topic prefix.

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

