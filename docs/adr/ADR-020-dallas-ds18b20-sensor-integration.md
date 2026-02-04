# ADR-020: Dallas DS18B20 Temperature Sensor Integration

**Status:** Accepted  
**Date:** 2019-01-01 (Estimated - Pre-GitHub)  
**Updated:** 2026-02-04 (Custom labels and graph integration)  
**Breaking Changes:** Planned for v1.0.0 final release (not yet released as of v1.0.0-rc6)

**Note on Dates:** This project's git history was truncated on April 23, 2021. Dates before 2021-04-23 are estimates. Git evidence shows sensor work in commits 9199e43 (2023-01-23) and fcd31a9 (2021-12-20). Custom labels added in 2026-02-04 (commits b2acbd7, 7c3a711).

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

## Breaking Changes (Planned for v1.0.0 Final Release)

**Status:** As of v1.0.0-rc6 (January 2026), these breaking changes are **PLANNED** but NOT YET in a stable release. The changes below describe what **will happen** when v1.0.0 final is released.

**1. GPIO pin default will change:**
- **Current (rc6):** GPIO 13 (D7)
- **Future (v1.0.0):** GPIO 10 (SD3)
- **Impact:** Users upgrading with sensors on GPIO 13 will need to either:
  - Reconnect sensors to GPIO 10 (recommended), OR
  - Update the GPIO setting via Web UI to continue using GPIO 13

**2. Address format will be fixed:**
- **Current (rc6):** 9-character compressed hex (buggy)
- **Future (v1.0.0):** 16-character standard hex
- **Impact:** Home Assistant automations using old addresses will break
- **Migration:** Re-scan sensors after upgrade, update HA automations with new addresses

**3. Discovery message ID will change:**
- **Current (rc6):** Not documented
- **Future (v1.0.0):** Message ID 246 + sensor index

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

## Custom Labels Feature (Added 2026-02-04)

**Context:** Sensor hex addresses (e.g., `28FF64D1841703F1`) are not user-friendly. Users want to assign custom names like "Living Room" or "Outdoor".

**Implementation:**
- **Label storage:** JSON in `settingDallasLabels[512]` field
- **Label structure:** Key-value pairs (hex address → custom label)
- **Max label length:** 16 characters
- **Default label:** Hex address until user customizes
- **Persistence:** Stored in LittleFS via settings.json
- **API endpoint:** `POST /api/v1/sensors/label` for updates
- **UI:** Non-blocking modal dialog for editing (see ADR-029)

**Label storage format:**
```json
{
  "28FF64D1841703F1": "Living Room",
  "28AB34CD561289EF": "Outdoor",
  "2801234567890ABC": "Boiler Room"
}
```

**Structure update:**
```cpp
struct {
  int id;
  DeviceAddress addr;
  float tempC;
  time_t lasttime;
  char label[17];  // Custom label (16 chars + null)
} DallasrealDevice[MAXDALLASDEVICES];
```

**Label management functions:**
```cpp
// Load custom label from settings during sensor init
void loadSensorLabel(const char* hexAddress, char* label, size_t labelSize);

// Save custom label to settings and update structure
void saveSensorLabel(const char* hexAddress, const char* newLabel);
```

**REST API:**
```
POST /api/v1/sensors/label
{
  "address": "28FF64D1841703F1",
  "label": "Living Room"
}

Response:
{
  "success": true,
  "address": "28FF64D1841703F1",
  "label": "Living Room"
}
```

**API data exposure:**
- Labels exposed as `{address}_label` fields in `/api/v1/otgw/otmonitor` and `/api/v2/otgw/otmonitor`
- Example: `28FF64D1841703F1_label: {"value": "Living Room", "unit": ""}`

**Frontend integration:**
- Graph displays custom labels instead of "Sensor 1 (28FF64D1)"
- Main page displays custom labels in sensor name column
- Click sensor name to edit label via modal dialog
- Labels update dynamically without page refresh

## Graph Visualization Feature (Added 2026-02-04)

**Context:** Sensors were only visible via MQTT and REST API. Users wanted real-time graphing in the Web UI.

**Implementation:**
- **Auto-detection:** JavaScript scans API data for 16-char hex addresses starting with 28/10/22
- **Color palette:** 16 unique colors per theme (light/dark)
- **Real-time updates:** Integrated with existing 1-second API polling
- **Temperature grid:** Sensors added to gridIndex 4 (temperature chart)
- **Data validation:** Temperature range -50°C to 150°C
- **Dynamic registration:** New sensors appear automatically without page refresh

**Detection logic:**
```javascript
// Scan API data for Dallas sensor addresses
if (key.length === 16 && 
    /^[0-9A-Fa-f]{16}$/.test(key) &&
    (key.startsWith('28') || key.startsWith('10') || key.startsWith('22'))) {
  // Register sensor for graphing
  registerSensor(key, label);
}
```

**Graph series config:**
```javascript
{
  id: 'sensor_0',
  label: 'Living Room',  // Uses custom label if available
  gridIndex: 4,          // Temperature grid
  type: 'line',
  color: '#FF6B6B'       // Unique color from palette
}
```

**See also:** ADR-029 for non-blocking modal dialog pattern used for label editing.

## Related Decisions
- ADR-006: MQTT Integration Pattern (sensor publishing)
- ADR-008: LittleFS for Configuration Persistence (sensors.json and label storage)
- ADR-018: ArduinoJson for Data Interchange (label JSON storage)
- ADR-019: REST API Versioning Strategy (new endpoint in v1 API)
- ADR-029: Non-Blocking Modal Dialogs (label editing UI)

## References
- Implementation: `sensors_ext.ino` (sensor reading and label management)
- Label API: `restAPI.ino` (POST /api/v1/sensors/label endpoint)
- Graph integration: `data/graph.js` (dynamic sensor detection and graphing)
- UI integration: `data/index.js` (label editing modal)
- OneWire library: https://github.com/PaulStoffregen/OneWire
- DallasTemperature library: https://github.com/milesburton/Arduino-Temperature-Control-Library
- DS18B20 datasheet: https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf
- Wiring diagram: `docs/wemosd1mini-pinout-ds18b20.png`
- Migration notes: README.md (v1.0.0 breaking changes)
