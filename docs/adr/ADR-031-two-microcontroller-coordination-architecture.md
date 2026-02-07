# ADR-031: Two-Microcontroller Coordination Architecture

**Status:** Accepted  
**Date:** 2026-02-07  
**Decision Maker:** Copilot Agent based on codebase analysis

## Context

The NodoShop OpenTherm Gateway hardware uses a dual-microcontroller architecture:
- **ESP8266**: Network controller (WiFi, HTTP, MQTT, WebSocket)
- **PIC microcontroller** (16F88 or 16F1847): OpenTherm protocol controller

This separation of concerns enables the ESP8266 to provide modern network connectivity while the PIC handles real-time OpenTherm communication with boilers/thermostats.

**Why two microcontrollers?**

1. **OpenTherm timing requirements**: OpenTherm protocol requires precise timing (microseconds) for Manchester encoding that is difficult to achieve on ESP8266 while running WiFi/network stack
2. **Proven PIC firmware**: Schelte Bron's PIC firmware (OTGW) is mature, stable, and battle-tested
3. **Separation of concerns**: Network layer (ESP8266) vs protocol layer (PIC)
4. **Upgrade path**: ESP8266 adds modern features without changing proven OpenTherm implementation

**Key challenges:**
- Coordinating two independent processors
- Reliable serial communication
- PIC firmware upgrade via ESP8266
- Reset and bootloader control
- Version detection and compatibility

## Decision

**Adopt Master/Slave architecture with ESP8266 as master, PIC as slave.**

**Architecture:**

```
┌────────────────────────────────────────────────────────────┐
│ User/Client                                                 │
│ (Browser, Home Assistant, MQTT, OTmonitor)                 │
└───────────────────┬────────────────────────────────────────┘
                    │
                    │ HTTP/MQTT/WebSocket/TCP
                    ▼
┌────────────────────────────────────────────────────────────┐
│ ESP8266 (Master Controller)                                │
│ - WiFi connectivity                                         │
│ - HTTP server (REST API, Web UI)                           │
│ - MQTT client (Home Assistant integration)                 │
│ - WebSocket server (real-time streaming)                   │
│ - TCP server (OTmonitor compatibility)                     │
│ - Command queue management                                 │
│ - Sensor integration (Dallas DS18B20, S0 pulse)           │
└────────────────────┬───────────────────────────────────────┘
                     │
                     │ Serial UART (9600 baud)
                     │ GPIO reset control (GPIO14)
                     ▼
┌────────────────────────────────────────────────────────────┐
│ PIC Microcontroller (Slave Controller)                     │
│ (16F88 or 16F1847)                                         │
│ - OpenTherm protocol implementation                         │
│ - Manchester encoding/decoding                              │
│ - Boiler ↔ Thermostat communication                        │
│ - Real-time timing control                                 │
│ - Command processing from ESP8266                          │
│ - Status reporting to ESP8266                              │
└────────────────────┬───────────────────────────────────────┘
                     │
                     │ OpenTherm protocol
                     ▼
┌────────────────────────────────────────────────────────────┐
│ Boiler ↔ Thermostat                                        │
└────────────────────────────────────────────────────────────┘
```

**Communication Protocol:**

1. **Serial Interface:**
   - Baud rate: 9600 bps
   - Format: 8N1 (8 data bits, no parity, 1 stop bit)
   - Terminator: ETX (0x03) character
   - Direction: Bidirectional

2. **Message Types (ESP8266 → PIC):**
   - **Commands**: Two-letter codes (e.g., `TT=20.5` for temp setpoint)
   - **Queries**: Single-letter codes (e.g., `PR=A` for version)
   - **Reset**: Hardware reset via GPIO14 (PICRST)

3. **Message Types (PIC → ESP8266):**
   - **OpenTherm messages**: Raw protocol frames
   - **Status responses**: Command acknowledgments
   - **Version info**: Firmware version and type
   - **Errors**: Error codes and diagnostics

4. **Reset Control:**
   - GPIO14 (D5) connected to PIC MCLR (reset) pin
   - Pull low to reset PIC
   - Pull high for normal operation
   - Used for: bootloader entry, error recovery, firmware upgrade

## Alternatives Considered

### Alternative 1: Single ESP8266 (No PIC)

**Pros:**
- Simpler hardware (one chip)
- Lower cost
- Fewer failure points
- No inter-chip communication

**Cons:**
- ESP8266 cannot meet OpenTherm timing requirements
- Manchester encoding difficult with WiFi interrupts
- Would need ESP32 (more expensive)
- Lose proven, stable PIC firmware
- ESP8266 WiFi interferes with microsecond timing

**Why not chosen:** ESP8266 cannot reliably handle OpenTherm protocol timing while running WiFi stack. Dual-chip approach proven and stable.

