# Settings Management Streaming Refactor Plan

## Executive Summary

This document outlines a plan to refactor the settings management system in OTGW-firmware to use streaming JSON serialization/deserialization. This will reduce memory usage and improve reliability, especially as settings grow larger with features like Dallas sensor labels.

**Created:** 2026-02-07  
**Updated:** 2026-02-07 (added streaming read analysis)  
**Status:** Proposed  
**Related Issue:** Code review feedback on settings JSON memory usage  
**Estimated Impact:** ~4.5KB RAM savings (70% reduction), improved flash write reliability

---

## Current Implementation Analysis

### Memory Usage Pattern

**writeSettings() - Current:**
```cpp
DynamicJsonDocument doc(2560);  // Allocates 2560 bytes on heap
JsonObject root = doc.to<JsonObject>();
// ... populate 35+ fields ...
serializeJsonPretty(root, file);  // Writes to file
```
**Peak memory: 2560 bytes heap**

**readSettings() - Current:**
```cpp
StaticJsonDocument<2560> doc;  // Allocates 2560 bytes on stack (DANGEROUS!)
DeserializationError error = deserializeJson(doc, file);
// ... read 35+ fields ...
```
**Peak memory: 2560 bytes stack + Dallas labels 1024 bytes RAM = ~3.5KB**

**Total peak memory during settings operations: ~6KB**
- Write: 2560 bytes heap allocation
- Read: 2560 bytes stack allocation (RISK: Stack overflow on ~4KB stack)
- Dallas labels: 1024 bytes global RAM (always allocated)

### Issues Identified

1. **CRITICAL: Stack overflow risk** - readSettings() allocates 2560 bytes on stack, which is ~60% of typical ESP8266 stack (4KB). This is dangerous and can cause crashes.
2. **Large heap allocation** in writeSettings() - 2560 bytes can fragment limited ESP8266 heap (~40KB available)
3. **No streaming** - entire JSON document loaded into RAM before processing
4. **Scaling problem** - adding more settings requires increasing buffer size further
5. **Dallas labels** recently added ~1KB to JSON document, necessitating buffer increase from 1536 to 2560
6. **Inefficient memory use** - Typical settings file is only ~500-800 bytes, but we allocate 2560 bytes

---

## Proposed Solution: Streaming JSON

ArduinoJson v6+ supports streaming serialization/deserialization that avoids loading entire JSON into memory.

### Memory & Complexity Comparison Table

| Approach | Peak Heap | Peak Stack | Code Lines | Complexity | Risk | Recommended |
|----------|-----------|------------|------------|------------|------|-------------|
| **Current** | 2560 bytes | 2560 bytes | Baseline | Low | HIGH (stack overflow) | ❌ |
| **Option 1: Dynamic Sizing** | ~800 bytes | 0 bytes | +10 lines | Very Low | Very Low | ✅ **BEST** |
| **Option 2: Manual Parser** | 0 bytes | ~200 bytes | +500 lines | Very High | High | ❌ |
| **Option 3: Multi-pass Filter** | ~300 bytes | 0 bytes | +150 lines | High | Medium | ❌ |
| **Hybrid (Write+Read)** | ~800 bytes | 0 bytes | +110 lines | Low | Very Low | ✅ **RECOMMENDED** |

### Detailed Memory Estimates

#### Write Operations

| Method | Peak Memory | Location | Notes |
|--------|-------------|----------|-------|
| Current (DynamicJsonDocument) | 2560 bytes | Heap | Full document in memory |
| Field-by-field streaming | 0 bytes | N/A | Direct file writes, ~50 bytes print buffer |
| **Savings** | **2560 bytes** | **Heap freed** | **100% reduction** |

#### Read Operations

| Method | Peak Memory | Location | Notes |
|--------|-------------|----------|-------|
| Current (StaticJsonDocument) | 2560 bytes | Stack | **DANGEROUS: Stack overflow risk** |
| Dynamic sizing | file.size() + ~200 bytes | Heap | Typical: ~800 bytes (500-800 byte files) |
| Manual parser | ~200 bytes | Stack | Parser state machine + temporary buffers |
| Multi-pass filter | ~300 bytes per pass | Heap | Small filtered documents, 35 passes |
| **Savings (Dynamic)** | **~1700 bytes** | **Stack→Heap** | **67% reduction + eliminates crash risk** |

#### Overall System Impact

