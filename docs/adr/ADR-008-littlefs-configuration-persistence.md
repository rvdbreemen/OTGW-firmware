# ADR-008: LittleFS for Configuration Persistence

**Status:** Accepted  
**Date:** 2020-06-01 (Migration from SPIFFS)  
**Updated:** 2026-01-28 (Documentation)

## Context

The OTGW-firmware needs to persist user configuration across reboots and firmware updates:
- WiFi credentials
- MQTT broker settings
- Device hostname and unique ID
- Sensor GPIO assignments
- Temperature offsets and calibration
- Boot commands
- NTP server configuration

**Requirements:**
- Survive firmware updates (OTA)
- Survive ESP8266 resets/power loss
- Minimize flash wear (limited write cycles)
- Fast read/write operations
- Support files for Web UI (HTML, CSS, JS)
- Human-readable format for debugging
- Backup and restore capability

**Historical context:**
- Originally used SPIFFS (Serial Peripheral Interface Flash File System)
- SPIFFS deprecated in ESP8266 Arduino Core 2.7.0+
- Migration required to continue supporting new core versions

## Decision

**Use LittleFS as the filesystem for configuration and web UI files.**

**Configuration approach:**
- **Format:** JSON files (ArduinoJson library)
- **Primary config:** `settings.json` (all user settings)
- **Sensor config:** `sensors.json` (Dallas sensor addresses)
- **Web UI:** HTML/CSS/JS files in filesystem
- **Partition:** 2MB dedicated filesystem partition (out of 4MB flash)

**Settings structure:**
```json
{
  "Hostname": "OTGW-123456",
  "MQTTbroker": "192.168.1.100",
  "MQTTport": 1883,
  "MQTTUser": "homeassistant",
  "MQTTpasswd": "secret",
  "NTPtimezone": "Europe/Amsterdam",
  "GPIOSENSORSpin": 10,
  // ... 30+ more settings
}
```

**Persistence strategy:**
- Read settings on boot
- Write settings only when changed (web UI, REST API)
- Auto-save every 5 minutes if dirty flag set
- Backup settings before firmware update

## Alternatives Considered

### Alternative 1: Continue Using SPIFFS
**Pros:**
- No migration needed
- Existing code works
- Well-tested

**Cons:**
- Deprecated by Espressif
- No longer maintained
- Performance issues (no wear leveling)
- Flash wear problems
- Will not work with future Arduino core versions

**Why not chosen:** SPIFFS is deprecated and will eventually break. Migration is necessary.

### Alternative 2: EEPROM
**Pros:**
- Simple API
- Fast access
- Small overhead

**Cons:**
- Limited size (4KB on ESP8266)
- Not enough space for all settings
- Cannot store web UI files
- Wear leveling manual
- Binary format (not human-readable)

**Why not chosen:** Insufficient size. Cannot store web UI files.

### Alternative 3: External SD Card
**Pros:**
- Large storage (GB)
- Easy backup
- Removable

**Cons:**
- Requires additional hardware
- SD card reader needed
- More GPIO pins used
- Reliability issues (card removal)
- Power consumption
- Cost increase

**Why not chosen:** Requires hardware changes. Not compatible with existing NodeMCU devices.

### Alternative 4: Preferences Library (NVS)
**Pros:**
- ESP32 native API
- Key-value store
- Wear leveling
- Type-safe

**Cons:**
- ESP32 only (not available on ESP8266)
- Not compatible with existing hardware
- Limited to key-value pairs

**Why not chosen:** ESP8266 hardware does not support NVS.

### Alternative 5: Cloud Storage (Firebase, AWS S3)
**Pros:**
- Unlimited storage
- Automatic backup
- Accessible anywhere

**Cons:**
- Requires internet connection
- Privacy concerns
- Dependency on external service
- Latency
- Cost
- Complexity

**Why not chosen:** Local network only deployment model (see ADR-003). Internet dependency unacceptable.

## Consequences

### Positive
- **Modern filesystem:** Active development, bug fixes
- **Wear leveling:** Built-in flash wear management
- **Performance:** Faster than SPIFFS for small files
- **Reliable:** Better crash recovery than SPIFFS
- **Future-proof:** Compatible with latest Arduino cores
- **Large files:** 2MB available for web UI assets
- **Human-readable:** JSON format easy to debug