### Alternative 2: ESP32 (Single Chip with Dual Core)

**Pros:**
- Dual core can dedicate one core to timing
- More RAM and processing power
- Built-in Bluetooth
- Better WiFi performance

**Cons:**
- 2-3x higher cost than ESP8266
- More complex development
- Higher power consumption
- Lose proven PIC firmware
- Would require complete rewrite
- Backwards incompatible with existing hardware

**Why not chosen:** ESP32 would require complete rewrite and doesn't leverage proven PIC firmware. Cost-benefit doesn't justify migration.

### Alternative 3: ESP8266 with SPI/I2C to PIC

**Pros:**
- Higher throughput than UART
- Hardware-assisted protocol
- More structured communication

**Cons:**
- PIC firmware doesn't support SPI/I2C
- More complex wiring
- Requires PIC firmware changes
- UART is sufficient for current needs
- Breaks compatibility with Schelte's firmware

**Why not chosen:** UART is sufficient and maintains compatibility with existing PIC firmware. No benefit to added complexity.

### Alternative 4: Raspberry Pi + USB OTGW

**Pros:**
- Much more powerful
- Full Linux environment
- Easy development

**Cons:**
- 10x higher cost
- Requires USB OTGW adapter
- Higher power consumption
- Longer boot time
- SD card reliability concerns
- Overkill for the application

**Why not chosen:** Excessive cost and complexity for a simple gateway device. ESP8266+PIC is cost-effective and reliable.

## Consequences

### Positive

1. **Proven OpenTherm Implementation** ✅
   - Leverages Schelte Bron's stable, tested PIC firmware
   - Years of field deployment
   - Known compatibility with wide range of boilers
   - No need to re-implement complex protocol

2. **Clean Separation of Concerns** ✅
   - ESP8266 focuses on network services
   - PIC focuses on OpenTherm protocol
   - Independent operation and testing
   - Clear interface boundary

3. **Real-Time Guarantee** ✅
   - PIC handles timing-critical operations
   - ESP8266 WiFi doesn't interfere with OpenTherm
   - Reliable Manchester encoding
   - Microsecond-level precision maintained

4. **Firmware Upgrade Capability** ✅
   - ESP8266 can upgrade PIC firmware via Web UI
   - No need for external programmer
   - Convenient for users
   - Version detection and compatibility checking

5. **Cost-Effective** ✅
   - ESP8266 is inexpensive (~$2-3)
   - PIC is inexpensive (~$1-2)
   - Total BOM lower than alternatives
   - Proven reliability reduces support costs

### Negative

1. **Coordination Complexity** ⚠️
   - Two processors to manage
   - Serial communication overhead
   - Reset synchronization needed
   - **Mitigation:** OTGWSerial library abstracts complexity
   - **Impact:** Well-contained in library code

2. **Two Points of Failure** ⚠️
   - Either chip can fail
   - Serial communication can fail
   - **Mitigation:** Hardware watchdog resets ESP8266
   - **Mitigation:** ESP8266 can reset PIC via GPIO
   - **Impact:** Redundant reset mechanisms improve reliability

3. **Version Compatibility** ⚠️
   - ESP8266 firmware and PIC firmware must be compatible
   - Version mismatch can cause issues
   - **Mitigation:** Version detection on boot
   - **Mitigation:** Web UI warnings for mismatches
   - **Impact:** Documented upgrade procedures

4. **Bootloader Timing** ⚠️
   - PIC bootloader entry requires precise timing
   - Reset sequence must be coordinated
   - **Mitigation:** OTGWSerial library handles timing
   - **Mitigation:** Retry logic for failed entries
   - **Impact:** Firmware upgrades are reliable

### Risks & Mitigation

**Risk 1:** Serial communication corruption
- **Impact:** Lost messages, incorrect commands
- **Mitigation:** ETX terminator for frame detection
- **Mitigation:** Checksum validation in PIC firmware
- **Mitigation:** Command queue with retry logic
- **Monitoring:** Serial errors logged to telnet

**Risk 2:** PIC firmware becomes unresponsive
- **Impact:** OpenTherm communication stops
- **Mitigation:** Hardware reset via GPIO14
- **Mitigation:** Watchdog timer in PIC
- **Mitigation:** ESP8266 can force reset
- **Monitoring:** Version queries detect unresponsive PIC

**Risk 3:** Firmware upgrade bricks PIC
- **Impact:** Device becomes unusable
- **Mitigation:** Bootloader is protected (cannot be overwritten)
- **Mitigation:** Hex file validation before upload
- **Mitigation:** Banner detection in hex files
- **Mitigation:** Recovery mode always available
- **Monitoring:** Upgrade progress streamed via WebSocket

## Implementation Details

### Hardware Connections

**UART:**
- ESP8266 TX → PIC RX
- ESP8266 RX → PIC TX
- Baud: 9600, 8N1

