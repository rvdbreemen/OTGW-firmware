# ADR-018: ArduinoJson for Data Interchange

**Status:** Accepted  
**Date:** 2018-01-01 (Estimated)  
**Updated:** 2026-01-28 (Documentation)

## Context

The OTGW-firmware exchanges structured data across multiple interfaces:
- **Settings persistence:** Configuration stored in LittleFS
- **REST API:** JSON responses for all endpoints
- **MQTT:** Home Assistant Auto-Discovery configs, sensor values
- **WebSocket:** Structured messages (not just raw text)
- **Debug output:** Structured data for diagnostics

**Requirements:**
- Serialize/deserialize structured data
- Memory-efficient for ESP8266 constraints
- Handle nested structures
- Type-safe access
- Standards-compliant JSON (RFC 8259)
- Fast parsing and generation
- Error handling for malformed data

## Decision

**Use ArduinoJson library for all structured data interchange.**

**Key characteristics:**
- **Library:** ArduinoJson 6.17.2+
- **Buffer size:** 1536 bytes (DynamicJsonDocument)
- **Usage:** Settings files, REST API responses, MQTT payloads
- **Memory:** Pre-allocated buffers to avoid fragmentation
- **Error handling:** Check `DeserializationError` on parse

**Typical usage:**
```cpp
DynamicJsonDocument doc(1536);  // 1536-byte buffer

// Serialize
doc["temperature"] = 20.5;
doc["status"] = "heating";
serializeJson(doc, httpServer);

// Deserialize
DeserializationError error = deserializeJson(doc, file);
if (!error) {
  float temp = doc["temperature"];
}
```

## Alternatives Considered

### Alternative 1: Manual String Concatenation
**Pros:**
- No library dependency
- Full control
- Zero overhead

**Cons:**
- Error-prone (missing commas, quotes, escaping)
- Hard to maintain
- No type safety
- Buffer overflow risks
- Cannot parse incoming JSON

**Why not chosen:** Too error-prone for complex structures. JSON generation is tedious and fragile.

### Alternative 2: sprintf/snprintf for JSON Generation
**Pros:**
- Lightweight
- No library needed
- Format strings familiar

**Cons:**
- Still error-prone (syntax errors)
- No escaping (quotes, backslashes)
- No parsing capability
- Hard to nest structures
- No type safety

**Why not chosen:** Cannot handle escaping correctly. Parsing requires separate solution.

### Alternative 3: cJSON Library
**Pros:**
- Lightweight C library
- Simple API
- Widely used

