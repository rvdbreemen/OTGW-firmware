# ADR-040: MQTT Source-Specific Topics for OpenTherm Values

**Status:** Accepted  
**Date:** 2026-02-16  
**Decision Maker:** User: Rob van den Breemen (rvdbreemen)

## Context

### Problem Statement

MQTT topics currently publish OpenTherm values from both thermostat (master) and boiler (slave) to the same topic. When the thermostat requests `TSet=20.5°C` and the boiler responds `TSet=20.0°C`, only the last value is visible in MQTT, losing visibility into what each device is doing.

**Example scenario:**
```
1. Thermostat sends: T10100000 (TSet=20.5°C - what thermostat wants)
2. Boiler responds:  B50100000 (TSet=20.0°C - what boiler provides)
3. Current MQTT:     TSet = "20.0" (only shows boiler's value)
4. Problem:          Lost visibility into thermostat's request
```

This creates issues when:
- **Diagnostics:** Cannot see when boiler limits requested temperature
- **Monitoring:** Cannot track gateway override operations separately
- **Troubleshooting:** Cannot determine if issue is thermostat request or boiler response
- **Automation:** Cannot create Home Assistant automations based on request vs actual values

### Background

OpenTherm Gateway (OTGW) PIC firmware prefixes messages to indicate source:
- `T` = Thermostat (Master) request
- `B` = Boiler (Slave) response
- `R` = Gateway request to boiler (overriding thermostat)
- `A` = Gateway answer to thermostat (overriding boiler)
- `E` = Parity error

Current firmware detects these prefixes in `processOT()` (OTGW-Core.ino:1568-1582) and stores in `OTdata.rsptype` but publishes all values to the same MQTT topic via `sendMQTTData()`.

### Constraints

- **Memory:** ESP8266 has ~45KB RAM available; additional topics must not exhaust memory
- **Backward Compatibility:** Existing Home Assistant integrations must continue working
- **OTmonitor Compatibility:** Should not break compatibility with OTmonitor expectations
- **Performance:** MQTT publishing rate must not cause backpressure or dropped messages
- **User Choice:** Users should be able to opt in/out based on their needs

### Stakeholders

- **End Users:** Need visibility into thermostat vs boiler values for diagnostics
- **Home Assistant Users:** Want to create automations based on request vs actual values
- **OTmonitor Users:** Expect compatibility with existing workflows
- **Developers:** Need clear patterns for MQTT value publishing

## Decision

**Implement source-specific MQTT topics with a feature flag (`settingMQTTSeparateSources`) that is enabled by default but maintains full backward compatibility.**

When enabled, publish OpenTherm values to both:
1. **Original topic** (for backward compatibility): `<prefix>/value/<node-id>/<sensor>`
2. **Source-specific topics** (new): `<prefix>/value/<node-id>/<sensor>_<source>`

Where `<source>` is:
- `thermostat` - for T messages (master requests)
- `boiler` - for B messages (slave responses)
- `gateway` - for R/A messages (gateway overrides)

**Example:**
```
Original (always published):
  otgw-firmware/value/otgw/TSet → "20.0" (last value)

Source-specific (when enabled):
  otgw-firmware/value/otgw/TSet_thermostat → "20.5"
  otgw-firmware/value/otgw/TSet_boiler → "20.0"
  otgw-firmware/value/otgw/TSet_gateway → "19.5"
```

### Why This Choice

1. **User Decision:** Maintainer explicitly chose this approach with default enabled
2. **Zero Breaking Changes:** Original topics continue working, existing integrations unaffected
3. **Opt-Out Available:** Users can disable if they don't need source tracking
4. **Clear Semantics:** Suffix naming makes source immediately obvious
5. **Home Assistant Ready:** Auto-discovery automatically creates source-specific sensors
6. **Flexible:** Can be extended with per-message-ID control if needed

### Implementation Summary

**High-level approach:**
1. Add boolean setting `settingMQTTSeparateSources` (default: `true`)
2. Modify `print_f88()`, `print_u16()`, `print_s16()`, `print_s8s8()` functions
3. When feature enabled, call `publishToSourceTopic()` after publishing to original topic
4. Extend MQTT auto-discovery to create configs for source-specific topics
5. Add Web UI checkbox to enable/disable feature
6. Apply to all 30 WRITE and READ/WRITE message IDs

**Scope:** All 30 WRITE/RW message IDs (as per user decision)

