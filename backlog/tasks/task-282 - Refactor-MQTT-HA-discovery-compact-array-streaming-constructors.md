---
id: TASK-282
title: 'Refactor MQTT HA discovery: compact array + streaming constructors'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-17 06:43'
updated_date: '2026-04-17 06:50'
labels:
  - refactor
  - mqtt
  - ha-discovery
dependencies: []
references:
  - 'https://www.home-assistant.io/integrations/mqtt/'
  - >-
    https://github.com/dawidchyrzynski/arduino-home-assistant (ArduinoHA -
    architecture reference, AGPL license prevents use)
  - >-
    https://github.com/plapointe6/HAMqttDevice (HAMqttDevice - API reference,
    GPL license prevents use)
  - >-
    Discord #beta-testing, crashevans Exception 3/28 crashes (PROGMEM root
    cause)
  - >-
    https://github.com/dawidchyrzynski/arduino-home-assistant/tree/main/src
    (ArduinoHA HASerializer streaming pattern - AGPL, architecture reference
    only)
  - >-
    https://github.com/dawidchyrzynski/arduino-home-assistant/blob/main/src/HAMqtt.cpp
    (ArduinoHA beginPublish/writePayload/endPublish streaming - key pattern to
    follow)
  - >-
    https://github.com/dawidchyrzynski/arduino-home-assistant/blob/main/src/device-types/HABaseDeviceType.cpp
    (ArduinoHA per-entity-type serializer pattern)
  - >-
    https://github.com/plapointe6/HAMqttDevice/blob/master/HAMqttDevice.cpp
    (HAMqttDevice payload builder - anti-pattern: String.concat heap
    fragmentation)
  - >-
    https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery (HA MQTT
    discovery spec - all available config keys)
  - >-
    https://www.home-assistant.io/integrations/sensor.mqtt/ (HA sensor discovery
    keys: device_class, state_class, icon, entity_category, enabled_by_default,
    expire_after, suggested_display_precision)
  - >-
    https://www.home-assistant.io/integrations/binary_sensor.mqtt/ (HA
    binary_sensor discovery keys)
  - >-
    https://www.home-assistant.io/integrations/climate.mqtt/ (HA climate
    discovery keys)
  - >-
    https://www.home-assistant.io/integrations/number.mqtt/ (HA number discovery
    keys)
  - >-
    https://www.home-assistant.io/integrations/mqtt/#origin (HA origin block
    spec - firmware identification in discovery)
  - >-
    https://developers.home-assistant.io/docs/device_registry_index/ (HA device
    registry - shared device block optimization)
  - >-
    https://pictogrammers.com/library/mdi/ (Material Design Icons library -
    mdi:thermometer, mdi:fire, etc.)
  - >-
    src/OTGW-firmware/OTGW-Core.h:326-386 (OTlookup_t/OTmap[] pattern - struct
    with PGM_P pointers, PROGMEM_readAnything accessor)
  - >-
    src/OTGW-firmware/helperStuff.ino:15-18 (PROGMEM_readAnything template -
    memcpy_P wrapper to follow)
documentation:
  - src/OTGW-firmware/data/mqttha.cfg
  - docs/api/MQTT.md
  - src/OTGW-firmware/OTGW-Core.h (OTlookup_t pattern at line 326)
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Replace the 165KB PROGMEM JSON string blobs with a compact data array (~15KB) plus streaming constructor functions that compose HA discovery JSON at runtime. Pattern inspired by ArduinoHA's HASerializer architecture but built in-house (MIT license, no external dependency).

## Problem
The current mqttha_progmem.cpp stores 345 complete JSON discovery payloads as PROGMEM strings. 95% of the content is identical boilerplate (dev block, avty_t, value_template). Only ~5 fields vary per entry (label, friendly name, device class, unit, state class). This wastes ~150KB flash and is unmaintainable.

## Solution
Three-layer architecture:
1. PROGMEM data arrays (like OTmap[]) with only the variable parts per entry
2. Streaming constructor functions that compose JSON and write directly to MQTT via beginPublish/write/endPublish
3. New HA discovery features: icons, entity_category, enabled_by_default, origin block, device block optimization

## Entity type analysis (345 entries total)

### sensor (289 entries) - uniform pattern
Variable fields per entry:
- id (uint8) - OT message ID
- label (PGM_P) - "TSet" - used for stat_t, uniq_id, topic path
- friendlyName (PGM_P) - "Control setpoint" - used for HA name  
- deviceClass (enum, 8 values: temperature/pressure/humidity/power/power_factor/energy/carbon_dioxide/none)
- unit (enum, 15 values: degC/percent/bar/lpm/kW/Hz/uA/mS/ppm/rpm/h/W/kWh/none/empty)
- stateClass (enum, 3 values: measurement/total_increasing/none)
- icon (enum, NEW - mdi:thermometer/fire/gauge/water-boiler/etc.)
- entityCategory (enum, NEW - none/diagnostic/config)
- enabledByDefault (bool, NEW)
- flags (uint8 - source template, PIC entry)
All other JSON fields are derived from these or are constant.

