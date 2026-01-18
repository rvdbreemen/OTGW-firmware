# Phase 1: Complete OTGW PIC Command Catalog

## Overview

This document catalogs all known OpenTherm Gateway (OTGW) PIC firmware commands based on official documentation from https://otgw.tclcode.com/firmware.html and codebase analysis.

## Command Statistics

- **Total Commands**: ~60 commands
- **Already in setcmds[] (MQTTstuff.ino)**: 28 commands (47%)
- **Missing from setcmds[]**: ~32 commands (53%)
- **Command Categories**: 13 categories

## Command Categories

### Category 1: Thermostat & Temperature Control (10 commands)

These commands control heating/cooling setpoints and temperature management.

| Command | Status | Description | Response Format | Example |
|---------|--------|-------------|-----------------|---------|
| **TT** | ✅ In setcmds[] | Thermostat temperature setpoint (override room setpoint) | `TT: <temp>` | `TT: 21.5` |
| **TC** | ✅ In setcmds[] | Constant temperature setpoint (override with constant) | `TC: <temp>` | `TC: 20.0` |
| **TG** | ❌ Missing | Target temperature (deprecated, use TT instead) | `TG: <temp>` | `TG: 19.0` |
| **OT** | ✅ In setcmds[] | Outside temperature (provide external sensor reading) | `OT: <temp>` | `OT: 12.5` |
| **SB** | ✅ In setcmds[] | Temperature setback value | `SB: <temp>` | `SB: 2.0` |
| **SH** | ✅ In setcmds[] | Maximum CH water setpoint | `SH: <temp>` | `SH: 70.0` |
| **SW** | ✅ In setcmds[] | Maximum DHW setpoint | `SW: <temp>` | `SW: 60.0` |
| **CS** | ✅ In setcmds[] | Control setpoint (standalone mode) | `CS: <temp>` | `CS: 21.0` |
| **C2** | ✅ In setcmds[] | Control setpoint 2 (standalone mode, 2nd circuit) | `C2: <temp>` | `C2: 19.0` |
| **SC** | ❌ Missing | Setpoint control (enable/disable setpoint control) | `SC: <0/1>` | `SC: 1` |

### Category 2: Enable/Disable Controls (5 commands)

Binary on/off controls for heating, hot water, and gateway modes.

| Command | Status | Description | Response Format | Example |
|---------|--------|-------------|-----------------|---------|
| **HW** | ✅ In setcmds[] | Hot water enable/disable | `HW: <0/1>` | `HW: 1` |
| **CH** | ✅ In setcmds[] | Central heating enable/disable | `CH: <0/1>` | `CH: 1` |
| **H2** | ✅ In setcmds[] | Central heating 2 enable/disable | `H2: <0/1>` | `H2: 0` |
| **GW** | ✅ In setcmds[] | Gateway mode (0=monitor, 1=gateway) | `GW: <0/1>` | `GW: 1` |
| **SM** | ❌ Missing | Summer/winter mode toggle | `SM: <0/1>` | `SM: 0` |

### Category 3: Modulation & Ventilation (3 commands)

Control heating modulation and ventilation systems.

| Command | Status | Description | Response Format | Example |
|---------|--------|-------------|-----------------|---------|
| **MM** | ✅ In setcmds[] | Maximum relative modulation level | `MM: <0-100>` | `MM: 80` |
| **VS** | ✅ In setcmds[] | Ventilation setpoint (for heat recovery systems) | `VS: <0-100>` | `VS: 50` |
| **BW** | ❌ Missing | DHW blocking (domestic hot water blocking) | `BW: <0/1>` | `BW: 0` |

### Category 4: Alternative Data IDs (6 commands)

Manage alternative OpenTherm message IDs for advanced configurations.

| Command | Status | Description | Response Format | Example |
|---------|--------|-------------|-----------------|---------|
| **AA** | ✅ In setcmds[] | Add alternative (add alternative data ID to table) | `AA: <id>` | `AA: 100` |
| **DA** | ✅ In setcmds[] | Delete alternative (remove alternative data ID) | `DA: <id>` | `DA: 100` |
| **UI** | ✅ In setcmds[] | Unknown ID (treat data ID as unknown) | `UI: <id>` | `UI: 101` |
| **KI** | ✅ In setcmds[] | Known ID (treat data ID as known) | `KI: <id>` | `KI: 101` |
| **PM** | ✅ In setcmds[] | Priority message (set message priority) | `PM: <id>` | `PM: 0` |
| **SR** | ✅ In setcmds[] | Set response (set response value for data ID) | `SR: <id>,<val>` | `SR: 56,1234` |

