# ADR-020: Dallas DS18B20 Temperature Sensor Integration

**Status:** Accepted  
**Date:** 2019-01-01 (Initial implementation)  
**Updated:** 2026-01-28 (Documentation)  
**Breaking Change:** 2025-12-15 (v1.0.0 - GPIO default changed, address format fixed)

## Context

OpenTherm provides boiler and return water temperatures, but users often want to monitor additional temperatures:
- Room temperature (not all thermostats support OpenTherm)
- Outdoor temperature
- DHW (hot water) storage tank temperature
- Underfloor heating manifold temperatures
- Multiple zone temperatures

**Requirements:**
- Support multiple temperature sensors simultaneously
- Auto-discovery of sensors on OneWire bus
- MQTT integration for Home Assistant
- Persist sensor addresses across reboots
- Low cost solution (DS18B20 sensors ~$2 each)

**Historical context:**
- Initial implementation had buggy address format (9-char hex)
- v1.0.0 fixed format to standard 16-char hex
- GPIO pin changed from GPIO 13 (D7) to GPIO 10 (SD3) in v1.0.0

## Decision

**Integrate Dallas DS18B20 sensors via OneWire protocol with dynamic discovery and MQTT publishing.**

**Architecture:**
- **Library:** OneWire + DallasTemperature (standard Arduino libraries)
- **GPIO:** Configurable pin (default GPIO 10 / SD3 as of v1.0.0)
- **Discovery:** Scan OneWire bus at boot, find up to 16 sensors
- **Storage:** Sensor addresses persisted in `sensors.json` (LittleFS)
- **MQTT:** Publish via virtual OpenTherm message ID 246
- **Update interval:** Configurable (default 30 seconds)
- **Address format:** 16-character hex string (8 bytes, big-endian)

**Sensor lifecycle:**
1. Boot: Initialize OneWire on configured GPIO
2. Scan: Discover all DS18B20 devices on bus
3. Store: Save addresses to `sensors.json`
4. Poll: Read temperatures at configured interval
5. Publish: Send to MQTT and make available via REST API

## Alternatives Considered

### Alternative 1: DHT11/DHT22 Sensors
**Pros:**
- Cheaper (~$1)
- Include humidity
- Single wire protocol

**Cons:**
- Limited range (5m vs 100m for DS18B20)
- One sensor per GPIO pin (OneWire supports 16+ on one pin)
- Less accurate (±2°C vs ±0.5°C)
- Not waterproof

**Why not chosen:** OneWire bus allows multiple sensors on one pin. DS18B20 more accurate and suitable for outdoor/wet environments.

### Alternative 2: I2C Temperature Sensors (BME280, SHT31)
**Pros:**
- High accuracy
- Include humidity/pressure
- I2C bus supports many devices

**Cons:**
- More expensive ($5-10 each)
- Requires breakout boards
- I2C bus already used for watchdog
- Limited cable length (<1m)

**Why not chosen:** Cost and cable length. DS18B20 can run 100m+ on Cat5 cable.

### Alternative 3: Analog Temperature Sensors (LM35, TMP36)
**Pros:**
- Very cheap (<$1)
- Simple analog output
- No libraries needed

**Cons:**
- ESP8266 has only one ADC (already used for VCC monitoring)
- Requires calibration
- Noise susceptible
- One sensor per ADC pin

**Why not chosen:** ESP8266 has only one ADC pin. No advantage over digital sensors.

### Alternative 4: Thermocouple (K-type, MAX31855)
**Pros:**
- High temperature range (-200°C to +1350°C)
- Industrial applications
- Fast response

**Cons:**
- Overkill for home heating (0-100°C range)
- More expensive
- Requires SPI interface chip
- Complex calibration

**Why not chosen:** DS18B20 range (-55°C to +125°C) is sufficient for HVAC.

### Alternative 5: WiFi/Zigbee Temperature Sensors
**Pros:**
- Wireless (no cable runs)
- Modern IoT approach
- Often battery powered

**Cons:**
- More expensive ($20-50 each)
- Battery replacement needed
- Network dependency
- Another protocol to integrate

**Why not chosen:** Wired sensors are more reliable for always-on monitoring. Lower cost.

## Consequences

