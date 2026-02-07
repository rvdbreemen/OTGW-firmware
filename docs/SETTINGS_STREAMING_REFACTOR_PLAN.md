# Settings Management Streaming Refactor Plan

## Executive Summary

This document outlines a plan to refactor the settings management system in OTGW-firmware to use streaming JSON serialization/deserialization. This will reduce memory usage and improve reliability, especially as settings grow larger with features like Dallas sensor labels.

**Created:** 2026-02-07  
**Status:** Proposed  
**Related Issue:** Code review feedback on settings JSON memory usage  
**Estimated Impact:** ~1-2KB RAM savings, improved flash write reliability

---

## Current Implementation Analysis

### Memory Usage Pattern

**writeSettings() - Current:**
```cpp
DynamicJsonDocument doc(2560);  // Allocates 2560 bytes on heap
JsonObject root = doc.to<JsonObject>();
// ... populate 30+ fields ...
serializeJsonPretty(root, file);  // Writes to file
```

**readSettings() - Current:**
```cpp
StaticJsonDocument<2560> doc;  // Allocates 2560 bytes on stack
DeserializationError error = deserializeJson(doc, file);
// ... read 30+ fields ...
```

**Total peak memory:** ~5KB (2560 × 2) during settings operations

### Issues Identified

1. **Large heap allocation** in writeSettings() - 2560 bytes can fragment limited ESP8266 heap (~40KB available)
2. **Large stack allocation** in readSettings() - 2560 bytes on stack (stack is only ~4KB typically)
3. **No streaming** - entire JSON document loaded into RAM before processing
4. **Scaling problem** - adding more settings requires increasing buffer size
5. **Dallas labels** recently added ~1KB to JSON document, necessitating buffer increase from 1536 to 2560

---

## Proposed Solution: Streaming JSON

ArduinoJson v6+ supports streaming serialization/deserialization that avoids loading entire JSON into memory.

### Benefits

1. **Reduced heap allocation:** Only temporary objects, not entire document
2. **Reduced stack usage:** No large StaticJsonDocument on stack
3. **Scalable:** Can handle arbitrarily large settings files
4. **Better flash writes:** Streaming reduces chance of out-of-memory during write
5. **Consistent with ADR-004:** Static buffer allocation (no unbounded growth)

### Trade-offs

1. **Slightly more complex code:** Need to handle streaming properly
2. **Multiple file passes:** May need to read file multiple times for complex operations
3. **Limited random access:** Can't easily jump to arbitrary setting
4. **Testing overhead:** Need to verify streaming works correctly

---

## Implementation Strategy

### Phase 1: Analysis & Preparation (1-2 hours)

**Tasks:**
- [x] Analyze current settings structure and field count
- [ ] Measure actual JSON size on device (via debug output)
- [ ] Research ArduinoJson streaming API capabilities
- [ ] Identify settings that can benefit from streaming
- [ ] Create test cases for current functionality

**Deliverables:**
- Documented current JSON structure and size
- List of streaming-compatible operations
- Test harness for settings operations

---

### Phase 2: Implement Streaming Write (2-3 hours)

**Goal:** Replace `DynamicJsonDocument(2560)` with streaming approach.

#### Approach A: Field-by-field streaming (Recommended)

Write settings directly to file without intermediate JSON document:

```cpp
void writeSettings(bool show) {
  File file = LittleFS.open(SETTINGS_FILE, "w");
  if (!file) return;
  
  // Write opening brace
  file.print(F("{"));
  
  // Write each field with proper JSON formatting
  writeJsonField(file, F("hostname"), settingHostname, false);
  writeJsonField(file, F("MQTTenable"), settingMQTTenable, true);
  // ... etc for all fields ...
  
  // Write closing brace
  file.print(F("}"));
  file.close();
}

// Helper function to write a single field
void writeJsonField(File& file, const __FlashStringHelper* key, 
                    const char* value, bool addComma) {
  if (addComma) file.print(F(","));
  file.print(F("\""));
  file.print(key);
  file.print(F("\":\""));
  escapeJsonString(file, value);  // Reuse existing escapeJsonString
  file.print(F("\""));
}

// Overloads for bool, int, etc.
void writeJsonField(File& file, const __FlashStringHelper* key, 
                    bool value, bool addComma);
void writeJsonField(File& file, const __FlashStringHelper* key, 
                    int value, bool addComma);
```

**Advantages:**
- Zero heap allocation for JSON document
- Minimal RAM usage (only file buffer)
- Direct write to flash

