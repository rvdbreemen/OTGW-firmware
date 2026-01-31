---
# METADATA
Document Title: MQTT Firmware Commands - Implementation Guide
Review Date: 2026-01-31
Branch Reviewed: dev-improvement-rest-api-compatibility
Target Version: 1.0.1
Reviewer: GitHub Copilot Advanced Agent
Document Type: Implementation Guide with Code Snippets
Status: COMPLETE
---

# MQTT Firmware Commands - Implementation Guide

## Quick Reference: What to Add

### Summary
- **New Commands:** 23
- **New MQTT Topics:** 23
- **Code Files to Modify:** 1 (MQTTstuff.ino)
- **Estimated Implementation Time:** 1-2 hours
- **Complexity:** Low (pattern-based additions)

---

## Step 1: Add String Constants to MQTTstuff.ino

**Location:** Lines 100-150 (after existing command string constants)

**Add this code block:**

```cpp
// ============================================================================
// MISSING MQTT COMMAND STRINGS - Add these after existing command constants
// ============================================================================

// Missing LED commands (Lines 100-105)
const char s_cmd_ledA[] PROGMEM = "ledA";
const char s_cmd_ledB[] PROGMEM = "ledB";
const char s_cmd_ledC[] PROGMEM = "ledC";
const char s_cmd_ledD[] PROGMEM = "ledD";
const char s_cmd_ledE[] PROGMEM = "ledE";
const char s_cmd_ledF[] PROGMEM = "ledF";

// Missing GPIO commands (Lines 107-108)
const char s_cmd_gpioA[] PROGMEM = "gpioA";
const char s_cmd_gpioB[] PROGMEM = "gpioB";

// Missing clock and summary commands (Lines 110-111)
const char s_cmd_setclock[] PROGMEM = "setclock";
const char s_cmd_printsummary[] PROGMEM = "printsummary";

// Missing cooling commands - PIC16F1847 only (Lines 113-114)
const char s_cmd_coolinglevel[] PROGMEM = "coolinglevel";
const char s_cmd_coolingenable[] PROGMEM = "coolingenable";

// Missing mode commands - PIC16F1847 only (Lines 116-118)
const char s_cmd_modeheating[] PROGMEM = "modeheating";
const char s_cmd_mode2ndcircuit[] PROGMEM = "mode2ndcircuit";
const char s_cmd_modewater[] PROGMEM = "modewater";

// Missing advanced control commands (Lines 120-126)
const char s_cmd_remoterequest[] PROGMEM = "remoterequest";
const char s_cmd_transparentparam[] PROGMEM = "transparentparam";
const char s_cmd_summermode[] PROGMEM = "summermode";
const char s_cmd_dhwblocking[] PROGMEM = "dhwblocking";
const char s_cmd_boilersetpoint[] PROGMEM = "boilersetpoint";
const char s_cmd_messageinterval[] PROGMEM = "messageinterval";
const char s_cmd_failsafe[] PROGMEM = "failsafe";
```

---

## Step 2: Add OTGW Command Code Constants

**Location:** Lines 160-190 (after existing OTGW command constants like s_otgw_TT, s_otgw_TC, etc.)

**Add this code block:**

```cpp
// ============================================================================
// MISSING OTGW FIRMWARE COMMAND CODES - Add after existing command codes
// ============================================================================

// Clock command (Line 160)
const char s_otgw_SC[] PROGMEM = "SC";

// LED configuration commands (Lines 162-167)
const char s_otgw_LA[] PROGMEM = "LA";
const char s_otgw_LB[] PROGMEM = "LB";
const char s_otgw_LC[] PROGMEM = "LC";
const char s_otgw_LD[] PROGMEM = "LD";
const char s_otgw_LE[] PROGMEM = "LE";
const char s_otgw_LF[] PROGMEM = "LF";

// GPIO configuration commands (Lines 169-170)
const char s_otgw_GA[] PROGMEM = "GA";
const char s_otgw_GB[] PROGMEM = "GB";

// Print summary command (Line 172)
const char s_otgw_PS[] PROGMEM = "PS";

// Cooling commands - PIC16F1847 (Lines 174-175)
const char s_otgw_CL[] PROGMEM = "CL";
const char s_otgw_CE[] PROGMEM = "CE";

// Mode commands - PIC16F1847 (Lines 177-179)
const char s_otgw_MH[] PROGMEM = "MH";
const char s_otgw_M2[] PROGMEM = "M2";
const char s_otgw_MW[] PROGMEM = "MW";

// Advanced control commands (Lines 181-187)
const char s_otgw_RR[] PROGMEM = "RR";
const char s_otgw_TP[] PROGMEM = "TP";
const char s_otgw_SM[] PROGMEM = "SM";
const char s_otgw_BW[] PROGMEM = "BW";
const char s_otgw_BS[] PROGMEM = "BS";
const char s_otgw_MI[] PROGMEM = "MI";
const char s_otgw_FS[] PROGMEM = "FS";
```

