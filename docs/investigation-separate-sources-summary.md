# MQTT Source Separation - Quick Summary

**Full Document:** [investigation-separate-sources.md](./investigation-separate-sources.md)  
**Issue:** Separation of values from thermostat and boiler  
**Status:** Investigation Complete - Awaiting Decision

---

## The Problem in 30 Seconds

OpenTherm messages come from different sources (thermostat, boiler, gateway), but MQTT publishes all values to the same topic. When thermostat requests 20.5°C and boiler responds with 20.0°C, you only see the last value—losing visibility into what each device is doing.

**Example:**
```
Current Behavior:
  T10100000 → TSet = "20.5"  (published)
  B50100000 → TSet = "20.0"  (overwrites previous)
  Result: Only see "20.0", lost thermostat request

Desired Behavior:
  T10100000 → TSet_thermostat = "20.5"
  B50100000 → TSet_boiler = "20.0"
  Result: See both values
```

---

## Four Options Analyzed

### Option 1: Separate Topics with Suffix
- Topics: `TSet_thermostat`, `TSet_boiler`, `TSet_gateway`
- ✅ Clear, backward compatible
- ❌ 3x topics, higher memory

### Option 2: JSON Payloads
- Single topic with JSON: `{"thermostat": 20.5, "boiler": 20.0}`
- ✅ Rich data, fewer topics
- ❌ **Breaking change**, complex parsing

### Option 3: Hierarchical Topics
- Topics: `thermostat/TSet`, `boiler/TSet`
- ✅ Clean organization
- ❌ **Breaking change**, migration complexity

### Option 4: Feature Flag (Recommended)
- Default: Enabled (backward compatible)
- When enabled: Option 1 (separate topics)
- ✅ **Backward compatible**, user choice
- ❌ Two code paths to maintain

---

## Recommendation: Option 4

**Why:** Best balance of compatibility, functionality, and user control.

**Implementation:**
1. Add setting: `settingMQTTSeparateSources` (default: true - enabled)
2. Original topics always published (backward compatibility)
3. When enabled (default): Also publishes to source-specific topics
4. Auto-discovery: Creates separate Home Assistant sensors

**Memory Impact:**
- When disabled: 0 bytes
- When enabled (default): ~10KB for 30 messages (manageable)
- ESP8266 has ~45KB available, leaves ~35KB

---

## Example Implementation

```cpp
// Settings
bool settingMQTTSeparateSources = true;  // Default enabled, user can toggle in Web UI

// Publishing
void print_f88(float& value) {
  // ... extract value ...
  
  // Always publish to original topic
  sendMQTTData("TSet", value);
  
  // If feature enabled, also publish to source-specific topic
  if (settingMQTTSeparateSources) {
    switch(OTdata.rsptype) {
      case OTGW_THERMOSTAT:
        sendMQTTData("TSet_thermostat", value);
        break;
      case OTGW_BOILER:
        sendMQTTData("TSet_boiler", value);
        break;
      case OTGW_REQUEST_BOILER:
      case OTGW_ANSWER_THERMOSTAT:
        sendMQTTData("TSet_gateway", value);
        break;
    }
  }
  
  // Update state
  value = _value;
}
```

---

## Key Message IDs to Separate

**High Priority (most user impact):**
- **ID 1 (TSet):** Control setpoint - primary heating control
- **ID 16 (TrSet):** Room setpoint
- **ID 56 (TdhwSet):** DHW setpoint
- **ID 57 (MaxTSet):** Max CH setpoint
- **ID 14 (MaxRelModLevelSetting):** Max modulation level

**Medium Priority:**
- IDs 4, 8, 20-23, 58: Various settings and commands

**Low Priority:**
- IDs 11, 59-63, 78-79, 113-123: Advanced/diagnostic values

**Total:** ~30 WRITE and READ/WRITE message IDs would benefit

---

## Implementation Phases

**Phase 1 (Week 1-2): Core**
- Settings infrastructure
- MQTT publishing modifications
- Auto-discovery updates
- Web UI

**Phase 2 (Week 3): Testing**
- Unit tests (T/B/R/A handling)
- Integration tests (Home Assistant)
- Memory and performance

**Phase 3 (Week 4): Advanced**
- Per-message-ID control
- REST API endpoints
- Telemetry

**Phase 4 (Week 5+): Release**
- Documentation
- Migration guide
- Community support

---

## User Benefits

1. **Visibility:** See what thermostat requests vs what boiler provides
2. **Diagnostics:** Detect when boiler limits setpoint
3. **Monitoring:** Track gateway override activity
4. **Automation:** Create HA automations on difference between request and actual

**Example Home Assistant Automation:**
```yaml
# Alert when boiler can't meet thermostat demand
trigger:
  - platform: template
    value_template: >
      {{ (states('sensor.tset_thermostat')|float - 
          states('sensor.tset_boiler')|float) > 2.0 }}
action:
  - service: notify.mobile_app
    data:
      message: "Boiler cannot meet thermostat demand"
```

---

## Migration for Existing Users

**Before upgrade:** Works as today  
**After upgrade (default):** Works as today (no changes)  
**After enabling feature:** New topics appear, old topics still work

**No breaking changes** - backward compatible by design.

---

## Next Steps

Need maintainer feedback on:
1. ✅ Approve Option 4 as implementation approach?
2. ❓ Which message IDs to prioritize (all WRITE/RW or subset)?
3. ❓ Default setting value (false = safest, but users must enable)
4. ❓ Target release version (v1.2.0?)
5. ❓ Testing requirements before merge

---

## Key Decisions Points

| Decision | Options | Recommendation |
|----------|---------|----------------|
| **Approach** | Options 1-4 | **Option 4: Feature Flag** |
| **Default** | Enabled/Disabled | **Disabled** (safest) |
| **Scope** | All IDs / WRITE only / High priority only | **All WRITE/RW** (~30 IDs) |
| **Topics** | Suffix / Hierarchical / JSON | **Suffix** (`_thermostat`, `_boiler`, `_gateway`) |
| **Backward Compat** | Breaking / Compatible | **Compatible** (critical) |

---

## Quick References

- **Full Investigation:** [investigation-separate-sources.md](./investigation-separate-sources.md)
- **ADR-006:** MQTT Integration Pattern
- **ADR-038:** OpenTherm Message Data Flow Pipeline
- **OpenTherm Spec:** `Specification/OpenTherm-Protocol-Specification-v4.2.pdf`
- **Current Code:** `OTGW-Core.ino` lines 1550-1920, `MQTTstuff.ino`

---

**Document Version:** 1.0  
**Date:** 2026-02-16  
**Prepared By:** GitHub Copilot Advanced Agent  
**Status:** Awaiting Maintainer Review