**Disadvantages:**
- Need to handle JSON escaping manually (but we already have escapeJsonString())
- More verbose code

#### Approach B: ArduinoJson streaming API

Use ArduinoJson's `serializeJson()` with callback:

```cpp
void writeSettings(bool show) {
  File file = LittleFS.open(SETTINGS_FILE, "w");
  if (!file) return;
  
  // Small document for one field at a time
  StaticJsonDocument<128> doc;
  
  // Manually write opening brace
  file.print(F("{"));
  
  bool first = true;
  
  // Write each field
  doc.clear();
  doc["hostname"] = settingHostname;
  if (!first) file.print(F(","));
  serializeJson(doc, file);
  first = false;
  
  // ... repeat for other fields ...
  
  file.print(F("}"));
  file.close();
}
```

**Advantages:**
- Uses ArduinoJson (familiar API)
- Handles JSON escaping automatically
- Small fixed buffer (128 bytes)

**Disadvantages:**
- Less efficient (multiple serialize calls)
- Still need manual JSON structure management

#### Recommended: Hybrid Approach

Combine both approaches:
- Use field-by-field for simple types (strings, ints, bools)
- Use small JsonDocument for complex structures (Dallas labels JSON)

---

### Phase 3: Implement Streaming Read (2-3 hours)

**Goal:** Replace `StaticJsonDocument<2560>` with streaming deserialization.

#### Approach: ArduinoJson streaming filter

```cpp
void readSettings(bool show) {
  File file = LittleFS.open(SETTINGS_FILE, "r");
  if (!file) return;
  
  // Use streaming filter to read one field at a time
  StaticJsonDocument<256> doc;  // Small buffer for single field
  DeserializationError error;
  
  // Read hostname
  doc.clear();
  error = deserializeJson(doc, file, 
                          DeserializationOption::Filter(doc["hostname"]));
  if (!error) {
    strlcpy(settingHostname, doc["hostname"] | "", sizeof(settingHostname));
  }
  
  // Seek back to beginning for next field
  file.seek(0);
  
  // Read next field...
  // ... etc
  
  file.close();
}
```

**Issues with this approach:**
- Need multiple file passes (inefficient for flash)
- Seeking is slow on flash filesystem

#### Better Approach: Single-pass parser

Use ArduinoJson's streaming deserializer with callback:

```cpp
void readSettings(bool show) {
  File file = LittleFS.open(SETTINGS_FILE, "r");
  if (!file) return;
  
  // Small document that gets reused
  StaticJsonDocument<256> doc;
  
  // Create streaming parser
  ReadBufferingStream bufferedFile(file, 64);
  
  // Parse with filter for each setting
  DeserializationError error = deserializeJson(doc, bufferedFile);
  
  if (!error) {
    // Extract all values from doc
    strlcpy(settingHostname, doc["hostname"] | "", sizeof(settingHostname));
    settingMQTTenable = doc["MQTTenable"] | settingMQTTenable;
    // ... etc for all fields ...
  }
  
  file.close();
}
```

**Issue:** Still loads entire JSON into memory.

#### Alternative: Incremental parsing

Since ArduinoJson doesn't support true streaming deserialization for objects, **keep current approach** but optimize:

```cpp
void readSettings(bool show) {
  File file = LittleFS.open(SETTINGS_FILE, "r");
  if (!file) return;
  
  // Calculate required size dynamically
  const size_t fileSize = file.size();
  const size_t capacity = JSON_OBJECT_SIZE(30) + fileSize;  // 30 fields estimate
  
  if (capacity > 3072) {  // Safety limit
    DebugTln(F("Settings file too large!"));
    return;
  }
  
  DynamicJsonDocument doc(capacity);  // Heap allocation sized to actual need
  DeserializationError error = deserializeJson(doc, file);
  
  // ... rest of function unchanged ...
  
  file.close();
}
```

**This is more realistic for ESP8266.**

---

### Phase 4: Optimize Dallas Labels Storage (1-2 hours)

Dallas sensor labels are the largest contributor to settings size growth. Consider separate file:

```cpp
// Instead of storing in main settings.ini:
char settingDallasLabels[JSON_BUFF_MAX] = "";  // 1024 bytes in RAM

// Store in separate file:
#define DALLAS_LABELS_FILE "/dallas_labels.json"

void saveDallasLabels() {
  File file = LittleFS.open(DALLAS_LABELS_FILE, "w");
  if (!file) return;
  
  // Write directly (already JSON string)
  file.print(settingDallasLabels);
  file.close();
}

void loadDallasLabels() {
  File file = LittleFS.open(DALLAS_LABELS_FILE, "r");
  if (!file) {
    settingDallasLabels[0] = '\0';
    return;
  }
  
  file.readBytes(settingDallasLabels, sizeof(settingDallasLabels) - 1);
  file.close();
}
```