**Target Release:** v1.2.0

## Alternatives Considered

### Alternative 1: Topic Suffix Only (Always Enabled)

**Description:** Always publish to source-specific topics, no feature flag.

**Pros:**
- Simpler code (no conditional logic)
- Cleaner implementation
- Always available

**Cons:**
- No opt-out for users who don't need it
- Memory always consumed (~10KB)
- No flexibility

**Why Not Chosen:** User requested default enabled BUT with backward compatibility and opt-out capability. Feature flag provides this flexibility.

### Alternative 2: JSON Payloads

**Description:** Single topic with JSON containing all sources:
```json
{
  "thermostat": 20.5,
  "boiler": 20.0,
  "gateway": null,
  "last": "boiler"
}
```

**Pros:**
- Single topic per sensor
- Rich data model
- Lower MQTT broker load

**Cons:**
- **Breaking change:** Existing integrations would break
- Home Assistant requires template sensors (not auto-discovered)
- ArduinoJson adds memory overhead
- Not compatible with OTmonitor expectations

**Why Not Chosen:** Breaking changes unacceptable per requirement for backward compatibility.

### Alternative 3: Hierarchical Topic Structure

**Description:** Organize by source first:
```
otgw-firmware/value/otgw/thermostat/TSet
otgw-firmware/value/otgw/boiler/TSet
otgw-firmware/value/otgw/gateway/TSet
```

**Pros:**
- Clean logical organization
- MQTT wildcard filtering possible
- Clear semantic grouping

**Cons:**
- **Breaking change:** Incompatible with existing topic structure
- Complex migration required
- Double MQTT traffic if maintaining backward compat

**Why Not Chosen:** Breaking changes unacceptable. Suffix approach maintains backward compatibility.

### Alternative 4: Disabled by Default

**Description:** Same as chosen approach but default `settingMQTTSeparateSources=false`.

**Pros:**
- Safest possible approach
- Zero impact for users who don't enable it

**Cons:**
- Users won't discover the feature
- Less useful by default

**Why Not Chosen:** User explicitly requested "Default enabled, BUT backward compatible." Default enabled provides better out-of-box experience while maintaining backward compatibility.

## Consequences

### Positive

- **Visibility:** Users can see thermostat requests vs boiler actual values
  - Example: See that thermostat wants 22°C but boiler limits to 20°C
- **Diagnostics:** Easier troubleshooting of heating system issues
  - Can identify if problem is thermostat request or boiler limitation
- **Monitoring:** Track gateway override activity separately from normal operation
  - See when user overrides via MQTT commands are active
- **Automation:** Home Assistant automations can use request vs actual differences
  - Example: Alert when boiler can't meet thermostat demand for >30 min
- **Backward Compatible:** Existing integrations continue working unchanged
  - Original topics still published with same behavior
- **User Control:** Feature can be disabled if not needed
  - Users who don't want extra topics can turn off in Web UI
- **Home Assistant Auto-Discovery:** New sensors appear automatically
  - No manual configuration needed for source-specific sensors

### Negative

- **Memory Usage:** ~10KB additional RAM when enabled (30 messages × 3 sources × ~100 bytes)
  - **Mitigation:** Feature can be disabled; ESP8266 has ~45KB available, leaving ~35KB
  - **Accepted:** Memory cost is reasonable for the functionality provided
- **MQTT Topic Count:** 3x topics for each separated message (90 additional topics)
  - **Mitigation:** Only for WRITE/RW messages where it provides value
  - **Accepted:** Modern MQTT brokers handle hundreds of topics easily
- **Code Complexity:** Two code paths (feature enabled vs disabled)
  - **Mitigation:** Clean abstraction via `publishToSourceTopic()` function
  - **Accepted:** Necessary for backward compatibility and user choice
- **Home Assistant Entity Count:** More sensors appear in HA
  - **Mitigation:** Users can hide/ignore unwanted entities
  - **Accepted:** Users who want this feature want the entities

### Risks & Mitigation

- **Risk:** Gateway override skip logic may interfere with source separation
  - **Impact:** Original T/B messages might be skipped when gateway overrides
  - **Mitigation:** Review skip logic in processOT() lines 1640-1645; may need adjustment
  - **Status:** Requires testing with actual gateway override scenarios

