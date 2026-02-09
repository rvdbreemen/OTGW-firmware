# Architecture Decision Records (ADRs)

This directory contains Architecture Decision Records (ADRs) that document significant architectural choices made in the OTGW-firmware project.

## What are ADRs?

Architecture Decision Records capture important architectural decisions along with their context, alternatives considered, and consequences. They serve as historical documentation to help current and future developers understand why the system is built the way it is.

## Quick Navigation

**By Topic:**
- [Platform & Build](#platform-and-build-system) (4 ADRs)
- [Memory Management](#memory-management) (4 ADRs) 🆕
- [Network & Security](#network-and-security) (3 ADRs) 🆕
- [Integration](#integration-and-communication) (3 ADRs) 🆕
- [Core Systems](#system-architecture) (6 ADRs)
- [Features & Extensions](#features-and-extensions) (7 ADRs)
- [Browser & Client](#browser-and-client-compatibility) (4 ADRs)
- [OTA & Updates](#ota-and-firmware-updates) (2 ADRs)

**Foundational ADRs** (most referenced by other ADRs):
- **ADR-001:** ESP8266 Platform Selection (establishes hardware constraints)
- **ADR-004:** Static Buffer Allocation (referenced by 8 other ADRs)
- **ADR-007:** Timer-Based Task Scheduling (enables non-blocking architecture)

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

- **[ADR-032: No Authentication Pattern (Local Network Security Model)](ADR-032-no-authentication-local-network-security.md)** 🆕  
  Explicit decision to omit authentication in favor of network-level security (WiFi encryption, network segmentation, VPN for remote access).

### Memory Management
- **[ADR-004: Static Buffer Allocation Strategy](ADR-004-static-buffer-allocation.md)**  
  How static buffer allocation prevents heap fragmentation and crashes on the memory-constrained ESP8266.

- **[ADR-009: PROGMEM Usage for String Literals](ADR-009-progmem-string-literals.md)**  
  Mandatory use of PROGMEM (F() and PSTR() macros) to move string literals from RAM to flash memory.

- **[ADR-028: File Streaming Over Loading for Memory Safety](ADR-028-file-streaming-over-loading.md)**  
  Never load files >2KB into RAM; use streaming patterns to prevent memory exhaustion crashes.

- **[ADR-030: Heap Memory Monitoring and Emergency Recovery](ADR-030-heap-memory-monitoring-emergency-recovery.md)** 🆕  
  Proactive heap monitoring with 4-level health system and adaptive throttling to prevent crashes (CRITICAL <3KB, WARNING 3-5KB, LOW 5-8KB, HEALTHY >8KB).

### Integration and Communication
- **[ADR-005: WebSocket for Real-Time Streaming](ADR-005-websocket-real-time-streaming.md)**  
  Using WebSocket protocol on port 81 for real-time OpenTherm message streaming to web browsers.

- **[ADR-006: MQTT Integration Pattern](ADR-006-mqtt-integration-pattern.md)**  
  MQTT client implementation with Home Assistant Auto-Discovery for zero-configuration integration.

- **[ADR-031: Two-Microcontroller Coordination Architecture](ADR-031-two-microcontroller-coordination-architecture.md)** 🆕  
  Master/Slave architecture with ESP8266 as network controller and PIC microcontroller for OpenTherm protocol (serial communication, GPIO reset control, firmware upgrade capability).

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

- **[ADR-033: Dallas Sensor Custom Labels and Graph Visualization](ADR-033-dallas-sensor-custom-labels-graph-visualization.md)** 🆕  
  Persistent custom sensor labels (16 chars max) with REST API endpoint, dynamic graph visualization with 16-color palette, and non-blocking inline editor.

### Browser and Client Compatibility
- **[ADR-025: Safari WebSocket Connection Management During Firmware Upload](ADR-025-safari-websocket-connection-management.md)**  
  Proactively closing WebSocket before firmware upload to prevent Safari's connection pool exhaustion from dropping it mid-transfer.

- **[ADR-026: Conditional JavaScript Cache-Busting for Firmware/Filesystem Version Mismatches](ADR-026-conditional-javascript-cache-busting.md)**  
  Smart cache management that enables normal browser caching when versions match but forces JavaScript reload during firmware/filesystem version transitions.

- **[ADR-027: Version Mismatch Warning System in Web UI](ADR-027-version-mismatch-warning-system.md)**  
  Prominent visual warning banner that automatically appears when firmware and filesystem versions don't match to prevent user confusion.

- **[ADR-034: Non-Blocking Modal Dialogs for User Input](ADR-034-non-blocking-modal-dialogs.md)** 🆕  
  Custom HTML/CSS modal dialogs instead of blocking prompt() to maintain real-time data flow.


### OTA and Firmware Updates
- **[ADR-028: File Streaming Over Loading for Memory Safety](ADR-028-file-streaming-over-loading.md)** 🆕  
  Never load files >2KB into RAM; use streaming patterns to prevent memory exhaustion crashes.

- **[ADR-029: Simple XHR-Based OTA Flash (KISS Principle)](ADR-029-simple-xhr-ota-flash.md)** 🆕  
  Simplified firmware flash mechanism using XHR with backend confirmation, eliminating WebSocket complexity and Safari bugs. Reduces code by 68.5% while improving reliability.

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
- **No authentication by default** on management endpoints (Web UI, REST API, filesystem, firmware upload)
- WebSocket uses ws:// not wss://
- **Security via network isolation** (primary security control)

**Security Recommendations:**
- Deploy only on trusted, isolated local networks
- Use VPN for remote access (never expose directly to internet)
- Consider adding authentication layer for production deployments
- Implement network segmentation to limit device access
- Regularly review network access controls

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
- Arduino framework (ADR-001, ADR-013)
- Modular .ino files (ADR-002)
- Arduino IDE support
- Standard Arduino libraries where possible

## Architectural Dependencies

**Foundation Layer** (all other ADRs depend on these):
```
ADR-001 (ESP8266) ──┬──> Establishes: 40KB RAM, no HTTPS, single-core
                     │
                     ├──> ADR-004 (Static Buffers) ──> Referenced by 8 ADRs
                     ├──> ADR-007 (Timers) ──────────> Referenced by 6 ADRs
                     └──> ADR-013 (Arduino) ─────────> Foundation for all
```

**Most Referenced ADRs:**
- **ADR-004:** Static Buffer Allocation (8 references)
- **ADR-001:** ESP8266 Platform (7 references)
- **ADR-007:** Timer-Based Scheduling (6 references)
- **ADR-008:** LittleFS Persistence (5 references)

**Decision Timeline** (earliest to latest):
1. 2016: ADR-001 (ESP8266), ADR-013 (Arduino)
2. 2018: ADR-002 (Modular), ADR-003 (HTTP-only), ADR-007 (Timers)
3. 2019: ADR-005 (WebSocket), ADR-012 (PIC upgrade), ADR-020 (Sensors)
4. 2020: ADR-004 (Static buffers), ADR-008 (LittleFS migration)
5. 2021: ADR-015 (NTP + AceTime - verified: commit 45b51f2)
6. 2024: ADR-019 (API v2)
7. 2026: ADR-025 (Safari WebSocket fix), ADR-026 (Cache-busting), ADR-027 (Version warnings)

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

## Implementation Notes

**Memory Measurements:**
The claimed memory savings in ADR-004 (3,130-3,730 bytes or 7.8-9.3% of RAM) are estimates based on:
- Static buffer conversions: ~1,500 bytes
- PROGMEM strings: ~2,000 bytes (see ADR-009)
- Optimized libraries: ~400-500 bytes

To verify these measurements:
```bash
# Build and check binary size
python build.py --firmware
size build/OTGW-firmware.elf

# Monitor heap at runtime via telnet (port 23)
> s  # Show status including free heap
```

**Heap Levels (Standardized):**
Throughout the codebase, use these constant names:
- `HEAP_CRITICAL` - Less than 3KB (emergency mode)
- `HEAP_WARNING` - 3-5KB (throttle aggressively)
- `HEAP_LOW` - 5-8KB (reduce message rates)
- Normal operation: Greater than 8KB

**Version Numbering:**
- Release versions: `v1.0.0`, `v1.0.1`, `v2.0.0`
- Release candidates: `v1.0.0-rc1`, `v1.0.0-rc4`
- Development builds: `v1.0.0-dev+gitSHA`

Refer to specific RC numbers when documenting pre-release features.

## Superseding ADRs

When an architectural decision changes:
1. Do NOT modify the original ADR
2. Create a new ADR that supersedes the old one
3. Update the old ADR's status to "Superseded by ADR-XXX"
4. Reference the original ADR in the new one

## ADR Skill

**NEW:** This repository includes a GitHub Copilot skill for ADR management!

- **Location:** `.github/skills/adr/SKILL.md`
- **Purpose:** Automated ADR creation, compliance checking, and enforcement
- **Usage Guide:** `.github/skills/adr/USAGE_GUIDE.md`

The ADR skill helps you:
- Create well-structured ADRs using best practices
- Check code changes against existing ADRs
- Document architectural decisions with proper alternatives
- Maintain ADR compliance in PRs and CI/CD

**To use the skill:**
```
Ask Copilot: "Use the ADR skill to create ADR-XXX for [decision]"
Ask Copilot: "Check my changes against existing ADRs"
Ask Copilot: "Does this require a new ADR?"
```

See `.github/skills/adr/USAGE_GUIDE.md` for comprehensive usage instructions and CI/CD integration examples.

## Resources

- **ADR Skill (Copilot):** `.github/skills/adr/SKILL.md` 🆕
- **ADR Skill Usage Guide:** `.github/skills/adr/USAGE_GUIDE.md` 🆕
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