**Benefits:**
- Removes ~1KB from main settings.json
- Reduces settings.json capacity requirement back to 1536 bytes
- Isolates Dallas-specific data
- Can grow independently

**Trade-offs:**
- Two files to manage
- Slightly more complex backup/restore
- Need to ensure both files stay in sync

---

## Detailed Task Breakdown

### Task 1: Measure Current State (30 min)
- [ ] Add debug output to show actual settings.json size
- [ ] Add debug output to show peak heap usage during write/read
- [ ] Document current field count and types
- [ ] Create baseline measurements

### Task 2: Create Test Harness (1 hour)
- [ ] Create test settings with known values
- [ ] Implement read/write verification
- [ ] Test with various settings combinations
- [ ] Test with full Dallas labels (16 sensors)

### Task 3: Implement Streaming Write (2 hours)
- [ ] Create helper functions for JSON field writing
- [ ] Convert writeSettings() to use field-by-field approach
- [ ] Add PROGMEM strings for all field names
- [ ] Test write functionality
- [ ] Verify JSON output format matches original

### Task 4: Optimize Read Function (2 hours)
- [ ] Implement dynamic capacity calculation
- [ ] Add file size validation
- [ ] Test with various file sizes
- [ ] Verify backward compatibility with existing settings.ini

### Task 5: Separate Dallas Labels File (1.5 hours)
- [ ] Create DALLAS_LABELS_FILE constant
- [ ] Implement saveDallasLabels() function
- [ ] Implement loadDallasLabels() function
- [ ] Update sensors_ext.ino to use new functions
- [ ] Test label persistence
- [ ] Add migration code for existing settingDallasLabels in settings.ini

### Task 6: Update Documentation (30 min)
- [ ] Update code comments
- [ ] Document new file structure
- [ ] Update ADR-008 (LittleFS persistence) if needed
- [ ] Create migration guide for users

### Task 7: Testing & Validation (2 hours)
- [ ] Test on actual hardware
- [ ] Test upgrade from old settings format
- [ ] Test with 0, 1, and 16 Dallas sensors
- [ ] Test settings persistence across reboots
- [ ] Verify memory usage improvements
- [ ] Test edge cases (corrupted files, missing fields)

---

## Memory Impact Analysis

### Current Implementation

| Component | Size | Location |
|-----------|------|----------|
| writeSettings() DynamicJsonDocument | 2560 bytes | Heap |
| readSettings() StaticJsonDocument | 2560 bytes | Stack |
| settingDallasLabels | 1024 bytes | Global RAM |
| **Total peak usage** | **6144 bytes** | **Mixed** |

### After Refactoring

| Component | Size | Location |
|-----------|------|----------|
| writeSettings() helpers | ~64 bytes | Stack |
| readSettings() DynamicJsonDocument | ~1600 bytes | Heap (sized to actual) |
| settingDallasLabels | *removed* | File-based |
| **Total peak usage** | **~1664 bytes** | **Mostly heap** |

**Net savings: ~4.5KB RAM** (~11% of available ESP8266 RAM)

---

## Risk Assessment

### High Risk
- **Stack overflow** if StaticJsonDocument moved to stack in current form
  - **Mitigation:** Use DynamicJsonDocument with calculated capacity
  
- **Settings corruption** during migration
  - **Mitigation:** Keep backup, validate before overwriting

### Medium Risk
- **Flash wear** from separate Dallas labels file
  - **Mitigation:** Dallas labels change infrequently
  
- **Backward compatibility** with existing settings.ini
  - **Mitigation:** Maintain read compatibility, add migration code

### Low Risk
- **Performance degradation** from field-by-field writing
  - **Mitigation:** Flash writes are already slow, negligible impact

---

## Success Criteria

1. ✅ Settings write uses ≤256 bytes heap allocation
2. ✅ Settings read uses ≤1600 bytes heap (dynamic sizing)
3. ✅ Dallas labels moved to separate file
4. ✅ All existing settings preserved and functional
5. ✅ No regression in settings persistence
6. ✅ Successful migration from old format
7. ✅ Memory usage reduced by >2KB
8. ✅ All tests pass

