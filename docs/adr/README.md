# Architecture Decision Records (ADRs)

This directory contains Architecture Decision Records (ADRs) that document significant architectural choices made in the OTGW-firmware project.

## What are ADRs?

Architecture Decision Records capture important architectural decisions along with their context, alternatives considered, and consequences. They serve as historical documentation to help current and future developers understand why the system is built the way it is.

## Quick Navigation

**By Topic:**

- [Platform & Build System](#platform-and-build-system) (2 ADRs)
- [Network & Security](#network-and-security) (5 ADRs)
- [Memory Management](#memory-management) (6 ADRs)
- [Integration & Communication](#integration-and-communication) (8 ADRs)
- [System Architecture](#system-architecture) (10 ADRs)
- [Hardware & Reliability](#hardware-and-reliability) (2 ADRs)
- [Development & Build](#development-and-build) (3 ADRs)
- [Core Services](#core-services) (6 ADRs)
- [Features & Extensions](#features-and-extensions) (9 ADRs)
- [Browser & Client](#browser-and-client-compatibility) (4 ADRs)
- [OTA & Updates](#ota-and-firmware-updates) (2 ADRs)

**Foundational ADRs** (most referenced by other ADRs):

- **ADR-001:** ESP8266 Platform Selection (establishes hardware constraints)
- **ADR-004:** Static Buffer Allocation (referenced by 8 other ADRs)
- **ADR-007:** Timer-Based Task Scheduling (enables non-blocking architecture)

## ADR Index

> **Numbering note:** ADR-063 was inadvertently skipped during the 2026-04-20 batch that added ADR-062 and ADR-064 (same commit). The number is unused and reserved as a no-op placeholder; do not retroactively assign it. Next available ADR numbers continue from the current highest. The Quick Navigation counts above predate ADR-061+ and do not yet reflect the full current index.

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
  Baseline local-network trust model for OTGW interfaces; partially superseded by ADR-056 for protected admin endpoints and secret-handling behavior.

- **[ADR-054: Optional HTTP Basic Authentication for Settings](ADR-054-optional-http-basic-auth.md)** *(Superseded by ADR-056)*  
  Historical introduction of opt-in Basic Auth for settings/admin operations before the broader protected-boundary and secret-handling contract was documented in ADR-056.

- **[ADR-056: Protected Admin Endpoint Security and Secret-Handling Contract](ADR-056-protected-admin-endpoint-security-and-secret-handling-contract.md)** 🆕  
  Defines the protected admin boundary, same-origin enforcement, password round-trip contract, OTA credential propagation, and local-network HTTP-only constraints.

### Memory Management

- **[ADR-004: Static Buffer Allocation Strategy](ADR-004-static-buffer-allocation.md)** *(Superseded by ADR-053)*  
  How static buffer allocation prevents heap fragmentation and crashes on the memory-constrained ESP8266.

- **[ADR-053: Large Feature Buffer Static Allocation](ADR-053-large-feature-buffer-static-allocation.md)** 🆕  
  Extends ADR-004: large feature-specific working buffers must be declared as file-static globals (`static T buf;`), never heap-allocated with `new` — even for "allocate-once, never-free" patterns. Canonical example: MQTT Home Assistant auto-discovery uses the shared static `sLine` buffer for payloads and `cMsg` as the topic buffer in `MQTTstuff.ino`.

- **[ADR-009: PROGMEM Usage for String Literals](ADR-009-progmem-string-literals.md)**  
  Mandatory use of PROGMEM (F() and PSTR() macros) to move string literals from RAM to flash memory.

- **[ADR-028: File Streaming Over Loading for Memory Safety](ADR-028-file-streaming-over-loading.md)**  
  Never load files >2KB into RAM; use streaming patterns to prevent memory exhaustion crashes.

- **[ADR-030: Heap Memory Monitoring and Emergency Recovery](ADR-030-heap-memory-monitoring-emergency-recovery.md)** 🆕
  Proactive heap monitoring with 4-level health system and adaptive throttling to prevent crashes (CRITICAL <3KB, WARNING 3-5KB, LOW 5-8KB, HEALTHY >8KB).

- **[ADR-044: Global State — extern Declaration in Header, Definition in .ino](ADR-044-global-state-header-definition-pattern.md)** 🆕
  `extern` declarations in headers + single definition in owning `.ino` to avoid ODR violations in any multi-TU build; applies to `msglastupdated[]`, `mqttlastsent[]`, `mqttPublishAllowed`, etc.

### Integration and Communication

- **[ADR-005: WebSocket for Real-Time Streaming](ADR-005-websocket-real-time-streaming.md)**  
  Using WebSocket protocol on port 81 for real-time OpenTherm message streaming to web browsers.

- **[ADR-006: MQTT Integration Pattern](ADR-006-mqtt-integration-pattern.md)**  
  MQTT client implementation with Home Assistant Auto-Discovery for zero-configuration integration.

- **[ADR-052: MQTT Publish Eligibility and Reconnect Refresh Contract](ADR-052-mqtt-publish-eligibility-contract.md)** 🆕
  Precise contract for first-seen, value-change, stale-refresh, and reconnect-reset behavior for normal MQTT topics plus combined and per-bit `msgid 0` status topics.

- **[ADR-031: Two-Microcontroller Coordination Architecture](ADR-031-two-microcontroller-coordination-architecture.md)** 🆕  
  Master/Slave architecture with ESP8266 as network controller and PIC microcontroller for OpenTherm protocol (serial communication, GPIO reset control, firmware upgrade capability).

- **[ADR-037: Gateway Mode Detection via PR=M Polling](ADR-037-gateway-mode-detection.md)** 🆕  
  Periodic polling (PR=M command, 30s interval with 60s cache) to detect gateway vs. monitor mode, with PS=1 impact on time sync suppression.

- **[ADR-040: MQTT Source-Specific Topics for OpenTherm Values](ADR-040-mqtt-source-specific-topics.md)** 🆕
  Additive source-specific MQTT and HA discovery topics using nested `<metric>/<source>` paths with opt-in enablement (`MQTTseparatesources`) and backward-compatible base topics.

- **[ADR-041: Just-In-Time Home Assistant MQTT Discovery](ADR-041-jit-ha-discovery.md)** *(Superseded by ADR-073)*
  Established the JIT discovery principle: OT MsgID discovery configs publish only when that MsgID is received on the bus, not as a bulk sweep at boot. Implementation gaps documented in ADR-041 are resolved by ADR-073.

- **[ADR-055: Webhook Outbound HTTP Integration](ADR-055-webhook-outbound-http-integration.md)** *(Superseded by ADR-057)*
  Historical record of introducing local-network outbound webhook support before retry, protected test-endpoint, and delivery policy were consolidated in ADR-057.

- **[ADR-057: Webhook Delivery, Retry, and Protected Test Endpoint Policy](ADR-057-webhook-delivery-retry-and-protected-test-endpoint-policy.md)** 🆕
  Defines edge-triggered outbound webhook delivery, bounded timeout and retry behavior, local-only URL policy, and the protected webhook test endpoint; builds on ADR-048's non-blocking state machine.

- **[ADR-065: otgw-pic/ MQTT Subtree as Stable Public Topic API](ADR-065-otgw-pic-mqtt-subtree.md)** *(Accepted)*
  Declares the `otgw-pic/` MQTT subtree a stable public API surface; renames, splits, or deprecations require a coordinated migration strategy. Introduces `kPicSubtreePrefix` as the single source of truth and fixes HA discovery `stat_t` mismatch that kept `boiler_connected` and `thermostat_connected` permanently unavailable.

- **[ADR-066: MQTT Publish Gating by Source and Per-MsgID Slave-Echo Classification](ADR-066-mqtt-publish-gating-by-source-and-slave-echo.md)** *(Proposed; refined by ADR-069)*
  Constrains the base topic to thermostat-side intent (Read-Ack and Write-Data only) and introduces per-MsgID `bSlaveEchoesValue` metadata to gate `/boiler` subtopic publications so that non-echo Write-Ack zeroes no longer flap the base or source topics.

- **[ADR-067: HA Discovery State Reconciliation on OTA Upgrade](ADR-067-ha-discovery-state-reconciliation-on-ota-upgrade.md)** *(Deprecated)*
  Automatic wipe of retained HA discovery topics on boot after a firmware upgrade was implemented and tested but proved too fragile on ESP8266 resource constraints; feature was withdrawn from implementation and users directed to manual cleanup via MQTT Explorer.

- **[ADR-068: bSeparateSources Makes Base and Source-Variant Entities Mutually Exclusive](ADR-068-bseparatesources-mutually-exclusive-base-and-source-variants.md)** *(Superseded by ADR-070)*
  Declared that enabling `bSeparateSources` suppresses the redundant base entity for source-templated MsgIDs, eliminating duplicate HA entity names; superseded by ADR-070 which also fixes the topic shape.

- **[ADR-069: MQTT Source-Subtopic Worldview Semantics](ADR-069-mqtt-source-topic-worldview-semantics.md)** *(Accepted)*
  Redefines per-source subtopic semantics from source-of-publication to worldview model: `/thermostat` shows what the thermostat side sees, `/boiler` shows what the boiler side receives, eliminating the `/gateway` third subtopic and fixing data-loss for write-only metrics during gateway override.

- **[ADR-070: MQTT Source-Topic Sibling-Suffix Shape](ADR-070-mqtt-source-topic-sibling-suffix-shape.md)** *(Superseded by ADR-071)*
  Replaced the nested `<metric>/thermostat` child-topic shape with sibling-suffix `<metric>_thermostat` for state topics; the discovery-topic carve-out in this ADR was subsequently found incorrect and corrected by ADR-071.

- **[ADR-071: MQTT Discovery Topic Sibling-Suffix Shape](ADR-071-mqtt-discovery-topic-sibling-suffix-shape.md)** *(Accepted)*
  Extends ADR-070 to apply the sibling-suffix shape to HA discovery topics (`<id>_thermostat/config`) after empirical investigation showed that HA's `TOPIC_MATCHER` regex rejects nested `object_id` segments; both state and discovery topics now use the flat sibling-suffix convention.

- **[ADR-072: HA Discovery Friendly-Name Format](ADR-072-ha-discovery-friendly-name-format.md)** *(Accepted)*
  Mandates a uniform `writeFriendlyName()` transform for all HA discovery `name` fields: underscores become spaces, each word is title-cased, and the hostname prefix is stripped; eliminates the confusing `OTGW_SomeCamelCase` labels that field testers flagged across multiple beta releases.

- **[ADR-073: JIT HA Discovery with Smart Reconnect](ADR-073-jit-ha-discovery-smart-reconnect.md)** *(Accepted)*
  Supersedes ADR-041. Removes the bulk-publish sweep from all automatic triggers, introduces a `publishNonOTDiscoveryConfigs()` set for non-OT pseudo-IDs, and adds a 5-minute offline heuristic to reset discovery bitmaps on probable broker restart without requiring the unavailable CONNACK `sessionPresent` flag.

- **[ADR-074: HA Entity Availability Reflects the MQTT Link, Not OpenTherm-Bus Liveness](ADR-074-ha-availability-reflects-mqtt-link-not-ot-bus.md)** *(Accepted)*
  HA entity availability is owned exclusively by the MQTT LWT/birth pair on `<toptopic>/<hostname>` — OT-bus liveness does not write to that topic. Eliminates the DHW Control / Thermostat unavailable-flap field testers reported during quiet OT-bus periods.

- **[ADR-075: MQTT Source-Topic Worldview Routing — Proxy-Answer Refinement](ADR-075-mqtt-source-topic-proxy-answer-routing.md)** *(Accepted)*
  Supersedes ADR-069. Adds `bAnswerOverride` discriminator so proxy-A (no preceding B) frames publish to `_thermostat`, `_boiler` AND canonical for proxy-answered IDs (e.g. MaxTSet/57), while genuine answer-override frames still publish to `_thermostat` only (ADR-069 invariant preserved). Fixes data-starvation on MQTT-Explorer-flagged read IDs.

- **[ADR-076: MQTT Status Fan-Out — Drop Global Rate-Gate, Keep Per-Slot Heartbeat](ADR-076-mqtt-status-fanout-drop-global-rate-gate.md)** *(Accepted)*
  Removes the 250 ms global `MQTT_GATED_PUBLISH_SPACING_MS` rate-gate that was starving first-seen ASF/RBP/VH bit/byte fan-out. Replaces it with per-slot heartbeat + commit-or-discard pending-slot bookkeeping; throughput becomes bounded by per-slot age rather than a global cooldown.

- **[ADR-077: Publish HA-Core-Style Capability-Flag Aliases on dev](ADR-077-mqtt-ha-core-capability-flag-aliases.md)** *(Superseded by ADR-078)*
  Proposed publishing HA-core-style self-describing aliases for the MsgID 2/3/6 capability/state/type/fault bits under a `bPublishHaCoreAliases` setting. Withdrawn from the dev (1.5.x) line by ADR-078 to avoid shipping the alias schema before its 2.0.0 successor has stabilised.

- **[ADR-078: Defer HA-Core Capability-Flag Aliases — Ship on 2.0.0 Only](ADR-078-defer-ha-core-aliases-to-2-0-0-revert-from-dev.md)** *(Accepted)*
  Reverts ADR-077 from the dev branch and reserves HA-core capability-flag aliases for the 2.0.0 line. Includes a forbid-pattern Enforcement block that prevents re-introduction on dev (the `bPublishHaCoreAliases` symbol must not reappear under `src/OTGW-firmware/`).

- **[ADR-085: Unified Off/Heat/Cool HA Climate Entity Derived from OpenTherm Status Bits](ADR-085-ha-climate-heat-cool-from-ot-status-bits.md)** *(Accepted)*
  Replaces the heating-only HA Thermostat entity with a unified off/heat/cool climate entity driven by two firmware-computed topics (`<pub>/hvac_mode`, `<pub>/hvac_action`) derived from the MsgID 0 status bits. Mode is reflective (no `mode_command_topic`; the thermostat owns heat/cool switching); heating reads the central-heating slave bit, not flame, so DHW does not show as heating. Fixes GH #665 (cooling invisible on a heatpump). Mirror of 2.0.0 ADR-156.

### System Architecture

- **[ADR-007: Timer-Based Task Scheduling](ADR-007-timer-based-task-scheduling.md)**  
  Non-blocking timer-based task scheduling with 49-day rollover protection for cooperative multitasking.

- **[ADR-008: LittleFS for Configuration Persistence](ADR-008-littlefs-configuration-persistence.md)**  
  Using LittleFS filesystem with JSON files for configuration storage that survives firmware updates.

- **[ADR-036: Boot Sequence Initialization Ordering](ADR-036-boot-sequence-ordering.md)** 🆕  
  Deterministic 5-phase boot sequence (Hardware → Filesystem → Network → Application → OTGW) with critical dependency ordering (NTP before WiFi DHCP, webserver before MQTT).

- **[ADR-038: OpenTherm Message Data Flow Pipeline](ADR-038-opentherm-data-flow-pipeline.md)** 🆕  
  Synchronous fan-out architecture for OpenTherm messages (PIC Serial → processOT → MQTT + WebSocket + REST + Telnet) with per-consumer availability checks and bidirectional command flow.

- **[ADR-045: PS=1 Print Summary Parsing](ADR-045-ps1-print-summary-parsing.md)** *(Superseded by ADR-046)*  
  Historical record of the original PS=1 synthetic-frame design.

- **[ADR-046: PS=1 Summary Translation with Shared Publish Helpers](ADR-046-ps1-summary-translation-shared-publish-helpers.md)** 🆕
  PS=1 uses a dedicated summary-translation path with strict parsing, centralized PS-mode helpers, and selective reuse of shared publish/state helpers.

- **[ADR-047: Non-Blocking WiFi Reconnect State Machine](ADR-047-nonblocking-wifi-reconnect.md)** 🆕
  Cooperative reconnect state machine that retries without blocking the main loop and reboots after repeated failure.

- **[ADR-048: Non-Blocking Webhook State Machine with Retry](ADR-048-nonblocking-webhook-state-machine.md)** 🆕
  Webhook delivery runs as a non-blocking state machine with bounded retry behavior to avoid stalling loop processing.

- **[ADR-050: Centralized API Route Dispatch Table](ADR-050-centralized-api-route-dispatch.md)** 🆕
  `/api/v2` routing uses a dispatch table instead of a long conditional chain to keep endpoint registration centralized and maintainable.

- **[ADR-051: Dual Encapsulating Structs (Settings + State)](ADR-051-dual-encapsulating-structs.md)** 🆕
  Persistent configuration and runtime state are grouped into dedicated top-level structs to replace sprawling flat globals.

### Hardware and Reliability

- **[ADR-011: External Hardware Watchdog for Reliability](ADR-011-external-hardware-watchdog.md)**  
  I2C hardware watchdog chip that automatically resets the ESP8266 if firmware hangs or crashes.

- **[ADR-012: PIC Firmware Upgrade via Web UI](ADR-012-pic-firmware-upgrade-via-web.md)**
  Safe PIC microcontroller firmware flashing through the Web UI with WebSocket progress streaming.

- **[ADR-060: PIC Availability Guard Pattern](ADR-060-pic-availability-guard-pattern.md)** 🆕
  Central `isPICEnabled()` guard that disables all PIC-dependent code paths when no PIC is detected, with auto-recovery via banner detection. Enables single firmware binary for both PIC and non-PIC hardware variants.

### Development and Build

- **[ADR-013: Arduino Framework Over ESP-IDF](ADR-013-arduino-framework-over-esp-idf.md)**  
  Using Arduino framework for rapid development and rich ecosystem instead of low-level ESP-IDF.

- **[ADR-049: String Class Prohibition in Protocol Paths](ADR-049-string-prohibition-protocol-paths.md)** 🆕
  Protocol hot paths use bounded char buffers instead of `String` to reduce heap fragmentation and peak RAM usage on ESP8266.

- **[ADR-014: Dual Build System (Makefile + Python Script)](ADR-014-dual-build-system.md)**  
  Makefile for CI/CD and build.py wrapper for cross-platform developer convenience.

### Core Services

- **[ADR-015: NTP and AceTime for Time Management](ADR-015-ntp-acetime-time-management.md)**  
  Network time synchronization with AceTime library for comprehensive timezone and DST support.

- **[ADR-016: OpenTherm Command Queue with Deduplication](ADR-016-opentherm-command-queue.md)**  
  Command queuing system to prevent serial buffer overruns and eliminate duplicate commands.

- **[ADR-017: WiFiManager for Initial Configuration](ADR-017-wifimanager-initial-configuration.md)**  
  Captive portal for easy first-time WiFi setup without hardcoded credentials.

- **[ADR-018: ArduinoJson for Data Interchange](ADR-018-arduinojson-data-interchange.md)** *(Superseded by ADR-042)*  
  ~~Standardized JSON handling for settings persistence, REST API, MQTT, and WebSocket communication.~~

- **[ADR-042: Streaming JSON I/O — No ArduinoJson](ADR-042-streaming-json-no-arduinojson.md)** 🆕
  Mandate streaming JSON helpers with global scratch buffers instead of ArduinoJson; eliminates ArduinoJson heap documents, avoids ArduinoJson-driven fragmentation, and fixes the settings-reset bug from buffer overflow.

- **[ADR-043: Reset-Pattern WiFi Recovery Trigger](ADR-043-reset-pattern-wifi-recovery.md)** 🆕
  Triple-reset within a 10-second window forces WiFiManager configuration portal and clears saved WiFi credentials for deterministic recovery on ESP8266.

### Features and Extensions

- **[ADR-019: REST API Versioning Strategy](ADR-019-rest-api-versioning-strategy.md)**  
  URL path-based API versioning (v0/v1/v2) with indefinite backward compatibility.

- **[ADR-035: RESTful API Compliance Strategy](ADR-035-restful-api-compliance-strategy.md)** 🆕  
  Expand v2 API with RESTful-compliant endpoints: consistent JSON errors, proper status codes, resource naming, and CORS headers.

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

- **[ADR-039: Real-Time OTGraph Charting Architecture](ADR-039-otgraph-real-time-charting.md)** 🆕  
  5-grid ECharts-based charting module with dynamic Dallas sensor registration, dual-theme palettes, LTTB sampling, and 24h data buffer for real-time OpenTherm monitoring.

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

`docs/adr/README.md` is the canonical ADR guide for this repository. Other instruction files should link here instead of restating ADR templates or lifecycle rules.

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

```text
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
8. 2026: ADR-036 (Boot sequence), ADR-037 (Gateway mode), ADR-038 (Data flow), ADR-039 (OTGraph)
9. 2026: ADR-040 (MQTT source topics), ADR-041 (JIT HA discovery, superseded by ADR-073), ADR-042 (No ArduinoJson), ADR-043 (Triple-reset WiFi)
10. 2026: ADR-044 (Global state header definition), ADR-045 (PS=1 summary parsing)
11. 2026: ADR-054 (Optional HTTP Basic Auth), ADR-055 (Webhook HTTP integration)
12. 2026: ADR-056 (Protected admin security contract), ADR-057 (Webhook delivery + test endpoint policy)
13. 2026: ADR-060 (PIC availability guard pattern)
14. 2026: ADR-065 (otgw-pic/ MQTT subtree stable API), ADR-066 (publish gating by source and slave-echo, Proposed)
15. 2026: ADR-067 (HA discovery OTA reconciliation, Deprecated), ADR-068 (bSeparateSources mutually exclusive, Superseded by ADR-070)
16. 2026: ADR-069 (MQTT source-subtopic worldview semantics), ADR-070 (sibling-suffix state topic, Superseded by ADR-071)
17. 2026: ADR-071 (MQTT discovery sibling-suffix shape), ADR-072 (HA discovery friendly-name format), ADR-073 (JIT HA discovery smart reconnect, supersedes ADR-041)
18. 2026: ADR-074 (HA availability reflects MQTT link, not OT bus), ADR-075 (MQTT source-topic proxy-answer routing, supersedes ADR-069)
19. 2026: ADR-076 (drop global status fan-out rate-gate), ADR-077 (HA-core capability-flag aliases, Superseded by ADR-078), ADR-078 (defer HA-core aliases to 2.0.0, supersedes ADR-077)

## When to Create an ADR

Create an ADR when making a decision that:

- Changes architecture, service/module boundaries, deployment topology, or integration patterns
- Changes non-functional requirements such as security, availability, performance, privacy/compliance, or resilience
- Changes external interfaces or contracts, including API behavior and breaking changes
- Introduces or replaces frameworks, libraries, tooling, or build/CI patterns with broad architectural impact
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
- Small dependency or tooling updates without architectural impact
- Minor feature additions within existing patterns

## ADR Workflow

- **Before implementing:** Read the relevant ADRs to align with existing decisions.
- **During planning:** Create or update an ADR when a change materially alters architecture, protocols, data flow, or external behavior.
- **After implementation:** Update the ADR status as needed and link the ADR from the PR or review description.

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

1. Accepted ADRs are immutable; do NOT modify the original ADR beyond the status line needed to record supersession
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

```text
Ask Copilot: "Use the ADR skill to create ADR-XXX for [decision]"
Ask Copilot: "Check my changes against existing ADRs"
Ask Copilot: "Does this require a new ADR?"
```

See `.github/skills/adr/USAGE_GUIDE.md` for comprehensive usage instructions and CI/CD integration examples.

## Resources

- **ADR Skill (Copilot):** `.github/skills/adr/SKILL.md` 🆕
- **ADR Skill Usage Guide:** `.github/skills/adr/USAGE_GUIDE.md` 🆕
- **ADR Best Practices:** <https://adr.github.io/>
- **Michael Nygard's ADR Template:** <https://github.com/joelparkerhenderson/architecture-decision-record>
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