| Component | Current | Proposed | Savings | Notes |
|-----------|---------|----------|---------|-------|
| Write heap | 2560 bytes | 0 bytes | 2560 bytes | Field-by-field streaming |
| Read stack | 2560 bytes | 0 bytes | 2560 bytes | **Eliminates crash risk** |
| Read heap | 0 bytes | ~800 bytes | -800 bytes | Dynamic sizing (safer than stack) |
| Dallas labels RAM | 1024 bytes | 0 bytes | 1024 bytes | Move to separate file |
| **Total** | **~6KB** | **~1.6KB** | **~4.5KB (73%)** | **Massive improvement** |

### Complexity Scores (1-10, lower is better)

| Aspect | Current | Dynamic Sizing | Manual Parser | Multi-pass Filter |
|--------|---------|----------------|---------------|-------------------|
| Implementation | 1 | 2 | 9 | 7 |
| Testing | 2 | 3 | 10 | 8 |
| Maintenance | 1 | 2 | 9 | 6 |
| Debugging | 2 | 3 | 10 | 7 |
| Risk of bugs | 2 | 2 | 10 | 6 |
| **Overall** | **8/50** | **12/50** | **48/50** | **34/50** |

**Conclusion:** Dynamic sizing is only slightly more complex (+4 points) but provides 73% memory savings. Manual parser is too risky (+40 points complexity) for the same benefit.

### Benefits

1. **Reduced heap allocation:** Only temporary objects, not entire document
2. **Reduced stack usage:** No large StaticJsonDocument on stack
3. **Scalable:** Can handle arbitrarily large settings files
4. **Better flash writes:** Streaming reduces chance of out-of-memory during write
5. **Consistent with ADR-004:** Static buffer allocation (no unbounded growth)
6. **Eliminates crash risk:** 2560 bytes on 4KB stack = 64% stack usage (CRITICAL)

### Trade-offs

1. **Slightly more complex code:** Need to handle streaming properly
2. **Multiple file passes:** May need to read file multiple times for complex operations (only for multi-pass filter approach)
3. **Limited random access:** Can't easily jump to arbitrary setting (not an issue in practice)
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

#### Recommended Approach: Dynamic Sizing with DynamicJsonDocument

Since ArduinoJson v6 doesn't support true incremental/streaming deserialization for JSON objects, the best practical approach is to size the document dynamically based on actual file size:

```cpp
void readSettings(bool show) {
  File file = LittleFS.open(SETTINGS_FILE, "r");
  if (!file) return;
  
  // Get actual file size
  const size_t fileSize = file.size();
  
  // Calculate required capacity:
  // fileSize for JSON text + JSON_OBJECT_SIZE(fieldCount) for structure overhead
  // Typical: 500-800 bytes file + ~200 bytes overhead = ~700-1000 bytes
  const size_t capacity = fileSize + JSON_OBJECT_SIZE(35) + 100;  // 35 fields + margin
  
  // Safety check to prevent excessive allocation
  if (capacity > 3072) {  // Maximum reasonable size
    DebugTf(PSTR("Settings file too large: %d bytes\r\n"), fileSize);
    file.close();
    return;
  }
  
  DynamicJsonDocument doc(capacity);  // Heap allocation sized to actual need
  DeserializationError error = deserializeJson(doc, file);
  
  if (error) {
    DebugTf(PSTR("Settings deserialization error: %s\r\n"), error.c_str());
    file.close();
    return;
  }
  
  // ... extract all fields (unchanged) ...
  
  file.close();
}
```

**Benefits:**
- Reduces typical read memory from 2560 bytes to ~700-1000 bytes (60-70% savings)
- No code complexity increase
- Still uses proven ArduinoJson deserializer
- Heap allocation instead of stack (safer)
- Adapts to actual file size automatically

**Memory comparison:**
- Current: Fixed 2560 bytes on stack
- Proposed: Dynamic ~700-1000 bytes on heap (typical), up to 3072 max

#### Alternative: Manual Streaming Parser (Not Recommended)

For completeness, true zero-memory streaming would require manual character-by-character parsing:

```cpp
// Pseudo-code (NOT RECOMMENDED for production)
void readSettings(bool show) {
  File file = LittleFS.open(SETTINGS_FILE, "r");
  
  char key[64];
  char value[256];
  
  while (file.available()) {
    // Read next key-value pair manually
    if (parseNextKeyValue(file, key, value)) {
      // Apply to appropriate setting variable
      if (strcmp_P(key, PSTR("hostname")) == 0) {
        strlcpy(settingHostname, value, sizeof(settingHostname));
      } else if (strcmp_P(key, PSTR("MQTTenable")) == 0) {
        settingMQTTenable = (strcmp_P(value, PSTR("true")) == 0);
      }
      // ... repeat for all 35+ settings ...
    }
  }
  
  file.close();
}
```

