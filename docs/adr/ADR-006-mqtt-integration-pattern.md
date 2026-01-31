# ADR-006: MQTT Integration Pattern

**Status:** Accepted  
**Date:** 2018-06-01 (Estimated)  
**Updated:** 2026-01-31 (52 commands, PIC16F1847 advanced features)

## Context

The primary goal of OTGW-firmware is **reliable Home Assistant integration**. Home Assistant uses MQTT as the standard protocol for integrating IoT devices.

**Requirements:**
- Publish all OpenTherm sensor values (temperature, pressure, status bits)
- Support MQTT Auto-Discovery for zero-configuration Home Assistant integration
- Allow remote commands to control the boiler
- Handle connection failures gracefully
- Work with any MQTT broker (Mosquitto, Home Assistant built-in, etc.)
- Minimize memory usage and prevent heap fragmentation

**Integration goals:**
- One-click setup in Home Assistant (via Auto-Discovery)
- Automatic entity creation (sensors, binary sensors, climate control)
- Real-time updates as OpenTherm values change
- Support for setting temperature, hot water, etc.

## Decision

**Implement MQTT client with Home Assistant Auto-Discovery support using the PubSubClient library.**

**Architecture:**
- **Library:** PubSubClient (lightweight MQTT client for Arduino)
- **Protocol:** MQTT 3.1.1
- **Discovery:** Home Assistant MQTT Auto-Discovery protocol
- **Topic structure:**
  - State: `<prefix>/value/<node-id>/<sensor>`
  - Commands: `<prefix>/set/<node-id>/<command>`
  - Discovery: `homeassistant/<component>/<node-id>_<object-id>/config`
- **QoS:** 0 (at most once) for performance
- **Retain:** Yes for sensor values, discovery configs
- **Buffer:** 1200 bytes maximum message size
- **Reconnection:** Automatic with exponential backoff

**State management:**
```cpp
enum MQTTStates {
  MQTT_STATE_INIT,
  TRY_TO_CONNECT,
  IS_CONNECTED,
  WAIT_FOR_RECONNECT
};
```

**Chunked streaming (v1.0.0):**
- Splits large messages into 128-byte chunks
- Eliminates buffer resize cycles
- Saves 200-400 bytes of heap

## Alternatives Considered

### Alternative 1: REST API Only (No MQTT)
**Pros:**
- Simpler implementation
- No external broker dependency
- HTTP is universal

**Cons:**
- Requires polling (inefficient)
- No Home Assistant Auto-Discovery
- Manual entity configuration required
- Higher latency
- More network traffic

**Why not chosen:** Home Assistant integration is the primary goal. MQTT Auto-Discovery is essential for ease of use.

### Alternative 2: CoAP (Constrained Application Protocol)
**Pros:**
- Designed for IoT/constrained devices
- UDP-based (lower overhead than TCP)
- RESTful patterns

**Cons:**
- Not supported by Home Assistant natively
- Less mature Arduino libraries
- Requires custom integration
- No discovery protocol

**Why not chosen:** Lack of Home Assistant support makes this a non-starter for the primary use case.

### Alternative 3: Homie Convention
**Pros:**
- Standardized MQTT convention
- Self-describing devices
- Better structure than ad-hoc topics

**Cons:**
- Not Home Assistant's native protocol
- Additional complexity
- Requires Homie discovery integration in Home Assistant
- Less flexible than HA Auto-Discovery

**Why not chosen:** Home Assistant's native Auto-Discovery is simpler and more widely supported.

### Alternative 4: Custom TCP Protocol
**Pros:**
- Complete control over protocol
- Minimal overhead
- No broker dependency

**Cons:**
- Requires custom Home Assistant integration
- No community support
- Complex to maintain
- Reinventing the wheel

**Why not chosen:** MQTT is the industry standard. No benefit to custom protocol.

## Consequences

### Positive
- **Zero-configuration:** Home Assistant automatically discovers all entities
- **Reliable:** MQTT broker handles delivery, buffering
- **Efficient:** Publish-only model (no polling)
- **Standard:** Works with any MQTT-compatible system
- **Flexible:** Topic structure allows custom integrations
- **Scalable:** Broker can handle many clients
- **Automatic entities:** 30+ sensors appear in Home Assistant automatically

**Entity types auto-created:**
- Climate entity (thermostat control)
- Sensors (temperatures, pressures, setpoints)
- Binary sensors (flame status, DHW active, heating active)
- Number inputs (setpoint override)

