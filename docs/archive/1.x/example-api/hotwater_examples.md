# Hot Water Control Examples

This document provides examples of how to control the domestic hot water (DHW) enable option using MQTT.

## Overview

The OTGW firmware supports controlling the domestic hot water enable option via the `hotwater` MQTT command. This command maps to the OpenTherm Gateway's `HW` command, which allows you to override the thermostat's control of when to keep a small amount of water preheated.

**Note:** This feature only works if your boiler has been configured to let the room unit (thermostat) control the domestic hot water enable option. The DHW push functionality is experimental and only available in PIC16F1847 firmware.

## MQTT Command Structure

The command follows the standard MQTT topic structure:

```
<mqtt-prefix>/set/<node-id>/hotwater
```

**Parameters:**
- `<mqtt-prefix>`: Your configured MQTT topic prefix (e.g., `OTGW`)
- `<node-id>`: Your configured node ID (e.g., `otgw`)
- Payload: Control value (see below)

## Valid Values

| Value | Description | Use Case |
|-------|-------------|----------|
| `0` | Off - Do not keep water preheated | Disable DHW to save energy when not needed |
| `1` | On - Keep water preheated | Ensure hot water is always available |
| `P` | DHW Push - Heat the water tank once | Experimental: One-time tank heating (PIC16F1847 only) |
| `A` (or any other character) | Auto - Let thermostat control | Return control to the thermostat |