**Why not recommended:**
- 500+ lines of error-prone parsing code
- Must handle: escaping, nested objects, arrays, numbers, booleans
- High risk of bugs (JSON parsing is complex)
- No practical benefit over dynamic sizing
- Violates KISS principle (ADR-029)

**Decision:** Use dynamic sizing approach (DynamicJsonDocument with file.size()). Provides excellent memory savings with minimal risk and code complexity.

---

### Phase 4: Optimize Dallas Labels Storage (1-2 hours)

Dallas sensor labels are the largest contributor to settings size growth. Consider separate file:

```cpp
// Instead of storing in main settings.ini:
char settingDallasLabels[JSON_BUFF_MAX] = "";  // 1024 bytes in RAM

// Store in separate file:
#define DALLAS_LABELS_FILE "/dallas_labels.ini"

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

## Memory Impact Summary

### Current vs Proposed Memory Usage

| Component | Current | Proposed | Savings |
|-----------|---------|----------|---------|
| **Write Operation** | | | |
| - JSON document (heap) | 2560 bytes | 0 bytes | 2560 bytes |
| - Field-by-field writes | N/A | 0 bytes | N/A |
| **Read Operation** | | | |
| - JSON document (stack) | 2560 bytes | 0 bytes | 2560 bytes |
| - JSON document (heap) | 0 bytes | ~800 bytes | -800 bytes |
| **Dallas Labels Storage** | | | |
| - In RAM (global) | 1024 bytes | 0 bytes | 1024 bytes |
| - Separate file | N/A | 0 bytes RAM | N/A |
| **Total Peak Memory** | ~6KB | ~1.6KB | **~4.5KB (70%)** |

### Breakdown of Savings

**Write Operation:**
- Current: 2560 bytes heap allocation
- Proposed: Field-by-field streaming (zero heap for JSON document)
- **Savings: 2560 bytes**

**Read Operation:**
- Current: 2560 bytes stack allocation (dangerous!)
- Proposed: Dynamic heap sizing (~600-900 bytes typical, max 3072)
- **Savings: ~1700-1960 bytes + eliminates stack overflow risk**

**Dallas Labels:**
- Current: 1024 bytes always in RAM
- Proposed: Load on-demand from separate file, zero persistent RAM
- **Savings: 1024 bytes**

**Net Result:**
- Total savings: ~4.5KB (~11% of ESP8266's 40KB available RAM)
- Eliminates critical stack overflow risk
- Improves scalability for future features

---

## Implementation Complexity Analysis

### Code Size Estimates (Lines of Code)

| Component | Current | Option 1: Dynamic | Option 2: Manual | Option 3: Multi-pass | Hybrid |
|-----------|---------|-------------------|------------------|---------------------|--------|
| **Write Function** | 70 lines | 80 lines (+10) | 120 lines (+50) | 90 lines (+20) | 80 lines (+10) |
| **Read Function** | 60 lines | 70 lines (+10) | 350 lines (+290) | 150 lines (+90) | 70 lines (+10) |
| **Helper Functions** | 0 lines | 0 lines | 200 lines (+200) | 50 lines (+50) | 40 lines (+40) |
| **Dallas Labels** | 30 lines | 40 lines (+10) | 40 lines (+10) | 40 lines (+10) | 40 lines (+10) |
| **Total New Code** | **0** | **+30 lines** | **+550 lines** | **+170 lines** | **+70 lines** |

### Effort Estimates (Person-Hours)

| Phase | Current | Option 1 | Option 2 | Option 3 | Hybrid |
|-------|---------|----------|----------|----------|--------|
| Planning & Analysis | N/A | 1h | 2h | 1.5h | 1h |
| Implementation | N/A | 2h | 12h | 6h | 4h |
| Testing | N/A | 2h | 8h | 5h | 3h |
| Debugging | N/A | 1h | 6h | 3h | 1.5h |
| Documentation | N/A | 0.5h | 2h | 1h | 0.5h |
| **Total Effort** | **0h** | **6.5h** | **30h** | **16.5h** | **10h** |

### Risk Assessment

| Risk Factor | Current | Option 1 | Option 2 | Option 3 | Hybrid |
|-------------|---------|----------|----------|----------|--------|
| **Stack Overflow** | 🔴 CRITICAL | 🟢 None | 🟢 None | 🟢 None | 🟢 None |
| **Memory Leak** | 🟡 Low | 🟢 Very Low | 🟡 Low | 🟡 Low | 🟢 Very Low |
| **Parsing Bugs** | 🟢 None | 🟢 Very Low | 🔴 HIGH | 🟡 Medium | 🟢 Very Low |
| **Flash Corruption** | 🟡 Low | 🟢 Very Low | 🟡 Low | 🟡 Low | 🟢 Very Low |
| **Backward Compat** | N/A | 🟢 Easy | 🟡 Medium | 🟡 Medium | 🟢 Easy |
| **Maintenance** | 🟢 Simple | 🟢 Simple | 🔴 Complex | 🟡 Medium | 🟢 Simple |

### Detailed Option Comparison

#### Option 1: Dynamic Sizing (DynamicJsonDocument)

**Implementation Complexity: ⭐⭐ (Very Low)**

```cpp
// BEFORE (60 lines)
StaticJsonDocument<2560> doc;
deserializeJson(doc, file);

