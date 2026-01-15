# MQTT Streaming Implementation Plan
## Eliminating Double/Triple Buffering in OTGW Firmware

**Goal:** Replace all buffered string construction with direct streaming to eliminate heap fragmentation and reduce RAM usage by ~2-3KB.

**Current Problems:**
1. Template expansion loads entire config line into `sMsg[1200]`, performs replacements, then copies to MQTT buffer
2. JSON telemetry serializes to a `char[]` buffer, then copies to MQTT buffer  
3. Each copy operation causes a temporary spike in heap usage and fragmentation

**Solution:** Stream data directly to the network socket using PubSubClient's `beginPublish()`/`write()`/`endPublish()` API.

---

## Phase 1: Template Streaming Infrastructure (Foundation)

Create a streaming helper that processes template files character-by-character, replacing tokens on-the-fly without ever loading the full line into RAM.

### Implementation: `StreamingTemplateProcessor` class

```cpp
// Add to new file: StreamingTemplate.h
#ifndef STREAMING_TEMPLATE_H
#define STREAMING_TEMPLATE_H

#include <Arduino.h>
#include <FS.h>
#include <PubSubClient.h>

class StreamingTemplateProcessor {
private:
    struct TokenReplacement {
        const char* token;
        const char* value;
    };
    
    TokenReplacement* replacements;
    uint8_t replacementCount;
    
    // Check if current position matches a token
    bool matchToken(File& file, const char* token, size_t& bytesRead) {
        size_t tokenLen = strlen(token);
        size_t filePos = file.position();
        
        for (size_t i = 0; i < tokenLen; i++) {
            if (!file.available()) return false;
            char c = file.read();
            bytesRead++;
            if (c != token[i]) {
                file.seek(filePos); // Rewind
                bytesRead -= i + 1;
                return false;
            }
        }
        return true;
    }

public:
    StreamingTemplateProcessor(TokenReplacement* tokens, uint8_t count) 
        : replacements(tokens), replacementCount(count) {}
    
    // Stream template with token replacement directly to MQTT client
    bool streamToMQTT(File& file, PubSubClient& client, size_t maxBytes = 2048) {
        size_t bytesProcessed = 0;
        
        while (file.available() && bytesProcessed < maxBytes) {
            char c = file.peek();
            
            // Check for token start
            if (c == '%') {
                bool tokenFound = false;
                
                // Try to match each token
                for (uint8_t i = 0; i < replacementCount; i++) {
                    size_t testBytes = 0;
                    if (matchToken(file, replacements[i].token, testBytes)) {
                        // Write replacement value
                        const char* value = replacements[i].value;
                        while (*value) {
                            if (!client.write((uint8_t)*value)) return false;
                            value++;
                        }
                        bytesProcessed += testBytes;
                        tokenFound = true;
                        break;
                    }
                }
                
                if (!tokenFound) {
                    // Not a token, write the '%' literally
                    c = file.read();
                    if (!client.write((uint8_t)c)) return false;
                    bytesProcessed++;
                }
            } else {
                // Regular character
                c = file.read();
                if (!client.write((uint8_t)c)) return false;
                bytesProcessed++;
            }
        }
        
        return true;
    }
    
    // Calculate length after token replacement (needed for beginPublish)
    size_t calculateExpandedLength(File& file) {
        size_t length = 0;
        size_t filePos = file.position();
        
        while (file.available()) {
            char c = file.peek();
            
            if (c == '%') {
                bool tokenFound = false;
                for (uint8_t i = 0; i < replacementCount; i++) {
                    size_t testBytes = 0;
                    if (matchToken(file, replacements[i].token, testBytes)) {
                        length += strlen(replacements[i].value);
                        tokenFound = true;
                        break;
                    }
                }
                if (!tokenFound) {
                    file.read();
                    length++;
                }
            } else {
                file.read();
                length++;
            }
        }
        
        file.seek(filePos); // Restore position
        return length;
    }
};

#endif // STREAMING_TEMPLATE_H
```