**Reset Control:**
- ESP8266 GPIO14 (D5) → PIC MCLR
- Pull-up resistor on MCLR
- Active-low reset

**LEDs:**
- LED1 (GPIO2/D4): Status indicator
- LED2 (GPIO16/D0): Controlled by PIC, reports to ESP8266

### OTGWSerial Library

**Location:** `src/libraries/OTGWSerial/`

**Key Features:**
- PIC type detection (16F88 vs 16F1847)
- Firmware version querying
- Bootloader entry/exit
- Hex file parsing and upload
- Progress callbacks for Web UI
- Error handling and retry logic

**Example Usage:**

```cpp
// Initialize
#define PICRST D5
OTGWSerial OTGWSerial(PICRST, LED2);

// Reset PIC
digitalWrite(PICRST, LOW);
delay(100);
digitalWrite(PICRST, HIGH);

// Send command to PIC
OTGWSerial.println("TT=20.5");  // Set temp to 20.5°C

// Read response from PIC
while (OTGWSerial.available()) {
  String line = OTGWSerial.readStringUntil('\n');
  // Process OpenTherm message or response
}

// Upgrade PIC firmware
OTGWSerial.startUpgrade("/firmware.hex");
```

### Command Queue

**Location:** `src/OTGW-firmware/OTGW-Core.ino`

Commands from multiple sources (HTTP, MQTT, WebSocket) are queued to prevent serial buffer overruns:

```cpp
addOTWGcmdtoqueue(command);  // Queue command
processOTGWqueue();          // Send queued commands to PIC
```

### Version Detection

**On Boot:**
```cpp
// Query PIC version
OTGWSerial.println("PR=A");

// Parse response
// OpenTherm Gateway 4.2.5
//                  ^ ^ ^
//         Major ───┘ │ │
//         Minor ─────┘ │
//         Build ───────┘
```

### Bootloader Entry

**Sequence for Firmware Upgrade:**
1. Pull GPIO14 low (reset PIC)
2. Wait 100ms
3. Release GPIO14 high
4. Send bootloader command within timing window
5. PIC enters bootloader mode
6. Upload hex file via serial
7. Reset PIC to run new firmware

## Message Format Examples

**ESP8266 → PIC Commands:**
```
TT=20.5        # Set temperature to 20.5°C
PR=A           # Query version (returns "A")
SW=60          # Set DHW setpoint to 60°C
GW=1           # Gateway mode command
```

**PIC → ESP8266 Responses:**
```
T80000100      # OpenTherm message (temp read)
OpenTherm Gateway 4.2.5   # Version response
Error 01       # Error code
```

## PIC Firmware Types

The PIC can run different firmware:
- **OTGW**: Standard OpenTherm Gateway (production)
- **DIAG**: Diagnostic firmware (testing)
- **INTF**: Interface test firmware (development)

Version detection identifies firmware type and compatibility.

## Coordination Patterns

**Pattern 1: Command-Response**
```
ESP8266: Send command
ESP8266: Wait for response
PIC:     Process command
PIC:     Send response
ESP8266: Process response
```

**Pattern 2: Unsolicited Messages**
```
PIC:     Receives OpenTherm message from boiler
PIC:     Forwards to ESP8266 via serial
ESP8266: Broadcasts via WebSocket/MQTT
ESP8266: No response required
```

**Pattern 3: Reset Recovery**
```
ESP8266: Detects PIC unresponsive
ESP8266: Pull GPIO14 low (reset)
ESP8266: Wait 100ms
ESP8266: Release GPIO14 high
PIC:     Boots and resumes operation
ESP8266: Re-query version
```

## Related Decisions

- **ADR-001:** ESP8266 Platform Selection (establishes network controller choice)
- **ADR-012:** PIC Firmware Upgrade via Web UI (documents upgrade process)
- **ADR-016:** OpenTherm Command Queue (prevents serial overruns)
- **ADR-011:** External Hardware Watchdog (ESP8266 recovery mechanism)

## References

- **OTGWSerial Library:** `src/libraries/OTGWSerial/` (PIC communication abstraction)
- **PIC Firmware:** https://otgw.tclcode.com/ (Schelte Bron's site)
- **OpenTherm Specification:** `Specification/OpenTherm-Protocol-Specification-v4.2.pdf`
- **Hardware Schematics:** NodoShop OTGW hardware documentation
- **OTGW Commands:** https://otgw.tclcode.com/firmware.html (PIC command reference)
- **ADR-012:** PIC Firmware Upgrade via Web UI

---

**This ADR documents the two-microcontroller architecture that enables reliable OpenTherm protocol handling while providing modern network connectivity. The master/slave coordination pattern leverages the strengths of each processor: ESP8266 for networking, PIC for real-time protocol.**