---

## Step 3: Add Parameter Type Constants (if not already defined)

**Location:** After the existing parameter types (look for s_temp, s_on, s_level, etc.)

**Check if these exist; if not, add:**

```cpp
// Parameter types - ADD ONLY IF NOT ALREADY DEFINED
// (These may already exist, check first!)

// Time format (HH:MM/N)
// const char s_time[] PROGMEM = "time";

// LED function type
// const char s_led[] PROGMEM = "led";

// GPIO function type (0-8)
// const char s_gpio[] PROGMEM = "gpio";

// Operating mode type (0-6)
// const char s_mode[] PROGMEM = "mode";

// Remote request type (0-12)
// const char s_request[] PROGMEM = "request";

// Transparent parameter type
// const char s_tsp[] PROGMEM = "tsp";
```

---

## Step 4: Expand the setcmds[] Array

**Location:** Line 196 onwards (find the existing `const MQTT_set_cmd_t setcmds[] PROGMEM = {`)

**Replace the closing brace and nrcmds calculation with this:**

```cpp
const MQTT_set_cmd_t setcmds[] PROGMEM = {
  // EXISTING COMMANDS (Keep all 29 as-is)
  {   s_cmd_command, s_empty, s_raw },
  {   s_cmd_setpoint, s_otgw_TT, s_temp },
  {   s_cmd_constant, s_otgw_TC, s_temp },
  {   s_cmd_outside, s_otgw_OT, s_temp },
  {   s_cmd_hotwater, s_otgw_HW, s_on },
  {   s_cmd_gatewaymode, s_otgw_GW, s_on },
  {   s_cmd_setback, s_otgw_SB, s_temp },
  {   s_cmd_maxchsetpt, s_otgw_SH, s_temp },
  {   s_cmd_maxdhwsetpt, s_otgw_SW, s_temp },
  {   s_cmd_maxmodulation, s_otgw_MM, s_level },        
  {   s_cmd_ctrlsetpt, s_otgw_CS, s_temp },        
  {   s_cmd_ctrlsetpt2, s_otgw_C2, s_temp },        
  {   s_cmd_chenable, s_otgw_CH, s_on },        
  {   s_cmd_chenable2, s_otgw_H2, s_on },        
  {   s_cmd_ventsetpt, s_otgw_VS, s_level },
  {   s_cmd_temperaturesensor, s_otgw_TS, s_function },
  {   s_cmd_addalternative, s_otgw_AA, s_function },
  {   s_cmd_delalternative, s_otgw_DA, s_function },
  {   s_cmd_unknownid, s_otgw_UI, s_function },
  {   s_cmd_knownid, s_otgw_KI, s_function },
  {   s_cmd_priomsg, s_otgw_PM, s_function },
  {   s_cmd_setresponse, s_otgw_SR, s_function },
  {   s_cmd_clearrespons, s_otgw_CR, s_function },
  {   s_cmd_resetcounter, s_otgw_RS, s_function },
  {   s_cmd_ignoretransitations, s_otgw_IT, s_function },
  {   s_cmd_overridehb, s_otgw_OH, s_function },
  {   s_cmd_forcethermostat, s_otgw_FT, s_function },
  {   s_cmd_voltageref, s_otgw_VR, s_function },
  {   s_cmd_debugptr, s_otgw_DP, s_function },
  
  // NEW STANDARD COMMANDS (Add 10 commands)
  {   s_cmd_setclock, s_otgw_SC, s_function },      // Time format: HH:MM/N
  {   s_cmd_ledA, s_otgw_LA, s_function },         // LED A function
  {   s_cmd_ledB, s_otgw_LB, s_function },         // LED B function
  {   s_cmd_ledC, s_otgw_LC, s_function },         // LED C function
  {   s_cmd_ledD, s_otgw_LD, s_function },         // LED D function
  {   s_cmd_ledE, s_otgw_LE, s_function },         // LED E function
  {   s_cmd_ledF, s_otgw_LF, s_function },         // LED F function
  {   s_cmd_gpioA, s_otgw_GA, s_function },        // GPIO A function
  {   s_cmd_gpioB, s_otgw_GB, s_function },        // GPIO B function
  {   s_cmd_printsummary, s_otgw_PS, s_on },       // Print summary toggle
  
  // NEW PIC16F1847 ADVANCED COMMANDS (Add 13 commands)
  {   s_cmd_coolinglevel, s_otgw_CL, s_level },    // Cooling level %
  {   s_cmd_coolingenable, s_otgw_CE, s_on },      // Cooling enable
  {   s_cmd_modeheating, s_otgw_MH, s_function },  // Mode heating (0-6)
  {   s_cmd_mode2ndcircuit, s_otgw_M2, s_function }, // Mode 2nd circuit (0-6)
  {   s_cmd_modewater, s_otgw_MW, s_function },    // Mode water (0-6)
  {   s_cmd_remoterequest, s_otgw_RR, s_function }, // Remote request (0-12)
  {   s_cmd_transparentparam, s_otgw_TP, s_function }, // TSP/FHB read/write
  {   s_cmd_summermode, s_otgw_SM, s_on },         // Summer mode
  {   s_cmd_dhwblocking, s_otgw_BW, s_on },        // DHW blocking
  {   s_cmd_boilersetpoint, s_otgw_BS, s_temp },   // Boiler setpoint
  {   s_cmd_messageinterval, s_otgw_MI, s_function }, // Message interval (ms)
  {   s_cmd_failsafe, s_otgw_FS, s_on },           // Fail safe enable
};

const int nrcmds = sizeof(setcmds) / sizeof(setcmds[0]);  // Now 52 commands total
```