- **Risk:** Memory exhaustion under high message rates
  - **Impact:** Could cause heap exhaustion and crashes
  - **Mitigation:** Existing `canPublishMQTT()` backpressure (ADR-030) protects heap
  - **Status:** Existing safeguards should handle this

- **Risk:** MQTT broker overload with 3x message rate
  - **Impact:** Messages could be dropped or delayed
  - **Mitigation:** Modern brokers handle this; can disable feature if issues occur
  - **Status:** Unlikely given typical message rates (1-4 messages/second)

- **Risk:** Users confused by multiple topics for same sensor
  - **Impact:** Support burden increases
  - **Mitigation:** Clear documentation and examples in README/wiki
  - **Status:** Documentation required before release

### Impact Areas

- **Performance:** Minimal - MQTT publish is already async, 3x topics adds negligible overhead
- **Maintainability:** Moderate increase due to dual code paths, but isolated to MQTT publishing functions
- **Security:** No impact - same security model applies to all topics
- **Scalability:** Slight decrease - more topics per device, but still manageable
- **Developer Experience:** Improved - clearer separation of concerns in MQTT publishing

## Implementation Notes

### Key Files/Modules Affected

- `src/OTGW-firmware/OTGW-Core.ino` - Modify `print_f88()`, `print_u16()`, `print_s16()`, `print_s8s8()` functions
- `src/OTGW-firmware/MQTTstuff.ino` - Add `publishToSourceTopic()` helper function
- `src/OTGW-firmware/MQTTstuff.ino` - Extend `doAutoConfigureMsgid()` for source-specific topics
- `src/OTGW-firmware/settingStuff.ino` - Add `settingMQTTSeparateSources` boolean
- `src/OTGW-firmware/data/index.html` - Add Web UI checkbox for feature toggle
- `src/OTGW-firmware/OTGW-firmware.h` - Add setting struct member
- `docs/adr/README.md` - Add ADR-040 to index

### Code Examples