// AFTER (70 lines, +10)
const size_t capacity = file.size() + JSON_OBJECT_SIZE(35) + 100;
if (capacity > 3072) { /* error */ }
DynamicJsonDocument doc(capacity);
deserializeJson(doc, file);
```

**Changes Required:**
- Replace `StaticJsonDocument<2560>` with `DynamicJsonDocument(calculated_size)` (**1 line change**)
- Add file size calculation (**3 lines**)
- Add safety check (**3 lines**)
- Add error logging (**3 lines**)
- **Total: ~10 new lines**

**Testing Complexity: Low**
- Same ArduinoJson API
- Only need to test capacity calculation
- Easy to verify with different file sizes

#### Option 2: Manual Character-by-Character Parser

**Implementation Complexity: ⭐⭐⭐⭐⭐⭐⭐⭐⭐⭐ (Very High)**

```cpp
// Pseudocode only - NOT RECOMMENDED
enum ParseState { EXPECT_KEY, IN_KEY, EXPECT_COLON, EXPECT_VALUE, IN_STRING_VALUE, IN_NUMBER_VALUE, ... };

void readSettings(bool show) {
  File file = LittleFS.open(SETTINGS_FILE, "r");
  
  ParseState state = EXPECT_KEY;
  char key[64];
  char value[256];
  int keyPos = 0, valuePos = 0;
  bool inEscape = false;
  int braceDepth = 0;
  
  while (file.available()) {
    char c = file.read();
    
    switch (state) {
      case EXPECT_KEY:
        if (c == '"') {
          state = IN_KEY;
          keyPos = 0;
        } else if (c == '}' && braceDepth == 0) {
          // End of JSON
          break;
        }
        // ... handle whitespace, commas ...
        break;
        
      case IN_KEY:
        if (inEscape) {
          // Handle \", \\, \n, etc.
          inEscape = false;
          key[keyPos++] = handleEscape(c);
        } else if (c == '\\') {
          inEscape = true;
        } else if (c == '"') {
          key[keyPos] = '\0';
          state = EXPECT_COLON;
        } else {
          key[keyPos++] = c;
        }
        break;
        
      // ... 10+ more states for: colon, value types, nested objects, arrays ...
      
      case IN_STRING_VALUE:
        // Similar escape handling
        // ... 50+ lines ...
        break;
        
      case IN_NUMBER_VALUE:
        // Parse integers, floats
        // ... 30+ lines ...
        break;
        
      // ... and so on ...
    }
  }
  
  file.close();
}