---

## Step 5: Version Bump

**File:** `version.h`

**Update version:**

```cpp
// Old:
// const char version[] PROGMEM = "1.0.0";

// New:
const char version[] PROGMEM = "1.0.1";
const char versionDate[] PROGMEM = "2026-01-31";
```

---

## Step 6: Update nrcmds Comment

**Location:** Line after setcmds[] definition

**Change from:**
```cpp
const int nrcmds = sizeof(setcmds) / sizeof(setcmds[0]);  // 29 commands
```

**Change to:**
```cpp
const int nrcmds = sizeof(setcmds) / sizeof(setcmds[0]);  // 52 commands (29 standard + 23 new)
```

---

## Step 7: Documentation Updates

### Update README.md

Add new section or expand MQTT Commands section with:

```markdown
### MQTT Commands - New in v1.0.1

#### Clock & Time
- `setclock` → `SC` - Set gateway clock (format: HH:MM/N where N=1-7 for day)

#### Hardware Configuration (LEDs & GPIO)
- `ledA`, `ledB`, `ledC`, `ledD`, `ledE`, `ledF` → `LA`, `LB`, `LC`, `LD`, `LE`, `LF`
  - Functions: R(eceive), X(transmit), T(master), B(slave), O(override), F(flame), H(heating), W(water), C(comfort), E(error), M(maintenance), P(power)
- `gpioA`, `gpioB` → `GA`, `GB`
  - Functions: 0(input), 1(ground), 2(Vcc), 3(LED E), 4(LED F), 5(home), 6(away), 7(DS1820), 8(DHWBlock)

#### Control & Status
- `printsummary` → `PS` - Toggle continuous/on-demand reporting

#### PIC16F1847 Advanced Features
- `coolinglevel`, `coolingenable` → `CL`, `CE` - Cooling control
- `modeheating`, `mode2ndcircuit`, `modewater` → `MH`, `M2`, `MW` - Operating mode (0-6)
- `remoterequest` → `RR` - Service requests (0-12)
- `transparentparam` → `TP` - Read/write TSP parameters
- `summermode` → `SM` - Force summer mode
- `dhwblocking` → `BW` - Block DHW heating
- `boilersetpoint` → `BS` - Override room setpoint to boiler
- `messageinterval` → `MI` - Message interval in stand-alone mode (100-1275ms)
- `failsafe` → `FS` - Enable/disable fail-safe recovery mode
```

### Create MQTT_COMMANDS.md

Create a comprehensive reference document with all 52 commands, their parameters, and examples.

---

## Testing Checklist

### Unit Tests

- [ ] Verify all 23 new command strings compile without errors
- [ ] Verify all 23 new OTGW command codes are correct (per PIC firmware spec)
- [ ] Verify setcmds array has exactly 52 entries
- [ ] Verify no duplicate command names in setcmds array

### Integration Tests

