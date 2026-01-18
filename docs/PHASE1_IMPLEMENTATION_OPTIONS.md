# Phase 1: Implementation Options Analysis

## Overview

This document presents 7 different architectural approaches for implementing comprehensive OTGW PIC command response broadcasting to MQTT. Each option is evaluated based on resource usage, complexity, flexibility, and suitability for ESP8266 platform.

---

## Option 1: Enhanced processOT() with Command Registry Table

### Approach
Create a comprehensive command metadata table with per-command configuration, stored in PROGMEM to minimize RAM usage.

### Architecture

```cpp
// Command metadata structure
typedef struct {
  const char* cmd_code;          // "TT", "PR", "PS", etc.
  const char* mqtt_topic_suffix; // "command/setpoint", "report/version", etc.
  const char* category;          // "control", "query", "diagnostic", "config"
  uint8_t broadcast_flags;       // Bitfield: MQTT, Debug, Event
  const char* response_parser;   // "temp", "on", "string", "level", etc.
} PROGMEM otgw_cmd_metadata_t;

// Broadcast flag definitions
#define BROADCAST_MQTT    0x01  // Send to dedicated MQTT topic
#define BROADCAST_EVENT   0x02  // Send to event_report topic
#define BROADCAST_DEBUG   0x04  // Debug log only
#define BROADCAST_NONE    0x00  // No broadcast (filtered)

// Registry array (PROGMEM to save RAM)
const otgw_cmd_metadata_t PROGMEM cmdRegistry[] = {
  {"TT", "command/setpoint", "control", BROADCAST_MQTT, "temp"},
  {"TC", "command/constant", "control", BROADCAST_MQTT, "temp"},
  {"HW", "command/hotwater", "control", BROADCAST_MQTT, "on"},
  {"PR", "query/report", "diagnostic", BROADCAST_DEBUG, "string"},
  {"PS", "diagnostic/summary", "diagnostic", BROADCAST_EVENT, "on"},
  {"LA", "config/led_a", "config", BROADCAST_MQTT, "string"},
  {"GA", "config/gpio_a", "config", BROADCAST_MQTT, "string"},
  // ... ~60 total entries
  {"", "", "", 0, ""}  // Null terminator
};

// Enhanced processOT()
void processOT(const char *buf, int len) {
  // ... existing OT message handling ...
  
  if (buf[2]==':') {
    checkOTGWcmdqueue(buf, len);
    Debugln(buf);  // Always debug log
    
    // Extract command code
    char cmd[3] = {buf[0], buf[1], '\0'};
    const char* value = buf + 3;
    
    // Look up in registry
    const otgw_cmd_metadata_t* meta = findCommandMetadata(cmd);
    
    if (meta) {
      uint8_t flags = pgm_read_byte(&meta->broadcast_flags);
      
      // Route based on metadata flags
      if (flags & BROADCAST_MQTT) {
        char topic[64];
        char suffix[32];
        strcpy_P(suffix, (const char*)pgm_read_ptr(&meta->mqtt_topic_suffix));
        snprintf_P(topic, sizeof(topic), PSTR("otgw-pic/%s"), suffix);
        sendMQTTData(topic, value);
      }
      
      if (flags & BROADCAST_EVENT) {
        sendMQTTData(F("event_report"), buf);
      }
      
      // BROADCAST_DEBUG: already logged above
      // BROADCAST_NONE: no additional action
    }
    else {
      // Unknown command - fallback to event_report for backward compat
      sendMQTTData(F("event_report"), buf);
    }
  }
  
  // ... rest of processOT ...
}

// Fast lookup helper
const otgw_cmd_metadata_t* findCommandMetadata(const char* cmd) {
  for (size_t i = 0; ; i++) {
    char regCmd[3];
    strcpy_P(regCmd, (const char*)pgm_read_ptr(&cmdRegistry[i].cmd_code));
    
    if (regCmd[0] == '\0') break;  // End of registry
    
    if (strcmp(cmd, regCmd) == 0) {
      return &cmdRegistry[i];
    }
  }
  return nullptr;
}
```