### Negative
- **Broker dependency:** Requires MQTT broker running
  - Mitigation: Most Home Assistant setups have built-in broker
  - Documentation: Setup guide for Mosquitto
- **Configuration required:** Users must enter broker IP, credentials
  - Mitigation: Web UI settings page with validation
- **Network traffic:** Publishes every value change
  - Mitigation: Only publish on change, not every loop iteration
  - Mitigation: 30-second interval for non-critical updates
- **Memory for client:** MQTT library uses ~2KB RAM
  - Accepted: Essential for primary use case
- **Reconnection complexity:** Must handle broker restarts
  - Implemented: State machine with exponential backoff

### Risks & Mitigation
- **Buffer overflows:** MQTT messages can be large (discovery configs ~800 bytes)
  - **Mitigation:** 1200-byte buffer limit, chunked streaming (v1.0.0)
  - **Mitigation:** Split large discovery configs into multiple messages
- **Heap fragmentation:** PubSubClient uses String class internally
  - **Mitigation:** Modified library to use static buffers
  - **Mitigation:** Chunked streaming eliminates resize cycles
- **Connection storms:** Many devices reconnecting simultaneously after broker restart
  - **Mitigation:** Randomized reconnection delay
- **Topic explosion:** Too many topics could overwhelm broker
  - **Accepted:** ~30 topics is reasonable for MQTT broker

## Implementation Patterns

**Publishing sensor values:**
```cpp
void publishValue(const char* sensor, const char* value) {
  char topic[100];
  snprintf_P(topic, sizeof(topic), 
    PSTR("%s/value/%s/%s"), 
    settingMqttTopTopic, 
    settingMqttUniqueID,
    sensor);
  
  mqttClient.publish(topic, value, true); // retain=true
}
```

**Command subscription:**
```cpp
// Subscribe to: otgw-firmware/set/<node-id>/#
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Parse command from topic
  // Map to OTGW command (TT, SW, etc.)
  // Add to command queue
}
```

**Auto-Discovery:**
```cpp
// Publish discovery config for each entity
void publishAutoDiscovery() {
  for (each sensor) {
    char topic[150];
    snprintf_P(topic, sizeof(topic),
      PSTR("homeassistant/sensor/%s_%s/config"),
      settingMqttUniqueID, sensorName);
    
    // Build JSON config (ArduinoJson)
    // Publish with retain=true
  }
}
```

**Chunked streaming (v1.0.0):**
```cpp
// For messages > 128 bytes, split into chunks
void publishChunked(const char* topic, const char* message) {
  const size_t chunkSize = 128;
  size_t len = strlen(message);
  
  for (size_t i = 0; i < len; i += chunkSize) {
    size_t remaining = len - i;
    size_t thisChunk = (remaining < chunkSize) ? remaining : chunkSize;
    
    // Publish chunk with sequence number in topic
    char chunkTopic[150];
    snprintf_P(chunkTopic, sizeof(chunkTopic),
      PSTR("%s/chunk/%d"), topic, chunkNumber++);
    
    mqttClient.publish(chunkTopic, message + i, thisChunk);
  }
}
```

## Home Assistant Configuration

**Automatic entities (examples):**
```yaml
# Climate entity (auto-discovered)
climate.otgw_thermostat:
  temperature: sensor.otgw_room_temp
  target_temp: sensor.otgw_room_setpoint
  
# Sensors (auto-discovered)
sensor.otgw_boiler_temp
sensor.otgw_return_temp
sensor.otgw_dhw_temp
sensor.otgw_outside_temp
sensor.otgw_ch_pressure

# Binary sensors (auto-discovered)
binary_sensor.otgw_flame_status
binary_sensor.otgw_dhw_active
binary_sensor.otgw_heating_active
```

**Commands:**
- `TT`: Temporary temperature override
- `SW`: DHW setpoint
- `GW`: Gateway mode
- `PS`: Publish settings

## Related Decisions
- ADR-004: Static Buffer Allocation Strategy (buffer sizing, chunked streaming)
- ADR-007: Timer-Based Task Scheduling (periodic MQTT publishes)
- ADR-025: PIC Version-Aware Command Validation (validates commands against PIC hardware before sending)

## References
- PubSubClient library: https://github.com/knolleary/pubsubclient
- Home Assistant MQTT Discovery: https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery
- Implementation: `MQTTstuff.ino`
- Command mappings: `MQTTstuff.ino` (setcmds array)
- Settings: `settingStuff.ino` (MQTT broker config)
- Chunked streaming: README.md (v1.0.0 features)