- [ ] Test parsing of SC command with valid time format (e.g., "09:00/1")
- [ ] Test parsing of LED commands (e.g., "ledA=F")
- [ ] Test parsing of GPIO commands (e.g., "gpioB=7")
- [ ] Test parsing of new integer commands (CL, MH, M2, MW, RR, TP, MI)
- [ ] Test that invalid commands still show "No match found" error
- [ ] Test command queue doesn't overflow with all 52 commands

### Hardware Tests (on ESP8266 with OTGW)

- [ ] Flash firmware with new commands
- [ ] Verify MQTT connection and auto-discovery still works
- [ ] Send LED configuration via MQTT: `homeautomation/set/OTGW/ledA` → `F`
- [ ] Send GPIO configuration via MQTT: `homeautomation/set/OTGW/gpioB` → `7`
- [ ] Send clock via MQTT: `homeautomation/set/OTGW/setclock` → `09:00/1`
- [ ] Verify PIC firmware responds correctly to all new commands
- [ ] Test with actual boiler and thermostat for mode and advanced commands
- [ ] Monitor telnet debug port (23) for command processing
- [ ] Verify no memory issues or crashes with extended command set

### Backward Compatibility Tests

- [ ] All 29 existing commands still work
- [ ] Existing Home Assistant MQTT entities still function
- [ ] Home Assistant auto-discovery still works
- [ ] REST API commands unaffected
- [ ] Web UI still functions properly

---

## Validation Script (Optional)

Add this to `evaluate.py` or create `validate_mqtt_commands.py`:

```python
#!/usr/bin/env python3
"""
Validate MQTT command configuration in MQTTstuff.ino
"""

import re

def validate_mqtt_commands(filepath):
    """Verify command array is properly configured"""
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Find setcmds array
    setcmds_match = re.search(r'const MQTT_set_cmd_t setcmds\[\] PROGMEM = \{([^}]+)\}', content, re.DOTALL)
    if not setcmds_match:
        print("ERROR: setcmds array not found")
        return False
    
    # Count entries (each starts with {)
    entries = setcmds_match.group(1).count('{')
    print(f"Found {entries} MQTT commands in setcmds array")
    
    # Find nrcmds
    nrcmds_match = re.search(r'const int nrcmds = sizeof\(setcmds\) / sizeof\(setcmds\[0\]\);', content)
    if not nrcmds_match:
        print("ERROR: nrcmds definition not found")
        return False
    
    # Extract and validate command names
    cmd_names = re.findall(r'{\s*s_cmd_(\w+)', content)
    print(f"\nCommand names found: {len(cmd_names)}")
    
    # Check for missing commands
    required = [
        'command', 'setpoint', 'constant', 'outside', 'hotwater', 'gatewaymode',
        'setback', 'maxchsetpt', 'maxdhwsetpt', 'maxmodulation', 'ctrlsetpt', 'ctrlsetpt2',
        'chenable', 'chenable2', 'ventsetpt', 'temperaturesensor', 'addalternative',
        'delalternative', 'unknownid', 'knownid', 'priomsg', 'setresponse', 'clearrespons',
        'resetcounter', 'ignoretransitations', 'overridehb', 'forcethermostat', 'voltageref',
        'debugptr',
        # New commands
        'ledA', 'ledB', 'ledC', 'ledD', 'ledE', 'ledF', 'gpioA', 'gpioB', 'setclock',
        'printsummary', 'coolinglevel', 'coolingenable', 'modeheating', 'mode2ndcircuit',
        'modewater', 'remoterequest', 'transparentparam', 'summermode', 'dhwblocking',
        'boilersetpoint', 'messageinterval', 'failsafe'
    ]
    
    missing = [cmd for cmd in required if cmd not in cmd_names]
    if missing:
        print(f"\nMISSING COMMANDS: {missing}")
        return False
    
    print("✓ All required commands present")
    return True

if __name__ == '__main__':
    result = validate_mqtt_commands('MQTTstuff.ino')
    exit(0 if result else 1)
```

---

## Git Commit Message Template

```
feat(mqtt): Add 23 new firmware commands to MQTT interface

- Standard commands: SC (clock), LA-LF (LEDs), GA-GB (GPIO), PS (summary)
- PIC16F1847 advanced: CL/CE (cooling), MH/M2/MW (modes), RR (remote),
  TP (transparent param), SM (summer), BW (DHW block), BS (boiler setpoint),
  MI (message interval), FS (fail-safe)
- Extends MQTT command coverage from 29 to 52 commands
- All commands mapped to PIC firmware codes per specification
- Backward compatible - no breaking changes
- Adds comprehensive support for hardware configuration via MQTT

BREAKING CHANGE: none
VERSION: 1.0.1
```

