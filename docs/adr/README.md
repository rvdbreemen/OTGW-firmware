# Architecture Decision Records (ADRs)

This directory contains Architecture Decision Records (ADRs) that document significant architectural choices made in the OTGW-firmware project.

## What are ADRs?

Architecture Decision Records capture important architectural decisions along with their context, alternatives considered, and consequences. They serve as historical documentation to help current and future developers understand why the system is built the way it is.

## ADR Index

### Platform and Build System
- **[ADR-001: ESP8266 Platform Selection](ADR-001-esp8266-platform-selection.md)**  
  Why we chose ESP8266 over ESP32, Raspberry Pi, and other alternatives for the network controller platform.

- **[ADR-002: Modular .ino File Architecture](ADR-002-modular-ino-architecture.md)**  
  How the codebase is organized into multiple `.ino` files by functional domain while maintaining Arduino compatibility.

### Network and Security
- **[ADR-003: HTTP-Only Network Architecture (No HTTPS)](ADR-003-http-only-no-https.md)**  
  Why the firmware uses HTTP only (no HTTPS/TLS) and the security model for local network deployment.

- **[ADR-010: Multiple Concurrent Network Services](ADR-010-multiple-concurrent-network-services.md)**  
  Running HTTP, WebSocket, Telnet, and MQTT services simultaneously on different ports.

### Memory Management
- **[ADR-004: Static Buffer Allocation Strategy](ADR-004-static-buffer-allocation.md)**  
  How static buffer allocation prevents heap fragmentation and crashes on the memory-constrained ESP8266.

- **[ADR-009: PROGMEM Usage for String Literals](ADR-009-progmem-string-literals.md)**  
  Mandatory use of PROGMEM (F() and PSTR() macros) to move string literals from RAM to flash memory.

### Integration and Communication
- **[ADR-005: WebSocket for Real-Time Streaming](ADR-005-websocket-real-time-streaming.md)**  
  Using WebSocket protocol on port 81 for real-time OpenTherm message streaming to web browsers.

- **[ADR-006: MQTT Integration Pattern](ADR-006-mqtt-integration-pattern.md)**  
  MQTT client implementation with Home Assistant Auto-Discovery for zero-configuration integration.

### System Architecture
- **[ADR-007: Timer-Based Task Scheduling](ADR-007-timer-based-task-scheduling.md)**  
  Non-blocking timer-based task scheduling with 49-day rollover protection for cooperative multitasking.

- **[ADR-008: LittleFS for Configuration Persistence](ADR-008-littlefs-configuration-persistence.md)**  
  Using LittleFS filesystem with JSON files for configuration storage that survives firmware updates.

### Hardware and Reliability
- **[ADR-011: External Hardware Watchdog for Reliability](ADR-011-external-hardware-watchdog.md)**  
  I2C hardware watchdog chip that automatically resets the ESP8266 if firmware hangs or crashes.

- **[ADR-012: PIC Firmware Upgrade via Web UI](ADR-012-pic-firmware-upgrade-via-web.md)**  
  Safe PIC microcontroller firmware flashing through the Web UI with WebSocket progress streaming.

### Development and Build
- **[ADR-013: Arduino Framework Over ESP-IDF](ADR-013-arduino-framework-over-esp-idf.md)**  
  Using Arduino framework for rapid development and rich ecosystem instead of low-level ESP-IDF.

- **[ADR-014: Dual Build System (Makefile + Python Script)](ADR-014-dual-build-system.md)**  
  Makefile for CI/CD and build.py wrapper for cross-platform developer convenience.

### Core Services
- **[ADR-015: NTP and AceTime for Time Management](ADR-015-ntp-acetime-time-management.md)**  
  Network time synchronization with AceTime library for comprehensive timezone and DST support.

- **[ADR-016: OpenTherm Command Queue with Deduplication](ADR-016-opentherm-command-queue.md)**  
  Command queuing system to prevent serial buffer overruns and eliminate duplicate commands.

- **[ADR-017: WiFiManager for Initial Configuration](ADR-017-wifimanager-initial-configuration.md)**  
  Captive portal for easy first-time WiFi setup without hardcoded credentials.

- **[ADR-018: ArduinoJson for Data Interchange](ADR-018-arduinojson-data-interchange.md)**  
  Standardized JSON handling for settings persistence, REST API, MQTT, and WebSocket communication.