---

## Alternative Approaches Considered

### 1. Keep Current Implementation, Just Optimize Buffer Sizes
**Rejected:** Doesn't solve scalability problem, band-aid solution

### 2. Move All Settings to EEPROM
**Rejected:** ESP8266 EEPROM is flash emulation, no benefit over LittleFS

### 3. Use Binary Format Instead of JSON
**Rejected:** Breaks human readability, harder to debug, migration nightmare

### 4. Compress JSON in RAM
**Rejected:** Adds complexity, limited benefit on small settings file

---

## Implementation Priority

### Must Have (MVP)
1. ✅ Streaming write implementation (field-by-field)
2. ✅ Dynamic capacity read optimization
3. ✅ Dallas labels separate file
4. ✅ Migration code

### Should Have
1. Helper functions for JSON field writing
2. Comprehensive testing
3. Documentation updates
4. Memory usage measurements

### Nice to Have
1. Settings backup/restore endpoint
2. Settings validation on load
3. Settings diff functionality
4. Web UI for Dallas labels file management

---

## Timeline Estimate

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| Analysis & Prep | 1-2 hours | None |
| Streaming Write | 2-3 hours | Phase 1 |
| Optimize Read | 2-3 hours | Phase 1 |
| Dallas Labels File | 1-2 hours | Phase 2, 3 |
| Documentation | 0.5-1 hour | All phases |
| Testing | 2-3 hours | All phases |
| **Total** | **9-14 hours** | Sequential |

---

## Conclusion

The streaming refactor will significantly reduce memory usage and improve scalability of the settings system. The hybrid approach (field-by-field write + dynamic read) provides the best balance of memory savings and code complexity.

**Recommendation:** Proceed with phased implementation, starting with measurements and testing harness, then implement streaming write, followed by Dallas labels separation.

**Next Steps:**
1. Get approval from @rvdbreemen on approach
2. Create feature branch for refactoring
3. Begin Phase 1 (Analysis & Preparation)
4. Implement incrementally with testing at each phase

---

## Appendix A: Code Examples

### Field-by-Field Write Example

```cpp
void writeJsonStringField(File& file, const __FlashStringHelper* key, 
                          const char* value, bool needsComma) {
  if (needsComma) file.print(F(",\r\n  "));
  else file.print(F("\r\n  "));
  
  file.print(F("\""));
  file.print(key);
  file.print(F("\": \""));
  
  // Use existing escapeJsonString() helper
  String escaped = "";
  for (size_t i = 0; value[i] != '\0'; i++) {
    char c = value[i];
    if (c == '"') escaped += "\\\"";
    else if (c == '\\') escaped += "\\\\";
    else if (c < 0x20) {
      char buf[7];
      snprintf(buf, sizeof(buf), "\\u%04X", (unsigned int)c);
      escaped += buf;
    } else {
      escaped += c;
    }
  }
  file.print(escaped);
  
  file.print(F("\""));
}

void writeJsonBoolField(File& file, const __FlashStringHelper* key, 
                        bool value, bool needsComma) {
  if (needsComma) file.print(F(",\r\n  "));
  else file.print(F("\r\n  "));
  
  file.print(F("\""));
  file.print(key);
  file.print(F("\": "));
  file.print(value ? F("true") : F("false"));
}

void writeJsonIntField(File& file, const __FlashStringHelper* key, 
                       int value, bool needsComma) {
  if (needsComma) file.print(F(",\r\n  "));
  else file.print(F("\r\n  "));
  
  file.print(F("\""));
  file.print(key);
  file.print(F("\": "));
  file.print(value);
}
```

---

## Appendix B: Migration Strategy

### Automatic Migration on First Boot

```cpp
void migrateSettingsIfNeeded() {
  // Check if Dallas labels are in main settings.ini
  File file = LittleFS.open(SETTINGS_FILE, "r");
  if (!file) return;
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (!error && doc.containsKey(F("DallasLabels"))) {
    // Old format detected - migrate
    const char* labels = doc[F("DallasLabels")] | "";
    if (strlen(labels) > 0) {
      // Save to new file
      File labelsFile = LittleFS.open(DALLAS_LABELS_FILE, "w");
      if (labelsFile) {
        labelsFile.print(labels);
        labelsFile.close();
      }
    }
    
    // Rewrite settings without DallasLabels field
    // (will happen automatically on next writeSettings())
    DebugTln(F("Migrated Dallas labels to separate file"));
  }
}
```

---

*End of Plan*
