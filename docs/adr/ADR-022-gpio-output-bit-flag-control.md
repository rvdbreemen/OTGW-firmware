# ADR-022: GPIO Output Control (Bit-Flag Triggered Relays)

**Status:** Accepted  
**Date:** 2020-06-01 (Estimated)  
**Updated:** 2026-01-28 (Documentation)

## Context

Users wanted to control external devices based on OpenTherm status:
- Activate circulation pump when heating active
- Turn on ventilation when DHW (hot water) active
- Signal alerts when flame off but heating requested
- Control zone valves based on heating mode
- Trigger external cooling/heating systems

**Requirements:**
- React to OpenTherm status changes in real-time
- No complex logic (keep simple)
- Low latency (immediate response)
- Configurable trigger conditions
- Support standard relay modules

**OpenTherm status flags available:**
- Bit 0: CH (Central Heating) enable
- Bit 1: DHW (Domestic Hot Water) enable
- Bit 2: Cooling enable
- Bit 3: OTC (Outside Temperature Compensation) active
- Bit 4: CH2 (second heating circuit) enable
- Bit 5: Summer/Winter mode
- Bit 6: DHW blocking
- Bit 7-15: Reserved/other flags

## Decision

**Implement stateless GPIO output control triggered by configurable OpenTherm status bit.**

**Architecture:**
- **Trigger:** Select any bit from OpenTherm status flags
- **GPIO:** Single configurable output pin (0-16)
- **Logic:** Output HIGH when selected bit is set, LOW when clear
- **Evaluation:** Check on every OpenTherm message update
- **No state:** Output recomputed each time, no persistent state
- **Validation:** Pin and bit range validated on each operation

**Configuration:**
```json
{
  "GPIOOUTPUTSpin": 15,
  "GPIOOUTPUTStriggerBit": 0
}
```

**Bit 0 trigger = CH enable:**
- Bit 0 set → GPIO HIGH → Relay ON → Pump runs
- Bit 0 clear → GPIO LOW → Relay OFF → Pump stops

## Alternatives Considered

### Alternative 1: Temperature Threshold Triggers
**Pros:**
- More flexible (can trigger at specific temperatures)
- User-friendly (set temperature, not bits)
- Can implement complex logic

**Cons:**
- Duplicates thermostat logic
- More complex configuration
- Requires persistent state
- Threshold hysteresis needed
- Multiple thresholds = complex UI

**Why not chosen:** OpenTherm already provides status bits. Simpler to use existing protocol.

### Alternative 2: Multiple Independent GPIO Outputs
**Pros:**
- Control multiple devices independently
- Each with own trigger bit
- More flexible

**Cons:**
- More GPIO pins consumed
- More complex configuration
- More memory (arrays of configs)
- Overkill for most users

**Why not chosen:** Single output covers 90% of use cases. Can add more in future if needed.

### Alternative 3: PWM Output for Variable Control
**Pros:**
- Variable speed pump control
- Gradual transitions
- Energy efficiency

**Cons:**
- Much more complex
- Most relays are on/off only
- Requires PWM-capable pumps
- Adds complexity

**Why not chosen:** Binary on/off is simpler and matches most relay hardware.

### Alternative 4: MQTT-Controlled Output
**Pros:**
- Remote control via MQTT
- Home Assistant can control
- More flexible logic externally

**Cons:**
- Network latency
- Requires MQTT broker
- Not real-time (500ms+ delay)
- Separate from OpenTherm logic

**Why not chosen:** Want immediate hardware response to OpenTherm status, not network-dependent.

### Alternative 5: Scripting Language (Lua, JavaScript)
**Pros:**
- Unlimited flexibility
- User-programmable logic
- Can implement complex rules

**Cons:**
- Memory overhead (interpreter)
- Security risks (user code)
- Too complex for simple use case
- Debugging nightmare on ESP8266

**Why not chosen:** Far too complex. Bit-flag is simple and meets need.

## Consequences

### Positive
- **Simple configuration:** Two settings (pin + bit)
- **Real-time:** Immediate response to OpenTherm changes
- **Stateless:** No state tracking, always recomputed
- **Flexible:** Any status bit can trigger output
- **Low overhead:** Single GPIO check per OT message
- **Predictable:** Output always matches selected bit state
- **Debuggable:** Easy to verify (check bit, check output)

### Negative
- **Single output:** Only one GPIO controlled
  - Future: Could add array of outputs if needed
- **Binary only:** No PWM or variable control
  - Accepted: Relays are binary anyway
- **No logic:** Cannot combine multiple bits (e.g., "CH AND NOT DHW")
  - Accepted: Keep it simple, use Home Assistant for complex logic
- **No inversion:** Cannot trigger on bit clear (would require NOT config)
  - Mitigation: Could add "invert" setting in future

### Risks & Mitigation
- **Wrong GPIO:** User configures output that conflicts with input
  - **Mitigation:** Validate GPIO range, warn about reserved pins
  - **Mitigation:** Documentation lists safe GPIO pins
- **Relay sticking:** Mechanical relay fails closed or open
  - **Accepted:** Hardware failure, outside firmware scope
- **Bit confusion:** User selects wrong trigger bit
  - **Mitigation:** Web UI shows bit meanings
  - **Mitigation:** Documentation explains each bit
- **Electrical hazard:** User connects AC voltage to ESP8266
  - **Mitigation:** Documentation emphasizes relay module requirement
  - **Warning:** "Never connect AC directly to ESP8266"

## Implementation Details