**Cons:**
- Dynamic allocation (every node malloc'd)
- Heap fragmentation on ESP8266
- More verbose than ArduinoJson
- Less Arduino ecosystem integration

**Why not chosen:** Dynamic allocation conflicts with static buffer strategy (ADR-004).

### Alternative 4: PicoJSON
**Pros:**
- Header-only (no linking)
- C++ STL integration
- Modern C++

**Cons:**
- Uses std::string (heap allocation)
- Larger code size
- STL overhead on ESP8266
- Less Arduino-friendly

**Why not chosen:** STL usage adds overhead and heap allocation on ESP8266.

### Alternative 5: CBOR or MessagePack (Binary Formats)
**Pros:**
- More compact than JSON
- Faster parsing
- Binary safety

**Cons:**
- Not human-readable
- No browser support (REST API)
- MQTT convention is JSON
- Home Assistant expects JSON
- More complex debugging

**Why not chosen:** JSON is standard for MQTT, REST APIs, and Home Assistant. Human-readability is valuable.

## Consequences

### Positive
- **Type-safe:** Compile-time type checking for data access
- **Memory-safe:** No buffer overflows with proper size checks
- **Error handling:** `DeserializationError` catches parse errors
- **Nested structures:** Easy to create complex JSON
- **Arduino integration:** Designed for Arduino constraints
- **Streaming:** Can serialize directly to Stream (file, HTTP, serial)
- **Standards compliant:** Generates valid JSON per RFC 8259
- **Escaping:** Automatic escaping of special characters

### Negative
- **Memory overhead:** ~5KB flash for library code
  - Accepted: Essential for JSON handling
- **Buffer sizing:** Must estimate maximum JSON size
  - Mitigation: 1536 bytes chosen based on largest expected JSON
- **Dynamic allocation:** DynamicJsonDocument uses heap
  - Mitigation: Pre-allocate, reuse, keep sizes bounded
- **Learning curve:** API requires learning
  - Mitigation: Extensive examples in codebase

### Risks & Mitigation
- **Buffer overflow:** JSON larger than buffer
  - **Mitigation:** Check `doc.overflowed()` after serialization
  - **Mitigation:** Size buffer for maximum expected JSON
- **Heap fragmentation:** DynamicJsonDocument allocates on heap
  - **Mitigation:** Reuse same document, don't create/destroy frequently
  - **Mitigation:** Keep buffer sizes constant
- **Parse errors:** Malformed JSON crashes or hangs
  - **Mitigation:** Always check `DeserializationError`
  - **Mitigation:** Timeout on JSON parsing
- **Number precision:** Floats may lose precision
  - **Accepted:** 32-bit float precision sufficient for sensor values

## Implementation Patterns

**Settings file (read):**
```cpp
void readSettings() {
  File file = LittleFS.open("/settings.json", "r");
  if (!file) return;
  
  DynamicJsonDocument doc(1536);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    DebugTf(PSTR("JSON parse error: %s\r\n"), error.c_str());
    return;
  }
  
  // Read values with defaults
  strlcpy(settingHostname, 
    doc["Hostname"] | "OTGW", 
    sizeof(settingHostname));
  
  settingMqttPort = doc["MQTTport"] | 1883;
  settingNTPenable = doc["NTPenable"] | true;
}
```

**Settings file (write):**
```cpp
void writeSettings() {
  DynamicJsonDocument doc(1536);
  
  // Build JSON
  doc["Hostname"] = settingHostname;
  doc["MQTTbroker"] = settingMqttBroker;
  doc["MQTTport"] = settingMqttPort;
  doc["NTPenable"] = settingNTPenable;
  doc["NTPtimezone"] = settingNTPtimezone;
  // ... 30+ more settings
  
  File file = LittleFS.open("/settings.json", "w");
  if (!file) {
    DebugTln(F("Failed to open settings for writing"));
    return;
  }
  
  serializeJson(doc, file);
  file.close();
  
  DebugTln(F("Settings saved"));
}
```

**REST API response:**
```cpp
void handleAPIStatus() {
  DynamicJsonDocument doc(1536);
  
  doc["uptime"] = millis() / 1000;
  doc["heap_free"] = ESP.getFreeHeap();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["mqtt_connected"] = mqttClient.connected();
  
  // Nested object
  JsonObject otgw = doc.createNestedObject("otgw");
  otgw["flame"] = OTdata.flame;
  otgw["ch_active"] = OTdata.CHmode;
  otgw["dhw_active"] = OTdata.DHWmode;
  
  // Nested array
  JsonArray sensors = doc.createNestedArray("sensors");
  for (int i = 0; i < nrSensors; i++) {
    JsonObject sensor = sensors.createNestedObject();
    sensor["id"] = i;
    sensor["address"] = sensorAddr[i];
    sensor["value"] = sensorVal[i];
  }
  
  // Send JSON response
  String response;
  serializeJson(doc, response);
  httpServer.send(200, F("application/json"), response);
}
```

**MQTT Auto-Discovery config:**
```cpp
void publishAutoDiscovery() {
  DynamicJsonDocument doc(1536);
  
  // Sensor config
  doc["name"] = "Boiler Temperature";
  doc["state_topic"] = "otgw-firmware/value/123456/boiler_temp";
  doc["unit_of_measurement"] = "°C";
  doc["device_class"] = "temperature";
  doc["unique_id"] = "otgw_123456_boiler_temp";
  
  // Device info
  JsonObject device = doc.createNestedObject("device");
  device["identifiers"] = "otgw_123456";
  device["name"] = "OpenTherm Gateway";
  device["model"] = "OTGW ESP8266";
  device["manufacturer"] = "NodoShop";
  
  // Publish
  String payload;
  serializeJson(doc, payload);
  
  mqttClient.publish(
    "homeassistant/sensor/otgw_123456_boiler_temp/config",
    payload.c_str(),
    true  // retain
  );
}
```

**WebSocket structured message:**
```cpp
void sendWebSocketStatus() {
  DynamicJsonDocument doc(512);
  
  doc["type"] = "status";
  doc["flame"] = OTdata.flame;
  doc["ch_mode"] = OTdata.CHmode;
  doc["dhw_mode"] = OTdata.DHWmode;
  doc["boiler_temp"] = OTdata.Tboiler;
  
  String json;
  serializeJson(doc, json);
  
  webSocket.broadcastTXT(json);
}
```

**Filtering (reduce size):**
```cpp
// Only send specific fields
DynamicJsonDocument doc(1536);
// ... populate doc ...

DynamicJsonDocument filtered(512);
filtered["temperature"] = doc["temperature"];
filtered["status"] = doc["status"];

serializeJson(filtered, httpServer);
```

**Measuring size:**
```cpp
DynamicJsonDocument doc(1536);
// ... populate doc ...

size_t size = measureJson(doc);
DebugTf(PSTR("JSON size: %d bytes\r\n"), size);

if (doc.overflowed()) {
  DebugTln(F("JSON buffer overflow!"));
}
```

## Buffer Sizing Strategy

**Common buffer sizes:**
```cpp
// Settings file: 1536 bytes (all settings)
DynamicJsonDocument settingsDoc(1536);

// REST API: 1536 bytes (full status response)
DynamicJsonDocument apiDoc(1536);

// MQTT discovery: 1024 bytes (single entity config)
DynamicJsonDocument mqttDoc(1024);

// WebSocket message: 512 bytes (status update)
DynamicJsonDocument wsDoc(512);

// Small responses: 256 bytes
DynamicJsonDocument smallDoc(256);
```

**How buffer size was determined:**
1. Measure typical JSON size with `measureJson()`
2. Add 20-30% headroom for growth
3. Round up to convenient size (256, 512, 1024, 1536)
4. Test with maximum expected data
5. Validate with `doc.overflowed()`

## Error Handling Best Practices

**Always check errors:**
```cpp
// GOOD - Check for errors
DeserializationError error = deserializeJson(doc, file);
if (error) {
  DebugTf(PSTR("Parse failed: %s\r\n"), error.c_str());
  return;
}

// BAD - No error checking
deserializeJson(doc, file);
float temp = doc["temperature"];  // May be null if parse failed
```

**Check for overflow:**
```cpp
// After serialization
serializeJson(doc, response);

if (doc.overflowed()) {
  DebugTln(F("JSON too large for buffer"));
  // Increase buffer size or reduce data
}
```

**Validate data types:**
```cpp
// Check if field exists and has correct type
if (doc.containsKey("temperature") && doc["temperature"].is<float>()) {
  float temp = doc["temperature"];
} else {
  DebugTln(F("Invalid temperature value"));
}
```

## Related Decisions
- ADR-008: LittleFS for Configuration Persistence (JSON settings files)
- ADR-004: Static Buffer Allocation Strategy (fixed JSON buffer sizes)
- ADR-006: MQTT Integration Pattern (JSON payloads for Auto-Discovery)

## References
- ArduinoJson: https://arduinojson.org/
- ArduinoJson documentation: https://arduinojson.org/v6/doc/
- Memory usage: https://arduinojson.org/v6/assistant/
- Implementation: `settingStuff.ino`, `restAPI.ino`, `MQTTstuff.ino`, `jsonStuff.ino`
- JSON RFC 8259: https://tools.ietf.org/html/rfc8259