**Reference:** [OTGW PIC Firmware Documentation - HW Command](https://otgw.tclcode.com/firmware.html#cmdhw)

## Examples

### 1. Using mosquitto_pub (Command Line)

```bash
# Turn DHW off (save energy)
mosquitto_pub -h 192.168.1.100 -t "OTGW/set/otgw/hotwater" -m "0"

# Turn DHW on (always keep water warm)
mosquitto_pub -h 192.168.1.100 -t "OTGW/set/otgw/hotwater" -m "1"

# Perform a DHW push (PIC16F1847 only - experimental)
mosquitto_pub -h 192.168.1.100 -t "OTGW/set/otgw/hotwater" -m "P"

# Return to auto/thermostat control
mosquitto_pub -h 192.168.1.100 -t "OTGW/set/otgw/hotwater" -m "A"

# With authentication
mosquitto_pub -h 192.168.1.100 -u username -P password -t "OTGW/set/otgw/hotwater" -m "1"
```

### 2. Home Assistant - Manual Control

Create input select and automation to control DHW:

```yaml
# configuration.yaml
input_select:
  otgw_dhw_mode:
    name: Hot Water Mode
    options:
      - "Auto"
      - "On"
      - "Off"
      - "Push"
    initial: "Auto"
    icon: mdi:water-boiler

automation:
  - alias: "OTGW DHW Mode Control"
    description: "Control OTGW hot water mode"
    trigger:
      - platform: state
        entity_id: input_select.otgw_dhw_mode
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw/hotwater"
          payload: >
            {% if states('input_select.otgw_dhw_mode') == 'Auto' %}
              A
            {% elif states('input_select.otgw_dhw_mode') == 'On' %}
              1
            {% elif states('input_select.otgw_dhw_mode') == 'Off' %}
              0
            {% elif states('input_select.otgw_dhw_mode') == 'Push' %}
              P
            {% endif %}
```

### 3. Home Assistant - Schedule-based Control

Automatically enable DHW during peak usage hours:

```yaml
automation:
  - alias: "DHW Morning Enable"
    description: "Enable hot water in the morning"
    trigger:
      - platform: time
        at: "06:00:00"
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw/hotwater"
          payload: "1"

  - alias: "DHW Evening Enable"
    description: "Enable hot water in the evening"
    trigger:
      - platform: time
        at: "18:00:00"
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw/hotwater"
          payload: "1"

  - alias: "DHW Night Disable"
    description: "Disable hot water at night to save energy"
    trigger:
      - platform: time
        at: "23:00:00"
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw/hotwater"
          payload: "0"

  - alias: "DHW Morning Return to Auto"
    description: "Return to thermostat control after morning peak"
    trigger:
      - platform: time
        at: "09:00:00"
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw/hotwater"
          payload: "A"
```

### 4. Home Assistant - Presence-based Control

Enable DHW when someone is home:

```yaml
automation:
  - alias: "DHW Enable When Home"
    description: "Enable hot water when someone arrives home"
    trigger:
      - platform: state
        entity_id: binary_sensor.someone_home
        to: "on"
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw/hotwater"
          payload: "1"

  - alias: "DHW Disable When Away"
    description: "Disable hot water when everyone leaves (save energy)"
    trigger:
      - platform: state
        entity_id: binary_sensor.someone_home
        to: "off"
        for:
          hours: 1  # Wait 1 hour to avoid rapid on/off cycles
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw/hotwater"
          payload: "0"
```

### 5. Node-RED Example

```json
[
    {
        "id": "dhw_control",
        "type": "mqtt out",
        "broker": "mqtt_broker",
        "topic": "OTGW/set/otgw/hotwater",
        "qos": "0",
        "retain": "false",
        "name": "DHW Control"
    },
    {
        "id": "dhw_on",
        "type": "inject",
        "payload": "1",
        "payloadType": "str",
        "topic": "",
        "name": "DHW On",
        "wires": [["dhw_control"]]
    },
    {
        "id": "dhw_off",
        "type": "inject",
        "payload": "0",
        "payloadType": "str",
        "topic": "",
        "name": "DHW Off",
        "wires": [["dhw_control"]]
    },
    {
        "id": "dhw_auto",
        "type": "inject",
        "payload": "A",
        "payloadType": "str",
        "topic": "",
        "name": "DHW Auto",
        "wires": [["dhw_control"]]
    }
]
```

## Use Cases

### Energy Saving
Disable DHW during periods when hot water is not needed:
- Overnight when everyone is sleeping
- During work hours when the house is empty
- During vacations

### Comfort
Ensure hot water is always available:
- Morning routines (showers, kitchen)
- Evening routines (dishes, baths)
- When guests are expected

### DHW Push (Experimental - PIC16F1847 only)
One-time heating of the water tank:
- Before a shower when the tank has cooled
- After a period of low usage
- On-demand hot water boost

**Note:** The DHW push (`P`) functionality is experimental and only available with PIC16F1847 firmware. Check your OTGW PIC firmware version before using this feature.

## Monitoring DHW Status

You can monitor the DHW status through MQTT topics published by the OTGW:

```
<mqtt-prefix>/value/<node-id>/domestichotwater  # Binary sensor (ON/OFF)
<mqtt-prefix>/value/<node-id>/dhw_enable        # DHW enable status bit
```

Example Home Assistant automation to log DHW changes:

```yaml
automation:
  - alias: "Log DHW Status Changes"
    description: "Log when DHW status changes"
    trigger:
      - platform: state
        entity_id: binary_sensor.otgw_domestic_hot_water
    action:
      - service: logbook.log
        data:
          name: "OTGW DHW"
          message: >
            Domestic Hot Water changed to {{ states('binary_sensor.otgw_domestic_hot_water') }}
```

## Troubleshooting

### Command Not Working
1. **Verify boiler configuration**: Your boiler must be configured to allow room unit control of DHW
2. **Check MQTT connection**: Ensure MQTT is connected and topics are correct
3. **PIC firmware version**: DHW push requires PIC16F1847 firmware
4. **Monitor MQTT debug**: Check OTGW telnet debug output (port 23) for command responses

### DHW State Not Changing
- Some boilers do not support DHW control via OpenTherm
- Check if your boiler reports DHW enable capability (Message ID 3, bit 0)
- Verify OpenTherm Gateway is properly connected to both thermostat and boiler

## Related MQTT Commands

- `maxdhwsetpt` - Set maximum DHW setpoint temperature
- See [outside_temperature_override_examples.md](outside_temperature_override_examples.md) for outdoor sensor override

## References

- [OTGW PIC Firmware - HW Command](https://otgw.tclcode.com/firmware.html#cmdhw)
- [OpenTherm Protocol Specification](http://otgw.tclcode.com/index.html)
- [OTGW Firmware Wiki - MQTT Commands](https://github.com/rvdbreemen/OTGW-firmware/wiki/Using-the-MQTT-set-commands)