### Negative
- **Migration required:** One-time migration from SPIFFS
  - Mitigation: Automatic migration code in v0.8.0+
- **JSON overhead:** JSON files larger than binary
  - Accepted: Readability more important than space efficiency
- **ArduinoJson dependency:** Requires JSON library
  - Accepted: Already using ArduinoJson for REST API
- **Flash write limits:** Flash has ~10,000-100,000 write cycles
  - Mitigation: Write only on changes, auto-save throttling

### Risks & Mitigation
- **Filesystem corruption:** Power loss during write could corrupt filesystem
  - **Mitigation:** LittleFS has better crash recovery than SPIFFS
  - **Mitigation:** Backup settings before firmware update
  - **Mitigation:** Factory reset option to recover
- **Settings lost on corruption:** Configuration could be lost
  - **Mitigation:** Export settings feature in web UI
  - **Mitigation:** MQTT topic to publish settings (backup)
- **Flash wear:** Frequent writes could wear out flash
  - **Mitigation:** Auto-save throttled to 5-minute intervals
  - **Mitigation:** Write-on-change only (not every loop)
  - **Mitigation:** Wear leveling in LittleFS

## Implementation Patterns

**Read settings on boot:**
```cpp
void readSettings() {
  if (!LittleFS.begin()) {
    DebugTln(F("LittleFS mount failed"));
    return;
  }
  
  File file = LittleFS.open("/settings.json", "r");
  if (!file) {
    // Use defaults
    return;
  }
  
  DynamicJsonDocument doc(1536);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    DebugTf(PSTR("JSON parse error: %s\r\n"), error.c_str());
    return;
  }
  
  // Extract settings
  strlcpy(settingHostname, doc["Hostname"] | "OTGW", sizeof(settingHostname));
  // ... more settings
}
```

**Write settings on change:**
```cpp
void writeSettings() {
  DynamicJsonDocument doc(1536);
  
  // Build JSON
  doc["Hostname"] = settingHostname;
  doc["MQTTbroker"] = settingMqttBroker;
  // ... more settings
  
  File file = LittleFS.open("/settings.json", "w");
  if (!file) {
    DebugTln(F("Failed to open settings for writing"));
    return;
  }
  
  serializeJson(doc, file);
  file.close();
}
```

**Auto-save throttling:**
```cpp
bool settingsDirty = false;
DECLARE_TIMER_MIN(autoSaveTimer, 5);

// In loop()
if (DUE(autoSaveTimer) && settingsDirty) {
  writeSettings();
  settingsDirty = false;
}

// When settings changed via web UI
void updateSetting() {
  settingHostname = newValue;
  settingsDirty = true;  // Will save in <5 minutes
}
```

**Backup before OTA update:**
```cpp
void handleOTAUpdate() {
  // Backup settings
  writeSettings();
  
  // Perform update
  Update.begin();
  // ...
}
```

## File Structure

**Filesystem layout:**
```
/settings.json          # Main configuration
/sensors.json          # Dallas sensor addresses
/index.html            # Web UI home page
/FSexplorer.html       # File browser
/api.html              # REST API documentation
/css/style.css         # Styling
/js/app.js             # Web UI logic
```

## Migration from SPIFFS

**Automatic migration (v0.8.0):**
```cpp
void migrateFromSPIFFS() {
  if (SPIFFS.begin()) {
    // Copy settings.json from SPIFFS to LittleFS
    File source = SPIFFS.open("/settings.json", "r");
    File dest = LittleFS.open("/settings.json", "w");
    
    while (source.available()) {
      dest.write(source.read());
    }
    
    source.close();
    dest.close();
    SPIFFS.end();
  }
}
```

## Related Decisions
- ADR-002: Modular .ino File Architecture (settingStuff.ino handles persistence)
- ADR-007: Timer-Based Task Scheduling (auto-save timer)

## References
- LittleFS documentation: https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html
- Implementation: `settingStuff.ino`
- Migration code: Git history v0.8.0
- ArduinoJson: https://arduinojson.org/
- Flash specifications: ESP8266 datasheet