### Pros
- ✅ **Centralized metadata** - Single source of truth for all commands
- ✅ **Fine-grained control** - Per-command configuration via flags
- ✅ **PROGMEM usage** - Minimal RAM footprint (~100-200 bytes working buffers only)
- ✅ **Easy to extend** - Add new commands by adding one line to registry
- ✅ **Clear separation** - Command metadata separate from routing logic
- ✅ **Type safety** - Structured data with defined fields

### Cons
- ❌ **Large table** - ~60 entries × 16+ bytes per entry = ~1KB flash minimum
- ❌ **PROGMEM complexity** - Requires accessor functions and careful pointer handling
- ❌ **Lookup overhead** - Linear search for each command response
- ❌ **Flash usage** - Total impact ~2-3 KB (table + code + accessor functions)

### Estimated Resource Impact
- **Flash**: +2.0 to 3.0 KB (registry table + lookup code + routing logic)
- **RAM**: +100 to 200 bytes (working buffers for string operations)
- **CPU**: Minimal (simple linear search, ~60 iterations worst case)

### Best For
Projects requiring maximum flexibility and future extensibility, where 2-3 KB flash is acceptable.

---

## Option 2: Category-Based Routing with Prefix Matching

### Approach
Group commands by category and use direct string comparison chains instead of lookup tables.

### Architecture

```cpp
void processOT(const char *buf, int len) {
  if (buf[2]==':') {
    checkOTGWcmdqueue(buf, len);
    Debugln(buf);
    
    char cmd[3] = {buf[0], buf[1], '\0'};
    const char* value = buf + 3;
    
    // Temperature control commands -> broadcast to MQTT
    if (strcmp_P(cmd, PSTR("TT"))==0 || strcmp_P(cmd, PSTR("TC"))==0 || 
        strcmp_P(cmd, PSTR("SB"))==0 || strcmp_P(cmd, PSTR("SH"))==0 ||
        strcmp_P(cmd, PSTR("SW"))==0 || strcmp_P(cmd, PSTR("CS"))==0 ||
        strcmp_P(cmd, PSTR("C2"))==0 || strcmp_P(cmd, PSTR("OT"))==0) {
      sendMQTTData(F("otgw-pic/command/temperature"), buf);
    }
    // Enable/disable commands -> broadcast
    else if (strcmp_P(cmd, PSTR("HW"))==0 || strcmp_P(cmd, PSTR("CH"))==0 ||
             strcmp_P(cmd, PSTR("H2"))==0 || strcmp_P(cmd, PSTR("GW"))==0) {
      sendMQTTData(F("otgw-pic/command/control"), buf);
    }
    // PR=* commands -> selective broadcast based on suffix
    else if (strncmp_P(cmd, PSTR("PR"), 2)==0) {
      // PR=A, PR=B -> don't broadcast (internal diagnostics)
      // Already filtered by our recent PR: filter
      // Other PR commands could go to diagnostic topic if needed
      // Currently: debug log only
    }
    // PS command -> event only
    else if (strcmp_P(cmd, PSTR("PS"))==0) {
      sendMQTTData(F("event_report"), buf);
    }
    // LED configuration commands -> broadcast
    else if (strcmp_P(cmd, PSTR("LA"))==0 || strcmp_P(cmd, PSTR("LB"))==0 ||
             strcmp_P(cmd, PSTR("LC"))==0 || strcmp_P(cmd, PSTR("LD"))==0 ||
             strcmp_P(cmd, PSTR("LE"))==0 || strcmp_P(cmd, PSTR("LF"))==0) {
      char topic[48];
      snprintf_P(topic, sizeof(topic), PSTR("otgw-pic/config/led_%c"), cmd[1]);
      sendMQTTData(topic, value);
    }
    // GPIO configuration commands -> broadcast
    else if (strcmp_P(cmd, PSTR("GA"))==0 || strcmp_P(cmd, PSTR("GB"))==0) {
      char topic[48];
      snprintf_P(topic, sizeof(topic), PSTR("otgw-pic/config/gpio_%c"), cmd[1]);
      sendMQTTData(topic, value);
    }
    // Modulation and ventilation
    else if (strcmp_P(cmd, PSTR("MM"))==0 || strcmp_P(cmd, PSTR("VS"))==0) {
      sendMQTTData(F("otgw-pic/command/modulation"), buf);
    }
    // Unknown/other commands -> event_report for backward compatibility
    else {
      sendMQTTData(F("event_report"), buf);
    }
  }
}
```

