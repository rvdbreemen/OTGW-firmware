# MQTT Implementation Details

Implementation guide for MQTT command integration with OTGW-firmware.

## Overview

OTGW-firmware provides comprehensive MQTT control over all OpenTherm Gateway functions through a command queue system that communicates with the PIC firmware via serial interface.

## Architecture

### Command Flow

```
MQTT Broker
    ↓
MQTT Subscription (MQTTstuff.ino)
    ↓
Command Parser
    ↓
Command Queue (addOTWGcmdtoqueue)
    ↓
Serial Interface → PIC Firmware
    ↓
OpenTherm Protocol ↔ Boiler
```

### Key Files

- **MQTTstuff.ino** - MQTT subscription handling and command parsing
  - Contains: MQTT command definitions, setcmds[] array, command queue operations
  - Lines: 188-235 (string constants), 265-304 (array entries)
  - Functions: `onMQTTConnect()`, `onMQTTMessage()`, `addOTWGcmdtoqueue()`

- **OTGW-Core.ino** - Serial communication with PIC
  - Functions: `serialLoop()`, command transmission via telnet debug interface

- **OTGW-Core.h** - Message ID definitions
  - Contains: OpenTherm message ID constants used by all MQTT commands

## Command Definition Structure

All MQTT commands are defined in a PROGMEM-based array `setcmds[]` in MQTTstuff.ino:

```cpp
typedef struct {
  const char *mqtt_cmd;      // MQTT command name (from PROGMEM)
  const char *otgw_cmd;      // PIC firmware command code (from PROGMEM)
  uint8_t parameter_type;    // Parameter type (s_temp, s_on, s_level, s_function)
} MQTT_set_cmd_t;

const MQTT_set_cmd_t setcmds[] PROGMEM = {
  { s_cmd_setpoint, s_otgw_TT, s_temp },
  { s_cmd_hotwater, s_otgw_HW, s_function },
  // ... 50 more entries
};
```

### Parameter Types

| Type | Values | Commands |
|------|--------|----------|
| `s_temp` | -40 to +64°C | Temperature setpoints, boilersetpoint |
| `s_on` | 0/1, on/off | Binary controls (chenable, hotwater mode, etc.) |
| `s_level` | 0-100% | Percentage values (maxmodulation, coolinglevel) |
| `s_function` | Mode/function codes | Mode selection, LED/GPIO config |

## Command Processing

### 1. MQTT Message Reception

```cpp
void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
    // Extract command name and value from topic
    // Example: "OTGW/set/otgw/setpoint=18.5"
    // Extracts: command="setpoint", value="18.5"
    
    // Look up command in setcmds[] array
    // Find matching MQTT command name
    
    // Parse and validate parameter based on type
    
    // Queue command for transmission
    addOTWGcmdtoqueue(otgw_cmd_code, parsed_value);
}
```

### 2. Command Queue

Commands are queued to prevent serial buffer overrun:

```cpp
void addOTWGcmdtoqueue(const char *cmd, ...) {
    // Add command to command queue
    // Commands processed in FIFO order
    // Transmitted via serial at ~1 command per second
}
```

### 3. Serial Transmission to PIC

The PIC firmware expects commands in a specific format:
```
<cmd_code><space><value><CR/LF>
```

Example transmissions:
```
TT 18.5        // Set temporary setpoint to 18.5°C
LA F           // Configure LED A for flame status
MH 2           // Set heating mode to comfort
CL 75          // Set cooling level to 75%
```

## Home Assistant Integration

### MQTT Auto Discovery

OTGW-firmware publishes Home Assistant Auto Discovery messages that automatically create entities for all MQTT commands.

**Discovery Topic Format:**
```
homeassistant/<component>/<device_id>/<command>/config
```

**Auto-Discovered Entity Types:**
- **climate** - Temperature controls (setpoint, outside, ctrlsetpt)
- **select** - Mode selections (modeheating, mode2ndcircuit, etc.)
- **switch** - Binary controls (chenable, hotwater, cooling)
- **number** - Numeric values (maxmodulation, coolinglevel, messageinterval)

### Configuration Requirements

In Home Assistant `configuration.yaml`:
```yaml
mqtt:
  broker: <your-broker-ip>
  username: <mqtt-username>
  password: <mqtt-password>
  
homeassistant:
  name: Home
  latitude: <your-latitude>
  longitude: <your-longitude>
  elevation: <your-elevation>
  unit_system: metric
  time_zone: <your-timezone>
```

### Example Entity Creation

