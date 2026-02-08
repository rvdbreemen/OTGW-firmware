# Outside Temperature Override Examples

This document provides examples of how to override the built-in outside temperature sensor with an external sensor value using MQTT.

## Overview

The OTGW firmware supports overriding the outside temperature value sent to your boiler via the `outside` MQTT command. This is useful when:

- Your boiler doesn't have an outside temperature sensor
- The built-in sensor is malfunctioning or poorly positioned
- You want to use a more accurate weather station or sensor
- The sensor is affected by sun/wind exposure

## MQTT Command Structure

The command follows the standard MQTT topic structure:

```
<mqtt-prefix>/set/<node-id>/outside
```

**Parameters:**
- `<mqtt-prefix>`: Your configured MQTT topic prefix (e.g., `OTGW`)
- `<node-id>`: Your configured node ID (e.g., `otgw`)
- Payload: Temperature value in Celsius (e.g., `15.5`)

**Valid Range:** -40°C to 50°C (OpenTherm specification for Message ID 27)

## Examples

### 1. Using mosquitto_pub (Command Line)

```bash
# Set outside temperature to 15.5°C
mosquitto_pub -h 192.168.1.100 -t "OTGW/set/otgw/outside" -m "15.5"

# With authentication
mosquitto_pub -h 192.168.1.100 -u username -P password -t "OTGW/set/otgw/outside" -m "12.0"
```

### 2. Home Assistant Automation (Recommended)

Create an automation to automatically sync your preferred outside temperature sensor:

```yaml
automation:
  - alias: "Sync Outside Temperature to OTGW"
    description: "Override OTGW outside temp with weather station data"
    trigger:
      - platform: state
        entity_id: sensor.outdoor_temperature  # Your actual outdoor sensor
    condition:
      # Optional: Only sync if temperature changed significantly
      - condition: template
        value_template: >
          {{ (states('sensor.outdoor_temperature') | float - 
              states('sensor.otgw_outside_temperature') | float) | abs > 0.5 }}
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw/outside"  # Adjust to your MQTT prefix and node ID
          payload: "{{ states('sensor.outdoor_temperature') }}"
```

### 3. Home Assistant Number Entity (Auto-Discovery)

If you have MQTT Auto-Discovery enabled, a number entity will automatically appear in Home Assistant:

**Entity Name:** `number.otgw_outside_temperature_override`

You can use this entity in automations or scripts:

```yaml
service: number.set_value
target:
  entity_id: number.otgw_outside_temperature_override
data:
  value: 18.5
```

Or create a simple automation:

```yaml
automation:
  - alias: "Sync Weather Station to OTGW"
    trigger:
      - platform: state
        entity_id: sensor.weather_station_temperature
    action:
      - service: number.set_value
        target:
          entity_id: number.otgw_outside_temperature_override
        data:
          value: "{{ states('sensor.weather_station_temperature') }}"
```

### 4. Node-RED Example

```json
[
    {
        "id": "outside_temp_sync",
        "type": "mqtt out",
        "topic": "OTGW/set/otgw/outside",
        "qos": "0",
        "retain": "false",
        "broker": "mqtt_broker",
        "name": "Override OTGW Outside Temp"
    },
    {
        "id": "inject_temp",
        "type": "inject",
        "payload": "15.5",
        "payloadType": "num",
        "topic": "",
        "name": "Set to 15.5°C",
        "wires": [["outside_temp_sync"]]
    }
]
```

### 5. Python Script Example

```python
import paho.mqtt.client as mqtt

# MQTT Configuration
MQTT_BROKER = "192.168.1.100"
MQTT_PORT = 1883
MQTT_USER = "username"  # Optional
MQTT_PASS = "password"  # Optional
MQTT_TOPIC = "OTGW/set/otgw/outside"

def set_outside_temperature(temperature):
    """Override OTGW outside temperature"""
    client = mqtt.Client()
    
    # Set credentials if required
    if MQTT_USER and MQTT_PASS:
        client.username_pw_set(MQTT_USER, MQTT_PASS)
    
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.publish(MQTT_TOPIC, str(temperature))
    client.disconnect()
    
    print(f"Set outside temperature to {temperature}°C")

# Example usage
if __name__ == "__main__":
    # Get temperature from your preferred source
    # (weather API, local sensor, etc.)
    current_temp = 15.5
    set_outside_temperature(current_temp)
```

### 6. REST API Example (Alternative)

You can also use the REST API to send the OT command directly:

```bash
# Using curl
curl -X POST "http://192.168.1.50/api/v1/otgw/command/OT=15.5"

# Or using wget
wget -q -O - --post-data="" "http://192.168.1.50/api/v1/otgw/command/OT=15.5"
```

## Important Notes

### Persistence
- The override is **not persistent** across OTGW reboots
- You should set up an automation to re-apply the value after OTGW restarts
- Consider triggering on the `homeassistant/status` MQTT topic (online/offline events)

### OpenTherm Behavior
- The OTGW sends this value to the boiler via OpenTherm Message ID 27
- The boiler may or may not use this value depending on its configuration
- Some boilers ignore external temperature data if they have their own sensor
- Check your boiler's manual for OpenTherm compatibility

### Monitoring
You can verify the current outside temperature value by:
1. **MQTT:** Subscribe to `OTGW/value/otgw/Toutside`
2. **REST API:** GET `http://<ip>/api/v1/otgw/id/27`
3. **Home Assistant:** Check `sensor.otgw_outside_temperature`

### Debugging
If the override doesn't seem to work:
1. Check MQTT logs in the OTGW Web UI
2. Verify your MQTT topic prefix and node ID match your configuration
3. Ensure your boiler supports OpenTherm Message ID 27
4. Check that the value is within the valid range (-40°C to 50°C)
5. Try using the REST API method to confirm the command works

## See Also

- [OTGW Firmware README](../README.md) - Main documentation
- [MQTT Integration ADR](../docs/adr/ADR-006-mqtt-integration-pattern.md) - MQTT architecture
- [OpenTherm Specification](../Specification/) - Protocol details
- [Schelte Bron's OTGW Documentation](http://otgw.tclcode.com/) - Original OTGW project