### binary_sensor (53 entries) - even simpler
Variable fields: id, label, friendlyName, icon, entityCategory, enabledByDefault, flags

### climate (2 entries) - fully custom
Each has unique keys (action_template, modes, temp bounds, etc.). Keep as handcrafted PROGMEM strings.

### number (1 entry) - fully custom
Has unique keys (cmd_t, min, max, step, mode). Keep as handcrafted PROGMEM string.

## New HA discovery features to add
- icon: mdi:xxx icons per entity (currently missing, HA shows generic icons)
- entity_category: "diagnostic" for ASF flags, OEM codes, version info (~30 entries)
- enabled_by_default: false for VH/Solar/CH2 entities not everyone has
- origin block: {"name":"OTGW-firmware","sw":version,"url":"https://github.com/rvdbreemen/OTGW-firmware"}
- Device block optimization: first entity sends full dev{}, rest sends only {"identifiers":"%node_id%"} (saves ~85% MQTT traffic)
- expire_after: for critical temperature sensors (stale data detection)
- suggested_display_precision: for temperature sensors

## Architecture

### Data layer (mqttha_data.h / mqttha_data.cpp)

Enums for device class, unit, state class, icon, entity category:
```cpp
enum class HaDeviceClass : uint8_t { none, temperature, pressure, humidity, power, power_factor, energy, carbon_dioxide };
enum class HaUnit : uint8_t { none, empty, degC, percent, bar, lpm, kW, Hz, uA, mS, ppm, rpm, hours, W, kWh };
enum class HaStateClass : uint8_t { none, measurement, total_increasing };
enum class HaIcon : uint8_t { none, thermometer, fire, gauge, water_boiler, radiator, percent_outline, ... };
enum class HaEntityCat : uint8_t { none, diagnostic, config };
```

Structs:
```cpp
struct MqttHaSensorCfg {
    uint8_t      id;
    uint8_t      flags;
    PGM_P        label;
    PGM_P        friendlyName;
    HaDeviceClass deviceClass;
    HaUnit       unit;
    HaStateClass stateClass;
    HaIcon       icon;
    HaEntityCat  entityCat;
    bool         enabledByDefault;
};

struct MqttHaBinSensorCfg {
    uint8_t      id;
    uint8_t      flags;
    PGM_P        label;
    PGM_P        friendlyName;
    HaIcon       icon;
    HaEntityCat  entityCat;
    bool         enabledByDefault;
};
```

PROGMEM arrays (readable, like OTmap[]):
```cpp
const MqttHaSensorCfg PROGMEM mqttHaSensors[] = {
    {1, 0x00, ha_label_tset, ha_name_tset, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},
    {1, 0x07, ha_label_tset, ha_name_tset, HaDeviceClass::temperature, HaUnit::degC, HaStateClass::measurement, HaIcon::thermometer, HaEntityCat::none, true},  // source variant
    // ...
};
```

### Streaming layer (mqttha_stream.h / mqttha_stream.cpp)

Constructor functions that stream JSON directly to MQTT:
```cpp
bool streamSensorDiscovery(PubSubClient &client, const MqttHaSensorCfg &cfg, const HaDiscoveryContext &ctx);
bool streamBinarySensorDiscovery(PubSubClient &client, const MqttHaBinSensorCfg &cfg, const HaDiscoveryContext &ctx);
bool streamClimateDiscovery(PubSubClient &client, uint8_t climateIdx, const HaDiscoveryContext &ctx);
```

HaDiscoveryContext holds runtime data:
```cpp
struct HaDiscoveryContext {
    const char *nodeId;
    const char *hostname;
    const char *version;
    const char *mqttPubTopic;
    const char *mqttSubTopic;
    const char *haPrefix;
    bool isFirstEntity;  // controls full vs minimal device block
};
```

Each stream function:
1. Computes topic string (into cMsg buffer)
2. Calculates exact payload length (dry-run through all JSON keys)
3. Calls beginPublish(topic, length, retain=true)
4. Writes JSON key-value pairs in chunks via writeMqttChunk/writeMqttProgmemChunk
5. Calls endPublish()

### Integration layer (MQTTstuff.ino changes)