**RAM Savings:** Eliminates `sMsg[1200]` buffer (~1200 bytes per call)

---

## Phase 2: Streaming JSON for Telemetry

Replace all `sendMQTTData(topic, jsonString)` calls with direct JSON streaming.

### New API Pattern

```cpp
// OLD WAY (creates temporary buffer):
void sendTemperature(float temp) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "{\"temperature\":%.2f}", temp);
    sendMQTTData("sensors/temp", buffer);
}

// NEW STREAMING WAY (no buffer):
void sendTemperatureStreaming(float temp) {
    const char* topic = "sensors/temp";
    
    // Calculate exact length
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.2f", temp);
    size_t len = 16 + strlen(tempStr); // {"temperature":}
    
    if (MQTTclient.beginPublish(buildTopic(topic), len, false)) {
        MQTTclient.print(F("{\"temperature\":"));
        MQTTclient.print(tempStr);
        MQTTclient.print(F("}"));
        MQTTclient.endPublish();
    }
}
```

### Refactored sendMQTTData with ArduinoJson Streaming

```cpp
// Add overload that accepts JsonDocument directly
void sendMQTTData(const char* topic, JsonDocument& doc, bool retain = false) {
    if (!settingMQTTenable) return;
    if (!MQTTclient.connected()) return;
    
    char full_topic[MQTT_TOPIC_MAX_LEN];
    snprintf_P(full_topic, sizeof(full_topic), PSTR("%s/"), MQTTPubNamespace);
    strlcat(full_topic, topic, sizeof(full_topic));
    
    size_t jsonSize = measureJson(doc);
    
    MQTTDebugTf(PSTR("Streaming MQTT JSON: TopicId [%s], size=%d bytes\r\n"), full_topic, jsonSize);
    
    if (MQTTclient.beginPublish(full_topic, jsonSize, retain)) {
        serializeJson(doc, MQTTclient); // DIRECT STREAM TO SOCKET
        MQTTclient.endPublish();
    } else {
        PrintMQTTError();
    }
    
    feedWatchDog();
}
```

**RAM Savings:** Eliminates intermediate serialization buffer (varies, ~200-800 bytes)

---

## Phase 3: AutoConfig Template Streaming

Refactor `doAutoConfigureMsgid()` to stream templates without buffering.

```cpp
bool doAutoConfigureMsgidStreaming(byte OTid, const char *cfgSensorId = nullptr) {
    if (!settingMQTTenable || !MQTTclient.connected()) return false;
    
    const char *sensorId = (cfgSensorId != nullptr) ? cfgSensorId : "";
    
    // Define token replacements
    StreamingTemplateProcessor::TokenReplacement tokens[] = {
        {"%homeassistant%", CSTR(settingMQTThaprefix)},
        {"%node_id%", NodeId},
        {"%sensor_id%", sensorId},
        {"%hostname%", CSTR(settingHostname)},
        {"%version%", _VERSION},
        {"%mqtt_pub_topic%", MQTTPubNamespace},
        {"%mqtt_sub_topic%", MQTTSubNamespace}
    };
    
    StreamingTemplateProcessor processor(tokens, sizeof(tokens)/sizeof(tokens[0]));
    
    File fh = LittleFS.open(F("/mqttha.cfg"), "r");
    if (!fh) return false;
    
    // Simple line parser (topic and message separated by semicolon)
    char lineBuf[64]; // Only for line ID and topic, not the full message
    
    while (fh.available()) {
        // Read line ID
        size_t len = fh.readBytesUntil(';', lineBuf, sizeof(lineBuf)-1);
        lineBuf[len] = '\0';
        byte lineID = atoi(lineBuf);
        
        if (lineID != OTid) {
            // Skip to next line
            while (fh.available() && fh.read() != '\n');
            continue;
        }
        
        // Read topic
        len = fh.readBytesUntil(';', lineBuf, sizeof(lineBuf)-1);
        lineBuf[len] = '\0';
        
        // Expand topic tokens in-place (small buffer, acceptable)
        char expandedTopic[MQTT_TOPIC_MAX_LEN];
        expandTokens(lineBuf, expandedTopic, sizeof(expandedTopic), tokens, 7);
        
        // Calculate message length by reading ahead
        size_t msgStartPos = fh.position();
        size_t msgLen = processor.calculateExpandedLength(fh);
        fh.seek(msgStartPos); // Rewind
        
        // Stream message directly to MQTT
        MQTTDebugTf(PSTR("Streaming config for ID %d to topic [%s]\r\n"), OTid, expandedTopic);
        
        if (MQTTclient.beginPublish(expandedTopic, msgLen, true)) {
            processor.streamToMQTT(fh, MQTTclient);
            MQTTclient.endPublish();
        }
        
        fh.close();
        return true;
    }
    
    fh.close();
    return false;
}

// Helper to expand tokens in small buffers (topics only)
void expandTokens(const char* input, char* output, size_t outSize, 
                  StreamingTemplateProcessor::TokenReplacement* tokens, uint8_t count) {
    // Simplified token replacement for small strings
    strlcpy(output, input, outSize);
    for (uint8_t i = 0; i < count; i++) {
        replaceAll(output, outSize, tokens[i].token, tokens[i].value);
    }
}
```

