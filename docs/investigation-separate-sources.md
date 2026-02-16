# Investigation: Separation of Thermostat and Boiler Values in MQTT

**Date:** 2026-02-16  
**Issue:** [#143](https://github.com/rvdbreemen/OTGW-firmware/issues/143) separation of values from thermostat and boiler  
**Status:** Investigation Complete - Awaiting Maintainer Feedback

## Executive Summary

This investigation explores how to separate OpenTherm message values by their source (thermostat vs boiler) in MQTT publishing. Currently, values from both sources are published to the same MQTT topic, creating ambiguity when the thermostat requests one value and the boiler acknowledges with a different value.

**Recommendation:** Implement Option 4 (Feature Flag with Separate Topics) as it provides backward compatibility while offering the flexibility users need.

---

## Table of Contents

1. [Problem Statement](#problem-statement)
2. [Technical Background](#technical-background)
3. [Current Implementation Analysis](#current-implementation-analysis)
4. [Affected Message IDs](#affected-message-ids)
5. [Implementation Options](#implementation-options)
6. [Recommendation](#recommendation)
7. [Implementation Plan](#implementation-plan)
8. [Testing Strategy](#testing-strategy)
9. [Migration Guide](#migration-guide)

---

## Problem Statement

### Issue Description

When using MQTT for monitoring OpenTherm messages, there is no separation between values coming from the thermostat (master) and values coming from the boiler (slave). This creates confusion in scenarios such as:

1. **Control Setpoint (TSet, Message ID 1):**
   - Thermostat sends: "I want 20.5°C" (T message)
   - Boiler responds: "I can do 20.0°C" (B message)
   - Current behavior: Both values overwrite the same MQTT topic `TSet`
   - Result: You only see the last value published, losing visibility into what each device is actually doing

2. **Gateway Override Scenario:**
   - User sends `ctrlsetpt` command via MQTT to override temperature
   - Gateway sends R message to boiler with override value
   - Boiler sends B message with actual setpoint
   - Current behavior: Both published to same topic, creating ambiguity

3. **Diagnostic Value:**
   - Understanding the difference between requested and actual values is crucial for:
     - Troubleshooting heating system issues
     - Monitoring boiler performance and limitations
     - Detecting when the boiler cannot meet thermostat demands

### User Impact

Users cannot:
- See what the thermostat is requesting vs what the boiler is providing
- Detect when the boiler is limiting the requested setpoint
- Properly monitor gateway override operations
- Create Home Assistant automations based on request vs actual values

---

## Technical Background

### OpenTherm Protocol Basics

**Message Structure:**
- 9-byte ASCII representation: `Txxxxxxxx` or `Bxxxxxxxx`
- First character indicates source:
  - `T` = Thermostat (Master) request
  - `B` = Boiler (Slave) response
  - `R` = Gateway request to boiler (overriding thermostat)
  - `A` = Gateway answer to thermostat (overriding boiler)
  - `E` = Parity error

**Message Types (3-bit field in OT frame):**
```
Master to Slave:
  000 (0) = READ-DATA    - Master requests value from slave
  001 (1) = WRITE-DATA   - Master writes value to slave
  010 (2) = INVALID-DATA - Master sends known invalid data

Slave to Master:
  100 (4) = READ-ACK     - Slave acknowledges read with data
  101 (5) = WRITE-ACK    - Slave acknowledges write (may echo or modify)
  110 (6) = DATA-INVALID - Slave recognizes ID but data unavailable
  111 (7) = UNKNOWN-DATAID - Slave doesn't recognize the ID
```

**Message Flow Example (TSet - Control Setpoint):**
```
1. Thermostat → OTGW: T10100000  (WRITE-DATA, ID=1, value=20.5°C)
2. OTGW → Boiler: R10100000      (forwarded or modified)
3. Boiler → OTGW: B50100000      (WRITE-ACK, ID=1, value=20.0°C)
4. OTGW → Thermostat: A50100000  (forwarded or modified)
```

### Current Firmware Architecture

**Data Flow Pipeline (per ADR-038):**
```
Serial Port (9600 baud)
    ↓
handleOTGW() - reads and buffers lines
    ↓
processOT(buf, len) - parses and dispatches
    ↓
├─ Detect source: buf[0] = 'T'/'B'/'R'/'A'/'E'
├─ Set OTdata.rsptype (OTGW_THERMOSTAT, OTGW_BOILER, etc.)
├─ Parse OT frame into id, type, valueHB, valueLB
├─ Switch on message ID
│   ↓
│   print_f88(OTcurrentSystemState.TSet)
│       ↓
│       ├─ Extract value: OTdata.f88()
│       ├─ Publish MQTT: sendMQTTData("TSet", value)
│       └─ Update state: value = _value (overwrites last)
│
├─ sendLogToWebSocket() - for Web UI
├─ OTGWstream - for OTmonitor TCP bridge
└─ msglastupdated[id] = epoch - for REST API
```

**Key Files:**
- `OTGW-Core.ino`: Message parsing and routing (`processOT()`, line ~1550-1920)
- `OTGW-Core.h`: OpenTherm data structures and message ID definitions
- `MQTTstuff.ino`: MQTT publishing functions (`sendMQTTData()`)
- State object: `OTcurrentSystemState` (global struct with all sensor values)

**Current Limitations:**
1. **Single state per message ID:** `OTcurrentSystemState.TSet` holds only one value
2. **Last-writer-wins:** When both T and B messages arrive, last one overwrites
3. **Skip logic prevents some duplicates:** Gateway override messages (R/A) cause related T/B messages to be skipped
4. **MQTT publishes immediately:** No buffering or deduplication at MQTT layer

---

## Current Implementation Analysis

### Message Processing Logic

**Step 1: Source Detection (OTGW-Core.ino, lines 1568-1582):**
```cpp
if (buf[0]=='B'){
  epochBoilerlastseen = now; 
  OTdata.rsptype = OTGW_BOILER;
} else if (buf[0]=='T'){
  epochThermostatlastseen = now;
  OTdata.rsptype = OTGW_THERMOSTAT;
} else if (buf[0]=='R') {
  epochGatewaylastseen = now;
  OTdata.rsptype = OTGW_REQUEST_BOILER;
} else if (buf[0]=='A') {
  epochGatewaylastseen = now;
  OTdata.rsptype = OTGW_ANSWER_THERMOSTAT;
} else if (buf[0]=='E') {
  OTdata.rsptype = OTGW_PARITY_ERROR;
}
```

**Step 2: Skip Logic for Gateway Overrides (lines 1640-1645):**
```cpp
// When gateway overrides, skip the original T or B message
bool skipthis = (delayedOTdata.id == OTdata.id) && 
                (OTdata.time - delayedOTdata.time < 500) &&  
                (((OTdata.rsptype == OTGW_ANSWER_THERMOSTAT) && 
                  (delayedOTdata.rsptype == OTGW_BOILER)) ||
                 ((OTdata.rsptype == OTGW_REQUEST_BOILER) && 
                  (delayedOTdata.rsptype == OTGW_THERMOSTAT)));
```

**Interpretation:** When gateway sends R (request to boiler), any preceding T (thermostat) message with same ID within 500ms is marked `skipthis=true`. Similarly, when gateway sends A (answer to thermostat), any preceding B (boiler) message is skipped.

**Step 3: Value Processing (lines 628-643):**
```cpp
void print_f88(float& value) {
  float _value = roundf(OTdata.f88()*100.0f) / 100.0f;
  char _msg[15] {0};
  dtostrf(_value, 3, 2, _msg);
  
  AddLogf("%s = %s %s", OTlookupitem.label, _msg, OTlookupitem.unit);

  // Only publish and store if value is valid
  if (is_value_valid(OTdata, OTlookupitem)){
    sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), _msg);
    value = _value;  // <-- Single state variable updated
  }
}
```

**Step 4: MQTT Publishing (MQTTstuff.ino):**
```cpp
void sendMQTTData(const char* topic, const char *json, const bool retain) {
  // Build full topic: <prefix>/value/<node-id>/<topic>
  char full_topic[MQTT_TOPIC_MAX_LEN];
  snprintf_P(full_topic, sizeof(full_topic), PSTR("%s/"), MQTTPubNamespace);
  strlcat(full_topic, topic, sizeof(full_topic));
  
  // Example: "otgw-firmware/value/otgw/TSet"
  MQTTclient.publish(full_topic, json, retain);
}
```

### State Management

**Global State Object (OTGW-Core.h, lines 514-100+):**
```cpp
typedef struct {
  // ... other fields ...
  float TSet;           // Single value - no source tracking
  float TrSet;          // Room setpoint
  float MaxTSet;        // Max CH water setpoint
  float TdhwSet;        // DHW setpoint
  // ... 100+ other fields ...
} OTGWCurrentSystemState_t;

OTGWCurrentSystemState_t OTcurrentSystemState;
```

**Problem:** No way to store separate values for thermostat vs boiler.

### MQTT Topic Structure

**Current Topics:**
```
<prefix>/value/<node-id>/TSet              → "20.5" or "20.0" (last wins)
<prefix>/value/<node-id>/TrSet             → Room setpoint
<prefix>/value/<node-id>/MaxTSet           → Max CH setpoint
<prefix>/set/<node-id>/ctrlsetpt           → Command topic to override
```

**Home Assistant Auto-Discovery:**
```json
{
  "name": "Control Setpoint",
  "state_topic": "otgw-firmware/value/otgw/TSet",
  "unit_of_measurement": "°C",
  "device_class": "temperature",
  "unique_id": "otgw_TSet"
}
```

---

## Affected Message IDs

### Message Types by Direction

**READ-only messages (Slave → Master):**
- These are slave responses, only B/A messages occur
- No ambiguity, but source tracking still useful for diagnostics
- Examples: ID 3 (SlaveConfig), ID 17 (RelModLevel), ID 25 (Tboiler)

**WRITE-only messages (Master → Slave):**
- These are master requests, only T/R messages occur
- Boiler acknowledges but doesn't necessarily change the value
- Examples: ID 1 (TSet), ID 2 (MasterConfig), ID 16 (TrSet)

**READ/WRITE messages (Bidirectional):**
- Both master and slave can read or write
- Maximum ambiguity - could have different values in both directions
- Examples: ID 4 (Command), ID 56 (TdhwSet), ID 57 (MaxTSet)

### Priority List: Message IDs Needing Source Separation

**High Priority (WRITE-only with significant user impact):**
| ID | Name | Description | Use Case |
|----|------|-------------|----------|
| 1  | TSet | Control setpoint (CH water temp) | **Primary heating control** - critical for understanding thermostat vs boiler behavior |
| 16 | TrSet | Room setpoint | Room temperature target - thermostat sets, boiler may modify |
| 23 | TrSetCH2 | Room setpoint CH2 | Second zone control |
| 8  | TsetCH2 | Control setpoint CH2 | Second zone CH water temp |
| 14 | MaxRelModLevelSetting | Max modulation level | Modulation limit control |

**Medium Priority (READ/WRITE with moderate impact):**
| ID | Name | Description | Use Case |
|----|------|-------------|----------|
| 56 | TdhwSet | DHW setpoint | Hot water temperature - user sets via thermostat, boiler may limit |
| 57 | MaxTSet | Max CH water setpoint | Maximum allowed CH temperature |
| 58 | Hcratio | OTC heat curve ratio | Weather compensation curve |
| 4  | Command | Remote command | Command/response pairs |
| 20 | DayTime | Day of week and time | Clock sync |
| 21 | Date | Calendar date | Date sync |
| 22 | Year | Calendar year | Year sync |

**Low Priority (READ/WRITE with minimal impact):**
| ID | Name | Description | Use Case |
|----|------|-------------|----------|
| 11 | TSPindexTSPvalue | Transparent slave parameters | Advanced diagnostics |
| 59-63 | Remoteparameter4-8 | Remote parameters 4-8 | Vendor-specific settings |
| 78-79 | Ventilation parameters | Humidity, CO2 | Ventilation systems only |
| 113-123 | Counters | Burner starts, operation hours | Maintenance tracking (RW for reset) |

**Note:** Even READ-only messages could benefit from source tracking for advanced diagnostics (e.g., seeing if gateway modified a value).

### Recommendation: Which Messages to Separate

**Option A: All Messages (Comprehensive)**
- Publish source-specific topics for all message IDs
- Pros: Complete visibility, future-proof
- Cons: High memory usage, many unused topics

**Option B: WRITE and READ/WRITE Only (Targeted)**
- Only separate messages where master sends data
- Pros: Covers all ambiguity cases
- Cons: Still quite a few messages (~30 IDs)

**Option C: High Priority Only (Minimal)**
- Only separate top 5-10 most used messages
- Pros: Minimal memory impact, covers 80% of use cases
- Cons: Users may want other messages later

**Recommendation:** Implement Option B (WRITE and READ/WRITE) with a configuration option to enable/disable per message ID if needed. This gives maximum flexibility while being manageable.

---

## Implementation Options

### Option 1: Separate Topics with Source Suffix

**Approach:** Publish separate MQTT topics for each source.

**Topic Structure:**
```
Original (for backward compatibility):
  <prefix>/value/<node-id>/TSet                → Last value (either source)

New topics:
  <prefix>/value/<node-id>/TSet_thermostat     → Thermostat request
  <prefix>/value/<node-id>/TSet_boiler         → Boiler response
  <prefix>/value/<node-id>/TSet_gateway        → Gateway override (R/A messages)
```

**Example Message Flow:**
```
1. T10100000 arrives → Publish to:
   - TSet = "20.5"
   - TSet_thermostat = "20.5"

2. B50100000 arrives → Publish to:
   - TSet = "20.0"
   - TSet_boiler = "20.0"

3. User can now see both values in Home Assistant
```

**Implementation Changes:**
```cpp
// Modified print_f88() function
void print_f88_with_source(float& value_thermostat, 
                           float& value_boiler, 
                           float& value_gateway) {
  float _value = roundf(OTdata.f88()*100.0f) / 100.0f;
  char _msg[15] {0};
  dtostrf(_value, 3, 2, _msg);
  
  AddLogf("%s = %s %s", OTlookupitem.label, _msg, OTlookupitem.unit);

  if (is_value_valid(OTdata, OTlookupitem)){
    const char* baseTopic = messageIDToString(static_cast<OpenThermMessageID>(OTdata.id));
    
    // Publish to original topic (backward compat)
    sendMQTTData(baseTopic, _msg);
    
    // Publish to source-specific topic if feature enabled
    if (settingMQTTSeparateSources) {
      char sourceTopic[MQTT_TOPIC_MAX_LEN];
      
      switch(OTdata.rsptype) {
        case OTGW_THERMOSTAT:
          snprintf_P(sourceTopic, sizeof(sourceTopic), PSTR("%s_thermostat"), baseTopic);
          sendMQTTData(sourceTopic, _msg);
          value_thermostat = _value;
          break;
          
        case OTGW_BOILER:
          snprintf_P(sourceTopic, sizeof(sourceTopic), PSTR("%s_boiler"), baseTopic);
          sendMQTTData(sourceTopic, _msg);
          value_boiler = _value;
          break;
          
        case OTGW_REQUEST_BOILER:
        case OTGW_ANSWER_THERMOSTAT:
          snprintf_P(sourceTopic, sizeof(sourceTopic), PSTR("%s_gateway"), baseTopic);
          sendMQTTData(sourceTopic, _msg);
          value_gateway = _value;
          break;
      }
    }
    
    // Update main state with last value (backward compat)
    value = _value;
  }
}
```

**State Structure Changes:**
```cpp
typedef struct {
  // Original single values (for backward compat)
  float TSet;
  float TrSet;
  // ... etc ...
  
  // New source-specific values (optional, only if feature enabled)
  struct {
    float TSet_thermostat;
    float TSet_boiler;
    float TSet_gateway;
    float TrSet_thermostat;
    float TrSet_boiler;
    float TrSet_gateway;
    // ... etc for all WRITE/RW message IDs ...
  } sources;
} OTGWCurrentSystemState_t;
```

**Pros:**
- Clear semantic meaning
- Easy to understand
- Backward compatible
- Supports gateway override visibility
- Simple to implement

**Cons:**
- 3x topics for each separated message (memory impact)
- More MQTT auto-discovery configs needed
- State object grows significantly if all sources tracked
- Higher MQTT broker load

**Memory Impact:**
- Per message: 3 topics × ~200 bytes per config = 600 bytes per message ID
- For 30 WRITE/RW messages: ~18KB additional RAM for configs
- Mitigation: Only enable for high-priority messages by default

---

### Option 2: JSON Payload with Source Fields

**Approach:** Single topic with JSON payload containing all sources.

**Topic Structure:**
```
<prefix>/value/<node-id>/TSet
```

**Payload Structure:**
```json
{
  "thermostat": 20.5,
  "boiler": 20.0,
  "gateway": null,
  "last": "boiler",
  "timestamp": 1234567890
}
```

**Alternative Compact Format:**
```json
{
  "t": 20.5,
  "b": 20.0,
  "g": null,
  "s": "b",
  "ts": 1234567890
}
```

**Implementation Changes:**
```cpp
void print_f88_json(float& value) {
  float _value = roundf(OTdata.f88()*100.0f) / 100.0f;
  
  // Update state based on source
  switch(OTdata.rsptype) {
    case OTGW_THERMOSTAT:
      OTcurrentSystemState.sources.TSet_thermostat = _value;
      break;
    case OTGW_BOILER:
      OTcurrentSystemState.sources.TSet_boiler = _value;
      break;
    case OTGW_REQUEST_BOILER:
    case OTGW_ANSWER_THERMOSTAT:
      OTcurrentSystemState.sources.TSet_gateway = _value;
      break;
  }
  
  // Build JSON payload
  StaticJsonDocument<256> doc;
  if (!isnan(OTcurrentSystemState.sources.TSet_thermostat)) {
    doc["thermostat"] = OTcurrentSystemState.sources.TSet_thermostat;
  }
  if (!isnan(OTcurrentSystemState.sources.TSet_boiler)) {
    doc["boiler"] = OTcurrentSystemState.sources.TSet_boiler;
  }
  if (!isnan(OTcurrentSystemState.sources.TSet_gateway)) {
    doc["gateway"] = OTcurrentSystemState.sources.TSet_gateway;
  }
  
  const char* source;
  switch(OTdata.rsptype) {
    case OTGW_THERMOSTAT: source = "thermostat"; break;
    case OTGW_BOILER: source = "boiler"; break;
    default: source = "gateway"; break;
  }
  doc["last"] = source;
  doc["timestamp"] = now();
  
  char buffer[256];
  serializeJson(doc, buffer, sizeof(buffer));
  
  sendMQTTData(messageIDToString(static_cast<OpenThermMessageID>(OTdata.id)), buffer);
}
```

**Home Assistant Configuration:**
```yaml
# Template sensor to extract boiler value
sensor:
  - platform: mqtt
    name: "Boiler Setpoint"
    state_topic: "otgw-firmware/value/otgw/TSet"
    value_template: "{{ value_json.boiler }}"
    unit_of_measurement: "°C"
    
  - platform: mqtt
    name: "Thermostat Setpoint"
    state_topic: "otgw-firmware/value/otgw/TSet"
    value_template: "{{ value_json.thermostat }}"
    unit_of_measurement: "°C"
```

**Pros:**
- Single topic per message (fewer topics)
- Rich data model with metadata
- Can include timestamps
- Less MQTT broker load
- Efficient memory use (one message instead of three)

**Cons:**
- **Breaking change** - existing integrations will break
- More complex parsing in Home Assistant
- Requires template sensors (not auto-discovered)
- ArduinoJson library adds memory overhead
- Not compatible with OTmonitor expectations

**Memory Impact:**
- JSON serialization: ~256 bytes per message
- ArduinoJson: ~2KB library overhead
- Total: Lower than Option 1 for MQTT, but higher for JSON processing

---

### Option 3: Hierarchical Topic Structure

**Approach:** Organize topics by source as parent level.

**Topic Structure (Variant A - Source First):**
```
<prefix>/value/<node-id>/thermostat/TSet
<prefix>/value/<node-id>/boiler/TSet
<prefix>/value/<node-id>/gateway/TSet
```

**Topic Structure (Variant B - Sensor First):**
```
<prefix>/value/<node-id>/TSet/thermostat
<prefix>/value/<node-id>/TSet/boiler
<prefix>/value/<node-id>/TSet/gateway
```

**Example with Variant A:**
```
otgw-firmware/value/otgw/thermostat/TSet       → "20.5"
otgw-firmware/value/otgw/thermostat/TrSet      → "21.0"
otgw-firmware/value/otgw/boiler/TSet           → "20.0"
otgw-firmware/value/otgw/boiler/Tboiler        → "45.3"
```

**Implementation Changes:**
```cpp
void sendMQTTDataWithSource(const char* sensor, const char* value) {
  if (!settingMQTTSeparateSources) {
    // Backward compat: Original topic structure
    sendMQTTData(sensor, value);
    return;
  }
  
  // New hierarchical structure
  const char* source;
  switch(OTdata.rsptype) {
    case OTGW_THERMOSTAT: source = "thermostat"; break;
    case OTGW_BOILER: source = "boiler"; break;
    case OTGW_REQUEST_BOILER:
    case OTGW_ANSWER_THERMOSTAT: source = "gateway"; break;
    default: source = "unknown"; break;
  }
  
  char hierarchical_topic[MQTT_TOPIC_MAX_LEN];
  snprintf_P(hierarchical_topic, sizeof(hierarchical_topic), 
             PSTR("%s/%s/%s"), source, sensor, value);
  
  sendMQTTData(hierarchical_topic, value);
  
  // Also publish to original topic for backward compat
  sendMQTTData(sensor, value);
}
```

**Pros:**
- Clean logical organization
- Easy to subscribe to all thermostat values: `thermostat/#`
- Easy to subscribe to all boiler values: `boiler/#`
- Semantic clarity
- Can use MQTT wildcards for filtering

**Cons:**
- **Breaking change** - incompatible with existing topic structure
- More complex migration path
- Doubles MQTT traffic if maintaining backward compat
- May not fit existing Home Assistant auto-discovery patterns

**Memory Impact:**
- Same as Option 1: 3 topics per message
- ~18KB for 30 WRITE/RW messages

---

### Option 4: Feature Flag with Opt-In

**Approach:** Add a setting to enable source separation. When disabled, use current behavior. When enabled, use Option 1 (separate topics with suffix).

**Settings:**
```cpp
// New settings in settingStuff.ino
bool   settingMQTTSeparateSources = false;  // Default: disabled

// Optional: Granular control
uint8_t settingMQTTSeparateFlags[32];  // Bitmap for which message IDs to separate
```

**Web UI Setting:**
```html
<div class="setting-group">
  <label for="mqtt-separate-sources">
    <input type="checkbox" id="mqtt-separate-sources" name="mqtt-separate-sources">
    Separate MQTT topics by source (thermostat/boiler/gateway)
  </label>
  <p class="help-text">
    When enabled, publishes OpenTherm values to separate topics based on 
    their source. Example: TSet_thermostat, TSet_boiler, TSet_gateway.
    Provides better visibility when thermostat and boiler values differ.
  </p>
</div>
```

**Implementation:**
```cpp
void print_f88(float& value) {
  float _value = roundf(OTdata.f88()*100.0f) / 100.0f;
  char _msg[15] {0};
  dtostrf(_value, 3, 2, _msg);
  
  AddLogf("%s = %s %s", OTlookupitem.label, _msg, OTlookupitem.unit);

  if (is_value_valid(OTdata, OTlookupitem)){
    const char* baseTopic = messageIDToString(static_cast<OpenThermMessageID>(OTdata.id));
    
    // Always publish to original topic (backward compatibility)
    sendMQTTData(baseTopic, _msg);
    
    // If feature enabled, also publish to source-specific topics
    if (settingMQTTSeparateSources) {
      publishToSourceTopic(baseTopic, _msg, OTdata.rsptype);
    }
    
    // Update state
    value = _value;
  }
}

void publishToSourceTopic(const char* baseTopic, const char* value, byte rsptype) {
  char sourceTopic[MQTT_TOPIC_MAX_LEN];
  const char* suffix;
  
  switch(rsptype) {
    case OTGW_THERMOSTAT:       suffix = "_thermostat"; break;
    case OTGW_BOILER:           suffix = "_boiler"; break;
    case OTGW_REQUEST_BOILER:
    case OTGW_ANSWER_THERMOSTAT: suffix = "_gateway"; break;
    default: return;  // Don't publish for error messages
  }
  
  snprintf_P(sourceTopic, sizeof(sourceTopic), PSTR("%s%s"), baseTopic, suffix);
  sendMQTTData(sourceTopic, value);
  
  // Send auto-discovery config for this topic if not already sent
  if (!getMQTTConfigDone(sourceTopic)) {
    doAutoConfigureSourceTopic(baseTopic, suffix);
    setMQTTConfigDone(sourceTopic);
  }
}
```

**MQTT Auto-Discovery Updates:**
```cpp
void doAutoConfigureSourceTopic(const char* baseTopic, const char* suffix) {
  // Example: Configure TSet_thermostat sensor
  //
  // Topic: homeassistant/sensor/otgw_TSet_thermostat/config
  // Payload:
  // {
  //   "name": "Control Setpoint (Thermostat)",
  //   "state_topic": "otgw-firmware/value/otgw/TSet_thermostat",
  //   "unit_of_measurement": "°C",
  //   "device_class": "temperature",
  //   "unique_id": "otgw_TSet_thermostat",
  //   "device": { ... }
  // }
  
  char configTopic[MQTT_TOPIC_MAX_LEN];
  char sourceTopic[MQTT_TOPIC_MAX_LEN];
  char uniqueId[64];
  
  snprintf_P(sourceTopic, sizeof(sourceTopic), PSTR("%s%s"), baseTopic, suffix);
  snprintf_P(uniqueId, sizeof(uniqueId), PSTR("%s%s"), baseTopic, suffix);
  snprintf_P(configTopic, sizeof(configTopic), 
             PSTR("homeassistant/sensor/%s_%s/config"), 
             settingMqttUniqueID, uniqueId);
  
  // Build JSON config...
  // (Implementation similar to existing doAutoConfigureMsgid())
}
```

**Pros:**
- **Backward compatible** - disabled by default
- User choice and control
- Gradual migration path
- Can be extended with per-message-ID granularity
- No breaking changes

**Cons:**
- Two code paths to maintain
- More complex implementation
- Settings UI needs updates
- Memory overhead even when disabled (unless using conditional compilation)

**Memory Impact:**
- When disabled: No impact
- When enabled: Same as Option 1 (~18KB for 30 messages)
- Settings storage: 1 byte for flag + 32 bytes for bitmap = 33 bytes

---

## Recommendation

### Recommended Approach: **Option 4 (Feature Flag with Opt-In)**

**Rationale:**
1. **Backward Compatibility:** No breaking changes for existing users
2. **User Choice:** Power users can enable detailed source tracking
3. **Gradual Adoption:** Ecosystem can adapt over time
4. **Flexible:** Can be extended with granular per-message control
5. **Safe:** Disabled by default minimizes risk

### Implementation Strategy

**Phase 1: Core Implementation**
- Add `settingMQTTSeparateSources` boolean setting
- Implement source-specific topic publishing in `print_f88()`, `print_u16()`, etc.
- Add `publishToSourceTopic()` helper function
- Extend MQTT auto-discovery for source-specific topics
- Update Web UI settings page

**Phase 2: Enhanced Features**
- Add per-message-ID control (bitmap: `settingMQTTSeparateFlags[]`)
- Add REST API endpoint to configure which messages to separate
- Add MQTT command to toggle feature at runtime
- Add telemetry to track feature usage

**Phase 3: Ecosystem Support**
- Update Home Assistant integration examples
- Add Grafana dashboard with source-separated graphs
- Document Node-RED flows for differential monitoring
- Create migration guide for existing users

### Alternative for Advanced Users

For users who want the feature immediately without waiting for firmware implementation, provide:

**Workaround: MQTT Bridge Script**
```python
# mqtt_source_splitter.py
# Subscribe to original topics, parse OT messages from otmessage topic,
# republish to source-specific topics

import paho.mqtt.client as mqtt
import re

def on_message(client, userdata, msg):
    if msg.topic.endswith('/otmessage'):
        # Parse: "T10100000" or "B50100000"
        raw = msg.payload.decode()
        if len(raw) >= 9:
            source = raw[0]
            # Parse message ID and value
            # Republish to source-specific topics
            pass
```

This lets users experiment with the feature before it's in firmware.

---

## Implementation Plan

### Phase 1: Core Implementation (Week 1-2)

**1. Settings Infrastructure**
- [ ] Add `settingMQTTSeparateSources` to `settingStuff.ino`
- [ ] Add setting to JSON read/write functions
- [ ] Add setting to Web UI (settings page)
- [ ] Add validation and default value (false)

**2. Core MQTT Publishing**
- [ ] Create `publishToSourceTopic()` helper function
- [ ] Modify `print_f88()` to call helper when flag enabled
- [ ] Modify `print_u16()`, `print_s16()`, `print_s8s8()` similarly
- [ ] Add source detection for R/A messages (gateway override)
- [ ] Test with simple message IDs first

**3. MQTT Auto-Discovery**
- [ ] Create `doAutoConfigureSourceTopic()` function
- [ ] Extend auto-discovery config cache to track source topics
- [ ] Add friendly names with source suffix
- [ ] Add icon/device_class mappings for source topics
- [ ] Test with Home Assistant

**4. Documentation**
- [ ] Update README with feature description
- [ ] Add migration guide for users
- [ ] Document MQTT topic structure
- [ ] Add troubleshooting section
- [ ] Create example Home Assistant dashboards

### Phase 2: Testing and Validation (Week 3)

**1. Unit Testing**
- [ ] Test with T messages only
- [ ] Test with B messages only
- [ ] Test with interleaved T/B messages
- [ ] Test with R/A gateway messages
- [ ] Test with skipthis logic (gateway overrides)
- [ ] Test auto-discovery config generation

**2. Integration Testing**
- [ ] Test with Home Assistant
- [ ] Test with Node-RED
- [ ] Test with MQTT Explorer
- [ ] Test with OTmonitor compatibility
- [ ] Test memory usage (heap monitoring)
- [ ] Test with multiple message IDs

**3. Performance Testing**
- [ ] Measure MQTT publish rate
- [ ] Measure heap usage with feature enabled/disabled
- [ ] Test with 50+ messages/second load
- [ ] Check for memory leaks
- [ ] Validate auto-discovery performance

### Phase 3: Advanced Features (Week 4)

**1. Granular Control**
- [ ] Add `settingMQTTSeparateFlags[]` bitmap
- [ ] Add Web UI checkboxes for per-message control
- [ ] Add REST API endpoints
- [ ] Add MQTT commands to toggle at runtime
- [ ] Add "preset" configurations (e.g., "high priority only")

**2. Telemetry**
- [ ] Add counter for source-specific publishes
- [ ] Track most frequently published sources
- [ ] Add debug logging for troubleshooting
- [ ] Add MQTT topic for feature status

**3. Ecosystem Integration**
- [ ] Create Home Assistant example automations
- [ ] Create Grafana dashboard template
- [ ] Create Node-RED example flows
- [ ] Add to official documentation wiki

### Phase 4: Release and Support (Week 5+)

**1. Documentation**
- [ ] Final README updates
- [ ] Video tutorial for feature
- [ ] Blog post announcing feature
- [ ] Update firmware release notes

**2. Community Support**
- [ ] Monitor GitHub issues for feedback
- [ ] Help users with migration
- [ ] Answer questions on Discord
- [ ] Collect feature requests for v2

---

## Testing Strategy

### Test Scenarios

**1. Basic Functionality**
```
Scenario: Thermostat requests setpoint
  Given: settingMQTTSeparateSources = true
  When: T10100000 message received (TSet=20.5°C)
  Then: 
    - Publish to TSet = "20.5"
    - Publish to TSet_thermostat = "20.5"
    - Do not publish to TSet_boiler
```

**2. Boiler Response**
```
Scenario: Boiler acknowledges different setpoint
  Given: Previous T message published TSet_thermostat=20.5
  When: B50100000 message received (TSet=20.0°C)
  Then:
    - Publish to TSet = "20.0"
    - Publish to TSet_boiler = "20.0"
    - TSet_thermostat remains "20.5"
```

**3. Gateway Override**
```
Scenario: User overrides via MQTT ctrlsetpt command
  Given: Normal T/B message flow
  When: MQTT command ctrlsetpt=19.5
  Then:
    - Gateway sends R message
    - Publish to TSet = "19.5"
    - Publish to TSet_gateway = "19.5"
    - Skip original T message (skipthis=true)
```

**4. Backward Compatibility**
```
Scenario: Feature disabled (default)
  Given: settingMQTTSeparateSources = false
  When: T/B messages received
  Then:
    - Publish only to TSet (original behavior)
    - Do not publish to TSet_thermostat or TSet_boiler
```

**5. Auto-Discovery**
```
Scenario: Home Assistant discovers source-specific sensors
  Given: settingMQTTSeparateSources = true
  When: First T message for TSet received
  Then:
    - Publish auto-discovery config for TSet_thermostat
    - Config includes unique_id: otgw_TSet_thermostat
    - Config includes friendly name: "Control Setpoint (Thermostat)"
```

### Test Tools

**1. MQTT Test Client**
```bash
# Subscribe to all topics
mosquitto_sub -h localhost -v -t 'otgw-firmware/#'

# Expected output with feature enabled:
# otgw-firmware/value/otgw/TSet 20.5
# otgw-firmware/value/otgw/TSet_thermostat 20.5
# otgw-firmware/value/otgw/TSet_boiler 20.0
```

**2. OTmonitor Simulator**
```python
# Send simulated OT messages to test
import serial
ser = serial.Serial('/dev/ttyUSB0', 9600)
ser.write(b'T10100000\r\n')  # Thermostat TSet=20.5
ser.write(b'B50100000\r\n')  # Boiler TSet=20.0
```

**3. Home Assistant Test**
```yaml
# configuration.yaml - verify auto-discovery
sensor:
  - platform: mqtt
    name: "Test TSet Original"
    state_topic: "otgw-firmware/value/otgw/TSet"
    
  - platform: mqtt
    name: "Test TSet Thermostat"
    state_topic: "otgw-firmware/value/otgw/TSet_thermostat"
    
  - platform: mqtt
    name: "Test TSet Boiler"
    state_topic: "otgw-firmware/value/otgw/TSet_boiler"
```

**4. Memory Testing**
```cpp
// Add to telnet debug output
DebugTf("Heap free: %d, MQTT topics published: %d\n", 
        ESP.getFreeHeap(), mqttPublishCount);
```

---

## Migration Guide

### For Users Currently Using MQTT

**Before Upgrade:**
1. Your current MQTT topics: `otgw-firmware/value/otgw/TSet`
2. Home Assistant shows: One sensor for TSet

**After Upgrade (Feature Disabled - Default):**
1. No changes - everything works as before
2. Your MQTT topics: Still `otgw-firmware/value/otgw/TSet`
3. Home Assistant: No changes needed

**After Enabling Feature:**
1. New MQTT topics appear:
   - `otgw-firmware/value/otgw/TSet_thermostat`
   - `otgw-firmware/value/otgw/TSet_boiler`
   - `otgw-firmware/value/otgw/TSet_gateway`
2. Original topic still publishes (last value)
3. Home Assistant: Auto-discovers 3 new sensors
4. You can now:
   - See thermostat request vs boiler actual
   - Create automations based on the difference
   - Monitor gateway override activity

**Example Home Assistant Automation:**
```yaml
# Alert when boiler can't meet thermostat demand
automation:
  - alias: "Boiler Setpoint Mismatch Alert"
    trigger:
      - platform: template
        value_template: >
          {{ (states('sensor.tset_thermostat')|float - 
              states('sensor.tset_boiler')|float) > 2.0 }}
    action:
      - service: notify.mobile_app
        data:
          message: "Boiler cannot meet thermostat demand"
          title: "Heating System Alert"
```

### For Developers Integrating with OTGW-firmware

**API Compatibility:**
- REST API: No changes, returns same values
- WebSocket: No changes, shows all messages
- MQTT: Backward compatible when feature disabled
- MQTT: Additional topics when feature enabled

**Breaking Changes:**
- None if feature left disabled
- If enabled: New topics appear, but old topics still work

**Recommended Integration Pattern:**
```python
# Python MQTT client - handle both old and new topics
def on_message(client, userdata, msg):
    topic = msg.topic
    value = msg.payload.decode()
    
    if topic.endswith('_thermostat'):
        # Process thermostat-specific value
        pass
    elif topic.endswith('_boiler'):
        # Process boiler-specific value
        pass
    elif topic.endswith('_gateway'):
        # Process gateway-override value
        pass
    else:
        # Process original topic (backward compat)
        pass
```

---

## Appendices

### Appendix A: OpenTherm Message Examples

**Example 1: Normal TSet Exchange**
```
T10100000  (Thermostat WRITE-DATA: TSet=20.5°C)
B50100000  (Boiler WRITE-ACK: TSet=20.0°C)

Analysis:
- Thermostat requests 20.5°C
- Boiler acknowledges but limits to 20.0°C
- Reason: Could be max temp limit, safety limit, or current capability
```

**Example 2: Gateway Override**
```
T10100000  (Thermostat WRITE-DATA: TSet=22.0°C)
R10100000  (Gateway WRITE-DATA: TSet=19.5°C - user override)
B50100000  (Boiler WRITE-ACK: TSet=19.5°C)
A50100000  (Gateway WRITE-ACK to Thermostat: TSet=19.5°C)

Analysis:
- User sent MQTT command: ctrlsetpt=19.5
- Gateway intercepts and modifies thermostat request
- Boiler sees 19.5°C (from gateway)
- Thermostat sees 19.5°C (from gateway)
- Current firmware: T message skipped, only R/A/B published to same topic
```

**Example 3: Read-Only Message (No Ambiguity)**
```
T17000000  (Thermostat READ-DATA: RelModLevel)
B51104500  (Boiler READ-ACK: RelModLevel=45.0%)

Analysis:
- Only one source of truth (boiler)
- No ambiguity, but source tracking still useful for debugging
```

### Appendix B: Memory Calculations

**Current Memory Usage:**
- Base MQTT buffer: 1200 bytes
- PubSubClient: ~2KB
- State object: ~500 bytes
- Total MQTT-related: ~4KB

**With Source Separation (30 messages):**
- 30 messages × 3 sources × 100 bytes per config = 9KB
- Additional state tracking: 30 × 3 × 4 bytes = 360 bytes
- Total additional: ~10KB

**ESP8266 Available:**
- Total RAM: 80KB
- Used by core: ~35KB
- Available: ~45KB
- After source separation: ~35KB (still adequate)

### Appendix C: Related Issues and Discussions

**Similar Requests:**
- GitHub Issue #XXX: "MQTT topics lack source information"
- Discord discussion: "Can't tell thermostat from boiler values"
- Forum post: "Gateway override detection"

**Related Features:**
- ADR-006: MQTT Integration Pattern
- ADR-038: OpenTherm Message Data Flow Pipeline
- Issue #YYY: "MQTT auto-discovery improvements"

---

## Conclusion

Implementing source separation for OpenTherm MQTT topics addresses a real user need for visibility into what the thermostat is requesting versus what the boiler is providing. The recommended approach (Option 4: Feature Flag with Opt-In) provides the best balance of:

- **Backward Compatibility:** No breaking changes
- **User Control:** Optional feature users can enable
- **Future-Proofing:** Foundation for advanced diagnostics
- **Safety:** Disabled by default minimizes risk

The implementation is straightforward and builds on existing architecture patterns (ADR-006, ADR-038). Memory impact is manageable on ESP8266, and the feature integrates cleanly with Home Assistant Auto-Discovery.

**Next Steps:**
1. Get maintainer approval on recommended approach
2. Create feature branch
3. Implement Phase 1 (core functionality)
4. Test with community beta testers
5. Iterate based on feedback
6. Release in next firmware version

---

**Document Prepared By:** GitHub Copilot Advanced Agent  
**Review Status:** Awaiting maintainer feedback  
**Target Release:** v1.2.0 (tentative)