### Pros
- ✅ **Smaller code footprint** - No large lookup table needed
- ✅ **No PROGMEM complexity** - Direct string comparisons
- ✅ **Fast execution** - Optimized if/else chains, early exits
- ✅ **Easy to understand** - Clear, readable logic flow
- ✅ **Low RAM usage** - Only small working buffers needed

### Cons
- ❌ **Less flexible** - Hard to customize behavior per individual command
- ❌ **Maintenance burden** - Logic scattered across many if/else blocks
- ❌ **Difficult to configure** - No easy way to add per-command settings
- ❌ **Category ambiguity** - Some commands don't fit cleanly into categories
- ❌ **Code duplication** - Similar patterns repeated for each category

### Estimated Resource Impact
- **Flash**: +1.0 to 1.5 KB (if/else chains + string comparisons)
- **RAM**: +50 to 100 bytes (small working buffers)
- **CPU**: Minimal (fast strcmp operations)

### Best For
Simple implementations with clear command categories, where flexibility is less important than code size.

---

## Option 3: Hybrid Approach - Registry for New Commands Only

### Approach
Keep existing code paths for the 28 commands already in setcmds[], add a small registry only for the 32 new commands.

### Architecture

```cpp
// Small registry for NEW commands only (not in setcmds)
typedef struct {
  char cmd[3];
  const char* mqtt_topic;
  uint8_t flags;
} PROGMEM new_cmd_t;

const new_cmd_t PROGMEM newCommands[] = {
  {"LA", "otgw-pic/config/led_a", BROADCAST_MQTT},
  {"LB", "otgw-pic/config/led_b", BROADCAST_MQTT},
  {"LC", "otgw-pic/config/led_c", BROADCAST_MQTT},
  {"LD", "otgw-pic/config/led_d", BROADCAST_MQTT},
  {"LE", "otgw-pic/config/led_e", BROADCAST_MQTT},
  {"LF", "otgw-pic/config/led_f", BROADCAST_MQTT},
  {"GA", "otgw-pic/config/gpio_a", BROADCAST_MQTT},
  {"GB", "otgw-pic/config/gpio_b", BROADCAST_MQTT},
  {"SM", "otgw-pic/command/summermode", BROADCAST_MQTT},
  {"BW", "otgw-pic/command/dhw_blocking", BROADCAST_MQTT},
  // ... ~32 new commands only
  {"", "", 0}  // Null terminator
};

void processOT(const char *buf, int len) {
  if (buf[2]==':') {
    checkOTGWcmdqueue(buf, len);
    Debugln(buf);
    
    char cmd[3] = {buf[0], buf[1], '\0'};
    const char* value = buf + 3;
    
    // First check if this is a "known" command in setcmds
    bool isKnownCommand = checkIfInSetcmds(cmd);
    
    if (isKnownCommand) {
      // Use existing behavior for backward compatibility
      sendMQTTData(F("event_report"), buf);
    }
    else {
      // Try new commands registry
      const new_cmd_t* newCmd = findNewCommand(cmd);
      if (newCmd && (pgm_read_byte(&newCmd->flags) & BROADCAST_MQTT)) {
        char topic[64];
        strcpy_P(topic, (const char*)pgm_read_ptr(&newCmd->mqtt_topic));
        sendMQTTData(topic, value);
      }
      else {
        // Unknown command - event_report
        sendMQTTData(F("event_report"), buf);
      }
    }
  }
}
```

### Pros
- ✅ **Minimal disruption** - Existing commands unchanged
- ✅ **Backward compatible** - No risk to working code paths
- ✅ **Smaller table** - Only 32 new commands instead of all 60
- ✅ **Incremental implementation** - Can add commands gradually

