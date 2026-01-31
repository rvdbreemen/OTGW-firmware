# ADR-025: PIC Version-Aware Command Validation

**Status:** Accepted  
**Date:** 2026-01-31  
**Category:** MQTT Integration & Hardware Compatibility  
**Related ADRs:** ADR-006 (MQTT Integration Pattern), ADR-012 (PIC Firmware Upgrade), ADR-004 (Static Buffer Allocation)

## Context

The OTGW hardware supports two different PIC microcontroller versions with different capabilities:

**PIC16F88 (Original):**
- Older hardware versions (OTGW 6.x, legacy systems)
- Supports 39 standard OpenTherm commands
- No cooling system support
- No advanced mode selection
- Limited service functions

**PIC16F1847 (Enhanced):**
- Newer hardware versions (OTGW 7.x, NodoShop OTGW)
- Supports all 52 commands (39 standard + 12 advanced)
- Cooling system control
- Advanced operating modes (comfort, reduced, protection, etc.)
- Full transparent parameter access
- Fail-safe mode and DHW blocking

**Problem:**
- MQTT interface exposed 52 commands without validation
- Users with PIC16F88 could request commands not supported by their hardware
- Unsupported commands would be silently ignored or cause errors
- No feedback when incompatible commands were used
- Difficult to debug which commands work on which hardware version

**Requirements:**
- Prevent unsupported commands from being sent
- Provide clear error messages when incompatible commands are requested
- Log errors to debug interface for troubleshooting
- Maintain backward compatibility with existing configurations
- Minimal performance impact (command validation on every MQTT message)
- Store version requirements in PROGMEM (flash, not RAM)

**Current State (Branch commits):**
- Expanded from 29 to 52 total MQTT commands
- 23 new commands added (10 standard, 12 advanced PIC16F1847-only)
- All new commands fully functional
- No validation of hardware compatibility before sending

## Decision

**Implement PIC version-aware command validation in the MQTT command processor.**

**Architecture:**

1. **Command Metadata Structure:**
   ```cpp
   enum PIC_VERSION {
     PIC_ALL = 0,          // Available on all PIC versions
     PIC_16F1847_ONLY = 1  // Only on PIC16F1847
   };
   
   struct MQTT_set_cmd_t {
     PGM_P setcmd;              // MQTT command name
     PGM_P otgwcmd;             // PIC firmware command code
     PGM_P ottype;              // Parameter type (temp, on, level, function)
     PIC_VERSION minPicVersion; // Minimum PIC version required (NEW)
   };
   ```

2. **Validation Function:**
   ```cpp
   bool isCmdSupportedOnPIC(PIC_VERSION minVersion) {
     if (minVersion == PIC_16F1847_ONLY) {
       if (strcasecmp_P(sPICtype, PSTR("PIC16F1847")) != 0) {
         return false;  // Not supported on this PIC
       }
     }
     return true;  // Supported
   }
   ```

3. **Command Processing:**
   - Before sending MQTT command to PIC:
     - Read minPicVersion from PROGMEM
     - Call isCmdSupportedOnPIC()
     - If false: Log error, reject command, do NOT queue
     - If true: Proceed with command as normal

4. **Error Reporting:**
   - MQTT Debug: `ERROR: Command 'coolinglevel' not supported on PIC16F88 (PIC16F1847 only)`
   - Telnet Debug: `MQTT: Unsupported command 'coolinglevel' on PIC16F88 - Command requires PIC16F1847`
   - Command is NOT sent to PIC (prevents wasted queue space)
   - Command is NOT transmitted over serial

5. **Command Categorization:**
   - **40 standard commands** marked with `PIC_ALL` (work on both versions)
   - **12 advanced commands** marked with `PIC_16F1847_ONLY` (new features)

## Alternatives Considered

### Alternative 1: No Validation (Current State)
**Pros:**
- Simplest implementation
- Zero performance impact
- No code changes needed

