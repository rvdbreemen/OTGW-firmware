# PIC Version Compatibility & Command Validation

## Overview

The OTGW-firmware now validates MQTT commands against the PIC firmware version before sending them to the OpenTherm Gateway. This prevents unsupported commands from being sent to hardware that doesn't support them, and provides clear error messages when incompatible commands are requested.

## PIC Versions Supported

The firmware supports two PIC microcontroller versions:

### PIC16F88 (Original)
- **Supported Commands:** 39 standard commands
- **Unsupported Features:** Cooling control, advanced operating modes, some service functions
- **Typical Hardware:** OTGW Version 6.x and earlier

### PIC16F1847 (Enhanced)
- **Supported Commands:** All 52 commands (39 standard + 12 advanced)
- **Additional Features:** Cooling control, multiple circuit modes, transparent parameter access, fail-safe
- **Typical Hardware:** OTGW Version 7.x and NodoShop OTGW

## Command Categories by Compatibility

### Standard Commands (Available on All PIC Versions)

These 39 commands work on both PIC16F88 and PIC16F1847:

**Temperature & Setpoint Control:**
- setpoint, constant, outside, ctrlsetpt, ctrlsetpt2, setback

**DHW & Central Heating:**
- hotwater, maxchsetpt, maxdhwsetpt, chenable, chenable2

**Control & Modulation:**
- maxmodulation, ventsetpt

**Gateway & Hardware Config:**
- gatewaymode, setclock, ledA-F, gpioA-B, printsummary

**Data ID & Advanced Features:**
- temperaturesensor, addalternative, delalternative
- unknownid, knownid, priomsg
- setresponse, clearrespons, resetcounter
- ignoretransitations, overridehb, forcethermostat
- voltageref, debugptr

### Advanced Commands (PIC16F1847 Only)

These 12 commands **ONLY** work on PIC16F1847 hardware:

**Cooling System:**
- `coolinglevel` - Control cooling output level
- `coolingenable` - Enable/disable cooling

**Operating Modes:**
- `modeheating` - Central heating operating mode
- `mode2ndcircuit` - Second circuit (heating 2) operating mode
- `modewater` - DHW operating mode

**Service & Configuration:**
- `remoterequest` - Trigger service/maintenance operations
- `transparentparam` - Direct parameter read/write access
- `summermode` - Force summer mode operation
- `dhwblocking` - Block DHW production
- `boilersetpoint` - Override room setpoint to boiler
- `messageinterval` - Set message interval in stand-alone mode
- `failsafe` - Enable/disable fail-safe feature

## Error Handling

### What Happens When an Unsupported Command is Used?

When a user sends an MQTT command that is not supported on their PIC version:

1. **Command is NOT sent to the PIC**
2. **Error is logged to debug telnet interface:**
   ```
   MQTT: Unsupported command 'coolinglevel' on PIC16F88 - Command requires PIC16F1847
   ```
3. **Error message visible in MQTT debug logs:**
   ```
   ERROR: Command 'coolinglevel' not supported on PIC16F88 (PIC16F1847 only)
   ```

### Example Scenario

**Hardware:** OTGW with PIC16F88
**User sends MQTT command:**
```bash
mosquitto_pub -t "OTGW/set/otgw/coolinglevel" -m "75"
```

**Result:**
- Command is **rejected**
- Debug telnet shows: `MQTT: Unsupported command 'coolinglevel' on PIC16F88 - Command requires PIC16F1847`
- No command is sent to the PIC
- Home automation system receives no response or error status

## How to Check Your PIC Version

### Method 1: Debug Telnet Interface

Connect to telnet on port 23:
```bash
telnet <otgw-ip> 23
```

Type `#I` to get device information:
```
PIC: 06b | Type: PIC16F1847 | Version: 7.0.0
```

The "Type" field shows your PIC version.

### Method 2: Web UI System Info