### Cons
- ❌ **Two parallel systems** - Confusing dual-path logic
- ❌ **Inconsistent handling** - Old vs new commands behave differently
- ❌ **Technical debt** - Creates long-term maintenance issues
- ❌ **Code complexity** - More complex than unified approach

### Estimated Resource Impact
- **Flash**: +1.5 to 2.0 KB (smaller registry + dual routing)
- **RAM**: +75 to 150 bytes
- **CPU**: Minimal

### Best For
Conservative upgrades where backward compatibility is critical and disruption must be minimized.

---

## Option 4: Configuration-Driven with Settings

### Approach
Make broadcasting configurable via user settings, allowing users to control what gets broadcast.

### Architecture

```cpp
// Settings structure (add to existing settings)
struct OTGWBroadcastSettings {
  bool broadcastControlCommands;      // TT, HW, CH, etc.
  bool broadcastQueryCommands;        // PR=* commands
  bool broadcastDiagnosticCommands;   // PS, etc.
  bool broadcastConfigCommands;       // LA, GA, etc.
} otgwBroadcastSettings;

// Command categorization
typedef enum {
  CAT_CONTROL,
  CAT_QUERY,
  CAT_DIAGNOSTIC,
  CAT_CONFIG,
  CAT_UNKNOWN
} CommandCategory;

CommandCategory categorizeCommand(const char* cmd) {
  if (strcmp_P(cmd, PSTR("TT"))==0 || strcmp_P(cmd, PSTR("HW"))==0 ||
      strcmp_P(cmd, PSTR("CH"))==0 || /* ... */) {
    return CAT_CONTROL;
  }
  else if (strncmp_P(cmd, PSTR("PR"), 2)==0) {
    return CAT_QUERY;
  }
  else if (strcmp_P(cmd, PSTR("PS"))==0) {
    return CAT_DIAGNOSTIC;
  }
  else if (strcmp_P(cmd, PSTR("LA"))==0 || strcmp_P(cmd, PSTR("GA"))==0 ||
           /* ... */) {
    return CAT_CONFIG;
  }
  return CAT_UNKNOWN;
}

void processOT(const char *buf, int len) {
  if (buf[2]==':') {
    checkOTGWcmdqueue(buf, len);
    Debugln(buf);
    
    char cmd[3] = {buf[0], buf[1], '\0'};
    const char* value = buf + 3;
    
    CommandCategory cat = categorizeCommand(cmd);
    bool shouldBroadcast = false;
    
    switch (cat) {
      case CAT_CONTROL:
        shouldBroadcast = otgwBroadcastSettings.broadcastControlCommands;
        break;
      case CAT_QUERY:
        shouldBroadcast = otgwBroadcastSettings.broadcastQueryCommands;
        break;
      case CAT_DIAGNOSTIC:
        shouldBroadcast = otgwBroadcastSettings.broadcastDiagnosticCommands;
        break;
      case CAT_CONFIG:
        shouldBroadcast = otgwBroadcastSettings.broadcastConfigCommands;
        break;
      default:
        shouldBroadcast = false;
    }
    
    if (shouldBroadcast) {
      char topic[64];
      generateTopicForCommand(cmd, cat, topic, sizeof(topic));
      sendMQTTData(topic, value);
    }
  }
}
```

### Pros
- ✅ **User control** - Users can customize what gets broadcast
- ✅ **Reduced traffic** - Can disable unwanted broadcasts
- ✅ **Flexible** - Different users, different needs
- ✅ **Future-proof** - Easy to add more settings categories

### Cons
- ❌ **Complex implementation** - Requires settings UI changes
- ❌ **More testing needed** - Settings persistence, validation, UI
- ❌ **Configuration burden** - Users must understand categories
- ❌ **Flash overhead** - Settings UI + storage + validation

### Estimated Resource Impact
- **Flash**: +2.5 to 3.5 KB (settings code + UI + validation)
- **RAM**: +150 to 250 bytes (settings storage + working vars)
- **CPU**: Minimal

### Best For
Power users who want fine-grained control, where the complexity of configuration is acceptable.

---

## Option 5: Dual-Topic Strategy (Response + Event)