**Automatic (via Auto Discovery):**
```yaml
# These are created automatically when OTGW connects
climate.otgw_temperature
select.otgw_modeheating
switch.otgw_chenable
number.otgw_coolinglevel
```

**Manual (if needed):**
```yaml
climate:
  - platform: mqtt
    name: OTGW Setpoint
    modes: [auto, heat, cool]
    current_temperature_topic: "OTGW/value/otgw/roomtemp"
    temperature_command_topic: "OTGW/set/otgw/setpoint"
    temperature_unit: C
```

## Adding New Commands

### Step 1: Define String Constants

In MQTTstuff.ino, add PROGMEM string constants:

```cpp
const char s_cmd_yourcommand[] PROGMEM = "yourcommand";
const char s_otgw_YC[] PROGMEM = "YC";  // YC = your command code
```

### Step 2: Add Array Entry

Add entry to `setcmds[]` array:

```cpp
const MQTT_set_cmd_t setcmds[] PROGMEM = {
  // ... existing entries ...
  { s_cmd_yourcommand, s_otgw_YC, s_function },  // Add at end
};
```

### Step 3: Verify PIC Firmware Support

Check [PIC firmware documentation](https://otgw.tclcode.com/firmware.html) to confirm the command code is supported by your PIC version.

### Step 4: Update Documentation

1. Add command to appropriate section in [MQTT_API_REFERENCE.md](MQTT_API_REFERENCE.md)
2. Include parameter type and example usage
3. Update README MQTT Commands link if applicable

### Step 5: Test

1. Build firmware: `python build.py --firmware`
2. Flash to ESP8266: `python flash_esp.py --build`
3. Test command via MQTT:
   ```bash
   mosquitto_pub -h <broker> -t "OTGW/set/otgw/yourcommand" -m "value"
   ```
4. Verify in debug telnet output: `telnet <gateway-ip> 23`

## Backward Compatibility

All MQTT command implementations are **100% backward compatible**:

- Existing 29 commands remain unchanged
- New commands are purely additive
- Parameter types preserved
- Topic format consistent
- Existing automations continue working
- No configuration changes required

## Performance Considerations

### Command Queue Timing

- Commands processed at ~1 command per second
- Queued commands buffered in firmware memory
- Long sequences may take several seconds to process

### Memory Usage

- String constants stored in PROGMEM (flash, not RAM)
- setcmds[] array in PROGMEM
- Command queue in dynamic RAM (~20-30 bytes per queued command)

### MQTT Topic Size

- Standard commands: 20-30 bytes per topic
- Advanced commands: 30-50 bytes per topic
- Broker-dependent limitations: typically 256MB for topic names

## Debugging

### Enable Debug Output

Connect via telnet to debug port 23:
```bash
telnet <gateway-ip> 23
```

### Monitor MQTT Messages

Watch for debug output showing command parsing:
```
MQTT Command: setpoint=18.5
Parsed: 18.5°C
Queued: TT 18.5
```

### Check Command Queue

Review debug telnet output for:
- Command received messages
- Queue status
- Serial transmission confirmations
- Error messages

### Verify PIC Response

Monitor serial interface for responses:
```
100 HB:00 ... (response from PIC)
```

## Troubleshooting

### Command Not Received

1. Verify MQTT connection: `mosquitto_sub -h <broker> -t "OTGW/#"`
2. Check command spelling matches definition
3. Verify parameter value is valid for command type
4. Check firewall/network connectivity

### Parameter Rejected

1. Verify parameter format matches type:
   - `s_temp`: numeric with optional decimals (e.g., "18.5")
   - `s_on`: "0", "1", "on", "off"
   - `s_level`: 0-100 numeric
   - `s_function`: valid function code (F, R, X, T, etc.)

2. Check parameter range is valid for command
3. Review [MQTT_API_REFERENCE.md](MQTT_API_REFERENCE.md) for valid values

### Command Not Executed

1. Check PIC responds to serial commands: See debug output
2. Verify boiler is connected and responding
3. Check if command is PIC16F1847-specific (will be ignored on PIC16F88)
4. Review boiler manual for command support

## PIC Firmware Command Reference

Complete PIC firmware command documentation:
- **Official Documentation:** https://otgw.tclcode.com/firmware.html
- **Support Forum:** https://otgw.tclcode.com/support/forum
- **GitHub Issues:** Report issues at https://github.com/rvdbreemen/OTGW-firmware/issues

---

**Last Updated:** 2026-01-31  
**Status:** Production Ready (52 commands, PIC16F1847 support)