Access the OTGW Web UI and look for:
- **Device Type:** Shows PIC firmware type
- **Firmware Version:** Indicates feature set

### Method 3: MQTT Auto-Discovery

Home Assistant will show the device capability in the device info. PIC16F1847 devices will have additional climate and select entities.

### Method 4: REST API

```bash
curl http://<otgw-ip>/api/v1/health
```

Response includes `"pictype": "PIC16F1847"` or similar.

## Implementation Details

### Data Structure

Each MQTT command is defined with a version requirement:

```cpp
struct MQTT_set_cmd_t {
    PGM_P setcmd;              // MQTT command name
    PGM_P otgwcmd;             // PIC firmware command code
    PGM_P ottype;              // Parameter type (temp, on, level, function)
    PIC_VERSION minPicVersion; // Minimum PIC version required (PIC_ALL or PIC_16F1847_ONLY)
};
```

### Validation Function

The `isCmdSupportedOnPIC()` function checks if a command is supported:

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

### Processing Flow

1. User sends MQTT command
2. Firmware finds matching command entry
3. Firmware reads `minPicVersion` from command definition
4. Firmware calls `isCmdSupportedOnPIC(minVersion)`
5. If supported: Send command to PIC and queue for transmission
6. If not supported: Log error and reject command

## Migration Guide for Users

### If You Have PIC16F88

**Good news:** All your existing automations continue to work without changes!

**What won't work:**
- Cooling system commands (CL, CE)
- Advanced mode selection (MH, M2, MW)
- Service request (RR)
- Transparent parameter (TP)
- Summer mode (SM)
- DHW blocking (BW)
- Message interval (MI)
- Fail-safe (FS)

**If you send these commands:** They will be rejected with a clear error message.

### If You Upgrade to PIC16F1847

**New capabilities unlocked:**
- Full cooling system control
- Advanced mode selection per circuit
- Service and maintenance operations
- Direct parameter read/write
- Smart DHW blocking
- Fail-safe mode

**No migration needed:** All existing commands continue to work identically.

## Troubleshooting

### "Command not supported" Errors

**Problem:** Receiving errors like `Command 'coolinglevel' not supported on PIC16F88`

**Solution:** 
1. Verify your PIC version using methods above
2. If you need the command, upgrade to PIC16F1847 hardware
3. If not upgrading, use only standard commands from the list above

### Commands Working in OTmonitor but Not via MQTT

**Problem:** A command works in OTmonitor but MQTT rejects it

**Solution:**
1. OTmonitor may be using raw serial commands that bypass validation
2. Check firmware version in OTmonitor matches your hardware
3. Use telnet to verify PIC type
4. Check debug logs for validation errors

### Telnet Shows PIC16F1847 but MQTT Rejects Commands

**Problem:** Device info shows PIC16F1847 but advanced commands are rejected

**Cause:** String comparison issue or incomplete firmware detection

**Solution:**
1. Restart the OTGW device
2. Wait 30 seconds for full initialization
3. Reconnect telnet and verify PIC type
4. Check MQTT debug logs for error details

## Code References

- **PIC Version Enum:** Lines 236-240 in MQTTstuff.ino
- **MQTT_set_cmd_t Structure:** Lines 242-247 in MQTTstuff.ino
- **Validation Function:** Lines 250-261 in MQTTstuff.ino
- **Command Processing:** Lines 420-457 in MQTTstuff.ino
- **Error Logging:** Lines 432-433 in MQTTstuff.ino

## Related Documentation

- [MQTT_API_REFERENCE.md](MQTT_API_REFERENCE.md) - Complete command reference
- [MQTT_IMPLEMENTATION.md](MQTT_IMPLEMENTATION.md) - Technical implementation details
- [README.md](../README.md) - Main documentation

## Future Enhancement

Planned improvements:
- Auto-discover PIC capabilities during Home Assistant initialization
- Provide clearer error messages in MQTT payload
- Support conditional command execution based on hardware
- Extended validation for command-specific parameters
