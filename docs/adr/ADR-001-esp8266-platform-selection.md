# ADR-001: ESP8266 Platform Selection

**Status:** Accepted  
**Date:** 2016-01-01 (Initial implementation)  
**Updated:** 2026-01-28 (Documentation)

## Context

The NodoShop OpenTherm Gateway (OTGW) required network connectivity to provide remote monitoring and control capabilities. The hardware needed to:
- Interface with the existing PIC-based OpenTherm Gateway controller via serial communication
- Provide WiFi connectivity for local network integration
- Run multiple concurrent network services (HTTP, MQTT, WebSocket, TCP)
- Fit within cost constraints for a consumer device
- Have sufficient community support and tooling

The device would serve as a bridge between the PIC controller (handling OpenTherm protocol) and modern home automation systems (primarily Home Assistant).

## Decision

Use the **ESP8266 (NodeMCU v1.0 / Wemos D1 mini)** as the network controller platform with the Arduino framework.

**Key specifications:**
- CPU: 80/160 MHz Tensilica L106 32-bit
- RAM: ~80KB total (~40KB usable after Arduino core)
- Flash: 4MB (2MB for filesystem, ~1MB for OTA)
- WiFi: 802.11 b/g/n 2.4GHz
- Programming: Arduino C/C++ via ESP8266 Arduino Core
- Build tool: arduino-cli with Makefile automation

## Alternatives Considered

### Alternative 1: ESP32
**Pros:**
- More RAM (~520KB)
- Dual-core processor
- Bluetooth support
- Better WiFi performance

**Cons:**
- Higher cost (~2-3x ESP8266)
- Unnecessary features for the use case (Bluetooth, dual-core)
- More complex power requirements
- Larger physical footprint

**Why not chosen:** Cost and complexity. The ESP8266 provides sufficient resources for the OTGW bridge application at a much lower price point.

### Alternative 2: Arduino Ethernet Shield
**Pros:**
- Native Arduino compatibility
- Stable, well-known platform
- Wired network (no WiFi configuration needed)

**Cons:**
- Requires separate Arduino board
- No built-in WiFi (requires separate module or Ethernet)
- Higher total component cost
- Larger footprint
- Less community momentum for IoT applications

**Why not chosen:** Higher cost, larger size, and lack of integrated WiFi made it less suitable for a consumer IoT device.

### Alternative 3: Raspberry Pi Zero W
**Pros:**
- Much more powerful (1GHz ARM, 512MB RAM)
- Full Linux environment
- Better development tools

**Cons:**
- Significant cost increase (~$10 vs ~$2-3)
- Overkill for the application
- Higher power consumption
- Longer boot time
- More complex software stack
- Requires SD card (reliability concerns)

**Why not chosen:** Excessive cost and complexity for a device that only needs to bridge serial to network.

## Consequences

### Positive
- **Low cost:** ESP8266 enables affordable consumer pricing
- **WiFi integration:** Built-in WiFi eliminates need for additional hardware
- **Strong ecosystem:** Arduino framework provides extensive library support
- **OTA updates:** Flash memory allows remote firmware updates
- **Proven platform:** Millions of ESP8266 devices deployed successfully
- **Low power:** Suitable for always-on operation
- **Community support:** Large maker community provides examples and troubleshooting

### Negative
- **Limited RAM:** ~40KB usable memory requires careful resource management
  - Mitigation: Static buffer allocation, PROGMEM for string literals, heap monitoring
- **Single core:** Cooperative multitasking required (no threading)
  - Mitigation: Timer-based architecture with non-blocking operations
- **No HTTPS:** TLS/SSL too resource-intensive for ESP8266
  - Mitigation: Local network only deployment model, VPN for remote access
- **WiFi only:** No wired network option
  - Accepted: Target use case is home installation with WiFi available
- **49-day millis() rollover:** Timer wraps after ~49 days
  - Mitigation: Safe timer implementation in `safeTimers.h`

### Risks & Mitigation
- **Heap fragmentation:** ESP8266 prone to memory fragmentation
  - **Mitigation:** Static buffers, PROGMEM strings, heap monitoring system (v1.0.0+)
- **Hardware watchdog:** ESP8266 can crash/hang
  - **Mitigation:** External I2C watchdog chip for automatic recovery
- **Flash wear:** Limited write cycles on flash memory
  - **Mitigation:** LittleFS with wear leveling, minimal writes to filesystem

## Related Decisions
- ADR-004: Static Buffer Allocation Strategy (addresses RAM constraints)
- ADR-003: HTTP-Only Network Architecture (addresses TLS limitations)
- ADR-007: Timer-Based Task Scheduling (addresses single-core constraint)
- ADR-009: PROGMEM Usage for String Literals (addresses RAM constraints)

## References
- ESP8266 Arduino Core: https://github.com/esp8266/Arduino
- ESP8266 Technical Reference: https://www.espressif.com/sites/default/files/documentation/esp8266-technical_reference_en.pdf
- NodeMCU Documentation: https://nodemcu.readthedocs.io/
- Hardware schematics: `hardware/` directory
- Build system: `BUILD.md`, `build.py`