### Category 5: Response Management (3 commands)

Commands for managing responses and counters.

| Command | Status | Description | Response Format | Example |
|---------|--------|-------------|-----------------|---------|
| **CR** | ✅ In setcmds[] | Clear response (clear set response) | `CR: <id>` | `CR: 56` |
| **RS** | ✅ In setcmds[] | Reset counter (reset error counters) | `RS: <0/1>` | `RS: 1` |
| **IT** | ✅ In setcmds[] | Ignore transitions (ignore setpoint transitions) | `IT: <0/1>` | `IT: 0` |

### Category 6: Advanced/Override (4 commands)

Advanced commands for firmware developers and testing.

| Command | Status | Description | Response Format | Example |
|---------|--------|-------------|-----------------|---------|
| **OH** | ✅ In setcmds[] | Override high byte (override message high byte) | `OH: <byte>` | `OH: FF` |
| **FT** | ✅ In setcmds[] | Force thermostat (force thermostat behavior) | `FT: <0/1>` | `FT: 0` |
| **VR** | ✅ In setcmds[] | Voltage reference (set ADC reference voltage) | `VR: <0-255>` | `VR: 128` |
| **DP** | ✅ In setcmds[] | Debug pointer (debug command for firmware developers) | `DP: <addr>` | `DP: 0x1000` |

### Category 7: LED Configuration (6 commands)

Configure LED A-F behavior and functions. NOT currently in setcmds[].

| Command | Status | Description | Response Format | Example |
|---------|--------|-------------|-----------------|---------|
| **LA** | ❌ Missing | LED A configuration (assign function to LED A) | `LA: <func>` | `LA: R` |
| **LB** | ❌ Missing | LED B configuration | `LB: <func>` | `LB: F` |
| **LC** | ❌ Missing | LED C configuration | `LC: <func>` | `LC: T` |
| **LD** | ❌ Missing | LED D configuration | `LD: <func>` | `LD: B` |
| **LE** | ❌ Missing | LED E configuration | `LE: <func>` | `LE: C` |
| **LF** | ❌ Missing | LED F configuration | `LF: <func>` | `LF: X` |

**LED Function Codes:**
- `R` - Receiving OpenTherm message
- `X` - Transmitting OpenTherm message  
- `T` - Transmitting or receiving message
- `O` - Remote setpoint override active
- `F` - Flame status
- `H` - Central heating active
- `W` - Hot water active
- `C` - Comfort mode (elevated DHW setpoint)
- `E` - Transmission error detected
- `B` - Boiler requires service/maintenance
- `P` - Raised power mode active

### Category 8: GPIO Configuration (2 commands)

Configure GPIO pin functions. NOT currently in setcmds[].

| Command | Status | Description | Response Format | Example |
|---------|--------|-------------|-----------------|---------|
| **GA** | ❌ Missing | GPIO A configuration (assign function to GPIO A) | `GA: <func>` | `GA: 2` |
| **GB** | ❌ Missing | GPIO B configuration (assign function to GPIO B) | `GB: <func>` | `GB: 5` |

**GPIO Function Codes:**
- `0` - No function (input, default)
- `1` - Ground (output low)
- `2` - Vcc (output high)
- `3` - LED E function
- `4` - LED F function
- `5` - DHW blocking (comfort mode)
- `6` - DHW blocking (setback)
- `7` - Home/Away mode
- `8` - DHW setpoint override

### Category 9: Temperature Sensor (1 command)

Configure which temperature sensor to use.

| Command | Status | Description | Response Format | Example |
|---------|--------|-------------|-----------------|---------|
| **TS** | ✅ In setcmds[] | Temperature sensor (configure which sensor to use) | `TS: <0/1>` | `TS: 0` |

### Category 10: Print/Report Commands (15+ commands)

Query/diagnostic commands that return information. NOT in setcmds[] - these are query-only.

