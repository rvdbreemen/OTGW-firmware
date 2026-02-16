# ADR-033: Dallas Sensor Custom Labels and Graph Visualization

## Status
Accepted

## Context

ADR-020 established Dallas DS18B20 sensor integration for the OTGW firmware, providing basic temperature sensing via I2C. However, users faced usability challenges:

1. **Sensor Identification**: Multiple sensors displayed as hex addresses (e.g., "28FF64D1841703F1"), making it difficult to identify which sensor is which physical location
2. **Data Visualization**: Temperature data was only available via REST API and main UI table; no graphing capability to visualize trends over time
3. **User Experience**: No way to customize sensor names for easier identification in home automation systems

**Constraints:**
- Must maintain backward compatibility with existing Dallas sensor functionality
- Limited ESP8266 RAM (~40KB available) requires careful memory management
- Labels must persist across reboots
- Must integrate with existing MQTT, REST API, and Web UI systems
- Must not block WebSocket traffic or real-time updates

**Scope:**
This ADR documents the addition of custom sensor labels and graph visualization as an enhancement to ADR-020, not a replacement or modification of the original decision.

## Decision

Implement a comprehensive sensor labeling and graphing system with three main components:

### 1. Persistent Custom Labels

**Storage:**
- Store labels as JSON key-value pairs in `settingDallasLabels[JSON_BUFF_MAX]` (1024 bytes)
- Format: `{"28FF64D1841703F1": "Living Room", "28AB12CD34567890": "Bedroom"}`
- Add `label[17]` field to `DallasrealDevice` structure (16 chars + null terminator)
- Persist via LittleFS in settings.json (existing ADR-008 pattern)

**API:**
- New REST endpoint: `POST /api/v1/sensors/label` for updating labels
- Expose labels in V1/V2 API as `{address}_label` fields
- Validate inputs: hex address (16 chars, [0-9A-Fa-f]), label length (≤16 chars), sensor existence

**Security:**
- Use ArduinoJson for all API responses (prevent JSON injection)
- Add `escapeJsonString()` helper for label output in V1/V2 APIs
- Validate hex addresses and check sensor existence before allowing label updates
- Buffer overflow protection with `measureJson()` checks before serialization

### 2. Dynamic Graph Visualization

**Frontend Detection:**
- Scan `/api/v2/otgw/otmonitor` for 16-char hex addresses matching `^(28|10|22)[0-9A-Fa-f]{14}$`
- Auto-register sensors on first appearance with unique colors (16-color palette per theme)
- Use custom labels if available, fallback to "Sensor N (hexprefix)" format
- Validate temperature range (-50°C to 150°C)

**Color Assignment:**
- 16 distinct colors for light theme (blues, greens, oranges, reds, purples)
- 16 distinct colors for dark theme (brighter versions for contrast)
- Assign colors via `colorIndex = sensorIndex % 16`
- Add sensors to existing temperature grid (gridIndex 4) alongside OpenTherm values

**Real-time Updates:**
- Poll API every 1 second for new sensor data
- Batch graph updates every 2 seconds to reduce render load
- Support up to 16 sensors (MAXDALLASDEVICES limit)
- Memory-efficient: bounded arrays with 864K points per sensor max

### 3. Non-Blocking Label Editor

**UI Pattern:**
- Replace blocking `prompt()` with custom modal dialog (ADR-034)
- Click sensor name on main page to edit label
- Modal features: keyboard shortcuts (Enter/Escape), inline validation, theme-aware styling
- WebSocket traffic and screen updates continue during editing

**Implementation:**
- Custom HTML/CSS modal dialog structure
- Event handlers: overlay click, Escape key, Enter key, button clicks
- Auto-focus input field with text pre-selection
- Inline error display (no blocking `alert()`)

## Alternatives Considered

### Alternative 1: Dedicated Label Settings Page
**Rejected:** Adds complexity and requires navigation away from main view. Inline editing is more user-friendly.

### Alternative 2: Labels Stored in Separate File
**Rejected:** Would require additional file I/O operations and complicate backup/restore. JSON in settings.json is simpler and leverages existing persistence pattern (ADR-008).

### Alternative 3: Client-Side Only Labels (LocalStorage)
**Rejected:** Labels wouldn't sync across devices or persist if browser cache cleared. Server-side storage ensures consistency.

### Alternative 4: Use Third-Party Charting Library
**Rejected:** ECharts already integrated and working well. No need for additional dependency.

### Alternative 5: ES5 JavaScript for Legacy Browser Support
**Rejected:** Modern browsers (Chrome, Firefox, Safari) all support ES6. Code maintainability and developer experience outweigh need for IE11 support. Target audience uses modern browsers.

## Consequences

### Positive