---

## Expected MQTT Topics After Implementation

```
homeautomation/set/OTGW/command              # Raw command
homeautomation/set/OTGW/setpoint             # TT
homeautomation/set/OTGW/constant             # TC
homeautomation/set/OTGW/outside              # OT
homeautomation/set/OTGW/hotwater             # HW
homeautomation/set/OTGW/gatewaymode          # GW
homeautomation/set/OTGW/setback              # SB
homeautomation/set/OTGW/maxchsetpt           # SH
homeautomation/set/OTGW/maxdhwsetpt          # SW
homeautomation/set/OTGW/maxmodulation        # MM
homeautomation/set/OTGW/ctrlsetpt            # CS
homeautomation/set/OTGW/ctrlsetpt2           # C2
homeautomation/set/OTGW/chenable             # CH
homeautomation/set/OTGW/chenable2            # H2
homeautomation/set/OTGW/ventsetpt            # VS
homeautomation/set/OTGW/temperaturesensor    # TS
homeautomation/set/OTGW/addalternative       # AA
homeautomation/set/OTGW/delalternative       # DA
homeautomation/set/OTGW/unknownid            # UI
homeautomation/set/OTGW/knownid              # KI
homeautomation/set/OTGW/priomsg              # PM
homeautomation/set/OTGW/setresponse          # SR
homeautomation/set/OTGW/clearrespons         # CR
homeautomation/set/OTGW/resetcounter         # RS
homeautomation/set/OTGW/ignoretransitations  # IT
homeautomation/set/OTGW/overridehb           # OH
homeautomation/set/OTGW/forcethermostat      # FT
homeautomation/set/OTGW/voltageref           # VR
homeautomation/set/OTGW/debugptr             # DP
homeautomation/set/OTGW/setclock             # SC (NEW)
homeautomation/set/OTGW/ledA                 # LA (NEW)
homeautomation/set/OTGW/ledB                 # LB (NEW)
homeautomation/set/OTGW/ledC                 # LC (NEW)
homeautomation/set/OTGW/ledD                 # LD (NEW)
homeautomation/set/OTGW/ledE                 # LE (NEW)
homeautomation/set/OTGW/ledF                 # LF (NEW)
homeautomation/set/OTGW/gpioA                # GA (NEW)
homeautomation/set/OTGW/gpioB                # GB (NEW)
homeautomation/set/OTGW/printsummary         # PS (NEW)
homeautomation/set/OTGW/coolinglevel         # CL (NEW - PIC16F1847)
homeautomation/set/OTGW/coolingenable        # CE (NEW - PIC16F1847)
homeautomation/set/OTGW/modeheating          # MH (NEW - PIC16F1847)
homeautomation/set/OTGW/mode2ndcircuit       # M2 (NEW - PIC16F1847)
homeautomation/set/OTGW/modewater            # MW (NEW - PIC16F1847)
homeautomation/set/OTGW/remoterequest        # RR (NEW - PIC16F1847)
homeautomation/set/OTGW/transparentparam     # TP (NEW - PIC16F1847)
homeautomation/set/OTGW/summermode           # SM (NEW - PIC16F1847)
homeautomation/set/OTGW/dhwblocking          # BW (NEW - PIC16F1847)
homeautomation/set/OTGW/boilersetpoint       # BS (NEW - PIC16F1847)
homeautomation/set/OTGW/messageinterval      # MI (NEW - PIC16F1847)
homeautomation/set/OTGW/failsafe             # FS (NEW - PIC16F1847)
```

---

## Summary

This implementation adds **23 new firmware commands** to the MQTT interface, bringing total coverage from 29 to 52 commands. All additions follow existing patterns and maintain full backward compatibility.

**Estimated effort:** 1-2 hours for implementation + testing

**Files changed:**
1. `MQTTstuff.ino` - Add constants and expand setcmds array
2. `version.h` - Bump version to 1.0.1
3. `README.md` - Document new commands
4. (Optional) Create `MQTT_COMMANDS.md` for reference

**Benefits:**
✅ Complete MQTT coverage of PIC firmware commands
✅ Enable hardware configuration (LEDs, GPIO) via MQTT
✅ Support advanced PIC16F1847 features
✅ Improve Home Assistant integration
✅ Zero breaking changes