| Command | Status | Description | Response Format | Example |
|---------|--------|-------------|-----------------|---------|
| **PR=A** | ❌ Missing | Print report: banner/version | `PR: OpenTherm Gateway <ver>` | `PR: OpenTherm Gateway 5.0` |
| **PR=B** | ❌ Missing | Print report: build date | `PR: <date>` | `PR: 12:34:56 Jan 1 2024` |
| **PR=E** | ❌ Missing | Print report: external sensor temperature | `PR: <temp>` | `PR: 21.50` |
| **PR=F** | ❌ Missing | Print report: PIC can be flashed | `PR: <0/1>` | `PR: 1` |
| **PR=G** | ❌ Missing | Print report: GPIO functions | `PR: <A func>,<B func>` | `PR: 0,5` |
| **PR=I** | ❌ Missing | Print report: GPIO input states | `PR: <A state>,<B state>` | `PR: 0,1` |
| **PR=L** | ❌ Missing | Print report: LED functions | `PR: ABCDEF` | `PR: RXFTCB` |
| **PR=M** | ❌ Missing | Print report: gateway mode | `PR: <0/1>` | `PR: 1` |
| **PR=O** | ❌ Missing | Print report: OpenTherm messages (raw) | `PR: <messages>` | - |
| **PR=P** | ❌ Missing | Print report: setpoint adjustments | `PR: <adjustments>` | `PR: 0.00` |
| **PR=R** | ❌ Missing | Print report: remote override parameters | `PR: <params>` | - |
| **PR=S** | ❌ Missing | Print report: smart power configuration | `PR: <config>` | `PR: 0` |
| **PR=T** | ❌ Missing | Print report: thermostat detection | `PR: <0/1>` | `PR: 1` |
| **PR=V** | ❌ Missing | Print report: voltage reference | `PR: <0-255>` | `PR: 128` |
| **PR=W** | ❌ Missing | Print report: hot water settings | `PR: <settings>` | - |

### Category 11: Print Summary (2 commands)

Control data reporting mode. NOT in setcmds[].

| Command | Status | Description | Response Format | Example |
|---------|--------|-------------|-----------------|---------|
| **PS=0** | ❌ Missing | Print summary off (raw mode - continuous OT messages) | `PS: 0` | `PS: 0` |
| **PS=1** | ❌ Missing | Print summary on (summary mode - only updates on request) | `PS: 1` | `PS: 1` |

### Category 12: Reset & Special (3 commands)

System control commands. NOT in setcmds[].

| Command | Status | Description | Response Format | Example |
|---------|--------|-------------|-----------------|---------|
| **GW=R** | ❌ Missing | Gateway reset (soft reset of PIC firmware) | `GW: R` | `GW: R` |
| **MI** | ❌ Missing | Message interval (set standalone mode message interval) | `MI: <seconds>` | `MI: 10` |
| **BS** | ❌ Missing | Boiler setpoint (modify boiler setpoint message) | `BS: <temp>` | `BS: 65.0` |

### Category 13: Reference Parameters (1 command)

Reset parameter adjustments.

| Command | Status | Description | Response Format | Example |
|---------|--------|-------------|-----------------|---------|
| **RA** | ❌ Missing | Reset adjustments (reset all setpoint adjustments) | `RA: 0` | `RA: 0` |

## Command Response Format

All successful command responses follow the format: `XX: <value>`

Where:
- `XX` = Two-letter command code
- `:` = Separator
- `<value>` = Command-specific response value

### Error Responses

Commands can also return error codes instead of values:

| Error Code | Description |
|------------|-------------|
| **NG** | No Good - The command code is unknown |
| **SE** | Syntax Error - The command contained an unexpected character or was incomplete |
| **BV** | Bad Value - The command contained a data value that is not allowed |
| **OR** | Out of Range - A number was specified outside of the allowed range |
| **NS** | No Space - The alternative Data-ID could not be added because the table is full |
| **NF** | Not Found - The specified alternative Data-ID could not be removed because it does not exist |
| **OE** | Overrun Error - The processor was busy and failed to process all received characters |

## Current Implementation Status

### Implemented in setcmds[] (28 commands):
TT, TC, OT, HW, GW, SB, SH, SW, MM, CS, C2, CH, H2, VS, TS, AA, DA, UI, KI, PM, SR, CR, RS, IT, OH, FT, VR, DP

### Missing from setcmds[] (32+ commands):
TG, SC, SM, BW, LA, LB, LC, LD, LE, LF, GA, GB, PR=A through PR=W (15 variants), PS=0, PS=1, GW=R, MI, BS, RA

## Next Steps

1. Decide which missing commands should be added to MQTT broadcasting
2. Determine appropriate MQTT topic structure for responses
3. Implement command response parser and broadcaster
4. Test with actual OTGW hardware

## References

- Official OTGW Firmware Documentation: https://otgw.tclcode.com/firmware.html
- OTGW Changelog: https://otgw.tclcode.com/changelog.html
- Current codebase: MQTTstuff.ino (setcmds[] array, lines 130-160)
- Current response handler: OTGW-Core.ino (processOT() function, lines 1742-1820)