### Approach
Send ALL command responses to dedicated topics, plus important events to event_report for backward compatibility.

### Architecture

```cpp
void processOT(const char *buf, int len) {
  if (buf[2]==':') {
    checkOTGWcmdqueue(buf, len);
    Debugln(buf);
    
    char cmd[3] = {buf[0], buf[1], '\0'};
    const char* value = buf + 3;
    
    // ALWAYS send to command-specific response topic
    char responseTopic[64];
    snprintf_P(responseTopic, sizeof(responseTopic), 
               PSTR("otgw-pic/response/%c%c"), buf[0], buf[1]);
    sendMQTTData(responseTopic, value);
    
    // ALSO send important events to event_report for backward compat
    if (isImportantEvent(cmd)) {
      sendMQTTData(F("event_report"), buf);
    }
  }
}

bool isImportantEvent(const char* cmd) {
  // Only errors and state changes
  return (strcmp_P(cmd, PSTR("PS"))==0 || 
          strcmp_P(cmd, PSTR("GW"))==0 ||
          strcmp_P(cmd, PSTR("RS"))==0);
}
```

### Pros
- ✅ **Complete coverage** - All commands automatically handled
- ✅ **Backward compatible** - event_report still used for important events
- ✅ **Simple implementation** - No complex lookup or categorization
- ✅ **Predictable** - Clear, consistent topic structure

### Cons
- ❌ **High MQTT traffic** - ALL responses broadcast, not filtered
- ❌ **No filtering** - Can't disable unwanted command broadcasts
- ❌ **Performance impact** - May overwhelm MQTT broker with traffic
- ❌ **Network overhead** - Increased bandwidth usage

### Estimated Resource Impact
- **Flash**: +0.8 to 1.2 KB (simple routing code)
- **RAM**: +50 to 100 bytes (topic buffers)
- **CPU**: Minimal
- **MQTT Traffic**: +300% to 500% (all responses broadcast)

### Best For
Situations where complete data capture is required and MQTT traffic is not a concern.

---

## Option 6: Selective Broadcasting with Whitelist

### Approach
Only broadcast commands in a whitelist, all others go to debug log only.

### Architecture

```cpp
// Whitelist of commands to broadcast to MQTT
const char* PROGMEM mqttWhitelist[] = {
  // Control commands
  "TT", "TC", "HW", "CH", "GW", "SB", "SH", "SW",
  "MM", "CS", "VS", "H2", "C2",
  // Configuration commands
  "LA", "LB", "LC", "LD", "LE", "LF",
  "GA", "GB",
  // Diagnostic (selected)
  "PS",
  nullptr  // Null terminator
};

bool isInWhitelist(const char* cmd) {
  for (size_t i = 0; mqttWhitelist[i] != nullptr; i++) {
    char whiteCmd[3];
    strcpy_P(whiteCmd, mqttWhitelist[i]);
    if (strcmp(cmd, whiteCmd) == 0) {
      return true;
    }
  }
  return false;
}

void processOT(const char *buf, int len) {
  if (buf[2]==':') {
    checkOTGWcmdqueue(buf, len);
    Debugln(buf);  // Always debug log
    
    char cmd[3] = {buf[0], buf[1], '\0'};
    const char* value = buf + 3;
    
    if (isInWhitelist(cmd)) {
      // Broadcast to MQTT with command-specific topic
      char topic[64];
      snprintf_P(topic, sizeof(topic), PSTR("otgw-pic/command/%c%c"), 
                 buf[0], buf[1]);
      sendMQTTData(topic, value);
    }
    else {
      // Not in whitelist - debug log only (already done above)
      // PR=* commands handled here (filtered as desired)
    }
  }
}
```

### Pros
- ✅ **Controlled MQTT traffic** - Only whitelisted commands broadcast
- ✅ **Simple whitelist maintenance** - Easy to add/remove commands
- ✅ **Small memory footprint** - Just array of string pointers
- ✅ **Easy to customize** - Modify whitelist to user needs
- ✅ **Clear intent** - Explicit list of what gets broadcast