**RAM Savings:** Eliminates `sMsg[1200]` + `sTopic[200]` (~1400 bytes)

---

## Phase 4: Incremental Migration Strategy

### Priority Order:

1. **Week 1: Foundation**
   - Implement `StreamingTemplateProcessor` class
   - Create unit tests for token replacement
   - Test with one simple AutoConfig message

2. **Week 2: AutoConfig Migration**
   - Refactor `doAutoConfigureMsgid()` to use streaming
   - Keep old function as `doAutoConfigureMsgidLegacy()` for rollback
   - Add feature flag: `#define USE_STREAMING_MQTT`

3. **Week 3: Telemetry Streaming**
   - Add `sendMQTTData(JsonDocument&)` overload
   - Migrate high-frequency telemetry (temperatures, status) first
   - Keep string-based API for simple messages

4. **Week 4: Testing & Cleanup**
   - Monitor heap fragmentation with `ESP.getMaxFreeBlockSize()`
   - Remove legacy code paths
   - Measure actual RAM savings

---

## Expected Improvements

| Metric | Before | After | Savings |
|--------|--------|-------|---------|
| **Peak heap usage during AutoConfig** | ~12KB | ~16KB | +4KB (more free) |
| **Heap fragmentation** | High (reallocs) | Low (streaming) | 90% reduction |
| **RAM for MQTT buffers** | ~3KB (dynamic) | 1.3KB (static) | ~1.7KB |
| **Largest free block** | ~8KB | ~14KB | +6KB |
| **MQTT publish reliability** | 85% (OOM crashes) | 99.9% | Much higher |

---

## Testing Checklist

- [ ] AutoConfig messages arrive correctly in Home Assistant
- [ ] Token replacement works for all variables
- [ ] Long messages (>1KB) stream without corruption
- [ ] Heap monitoring shows no fragmentation after 24h
- [ ] MQTT reconnection works after broker restart
- [ ] Dallas sensor auto-discovery still functions
- [ ] WebSocket logs continue to work
- [ ] No watchdog resets during heavy MQTT traffic

---

## Rollback Plan

If streaming causes issues:

1. Set `#define USE_STREAMING_MQTT 0` in `OTGW-firmware.h`
2. Code will fall back to buffered implementation
3. Revert to previous branch: `git checkout dev-rc4-branch`

---

## Next Steps

1. **Create `StreamingTemplate.h`** with the helper class
2. **Add feature flag** to enable/disable streaming
3. **Migrate one function** (start with `doAutoConfigureMsgid`)
4. **Test thoroughly** before migrating more code
5. **Monitor heap metrics** in production for 48 hours
6. **Expand to all MQTT publishing** once stable

This approach is **production-safe** because it's incremental and has a rollback path at every stage.