1. **User Experience:**
   - Sensor identification via custom labels reduces cognitive load
   - Graph visualization enables trend analysis
   - Non-blocking modal preserves real-time data flow

2. **Integration:**
   - Labels exposed in MQTT, REST API, and Web UI consistently
   - Works seamlessly with Home Assistant MQTT discovery
   - Backward compatible: defaults to hex address if no label set

3. **Maintainability:**
   - Follows existing patterns (ADR-008, ADR-018, ADR-019)
   - Well-documented with comprehensive ADR
   - Security-first approach prevents injection attacks

4. **Performance:**
   - Minimal memory overhead: 17 bytes per sensor for label
   - Efficient graph updates: batched rendering every 2 seconds
   - No impact on real-time OpenTherm data flow

### Negative

1. **Memory Usage:**
   - Increased settings JSON from 1536 to 2560 bytes (1KB increase)
   - Label storage requires 1024 bytes (JSON_BUFF_MAX)
   - Total overhead: ~2KB RAM (acceptable on ESP8266 with 40KB available)

2. **Browser Compatibility:**
   - Requires modern browser with ES6 support
   - No support for IE11 or very old mobile browsers
   - Acceptable trade-off for better code maintainability

3. **Storage Limits:**
   - Maximum 16 sensors supported (hardware limit)
   - Label buffer overflow protection prevents silent corruption
   - Must handle "out of space" gracefully

4. **Complexity:**
   - Adds ~600 lines of code (backend + frontend)
   - More error paths to test
   - Additional documentation burden

### Risks and Mitigations

**Risk 1: Buffer Overflow**
- **Mitigation:** Use `measureJson()` before serialization, check truncation after `serializeJson()`
- **Code:** sensors_ext.ino:327-345

**Risk 2: JSON Injection**
- **Mitigation:** Use ArduinoJson for responses, escape user-controlled strings with `escapeJsonString()`
- **Code:** jsonStuff.ino:14-51, restAPI.ino:595-604, 525-536

**Risk 3: Settings Corruption**
- **Mitigation:** Validate inputs before saving, provide fallback to hex address on load failure
- **Code:** restAPI.ino:894-903 (validation)

**Risk 4: Memory Exhaustion**
- **Mitigation:** Fixed-size buffers, bounded arrays, batched graph updates
- **Limits:** 16 sensors max, 16-char labels max, 1024-byte label buffer

## Implementation

**Files Modified:**
- `OTGW-firmware.h` - Added label storage fields
- `sensors_ext.ino` - Label loading/saving with overflow protection
- `settingStuff.ino` - Increased JSON capacity to 2560 bytes
- `restAPI.ino` - New endpoint, validation, safe JSON output
- `jsonStuff.ino` - Added escapeJsonString() helper
- `data/graph.js` - Sensor detection and graphing
- `data/index.js` - Label editing integration
- `data/index.html` - Modal dialog HTML
- `data/index.css` - Modal styling (light theme)
- `data/index_dark.css` - Modal styling (dark theme)

**API Format:**
```json
POST /api/v1/sensors/label
{
  "address": "28FF64D1841703F1",
  "label": "Living Room"
}

Success: {"success": true, "address": "...", "label": "..."}
Error: {"success": false, "error": "..."}
```

**Memory Calculation:**
```
Worst-case: 16 sensors × 60 bytes JSON each = 960 bytes
Buffer size: 1024 bytes (JSON_BUFF_MAX)
Safety margin: 64 bytes (6.7%)
Settings capacity: 2560 bytes (previously 1536, +1024)
```

## Related Decisions

- **ADR-008 (LittleFS Configuration Persistence):** Labels stored in settings.json using existing persistence pattern
- **ADR-018 (ArduinoJson Data Interchange):** JSON serialization for label storage and API responses
- **ADR-019 (REST API Versioning Strategy):** New endpoint in v1 API maintains backward compatibility
- **ADR-034 (Non-Blocking Modal Dialogs):** Modal pattern used for label editing to preserve WebSocket flow
- **ADR-020 (Dallas DS18B20 Sensor Integration):** This ADR extends ADR-020 with custom labels and graph visualization

## Evidence

- **Commit 95bd160:** Added visual edit indicator and temperature precision formatting
- **Commit b2acbd7:** Implemented custom label support backend
- **Commit 7c3a711:** Replaced blocking prompt() with non-blocking modal
- **Commit 19e5210:** Security fixes (JSON injection, buffer overflow)
- **Commit a767276:** API documentation updates
- **Implementation Docs:** TEMPERATURE_SENSOR_GRAPH_IMPLEMENTATION.md, TEMPERATURE_SENSOR_FINAL_SUMMARY.md
- **Code Review:** All automated code review feedback addressed

## Date

2026-02-04 (Initial implementation)
2026-02-07 (Security fixes and ADR documentation)