**Modified print_f88() function:**
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
    
    // If feature enabled, also publish to source-specific topic
    if (settingMQTTSeparateSources) {
      publishToSourceTopic(baseTopic, _msg, OTdata.rsptype);
    }
    
    // Update state
    value = _value;
  }
}
```

**New helper function:**
```cpp
void publishToSourceTopic(const char* baseTopic, const char* value, byte rsptype) {
  char sourceTopic[MQTT_TOPIC_MAX_LEN];
  const char* suffix;
  
  switch(rsptype) {
    case OTGW_THERMOSTAT:
      suffix = "_thermostat";
      break;
    case OTGW_BOILER:
      suffix = "_boiler";
      break;
    case OTGW_REQUEST_BOILER:
    case OTGW_ANSWER_THERMOSTAT:
      suffix = "_gateway";
      break;
    default:
      return;  // Don't publish for error messages
  }
  
  snprintf_P(sourceTopic, sizeof(sourceTopic), PSTR("%s%s"), baseTopic, suffix);
  sendMQTTData(sourceTopic, value);
  
  // Send auto-discovery config for this topic if not already sent
  if (needsMQTTConfig(sourceTopic)) {
    doAutoConfigureSourceTopic(baseTopic, suffix);
    setMQTTConfigDone(sourceTopic);
  }
}
```

**MQTT Auto-Discovery extension:**
```cpp
void doAutoConfigureSourceTopic(const char* baseTopic, const char* suffix) {
  // Build discovery config for source-specific topic
  // Topic: homeassistant/sensor/<node-id>_<sensor>_<source>/config
  // Payload: Same as base sensor but with modified name and unique_id
  
  char configTopic[MQTT_TOPIC_MAX_LEN];
  char uniqueId[64];
  
  snprintf_P(uniqueId, sizeof(uniqueId), PSTR("%s%s"), baseTopic, suffix);
  snprintf_P(configTopic, sizeof(configTopic), 
             PSTR("homeassistant/sensor/%s_%s/config"), 
             settingMqttUniqueID, uniqueId);
  
  // Build JSON config with friendly name including source
  // Example: "Control Setpoint (Thermostat)"
  // ... (implementation similar to existing doAutoConfigureMsgid)
}
```

### Migration Required

None. Feature is additive and backward compatible. Existing MQTT topics and Home Assistant configurations continue working unchanged.

### Message IDs Affected

**All 30 WRITE and READ/WRITE message IDs** (per user decision):

**High Priority (5):**
- ID 1: TSet (Control setpoint)
- ID 16: TrSet (Room setpoint)
- ID 56: TdhwSet (DHW setpoint)
- ID 57: MaxTSet (Max CH setpoint)
- ID 14: MaxRelModLevelSetting (Max modulation level)

**Medium Priority (12):**
- ID 8: TsetCH2 (Control setpoint CH2)
- ID 4: Command (Remote command)
- ID 20: DayTime (Day of week and time)
- ID 21: Date (Calendar date)
- ID 22: Year (Calendar year)
- ID 23: TrSetCH2 (Room setpoint CH2)
- ID 58: Hcratio (OTC heat curve ratio)
- ID 59-63: Remoteparameter4-8

**Low Priority (13):**
- ID 11: TSPindexTSPvalue (Transparent slave parameters)
- ID 78-79: Ventilation parameters
- ID 87: NominalVentilationValue
- ID 89: TSPEntryVH
- ID 106: SolarStorageTSPindexTSPvalue
- ID 113-123: Counters (burner starts, operation hours, etc.)

## Verification

### How to Verify This Decision

1. **Enable feature in Web UI** or settings
2. **Send OpenTherm messages** via OTmonitor or real hardware
3. **Subscribe to MQTT topics:**
   ```bash
   mosquitto_sub -h localhost -v -t 'otgw-firmware/#'
   ```
4. **Verify topics published:**
   - Original topic: `otgw-firmware/value/otgw/TSet`
   - Source-specific: `otgw-firmware/value/otgw/TSet_thermostat`, `TSet_boiler`, `TSet_gateway`
5. **Check Home Assistant:** New sensors should appear via auto-discovery

### Testing Requirements

**Unit Tests:**
- Test with T messages only → publishes to original + _thermostat
- Test with B messages only → publishes to original + _boiler
- Test with R/A messages → publishes to original + _gateway
- Test with feature disabled → publishes only to original topic
- Test with interleaved T/B messages → both source topics updated correctly

**Integration Tests:**
- Home Assistant auto-discovery creates source-specific sensors
- Original sensors continue working unchanged
- MQTT broker handles increased topic count
- Memory usage remains within limits

**Performance Tests:**
- Measure heap usage with feature enabled vs disabled
- Verify MQTT publish rate doesn't cause backpressure
- Test with 50+ messages/second load
- Confirm no memory leaks

### Monitoring/Metrics

- **Heap monitoring:** Track `ESP.getFreeHeap()` with feature enabled
- **MQTT publish count:** Track successful vs dropped publishes
- **Topic count:** Monitor MQTT broker topic count
- **Auto-discovery success:** Track successful HA discovery configs sent

## Related Decisions

- **Depends on:** ADR-006 (MQTT Integration Pattern) - Foundation for MQTT publishing
- **Related to:** ADR-038 (OpenTherm Data Flow Pipeline) - Message source detection
- **Related to:** ADR-030 (Heap Memory Monitoring) - Backpressure protection
- **Related to:** ADR-016 (OpenTherm Command Queue) - Command handling patterns
- **Extends:** ADR-006 with source-specific publishing capability

## References

- **Investigation Document:** `docs/investigation-separate-sources.md`
- **Quick Summary:** `docs/investigation-separate-sources-summary.md`
- **Issue:** GitHub issue "separation of values from thermostat and boiler"
- **Implementation PR:** Branch `copilot/separate-thermostat-boiler-values`
- **OpenTherm Spec:** `Specification/OpenTherm-Protocol-Specification-v4.2.pdf`
- **Source Detection Code:** `src/OTGW-firmware/OTGW-Core.ino:1568-1582`
- **MQTT Publishing Code:** `src/OTGW-firmware/OTGW-Core.ino:628-700` (print_* functions)
- **Home Assistant MQTT Discovery:** https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery

## Timeline

- **2026-02-16:** Investigation completed, four options analyzed
- **2026-02-16:** User decision - Option 4 with default enabled
- **2026-02-16:** ADR created and accepted
- **2026-02-16:** Implementation started (target: v1.2.0)
- **TBD:** Implementation completed and tested
- **TBD:** Released in firmware v1.2.0

---

**Metadata:**
- **ADR Number:** 040
- **Status:** Accepted
- **Category:** Integration & Communication
- **Impact:** Medium (affects MQTT publishing, memory usage, user experience)
- **Backward Compatible:** Yes (critical requirement)