### Features and Extensions
- **[ADR-019: REST API Versioning Strategy](ADR-019-rest-api-versioning-strategy.md)**  
  URL path-based API versioning (v0/v1/v2) with indefinite backward compatibility.

- **[ADR-020: Dallas DS18B20 Temperature Sensor Integration](ADR-020-dallas-ds18b20-sensor-integration.md)**  
  OneWire-based multi-sensor temperature monitoring with MQTT integration and auto-discovery.

- **[ADR-021: S0 Pulse Counter Hardware Interrupt Architecture](ADR-021-s0-pulse-counter-interrupt-architecture.md)**  
  ISR-driven energy meter pulse counting with debounce logic and real-time power calculation.

- **[ADR-022: GPIO Output Control (Bit-Flag Triggered Relays)](ADR-022-gpio-output-bit-flag-control.md)**  
  Stateless relay control based on OpenTherm status bit flags for external device activation.

- **[ADR-023: File System Explorer HTTP Architecture](ADR-023-filesystem-explorer-http-api.md)**  
  Browser-based LittleFS file management with streaming upload/download and OTA firmware updates.

- **[ADR-024: Debug Telnet Command Console](ADR-024-debug-telnet-command-console.md)**  
  Interactive telnet-based debug console for real-time diagnostics and hardware testing.

## ADR Template

When creating new ADRs, use this structure:

```markdown
# ADR-XXX: [Title]

**Status:** Proposed | Accepted | Deprecated | Superseded by ADR-XXX  
**Date:** YYYY-MM-DD

## Context
Explain the situation or problem that prompted this decision.

## Decision
State the choice that was made.

## Alternatives Considered
List alternatives with pros/cons and why they weren't chosen.

## Consequences
What results from this decision?
- **Positive:** Benefits
- **Negative:** Drawbacks
- **Risks & Mitigation:** Potential issues and how they're addressed

## Related Decisions
Reference other ADRs that relate to this decision.

## References
Links to relevant documentation, code, or resources.
```

## Key Architectural Themes

### Memory Constraints
The ESP8266's limited RAM (~40KB usable) drives many architectural decisions:
- Static buffer allocation (ADR-004)
- PROGMEM for strings (ADR-009)
- No HTTPS/TLS (ADR-003)
- Client connection limits
- Heap monitoring and adaptive throttling

### Local Network Only
The firmware is designed for trusted local network deployment:
- HTTP only, no HTTPS (ADR-003)
- No authentication on endpoints
- WebSocket uses ws:// not wss://
- Security via network isolation

### Home Assistant Focus
Primary integration target is Home Assistant:
- MQTT Auto-Discovery (ADR-006)
- Standard HA entity patterns
- Climate control integration
- Zero-configuration setup

### Cooperative Multitasking
Single-core ESP8266 requires careful task management:
- Timer-based scheduling (ADR-007)
- Non-blocking operations
- Watchdog feeding
- No delay() calls

### Arduino Ecosystem
Maintaining Arduino compatibility for community contributions:
- Arduino framework (ADR-001)
- Modular .ino files (ADR-002)
- Arduino IDE support
- Standard Arduino libraries where possible

## When to Create an ADR

Create an ADR when making a decision that:
- Has long-term impact on the architecture
- Affects multiple components or modules
- Involves trade-offs between alternatives
- Constrains future development choices
- Addresses a significant technical challenge
- Changes existing architectural patterns

## When NOT to Create an ADR

Don't create ADRs for:
- Bug fixes that don't change architecture
- Code refactoring that maintains same structure
- Configuration changes
- Documentation updates
- Minor feature additions within existing patterns

## Superseding ADRs

When an architectural decision changes:
1. Do NOT modify the original ADR
2. Create a new ADR that supersedes the old one
3. Update the old ADR's status to "Superseded by ADR-XXX"
4. Reference the original ADR in the new one

## Resources

- **ADR Best Practices:** https://adr.github.io/
- **Michael Nygard's ADR Template:** https://github.com/joelparkerhenderson/architecture-decision-record
- **Copilot Instructions:** `.github/copilot-instructions.md` (references ADRs)
- **Evaluation Framework:** `evaluate.py` (enforces decisions like PROGMEM usage)

## Maintenance

ADRs are living documentation:
- Review ADRs when onboarding new developers
- Reference ADRs in code reviews
- Update ADR status when decisions change
- Link from code comments to relevant ADRs
- Use ADRs to inform copilot instructions

---

*For questions about these ADRs or to propose new ones, please open an issue on GitHub.*