### Cons
- ❌ **Manual curation** - Requires deciding what to whitelist
- ❌ **Limited access** - Non-whitelisted commands not available via MQTT
- ❌ **Less discoverable** - Users don't know what's available

### Estimated Resource Impact
- **Flash**: +0.5 to 1.0 KB (whitelist array + lookup code)
- **RAM**: +30 to 60 bytes (working buffers)
- **CPU**: Minimal (simple linear search)

### Best For
Production environments where MQTT traffic control is important and command needs are well-defined.

---

## Option 7: JSON Payload with Metadata

### Approach
Send all command responses as JSON payloads with rich metadata to a single topic.

### Architecture

```cpp
void processOT(const char *buf, int len) {
  if (buf[2]==':') {
    checkOTGWcmdqueue(buf, len);
    Debugln(buf);
    
    char cmd[3] = {buf[0], buf[1], '\0'};
    const char* value = buf + 3;
    const char* category = getCategoryName(cmd);
    
    // Build JSON payload with metadata
    char json[256];
    snprintf_P(json, sizeof(json), 
               PSTR("{\"cmd\":\"%s\",\"value\":\"%s\",\"category\":\"%s\",\"timestamp\":%lu}"),
               cmd, value, category, millis());
    
    // Send to single responses topic
    sendMQTTData(F("otgw-pic/responses"), json);
  }
}

const char* getCategoryName(const char* cmd) {
  // Categorize command
  if (strcmp_P(cmd, PSTR("TT"))==0 || /* ... */) return "control";
  if (strncmp_P(cmd, PSTR("PR"), 2)==0) return "query";
  if (strcmp_P(cmd, PSTR("LA"))==0 || /* ... */) return "config";
  return "other";
}
```

### Pros
- ✅ **Rich metadata** - Command, value, category, timestamp in one message
- ✅ **Easy parsing** - Standard JSON format for automation platforms
- ✅ **Timestamp included** - Know exactly when command was executed
- ✅ **Extensible** - Easy to add more fields (e.g., sequence number)
- ✅ **Single topic** - Simpler MQTT subscription

### Cons
- ❌ **Large payload size** - JSON overhead increases message size
- ❌ **Requires JSON parsing** - Client must parse JSON (complexity)
- ❌ **Higher flash usage** - JSON serialization code (if using library)
- ❌ **Higher RAM usage** - JSON buffer allocation
- ❌ **Single point of failure** - All commands on one topic

### Estimated Resource Impact
- **Flash**: +3.0 to 4.0 KB (JSON library or manual serialization)
- **RAM**: +400 to 600 bytes (JSON buffer)
- **CPU**: Moderate (JSON string building)

### Best For
Advanced integrations where rich metadata and structured data is valuable, and resources are less constrained.

---

## Comparison Matrix

| Option | Flash | RAM | MQTT Traffic | Flexibility | Complexity | Backwards Compat | Recommended For |
|--------|-------|-----|--------------|-------------|------------|------------------|-----------------|
| **1. Registry Table** | +2-3KB | +100-200B | Medium | High | Medium | ✅ Yes | Max flexibility & extensibility |
| **2. Category Routing** | +1-1.5KB | +50-100B | Medium | Low | Low | ✅ Yes | Simple, clear categories |
| **3. Hybrid** | +1.5-2KB | +75-150B | Medium | Medium | Medium | ✅ Yes | Conservative upgrades |
| **4. Config-Driven** | +2.5-3.5KB | +150-250B | Variable | Very High | High | ✅ Yes | Power users wanting control |
| **5. Dual-Topic** | +0.8-1.2KB | +50-100B | Very High | Low | Low | ✅ Yes | Complete data capture |
| **6. Whitelist** | +0.5-1KB | +30-60B | Low | Low | Low | ✅ Yes | **Production, traffic control** |
| **7. JSON Payload** | +3-4KB | +400-600B | Medium | Medium | Medium | ⚠️ Partial | Advanced integrations |

---

## Next Steps

1. Review options with stakeholders
2. Select preferred implementation approach
3. Proceed to Phase 2: Detailed design
4. Implement, test, and deploy