**Cons:**
- Confusing user experience (commands silently fail)
- Difficult debugging (unclear why command doesn't work)
- No error feedback to user
- Risk of wasting network/processing on unsupported commands
- Users might assume firmware is broken when commands don't work

**Why not chosen:** Poor user experience and debugging. Users have no way to know if their hardware supports a command.

### Alternative 2: Reject at REST API Level Only
**Pros:**
- Catches unsupported commands earlier
- Can provide HTTP error response
- REST-specific solution

**Cons:**
- MQTT commands still sent without validation
- Incomplete solution (only half of interfaces validated)
- REST API might not be enabled
- Duplicated validation logic

**Why not chosen:** Validation should be centralized in MQTT processor. REST API can add additional validation as wrapper.

### Alternative 3: Dynamic Command Discovery
**Pros:**
- Firmware could advertise capabilities on startup
- Users know immediately what commands work
- More flexible for future hardware versions

**Cons:**
- Requires significant complexity in discovery protocol
- Must integrate with Home Assistant discovery
- Overkill for current 2-hardware-version scenario
- Memory overhead for dynamic capability lists
- Harder to debug (capabilities might not match actual PIC)

**Why not chosen:** Static metadata in PROGMEM is simpler, more reliable, and sufficient for the current architectural constraints.

### Alternative 4: Version-Specific Command Arrays
**Pros:**
- Could compile out unsupported commands entirely for PIC16F88 builds
- Smaller memory footprint for single-hardware-version deployments

**Cons:**
- Requires separate firmware builds for each PIC version
- Users can't easily upgrade hardware without reflashing firmware
- Defeats purpose of unified firmware
- Maintenance nightmare (multiple codebases)
- OTA updates become complicated

**Why not chosen:** This firmware supports both hardware versions in a single build. Command arrays must include all commands.

## Consequences

### Positive
- **Clear Error Messages:** Users see immediately why command failed
- **Debugging:** Error logs show which commands are unsupported on their hardware
- **Safety:** Prevents queue pollution with unsupported commands
- **Reliability:** No wasted resources sending commands PIC will ignore
- **Documentation:** Commands marked as PIC16F1847-only in code and docs
- **User Experience:** Users know exactly what hardware supports what
- **Future-Proof:** Easily extensible to new PIC versions

**User Benefits:**
- Can identify hardware version via debug output
- Gets error message instead of silent failure
- Can refer to documentation for which commands work on their hardware
- Can make informed decision to upgrade hardware if needed

### Negative
- **Small Performance Cost:** One validation check per MQTT command
  - Mitigation: Check is minimal (single enum compare)
  - Impact: Negligible (~5 microseconds per command)
- **Code Complexity:** Added struct field, validation function, metadata
  - Mitigation: Well-documented, minimal complexity
  - Impact: +50 lines of code, straightforward logic
- **Maintenance:** Version requirements must be kept accurate
  - Mitigation: ADR documents which commands are PIC16F1847-only
  - Mitigation: Code comments identify version requirements
- **Backward Compatibility Note:** Existing unsupported commands will now return errors
  - Mitigation: Users likely never tried unsupported commands anyway
  - Mitigation: Documentation clearly lists supported commands per hardware

### Risks & Mitigation
- **Incorrect version detection:** sPICtype might not match exactly
  - **Mitigation:** Version detected during startup from PIC response
  - **Mitigation:** Verified via telnet `#I` command
  - **Mitigation:** Logged to debug output on startup
- **Hardware upgrade but firmware not updated:** User upgrades to PIC16F1847 but firmware thinks it's PIC16F88
  - **Mitigation:** Firmware redetects PIC type on every restart
  - **Mitigation:** Users can manually trigger detection via `#I` telnet command
  - **Mitigation:** Documentation recommends restarting after hardware upgrade
- **New PIC version added later:** What if PIC16F9999 is released?
  - **Mitigation:** ADR-025 can be updated with new enum value
  - **Mitigation:** Validation function easily extensible
  - **Mitigation:** Command struct supports arbitrary version values

## Implementation Details

**File:** `MQTTstuff.ino`

**Lines of code:**
- PIC_VERSION enum: 4 lines
- MQTT_set_cmd_t struct extension: 1 line
- isCmdSupportedOnPIC() function: 8 lines
- Command array updates: 52 entries (1 metadata field each)
- Validation check in processor: 4 lines
- Error logging: 2 lines

**Total:** ~65 lines of code added/modified

**PROGMEM Usage:**
- Version metadata stored with each command entry (1 byte per command)
- 52 commands × 1 byte = 52 bytes additional flash usage
- Negligible impact (ESP8266 has 4MB flash)

**RAM Usage:**
- No additional RAM usage (all metadata in PROGMEM)
- Validation function uses only local variables on stack

## Command Categorization

**40 Standard Commands (PIC_ALL):**
- 29 original commands (temperature, DHW, gateway, data ID)
- 10 new standard commands (clock, LED, GPIO, summary)
- 1 raw command entry (for direct serial)

**12 Advanced Commands (PIC_16F1847_ONLY):**
- Cooling: coolinglevel, coolingenable
- Modes: modeheating, mode2ndcircuit, modewater
- Service: remoterequest, transparentparam, summermode, dhwblocking
- Config: boilersetpoint, messageinterval, failsafe

## Testing Strategy

**Unit Level:**
- [ ] Verify isCmdSupportedOnPIC() returns true for PIC_ALL
- [ ] Verify isCmdSupportedOnPIC() returns false for PIC_16F1847_ONLY when sPICtype != "PIC16F1847"
- [ ] Verify isCmdSupportedOnPIC() returns true for PIC_16F1847_ONLY when sPICtype == "PIC16F1847"
- [ ] Verify PROGMEM reads work correctly for version metadata

**Integration Level:**
- [ ] Hardware test on PIC16F88: Standard command accepted, advanced command rejected
- [ ] Hardware test on PIC16F1847: Both standard and advanced commands accepted
- [ ] Verify error message appears in MQTT debug logs
- [ ] Verify error message appears in telnet debug output
- [ ] Verify unsupported command is NOT queued (check command queue)
- [ ] Verify supported command IS queued normally

**User Level:**
- [ ] User sees clear error when sending unsupported command
- [ ] User can identify their hardware version from debug output
- [ ] Documentation clearly lists which commands work on which hardware
- [ ] User can refer to [PIC_VERSION_COMPATIBILITY.md](../PIC_VERSION_COMPATIBILITY.md) for details

## Home Assistant Integration Impact

**No changes required** to existing Home Assistant integrations:
- All standard commands continue to work
- PIC16F1847 devices will show additional entities in auto-discovery
- Auto-discovery will add new climate/select entities for advanced commands

**User Experience:**
- PIC16F88 users: 40 entities, standard commands available
- PIC16F1847 users: 52 entities, all commands available
- Automatic based on detected hardware version

## Documentation Updates

- **MQTTstuff.ino:** Code comments identify version requirements
- **[PIC_VERSION_COMPATIBILITY.md](../PIC_VERSION_COMPATIBILITY.md):** User guide with command lists by hardware, troubleshooting
- **[MQTT_API_REFERENCE.md](../MQTT_API_REFERENCE.md):** Command reference updated with compatibility notes
- **[Copilot Instructions](../../.github/copilot-instructions.md):** Guidelines for command validation

## Related Decisions
- **ADR-006:** MQTT Integration Pattern (command processing architecture)
- **ADR-004:** Static Buffer Allocation (command queue management)
- **ADR-009:** PROGMEM String Literals (metadata storage)
- **ADR-012:** PIC Firmware Upgrade (understanding PIC versions)

## Future Enhancement Opportunities

1. **Capability Discovery:** Device advertises supported commands on startup
2. **REST API Integration:** Extend REST API to validate commands same way
3. **Home Assistant Integration:** Filter entities based on hardware during discovery
4. **OTA Detection:** Warn user if upgrading hardware requires firmware update
5. **Firmware Compilation:** Separate builds optimized for each PIC version (future)

## References
- Implementation: `MQTTstuff.ino` (lines 240-435)
- User Documentation: `docs/PIC_VERSION_COMPATIBILITY.md`
- Command Reference: `docs/MQTT_API_REFERENCE.md`
- MQTT API: `docs/MQTT_IMPLEMENTATION.md`
- PIC Firmware: https://otgw.tclcode.com/firmware.html

## Version History

**2026-01-31 (Initial):** ADR created based on branch implementation of command validation feature
- Added PIC version-aware validation to MQTT command processor
- Categorized 52 commands by hardware support level
- Created comprehensive user documentation