**Output evaluation:**
```cpp
void updateGPIOOutputs() {
  // Check if GPIO output configured
  if (settingGPIOOUTPUTSpin == 0) {
    return;  // Disabled
  }
  
  // Validate GPIO pin
  if (settingGPIOOUTPUTSpin > 16) {
    return;  // Invalid pin
  }
  
  // Validate trigger bit
  if (settingGPIOOUTPUTStriggerBit > 7) {
    return;  // Invalid bit
  }
  
  // Read status flags from OpenTherm
  uint16_t statusFlags = OTcurrentSystemState.Statusflags;
  
  // Check if selected bit is set
  bool bitState = (statusFlags & (1U << settingGPIOOUTPUTStriggerBit)) != 0;
  
  // Set GPIO output
  digitalWrite(settingGPIOOUTPUTSpin, bitState ? HIGH : LOW);
  
  DebugTf(PSTR("GPIO %d = %s (bit %d = %d)\r\n"),
    settingGPIOOUTPUTSpin,
    bitState ? "HIGH" : "LOW",
    settingGPIOOUTPUTStriggerBit,
    bitState);
}
```

**Initialization:**
```cpp
void initGPIOOutputs() {
  if (settingGPIOOUTPUTSpin == 0) {
    return;  // Disabled
  }
  
  // Configure pin as output
  pinMode(settingGPIOOUTPUTSpin, OUTPUT);
  
  // Initialize to LOW (relay off)
  digitalWrite(settingGPIOOUTPUTSpin, LOW);
  
  DebugTf(PSTR("GPIO output on pin %d, trigger bit %d\r\n"),
    settingGPIOOUTPUTSpin,
    settingGPIOOUTPUTStriggerBit);
}
```

**Called from OpenTherm handler:**
```cpp
void processOTMessage(OpenThermMessage msg) {
  // Parse message, update OTcurrentSystemState
  // ...
  
  // Update GPIO outputs based on new status
  updateGPIOOutputs();
  
  // Continue processing...
}
```

## Status Bit Reference

| Bit | Name | Description | Common Use |
|-----|------|-------------|------------|
| 0 | CH enable | Central heating active | Circulation pump |
| 1 | DHW enable | Hot water heating active | DHW pump/valve |
| 2 | Cooling | Cooling mode active | Cooling valve |
| 3 | OTC | Outside temp compensation | - |
| 4 | CH2 | Second heating circuit | Zone 2 valve |
| 5 | Summer/Winter | Summer mode flag | - |
| 6 | DHW blocking | DHW temporarily blocked | - |
| 7 | - | Reserved | - |

## Typical Configurations

**1. Circulation pump on heating:**
```json
{
  "GPIOOUTPUTSpin": 15,
  "GPIOOUTPUTStriggerBit": 0
}
```
Pump runs when CH active.

**2. DHW pump/valve:**
```json
{
  "GPIOOUTPUTSpin": 14,
  "GPIOOUTPUTStriggerBit": 1
}
```
Valve opens when DHW heating.

**3. Zone valve for CH2:**
```json
{
  "GPIOOUTPUTSpin": 13,
  "GPIOOUTPUTStriggerBit": 4
}
```
Separate zone control.

## Hardware Setup

**Relay module connection:**
```
ESP8266 GPIO → Relay Module Signal (IN)
ESP8266 GND → Relay Module GND
5V (external) → Relay Module VCC

Relay contacts:
  COM (common) → Load voltage source
  NO (normally open) → Load device
  
When GPIO HIGH:
  Relay energized → NO closes → Device ON
  
When GPIO LOW:
  Relay de-energized → NO opens → Device OFF
```

**Recommended relay modules:**
- 1-channel 5V relay module (opto-isolated)
- Rated for load voltage (AC 230V or DC 12/24V)
- Active HIGH trigger (most common)

**Safety notes:**
- **Never** connect AC voltage directly to ESP8266
- **Always** use relay module with isolation
- Verify relay voltage rating matches load
- Use proper wire gauge for load current

## Web UI Configuration

**Settings page fields:**
```html
<label>GPIO Output Pin (0-16, 0=disabled):</label>
<input type="number" id="gpioPin" min="0" max="16" 
       value="0">

<label>Trigger Bit (0-7):</label>
<select id="triggerBit">
  <option value="0">Bit 0 - CH Enable</option>
  <option value="1">Bit 1 - DHW Enable</option>
  <option value="2">Bit 2 - Cooling</option>
  <option value="3">Bit 3 - OTC Active</option>
  <option value="4">Bit 4 - CH2 Enable</option>
  <option value="5">Bit 5 - Summer/Winter</option>
  <option value="6">Bit 6 - DHW Blocking</option>
  <option value="7">Bit 7 - Reserved</option>
</select>
```

## Future Enhancements (Not Implemented)

**Potential additions:**
1. **Multiple outputs:** Array of pin/bit pairs
2. **Invert option:** Trigger on bit clear instead of set
3. **Hysteresis:** Delay before changing output
4. **Combined bits:** AND/OR logic between multiple bits
5. **PWM mode:** Variable output for speed control

**Why not now:**
- Current design covers 90% of use cases
- Complexity not justified
- Can be added backward-compatibly later

## Related Decisions
- ADR-007: Timer-Based Task Scheduling (GPIO updates in main loop)

## References
- Implementation: `outputs_ext.ino`
- OpenTherm specification: `Specification/OpenTherm-Protocol-Specification-v4.2.pdf`
- GPIO reference: ESP8266 pinout diagrams in `docs/`