### Positive
- **Multi-sensor support:** Up to 16 sensors on one GPIO pin
- **Auto-discovery:** No manual address entry required
- **Long cable runs:** Cat5 cable up to 100m works reliably
- **Waterproof:** Sensors available in stainless steel probe form
- **Accurate:** ±0.5°C accuracy sufficient for HVAC
- **Low cost:** ~$2 per sensor
- **MQTT integration:** Auto-discovered in Home Assistant
- **Standard protocol:** Well-documented, many libraries available

### Negative
- **GPIO usage:** Uses one GPIO pin (now GPIO 10, was GPIO 13)
  - Breaking change: v1.0.0 changed default, users must reconfigure
- **Boot scan only:** New sensors not detected until reboot
  - Mitigation: Could add REST API endpoint to trigger rescan
- **Address format change:** v1.0.0 fixed buggy format
  - Breaking change: Old Home Assistant automations need update
  - Mitigation: Documentation explains migration
- **Polling interval:** Not real-time (minimum 750ms per sensor read)
  - Accepted: 30-second updates sufficient for temperature monitoring
- **Bus limitation:** All sensors share one bus (crosstalk possible)
  - Mitigation: Keep cables short, use proper termination

### Risks & Mitigation
- **Sensor failure:** Sensor stops responding
  - **Mitigation:** Check CRC on every read, skip failed sensors
  - **Mitigation:** Log errors to telnet debug
- **Address conflicts:** Two sensors with same address
  - **Extremely rare:** 64-bit addresses make collision unlikely
  - **Mitigation:** Factory-programmed unique addresses
- **Cable too long:** Signal degradation at distance
  - **Mitigation:** Use lower pull-up resistor (2.2kΩ instead of 4.7kΩ)
  - **Mitigation:** Active pull-up for runs >50m
- **Incorrect GPIO:** User configures wrong pin
  - **Mitigation:** Web UI validates GPIO range
  - **Mitigation:** Default to known-good GPIO 10

## Implementation Details

**OneWire initialization:**
```cpp
#include <OneWire.h>
#include <DallasTemperature.h>

// Global instances
OneWire oneWire(settingGPIOSENSORSpin);  // Configurable GPIO
DallasTemperature dallasSensors(&oneWire);

void initSensors() {
  if (settingGPIOSENSORSpin == 0) {
    return;  // Sensors disabled
  }
  
  // Initialize OneWire
  dallasSensors.begin();
  
  // Discover sensors
  nrSensors = dallasSensors.getDeviceCount();
  DebugTf(PSTR("Found %d DS18B20 sensors\r\n"), nrSensors);
  
  // Limit to max supported
  if (nrSensors > 16) {
    nrSensors = 16;
  }
  
  // Read addresses
  for (int i = 0; i < nrSensors; i++) {
    DeviceAddress addr;
    if (dallasSensors.getAddress(addr, i)) {
      // Store address
      memcpy(sensorAddr[i], addr, 8);
      
      // Convert to hex string (16 chars)
      addressToHex(addr, sensorAddrStr[i]);
      
      DebugTf(PSTR("Sensor %d: %s\r\n"), i, sensorAddrStr[i]);
    }
  }
  
  // Save to LittleFS
  saveSensorConfig();
}
```

**Address format (v1.0.0 fix):**
```cpp
// BEFORE (buggy): 9-char compressed format
// Example: "28FF1234"
void addressToHexOld(DeviceAddress addr, char* str) {
  sprintf(str, "%02X%02X%02X%02X", 
    addr[0], addr[6], addr[5], addr[4]);
  // Missing bytes! Incompatible!
}

// AFTER (correct): 16-char standard format
// Example: "28FF64191601F4A1"
void addressToHex(DeviceAddress addr, char* str) {
  for (int i = 0; i < 8; i++) {
    sprintf(str + (i * 2), "%02X", addr[i]);
  }
  str[16] = '\0';
}
```

