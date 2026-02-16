# Investigation Complete: MQTT Source Separation

## TL;DR

**Issue:** Users cannot distinguish between thermostat requests and boiler responses in MQTT topics.

**Solution:** Add optional feature to publish source-specific topics (`TSet_thermostat`, `TSet_boiler`, `TSet_gateway`) while maintaining backward compatibility.

**Status:** ✅ Investigation complete, awaiting maintainer decision to proceed with implementation.

---

## Quick Decision Guide

**Do you approve this approach?**
- ✅ **Yes, implement as proposed** → Proceed with Phase 1 implementation
- 🤔 **Yes, but with modifications** → Please specify changes needed
- ❌ **No, prefer different approach** → Please specify preferred option (1-4 from investigation)
- ⏸️ **Need more information** → Please specify what additional info needed

---

## What Was Done

### Investigation Documents Created

1. **Full Investigation** (`docs/investigation-separate-sources.md`)
   - 37KB comprehensive analysis
   - Technical deep-dive with code examples
   - Four implementation options analyzed
   - Memory calculations and performance analysis
   - Phase-by-phase implementation plan
   - Testing strategy
   - Migration guide

2. **Quick Summary** (`docs/investigation-separate-sources-summary.md`)
   - 6KB executive summary
   - Key decision points
   - Quick reference for maintainers

### What Was Analyzed

- OpenTherm protocol message flow (T/B/R/A prefixes)
- Current MQTT publishing architecture
- Memory constraints (ESP8266 has ~45KB available)
- 30+ message IDs that need source separation
- Impact on Home Assistant integration
- Backward compatibility requirements
- Four alternative implementation approaches

---

## Recommended Approach

### Option 4: Feature Flag with Opt-In

**Default Behavior:** Disabled - works exactly as today (no changes)

**When Enabled:**
```
Original topic (backward compat):
  otgw-firmware/value/otgw/TSet → "20.0"

New source-specific topics:
  otgw-firmware/value/otgw/TSet_thermostat → "20.5"
  otgw-firmware/value/otgw/TSet_boiler → "20.0"
  otgw-firmware/value/otgw/TSet_gateway → "19.5"
```

**Why This Approach?**
- ✅ No breaking changes (disabled by default)
- ✅ User controlled (opt-in via Web UI)
- ✅ Backward compatible (original topics still work)
- ✅ Memory efficient (0 bytes when disabled, ~10KB when enabled)
- ✅ Future-proof (can extend with per-message control)

---

## Implementation Effort

### Phase 1: Core (Week 1-2)
- Settings infrastructure
- MQTT publishing modifications  
- Auto-discovery updates
- Web UI checkbox

### Phase 2: Testing (Week 3)
- Unit tests
- Integration tests (Home Assistant)
- Memory/performance validation

### Phase 3: Advanced (Week 4)
- Per-message-ID control
- REST API endpoints
- Telemetry

### Phase 4: Release (Week 5+)
- Documentation
- Migration guide
- Community support

**Total Estimated Effort:** 3-5 weeks for full implementation and testing

---

## Key Benefits

1. **Visibility:** See thermostat requests vs boiler actual values
2. **Diagnostics:** Detect when boiler limits setpoint
3. **Monitoring:** Track gateway override activity
4. **Automation:** Home Assistant automations on request/actual differences

**Example Use Case:**
```yaml
# Alert when boiler can't meet thermostat demand
automation:
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

## Decisions Needed

### 1. Approach Approval ⭐ (Required)
- [ ] Approve Option 4 (Feature Flag with Opt-In)?
- [ ] Or prefer different option (1, 2, or 3)?

### 2. Scope Selection ⭐ (Required)
- [ ] All 30 WRITE/RW message IDs?
- [ ] Just high priority 5 IDs (TSet, TrSet, TdhwSet, MaxTSet, MaxRelModLevelSetting)?
- [ ] Custom list (please specify)?

### 3. Default Setting ⭐ (Required)
- [ ] Disabled by default (safest, recommended)?
- [ ] Enabled by default (more features, but uses RAM)?

### 4. Release Target
- [ ] v1.2.0 (next feature release)?
- [ ] v1.3.0 (later)?
- [ ] No specific target?

### 5. Additional Requirements
- [ ] Create ADR for this architectural change?
- [ ] Beta testing with community before release?
- [ ] Specific testing requirements?
- [ ] Documentation updates needed?

---

## Risk Assessment

### Low Risk ✅
- No breaking changes when disabled
- Backward compatible by design
- Feature flag allows safe rollback
- Memory impact only when enabled

### Medium Risk ⚠️
- Increased code complexity (two code paths)
- More MQTT topics when enabled (broker load)
- Home Assistant may create many entities

### Mitigation Strategies
- Default to disabled
- Comprehensive testing before release
- Clear documentation and migration guide
- Community beta testing
- Can be disabled per-message-ID if needed

---

## Next Steps

**If Approved:**
1. Create feature branch from dev
2. Implement Phase 1 (core functionality)
3. Create PR for review
4. Beta test with community
5. Iterate based on feedback
6. Merge and release

**If More Discussion Needed:**
- Schedule discussion on Discord
- Review investigation documents together
- Address any concerns or questions
- Refine approach based on feedback

---

## Questions?

- **Full Technical Details:** See `docs/investigation-separate-sources.md`
- **Quick Reference:** See `docs/investigation-separate-sources-summary.md`
- **Code Examples:** Both documents include implementation samples
- **Memory Analysis:** Detailed calculations in full investigation

**Contact:** GitHub issue comments or Discord

---

**Investigation By:** GitHub Copilot Advanced Agent  
**Date:** 2026-02-16  
**Status:** ✅ Complete, awaiting decision  
**Branch:** `copilot/separate-thermostat-boiler-values`