Replace doAutoConfigure/doAutoConfigureMsgid to iterate the new arrays:
```cpp
void doAutoConfigure() {
    HaDiscoveryContext ctx = buildContext();
    ctx.isFirstEntity = true;
    
    // Stream sensor discoveries
    for (uint16_t i = 0; i < MQTT_HA_SENSOR_COUNT; i++) {
        MqttHaSensorCfg cfg;
        PROGMEM_readAnything(&mqttHaSensors[i], cfg);
        if (!isPICEnabled() && (cfg.flags & MQTT_HA_FLAG_IS_PIC_ENTRY)) continue;
        streamSensorDiscovery(MQTTclient, cfg, ctx);
        ctx.isFirstEntity = false;
        feedWatchDog();
    }
    // Stream binary_sensor discoveries
    for (uint16_t i = 0; i < MQTT_HA_BINSENSOR_COUNT; i++) { ... }
    // Stream climate discoveries (hardcoded, 2 entries)
    // Stream number discovery (hardcoded, 1 entry)
}
```

## Generator script (tools/generate_mqttha_data.py)

Input: src/OTGW-firmware/data/mqttha.cfg (preserved as documentation/source)
Output: mqttha_data.h + mqttha_data.cpp

The generator:
1. Parses cfg entries
2. Classifies by entity type (sensor/binary_sensor/climate/number)
3. Extracts variable fields (label, name, device_class, unit, state_class)
4. Maps string values to enum values
5. Assigns icons based on device_class and label heuristics
6. Assigns entity_category based on known diagnostic message IDs
7. Generates readable PROGMEM arrays with named label/name strings
8. Generates index arrays for OT ID lookup

## Memory impact

| | Current | New |
|---|---|---|
| PROGMEM strings | 165 KB | ~8 KB (labels + names only) |
| PROGMEM arrays | 2.8 KB | ~6 KB (sensor + binsensor structs) |
| Constructor code | 0 | ~3 KB |
| **Total flash** | **~168 KB** | **~17 KB** |
| **RAM per entity** | 0 | 0 |
| **Savings** | | **~151 KB (90%)** |

## Files to create/modify

New files:
- tools/generate_mqttha_data.py - new generator
- src/OTGW-firmware/mqttha_data.h - enums + structs + PROGMEM externs
- src/OTGW-firmware/mqttha_data.cpp - PROGMEM arrays (generated, readable)
- src/OTGW-firmware/mqttha_stream.h - streaming function declarations
- src/OTGW-firmware/mqttha_stream.cpp - streaming constructor implementations
- docs/mqtt-ha-discovery-architecture.md - architecture documentation

Modified files:
- src/OTGW-firmware/MQTTstuff.ino - use new streaming API
- src/OTGW-firmware/data/mqttha.cfg - preserved as source/documentation

Removed files:
- src/OTGW-firmware/mqttha_progmem.h - replaced by mqttha_data.h
- src/OTGW-firmware/mqttha_progmem.cpp - replaced by mqttha_data.cpp
- tools/generate_mqttha_progmem.py - replaced, kept in docs/ as reference
- tools/generate_mqttha_readable.py - replaced by generate_mqttha_data.py

## Verification

1. python tools/generate_mqttha_data.py - generates data files
2. python build.py --firmware - ESP8266 build must pass
3. python evaluate.py --quick - code quality check
4. Compare: all 345 entries produce identical discovery topics
5. Verify: JSON output matches HA MQTT discovery spec
6. Verify: new features (icons, entity_category) appear in generated JSON
7. Check: flash usage reduced by ~150KB vs current
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sensor entries stored as compact PROGMEM struct array (like OTmap[])
- [ ] #2 Binary sensor entries stored as separate compact PROGMEM struct array
- [ ] #3 Climate and number entries remain as handcrafted PROGMEM strings
- [ ] #4 Streaming constructor functions compose JSON and write directly to MQTT
- [ ] #5 No large RAM buffers needed for discovery JSON
- [ ] #6 All entries include icon (mdi:xxx) field
- [ ] #7 Diagnostic entries marked with entity_category=diagnostic
- [ ] #8 Optional entries have enabled_by_default=false
- [ ] #9 Origin block included in discovery payloads
- [ ] #10 Device block optimized (full on first entity, minimal on rest)
- [ ] #11 Generator script produces readable output from mqttha.cfg
- [ ] #12 ESP8266 build passes
- [ ] #13 evaluate.py --quick passes
- [ ] #14 Flash savings of ~130KB+ vs current approach
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-04-17: Library research completed. ArduinoHA (AGPL-3.0) has excellent streaming architecture via HASerializer but license is incompatible with MIT firmware. HAMqttDevice (GPL-3.0) uses String.concat() which causes heap fragmentation on ESP8266 - anti-pattern. Decision: build in-house following ArduinoHA's streaming principle (beginPublish/write chunks/endPublish) but with PROGMEM data arrays and no external dependency. Key HA discovery features to add: icon (mdi:xxx), entity_category (diagnostic), enabled_by_default, origin block, device block optimization (full on first entity, minimal identifiers-only on rest).
<!-- SECTION:NOTES:END -->