// Plus helper functions:
char handleEscape(char c) { /* 20 lines */ }
bool parseNumber(const char* str, int* intVal, float* floatVal) { /* 40 lines */ }
bool parseBool(const char* str) { /* 10 lines */ }
void applySettingValue(const char* key, const char* value) { /* 100 lines - big if/else chain */ }
```

**Changes Required:**
- Implement full JSON parser from scratch (**350+ lines**)
- Handle all JSON types: string, number, boolean, null, object, array (**100+ lines**)
- Implement escape sequence handling (**20+ lines**)
- Implement nested object support (**50+ lines**)
- Create state machine with 15+ states (**200+ lines**)
- Map 35+ settings to variables (**100+ lines**)
- Error handling for malformed JSON (**50+ lines**)
- **Total: ~550+ new lines**

**Testing Complexity: Very High**
- Must test all JSON constructs
- Edge cases: escaped quotes, nested objects, Unicode, malformed JSON
- Regression testing for each of 35+ settings
- **Estimated 100+ test cases needed**

**Maintenance Burden: Very High**
- Complex state machine hard to debug
- Easy to introduce bugs when adding new settings
- Difficult for other developers to understand
- High risk of security vulnerabilities (buffer overflows)

#### Option 3: Multi-pass Filtered Reads

**Implementation Complexity: ⭐⭐⭐⭐⭐⭐⭐ (High)**

```cpp
void readSettings(bool show) {
  File file = LittleFS.open(SETTINGS_FILE, "r");
  if (!file) return;
  
  // Pass 1: Read hostname
  {
    StaticJsonDocument<128> doc;
    StaticJsonDocument<64> filter;
    filter["hostname"] = true;
    
    file.seek(0);
    deserializeJson(doc, file, DeserializationOption::Filter(filter));
    strlcpy(settingHostname, doc["hostname"] | "", sizeof(settingHostname));
  }
  
  // Pass 2: Read MQTTenable
  {
    StaticJsonDocument<64> doc;
    StaticJsonDocument<64> filter;
    filter["MQTTenable"] = true;
    
    file.seek(0);
    deserializeJson(doc, file, DeserializationOption::Filter(filter));
    settingMQTTenable = doc["MQTTenable"] | settingMQTTenable;
  }
  
  // ... Repeat for all 35 settings (35 file seeks + deserializations!) ...
  
  file.close();
}
```

**Changes Required:**
- Create filtered read for each setting (**~90 lines**, repetitive)
- 35 file seeks (slow on flash) (**35 × 4 lines = 140 lines**)
- 35 small JsonDocuments (**35 allocations**)
- **Total: ~150+ new lines** (very repetitive)

**Performance Impact:**
- 35 file seeks on flash (SLOW - flash seek is expensive)
- 35 JSON parse operations
- Estimated **10-20x slower** than current implementation
- LittleFS flash seeks: ~5-20ms each × 35 = **175-700ms total read time**

**Testing Complexity: Medium**
- Need to verify each filtered read works
- Check for file seek failures
- Test performance impact

#### Hybrid Approach (Recommended)

**Implementation Complexity: ⭐⭐⭐ (Low)**

Combines best of multiple approaches:

```cpp
// Write: Field-by-field streaming (+40 lines)
void writeSettings(bool show) {
  File file = LittleFS.open(SETTINGS_FILE, "w");
  file.print(F("{"));
  writeJsonField(file, F("hostname"), settingHostname, false);
  writeJsonField(file, F("MQTTenable"), settingMQTTenable, true);
  // ... etc (simple helpers, easy to maintain)
  file.print(F("}"));
  file.close();
}

// Helper (20 lines total for string/int/bool versions)
void writeJsonField(File& file, const __FlashStringHelper* key, 
                    const char* value, bool addComma);

// Read: Dynamic sizing (+10 lines)
void readSettings(bool show) {
  File file = LittleFS.open(SETTINGS_FILE, "r");
  const size_t capacity = file.size() + JSON_OBJECT_SIZE(35) + 100;
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, file);
  // ... existing field extraction (unchanged)
  file.close();
}

// Dallas labels: Separate file (+40 lines)
void saveDallasLabels() { /* 20 lines */ }
void loadDallasLabels() { /* 20 lines */ }
```

**Changes Required:**
- Write helpers: 3 functions × ~15 lines each = **40 lines**
- Read optimization: **10 lines**
- Dallas labels file: **40 lines**
- **Total: ~90 new lines** (clean, maintainable)

**Benefits:**
- All the memory savings (4.5KB)
- Minimal code complexity
- Easy to review and test
- Easy to maintain
- Uses proven ArduinoJson library

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
- [ ] Measure actual settings file size on device
- [ ] Replace StaticJsonDocument with DynamicJsonDocument
- [ ] Calculate capacity as `file.size() + JSON_OBJECT_SIZE(35) + 100`
- [ ] Add safety check for maximum file size (3072 bytes)
- [ ] Move allocation from stack to heap (safer)
- [ ] Add error handling for out-of-memory
- [ ] Test read functionality with various file sizes
- [ ] Verify all settings load correctly
- [ ] Measure heap usage during read operation
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
