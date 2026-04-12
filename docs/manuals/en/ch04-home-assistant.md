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

### 4.5 Home Assistant Auto-Discovery

When the firmware connects to the MQTT broker, it automatically publishes discovery configuration messages to Home Assistant. You do not need to write any YAML.

#### How it works

Home Assistant listens on `homeassistant/#` for discovery messages. The firmware publishes a JSON configuration payload for each entity at:

```
homeassistant/{component}/{node_id}/{entity_id}/config
```

The firmware uses Just-In-Time (JIT) discovery: rather than flooding the broker with 200+ discovery messages at startup, it publishes a discovery config the first time it sees a particular OpenTherm message ID.

If you restart Home Assistant, the firmware detects this by monitoring `homeassistant/status`. When HA comes back online, the firmware re-publishes all discovery configurations automatically.

#### Triggering a full rediscovery manually

```bash
curl -X POST http://otgw.local/api/v2/otgw/discovery
```

---

### 4.6 Auto-Discovered Entities

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

The firmware publishes a climate entity showing the current room temperature, the active setpoint, and allows setting a new target temperature from the HA dashboard. If SAT is enabled, a second climate entity (`sat_climate`) appears.

---

### 4.7 Sending Commands from Home Assistant

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

### 4.8 Example Home Assistant Automations

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

### 4.9 Manual Home Assistant Configuration (Without Auto-Discovery)

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