**Temperature reading:**
```cpp
DECLARE_TIMER_SEC(sensorTimer, 30);  // Every 30 seconds

void handleSensors() {
  if (!DUE(sensorTimer)) return;
  
  if (nrSensors == 0) return;
  
  // Request temperatures (async start)
  dallasSensors.requestTemperatures();
  
  // Wait for conversion (750ms for 12-bit resolution)
  delay(800);
  
  // Read all sensors
  for (int i = 0; i < nrSensors; i++) {
    float tempC = dallasSensors.getTempC(sensorAddr[i]);
    
    // Validate reading
    if (tempC == DEVICE_DISCONNECTED_C) {
      DebugTf(PSTR("Sensor %d: disconnected\r\n"), i);
      continue;
    }
    
    // Store value
    sensorVal[i] = tempC;
    
    // Publish to MQTT
    publishSensorMQTT(i, tempC);
  }
}
```

**MQTT integration (fake message ID 246):**
```cpp
void publishSensorMQTT(int sensorIndex, float tempC) {
  char topic[128];
  char payload[16];
  
  // Topic: otgw-firmware/value/<node-id>/sensor_<address>
  snprintf_P(topic, sizeof(topic),
    PSTR("%s/value/%s/sensor_%s"),
    settingMqttTopTopic,
    settingMqttUniqueID,
    sensorAddrStr[sensorIndex]);
  
  // Payload: temperature as string
  dtostrf(tempC, 5, 2, payload);
  
  mqttClient.publish(topic, payload, true);  // retain
  
  // Also publish via fake OT message ID 246
  publishOTMessage(246 + sensorIndex, tempC);
}
```

**Home Assistant Auto-Discovery:**
```cpp
void publishSensorDiscovery(int sensorIndex) {
  DynamicJsonDocument doc(1024);
  
  char configTopic[150];
  snprintf_P(configTopic, sizeof(configTopic),
    PSTR("homeassistant/sensor/%s_sensor_%d/config"),
    settingMqttUniqueID, sensorIndex);
  
  doc["name"] = "Sensor " + String(sensorIndex);
  doc["state_topic"] = /* ... */;
  doc["unit_of_measurement"] = "°C";
  doc["device_class"] = "temperature";
  doc["unique_id"] = settingMqttUniqueID + "_sensor_" + String(sensorIndex);
  
  // Device info
  JsonObject device = doc.createNestedObject("device");
  device["identifiers"] = settingMqttUniqueID;
  device["name"] = "OpenTherm Gateway";
  
  String payload;
  serializeJson(doc, payload);
  
  mqttClient.publish(configTopic, payload.c_str(), true);
}
```

**Persistence (sensors.json):**
```json
{
  "sensors": [
    {
      "address": "28FF64191601F4A1",
      "name": "Living Room"
    },
    {
      "address": "28FF64191601F4B2",
      "name": "Outdoor"
    }
  ]
}
```

## Breaking Changes (v1.0.0)

**1. GPIO pin default changed:**
- **Old:** GPIO 13 (D7)
- **New:** GPIO 10 (SD3)
- **Impact:** Users with sensors on GPIO 13 must update setting or reconnect

**2. Address format fixed:**
- **Old:** 9-character compressed hex (buggy)
- **New:** 16-character standard hex
- **Impact:** Home Assistant automations using old addresses break
- **Migration:** Re-scan sensors, update HA automations

**3. Discovery message ID:**
- **Old:** Not documented
- **New:** Message ID 246 + sensor index

## Hardware Wiring

**Typical setup:**
```
ESP8266 GPIO 10 (SD3) ────┬──── VCC (3.3V)
                          │
                       4.7kΩ pull-up
                          │
     ┌────────────────────┴──────────────┐
     │                                    │
  DS18B20 #1                          DS18B20 #2
  (Data pin)                          (Data pin)
     │                                    │
     └─────────── GND ────────────────────┘
```

**Cable runs (Cat5):**
- Pin 1 (Orange): GND
- Pin 2 (Orange/White): Data
- Pin 3 (Green): VCC (3.3V)
- Twisted pairs reduce noise

## Related Decisions
- ADR-006: MQTT Integration Pattern (sensor publishing)
- ADR-008: LittleFS for Configuration Persistence (sensors.json storage)

## References
- Implementation: `sensors_ext.ino`
- OneWire library: https://github.com/PaulStoffregen/OneWire
- DallasTemperature library: https://github.com/milesburton/Arduino-Temperature-Control-Library
- DS18B20 datasheet: https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf
- Wiring diagram: `docs/wemosd1mini-pinout-ds18b20.png`
- Migration notes: README.md (v1.0.0 breaking changes)
