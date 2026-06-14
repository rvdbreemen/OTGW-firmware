# Architecture Decision Records (ADRs)

This directory contains Architecture Decision Records (ADRs) that document significant architectural choices made in the OTGW-firmware project.

## What are ADRs?

Architecture Decision Records capture important architectural decisions along with their context, alternatives considered, and consequences. They serve as historical documentation to help current and future developers understand why the system is built the way it is.

## Quick Navigation

**By Topic:**

- [Platform & Build System](#platform-and-build-system) (4 ADRs)
- [Network & Security](#network-and-security) (6 ADRs)
- [Memory Management](#memory-management) (8 ADRs)
- [Integration & Communication](#integration-and-communication) (9 ADRs, plus MQTT subsection of 8)
- [System Architecture](#system-architecture) (11 ADRs)
- [Hardware & Reliability](#hardware-and-reliability) (4 ADRs)
- [Development & Build](#development-and-build) (6 ADRs)
- [Core Services](#core-services) (6 ADRs)
- [Features & Extensions](#features-and-extensions) (10 ADRs)
- [Browser & Client](#browser-and-client-compatibility) (4 ADRs)
- [OTA & Updates](#ota-and-firmware-updates) (2 ADRs)
- [OTGW32 & Dual Platform](#otgw32-and-dual-platform) (5 ADRs)
- [SAT Subsystem](#sat-subsystem) (6 ADRs)
- [ADR Governance](#adr-governance) (1 ADR)

Counts above are advisory rather than hand-maintained; the canonical set is the per-section listing below.

**Foundational ADRs** (most referenced by other ADRs):

- **ADR-001:** ESP8266 Platform Selection (establishes hardware constraints)
- **ADR-004:** Static Buffer Allocation (referenced by 8 other ADRs; later extended by ADR-053)
- **ADR-007:** Timer-Based Task Scheduling (enables non-blocking architecture)
- **ADR-051:** Dual Encapsulating Structs for Settings + State (later amended by ADR-079, ADR-081)
- **ADR-080:** Binding ADR rules must have a CI gate (governance backbone for pattern-level ADRs)

## ADR Index

### Platform and Build System

- **[ADR-001: ESP8266 Platform Selection](ADR-001-esp8266-platform-selection.md)**  
  Why we chose ESP8266 over ESP32, Raspberry Pi, and other alternatives for the network controller platform.

- **[ADR-002: Modular .ino File Architecture](ADR-002-modular-ino-architecture.md)**  
  How the codebase is organized into multiple `.ino` files by functional domain while maintaining Arduino compatibility.

- **[ADR-082: ESP8266 Arduino Core 2.7.4 LTS pin (2.0.0 line)](ADR-082-esp8266-arduino-core-2.7.4-lts-pin.md)** 🆕  
  Pin Arduino Core to 2.7.4 for the 2.0.0 ESP8266 line; later Cores broke field stability around reboot reliability and PROGMEM alignment.

- **[ADR-083: PlatformIO as primary build system for dual-target firmware](ADR-083-platformio-as-primary-build-system.md)** 🆕  
  Supersedes ADR-014. PlatformIO `platformio.ini` is the primary build path for both ESP8266 and ESP32 targets in the 2.0.0 line; arduino-cli remains as a legacy fallback in `build.py`.

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

- **[ADR-075: WiFi Reconnect Timeout Tuning](ADR-075-wifi-reconnect-timeout-tuning.md)** 🆕  
  Reconnect timeout restored to 30s after the v1.3.x regression (5s) caused repeated cancelled associations on ESP8266; aligns with the v1.2.0 baseline.

### Memory Management

- **[ADR-004: Static Buffer Allocation Strategy](ADR-004-static-buffer-allocation.md)** *(Extended by ADR-053)*  
  How static buffer allocation prevents heap fragmentation and crashes on the memory-constrained ESP8266.

- **[ADR-053: Large Feature Buffer Static Allocation](ADR-053-large-feature-buffer-static-allocation.md)** 🆕  
  Extends ADR-004: large feature-specific working buffers must be declared as file-static globals (`static T buf;`), never heap-allocated with `new` — even for "allocate-once, never-free" patterns. Canonical example: MQTT Home Assistant auto-discovery uses the shared static `sLine` buffer for payloads and `cMsg` as the topic buffer in `MQTTstuff.ino`.

- **[ADR-009: PROGMEM Usage for String Literals](ADR-009-progmem-string-literals.md)**  
  Mandatory use of PROGMEM (F() and PSTR() macros) to move string literals from RAM to flash memory.

- **[ADR-028: File Streaming Over Loading for Memory Safety](ADR-028-file-streaming-over-loading.md)**  
  Never load files >2KB into RAM; use streaming patterns to prevent memory exhaustion crashes.

- **[ADR-030: Heap Memory Monitoring and Emergency Recovery](ADR-030-heap-memory-monitoring-emergency-recovery.md)** *(Amended by ADR-089)*  
  Proactive heap monitoring with 4-level health system and adaptive throttling to prevent crashes.

- **[ADR-044: Global State — extern Declaration in Header, Definition in .ino](ADR-044-global-state-header-definition-pattern.md)** 🆕  
  `extern` declarations in headers + single definition in owning `.ino` to avoid ODR violations in any multi-TU build; applies to `msglastupdated[]`, `mqttlastsent[]`, `mqttPublishAllowed`, etc.

- **[ADR-089: Heap Tier Machine Contract](ADR-089-heap-tier-machine-contract.md)** 🆕  
  Amends ADR-030: re-baselines tier thresholds to 1536/3072/5120 bytes (Crashevans field log evidence, TASK-344), adds fragmentation-aware promotion (`HEAP_FRAG_PROMOTE_MAXBLOCK` = 1536), adds tier-entry counters (`iEnteredLowCount`/`Warning`/`Critical`, TASK-346) with hourly retained MQTT publication. Three sub-rules CI-gated, two explicitly labelled guideline-level. Defense-in-depth peer with ADR-088.

- **[ADR-090: Re-entrancy Guard Pattern for Shared Scratch Buffers](ADR-090-re-entrancy-guard-pattern-shared-scratch-buffers.md)** 🆕  
  Guideline-level ADR (per ADR-080: 2 instances in 1 file is below the recurrence bar for binding pattern-level enforcement; no CI gate). Documents the acquisition contract for file-scope or function-local-static mutable scratch state shared across re-entrant call paths in the cooperative ESP8266 model. Two existing exemplars in `MQTTstuff.ino`: RAII `MQTTAutoConfigSessionLock` (preferred for new code) and the `publishToSourceTopic` function-local `inUse` flag.

### Integration and Communication

- **[ADR-005: WebSocket for Real-Time Streaming](ADR-005-websocket-real-time-streaming.md)**  
  Using WebSocket protocol on port 81 for real-time OpenTherm message streaming to web browsers.

- **[ADR-006: MQTT Integration Pattern](ADR-006-mqtt-integration-pattern.md)**  
  MQTT client implementation with Home Assistant Auto-Discovery for zero-configuration integration.

- **[ADR-031: Two-Microcontroller Coordination Architecture](ADR-031-two-microcontroller-coordination-architecture.md)** 🆕  
  Master/Slave architecture with ESP8266 as network controller and PIC microcontroller for OpenTherm protocol (serial communication, GPIO reset control, firmware upgrade capability).

- **[ADR-037: Gateway Mode Detection via PR=M Polling](ADR-037-gateway-mode-detection.md)** 🆕  
  Periodic polling (PR=M command, 30s interval with 60s cache) to detect gateway vs. monitor mode, with PS=1 impact on time sync suppression.

- **[ADR-055: Webhook Outbound HTTP Integration](ADR-055-webhook-outbound-http-integration.md)** *(Superseded by ADR-057)*  
  Historical record of introducing local-network outbound webhook support before retry, protected test-endpoint, and delivery policy were consolidated in ADR-057.

- **[ADR-057: Webhook Delivery, Retry, and Protected Test Endpoint Policy](ADR-057-webhook-delivery-retry-and-protected-test-endpoint-policy.md)** 🆕  
  Defines edge-triggered outbound webhook delivery, bounded timeout and retry behavior, local-only URL policy, and the protected webhook test endpoint; builds on ADR-048's non-blocking state machine.

- **[ADR-058: Non-blocking PIC Command/Response for PR= Queries](ADR-058-nonblocking-pic-command-response.md)** 🆕  
  Decouple PR= settings reads from the HTTP request handler so blocking PIC reads do not stall the webserver; queue the command, parse the response asynchronously.

- **[ADR-059: Ser2net Queue Awareness and Serial Bus Coordination](ADR-059-ser2net-queue-awareness.md)** 🆕  
  Detect ser2net traffic on port 25238 and pause the OTGW command queue for 2 seconds to avoid colliding writes; remove conflicting queue entries automatically.

- **[ADR-066: Thermostat Auto-Detection and Master Mode](ADR-066-thermostat-auto-detection-master-mode.md)** 🆕  
  Detect the absence of a thermostat at boot and switch to master/standalone MQTT mode so the gateway publishes useful data even without an upstream thermostat.

#### MQTT

- **[ADR-040: MQTT Source-Specific Topics for OpenTherm Values](ADR-040-mqtt-source-specific-topics.md)** 🆕  
  Additive source-specific MQTT and HA discovery topics using nested `<metric>/<source>` paths with opt-in enablement (`MQTTseparatesources`) and backward-compatible base topics.

- **[ADR-052: MQTT Publish Eligibility and Reconnect Refresh Contract](ADR-052-mqtt-publish-eligibility-contract.md)** 🆕  
  Precise contract for first-seen, value-change, stale-refresh, and reconnect-reset behavior for normal MQTT topics plus combined and per-bit `msgid 0` status topics.

- **[ADR-062: Retained Discovery Verification via Wildcard Subscribe](ADR-062-retained-discovery-verification.md)** 🆕  
  Daily wildcard subscribe verifies retained HA discovery state on the broker, re-announces missing configs through the drip publisher, and tears down cleanly. RAM-tuned subscribe buffer.

- **[ADR-065: otgw-pic/ MQTT Subtree as Stable Public Topic API](ADR-065-otgw-pic-mqtt-subtree.md)** 🆕  
  Stable public topic API for PIC-derived state under `otgw-pic/`; clients should subscribe to this prefix rather than transient `otgw/` paths. Later partially generalised by ADR-084.

- **[ADR-077: Streaming MQTT HA Discovery Architecture](ADR-077-streaming-mqtt-ha-discovery-architecture.md)** 🆕  
  Streaming, bitmap-driven drip publisher replaces the static `mqttha.cfg` template; 309 configs across 80+ msgIDs without the 1350-byte staging buffer.

- **[ADR-078: MQTT Sub-command Dispatch Tables](ADR-078-mqtt-subcommand-dispatch-tables.md)** 🆕  
  `<topic>/set/<command>` handlers route through small dispatch tables instead of long if-else chains; centralises command registration.

- **[ADR-084: Generic OT-bus State MQTT Topics (amends ADR-065)](ADR-084-generic-ot-bus-state-topics.md)** 🆕  
  Generalises `otgw-pic/` to platform-neutral OT-bus state topics so OT-Direct (PIC-less) builds emit the same shape; partially breaks 1.4.x and pre-2.0.0 MQTT API consumers.

- **[ADR-088: MQTT Status Burst Windowing and Post-Burst Cooldown](ADR-088-mqtt-status-burst-windowing-and-cooldown.md)** 🆕  
  Pattern-level contract that brackets Status-frame fanout with `beginStatusBurst()`/`endStatusBurst()` and defers the discovery drip during the burst plus a post-burst cooldown bounded below the Status cadence. Three sub-rules are CI-gated; the timeout self-heal is explicitly labelled guideline-level.

- **[ADR-093: Home Assistant Discovery Retained-Config Orphan Cleanup](ADR-093-ha-discovery-retained-config-orphan-cleanup.md)** 🆕  
  Guideline-level under ADR-080 (one adopter today: BLE roster). When the firmware terminates the lifecycle of a previously advertised HA entity, it publishes a zero-byte retained payload to each discovery config topic via the same streaming primitives as the publish path. Documents the symmetry contract for `satBLEUnpublishDiscovery` in `MQTTstuff.ino` and the `bDiscoveryPublished` gate in `satBLERosterForget`.

- **[ADR-124: HA Discovery Seven-Device Topology — Dedicated OT-Core and Sensors Devices, Gateway via_device Hub](ADR-124-ha-discovery-seven-device-topology.md)** 🆕  
  Proposed (Decision Maker: Robert van den Breemen, 2026-06-05). **Supersedes ADR-122.** Expands the HA discovery device set to seven — `{Boiler, Thermostat, Gateway, Esp, Pic, Sat, Sensors}` — giving the OpenTherm-core (PIC `HAS_PIC` / OTDirect `HAS_DIRECT_OT`, named per hardware) and the physical hardware sensors (Dallas + S0) their own devices, and adding a Gateway `via_device` hub. The `via_device` addition is a deliberate divergence from HA core 2024.10 (which sets none, the reason ADR-122 §8 omitted it). Carries forward ADR-122's `haDeviceForEntity()` routing, two-axis legacy model, and migration design. Binding under ADR-080: `check_ha_discovery_device_routing` + golden-file test updated to seven devices in TASK-826.

- **[ADR-122: HA Discovery Five-Device Topology](ADR-122-ha-discovery-five-device-topology.md)**  
  **Superseded by ADR-124** (2026-06-05). The original five-device split `{Boiler, Thermostat, Gateway, Esp, Sat}` (TASK-648): HA-core-authoritative OT entity routing via the single auditable `haDeviceForEntity()` PROGMEM map, two independent legacy axes (`bLegacyMode` topology/unique_id; `bUseLegacyOtTopics` topic naming), bilateral replication, and orphan-cleanup migration. Routing engine and legacy model carried forward by ADR-124; only the device set, OT-core/sensors placement, and `via_device` omission are revised.

- **[ADR-097: MQTT Publish Gating by Source and Per-MsgID Slave-Echo Classification](ADR-097-mqtt-publish-gating-by-source-and-slave-echo.md)** 🆕  
  Constrains the legacy base topic to Write-Data only (no Write-Ack) and gates `/boiler` subtopic publication per MsgID via `bSlaveEchoesValue` in the OTlookup table. Prevents base-topic flapping and fake-zero `/boiler` readings for MsgIDs where the OT v4.2 spec defines the Write-Ack data byte as undefined. Refined by ADR-096 (worldview semantics, superseded by ADR-103).

- **[ADR-119: MQTT Source-Topic Sibling-Suffix Shape (renumbered from ADR-097)](ADR-119-mqtt-source-topic-sibling-suffix-shape.md)**  
  Superseded by ADR-098. **Number disambiguation:** originally filed as ADR-097, colliding with the still-live publish-gating ADR-097 directly above. Renumbered to ADR-119 on 2026-06-02 — the publish-gating ADR keeps 097 (it has ~13 firmware source refs and was itself already renumbered once, ADR-066 → ADR-097). Older Accepted/Superseded ADRs (095/096/098/101/105) and docs (`docs/api/MQTT.md`, the manuals, Domoticz/openHAB guides) still cite this decision as "ADR-097"; where the reference concerns **source-topic / discovery sibling-suffix shape** it means this document, ADR-119. Those frozen bodies are not rewritten, per the ADR immutability rule.

- **[ADR-101: Flat Per-Value MQTT Topics Over Aggregated JSON Payloads](ADR-101-flat-per-value-mqtt-topics-over-aggregated-json-payloads.md)** 🆕  
  Binding decision (owner: Robert van den Breemen, 2026-05-08): OTGW-firmware publishes one plain scalar per topic; aggregated JSON blobs on value topics are forbidden. HA auto-discovery metadata travels separately on `homeassistant/#` config topics. OT-Thing nested-JSON and Tasmota SENSOR-JSON dialects are explicitly not supported. Enforced by `adr-judge` pre-commit hook.

- **[ADR-102: HA Entity Availability Reflects the MQTT Link, Not OpenTherm-Bus Liveness (2.0.0)](ADR-102-ha-availability-reflects-mqtt-link-not-ot-bus.md)** 🆕  
  2.0.0 sibling of dev ADR-074 (dev PR #583). HA entity availability (`avty_t` = base namespace topic) must reflect only the ESP↔MQTT link (birth/LWT); `publishOTGWConnectedState()` no longer overwrites it with OT-bus liveness, which flapped every HA entity (`DHW Control`/`Thermostat`/SAT cards most visibly). OT-bus liveness stays on the `otgw_connected` sensor. Binding pattern; CI gate pending under ADR-080 (TASK-623).

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

- **[ADR-051: Dual Encapsulating Structs (Settings + State)](ADR-051-dual-encapsulating-structs.md)** *(Amended by ADR-079, ADR-081)*  
  Persistent configuration and runtime state are grouped into dedicated top-level structs to replace sprawling flat globals.

- **[ADR-086: Time-Boundary Helpers MUST Have Exactly One Call Site](ADR-086-time-boundary-single-caller-contract.md)** 🆕  
  `minuteChanged()` / `hourChanged()` / `dayChanged()` / `yearChanged()` are consume-on-read; calling them from more than one site silently drops boundary events. Renumbered from earlier ADR-064 to resolve duplicate numbering.

- **[ADR-130: PIC-UART Dedicated FreeRTOS Task as Sole OTGWSerial Owner (ADR-123 Phase 1)](ADR-130-pic-uart-dedicated-task-sole-serial-owner.md)** 🆕  
  Proposed (2026-06-13). Lifts the PIC UART runtime byte I/O onto a dedicated FreeRTOS task pinned to the app core, the sole runtime owner of `OTGWSerial` read/write; deletes the four-lines-per-call bound (`kMaxLinesPerDrain`, TASK-671). RX assembles lines onto the ADR-129 frame queue; TX uses a new value-copy `otTxQueue`; RX errors are detected in the task but reported loop-side to keep `OTGWState` single-writer. Task parks during a PIC flash (ADR-012 handshake) and on an OTDirect boot (ADR-127), gating on `isOTDirectEnabled()` so ADR-060 banner recovery survives. Refines ADR-123 §model-1's "high-priority task" to equal-to-loop priority for the Phase-1 (still-synchronous-networking) environment. First consumer of ADR-129; supersedes nothing. New CI gate `check_pic_uart_task_owns_serial`.

- **[ADR-132: HTTP Stack on ESPAsyncWebServer with an Imperative-Push to Async-Pull Bridge (ADR-123 Phase 3)](ADR-132-async-web-server-and-imperative-push-bridge.md)** 🆕 *(Supersedes ADR-109)*  
  Proposed (2026-06-14). Moves the web stack off the cooperative `loop()` onto ESPAsyncWebServer (over AsyncTCP): one `AsyncWebServer server(80)` instantiated once (ADR-044) and exposed `extern` for WebSocket (seq10) and OTA (seq11); the four-deep `handleClient()` drain (TASK-817) and the `OTGWWebServer = WebServer` alias are deleted. Removes the sat-slider stall at the root (parallel sockets served on the AsyncTCP task, not one-per-loop-turn). New `webServerCompat.h` bridges the imperative-push JSON builders (`sendStartJsonMap` → N×`sendJsonMapEntry` → `sendEndJsonMap`) to the async pull/callback API via a file-static per-request context (safe under single-AsyncTCP-task serialization), send-exactly-once helpers, merged `argCompat()` semantics, and a body-capture hook. Bounded JSON streams via `AsyncResponseStream` (retiring ADR-109's `sTxBuf`); static files stream straight from LittleFS; the ~39 KB index streams chunked. ADR-056 auth/CSRF preserved verbatim. OTA is intentionally dark across the 865.9→865.11 seam. Supersedes ADR-109 (whose "do NOT migrate to AsyncWebServer" premise fell with ADR-128 dropping ESP8266). Proposed Phase-3 CI gate forbids `httpServer.`/`handleClient(`/`OTGWWebServer` in app files.

- **[ADR-133: WebSocket Live-Log on AsyncWebSocket at /ws on Port 80 (ADR-123 Phase 3)](ADR-133-websocket-live-log-on-asyncwebsocket-port-80.md)** 🆕 *(Supersedes ADR-005)*  
  Proposed (2026-06-14). The WebSocket half of ADR-123 Phase 3 (seq10): retires the dedicated port-81 `WebSocketsServer` (Links2004 / `WebSockets @ 2.3.6`) and streams the OpenTherm live-log over a single `AsyncWebSocket otLogWs("/ws")` attached to the shared port-80 `AsyncWebServer server` from ADR-132 (`server.addHandler(&otLogWs)`). No dedicated WebSocket port, no second TCP listener, no per-loop `webSocket.loop()` poll: the socket is serviced on the AsyncTCP task and `handleWebSocket()` becomes a 1 s timer doing only `cleanupClients()` plus the 30 s ADR-025 keepalive. `startWebSocket()` is now attach-once idempotent (guarded by `wsInitialized`) because it is called on every WiFi/Ethernet transition without tearing the server down. The parallel `wsClientCount` is deleted in favour of `otLogWs.count()`, and the connect cap flips to `count() > MAX_WEBSOCKET_CLIENTS` since AsyncWebSocket inserts the client before firing `WS_EVT_CONNECT`. All transmit paths route through `otLogWs.textAll()`; `sendLogToWebSocket()` keeps its `&& canSendWebSocket()` OT-hot-path heap gate verbatim (ADR-121 preserved). The Web UI swaps `WEBSOCKET_PORT = 81` / `ws://host:81/` for `WEBSOCKET_PATH = '/ws'` / `ws://host/ws` from `window.location.host`; ADR-025 Safari guard retained. Supersedes ADR-005 (dedicated port 81 + Links2004 server); the streaming concept survives, re-homed. Proposed Phase-3 CI gate forbids `WebSocketsServer`/`webSocket.loop(`/`broadcastTXT` in app files and `WEBSOCKET_PORT` in index.js.

### Hardware and Reliability

- **[ADR-011: External Hardware Watchdog for Reliability](ADR-011-external-hardware-watchdog.md)**  
  I2C hardware watchdog chip that automatically resets the ESP8266 if firmware hangs or crashes.

- **[ADR-012: PIC Firmware Upgrade via Web UI](ADR-012-pic-firmware-upgrade-via-web.md)**  
  Safe PIC microcontroller firmware flashing through the Web UI with WebSocket progress streaming.

- **[ADR-060: PIC Availability Guard Pattern](ADR-060-pic-availability-guard-pattern.md)** 🆕  
  Central `isPICEnabled()` guard that disables all PIC-dependent code paths when no PIC is detected, with auto-recovery via banner detection. Enables single firmware binary for both PIC and non-PIC hardware variants.

- **[ADR-061: Unified ESP8266/ESP32 Platform Abstraction](ADR-061-unified-esp8266-esp32-platform-abstraction.md)** 🆕  
  Single source codebase compiles for both ESP8266 (PIC-based OTGW) and ESP32 (OTGW32, OT-Direct, SAT) via `platform.h` / `platform_esp{8266,32}.h` shims; SDK differences encapsulated behind a thin abstraction.

### Development and Build

- **[ADR-013: Arduino Framework Over ESP-IDF](ADR-013-arduino-framework-over-esp-idf.md)**  
  Using Arduino framework for rapid development and rich ecosystem instead of low-level ESP-IDF.

- **[ADR-014: Dual Build System (Makefile + Python Script)](ADR-014-dual-build-system.md)** *(Superseded by ADR-083)*  
  Makefile for CI/CD and build.py wrapper for cross-platform developer convenience. Replaced by PlatformIO as primary in the 2.0.0 line.

- **[ADR-049: String Class Prohibition in Protocol Paths](ADR-049-string-prohibition-protocol-paths.md)** 🆕  
  Protocol hot paths use bounded char buffers instead of `String` to reduce heap fragmentation and peak RAM usage on ESP8266.

- **[ADR-074: ADR Audit — SAT Integration Phase](ADR-074-adr-audit-sat-integration-phase.md)** 🆕  
  One-time audit captured during the SAT integration phase to bring the ADR corpus in line with the SAT codebase; documents which decisions needed dedicated ADRs vs which were already covered.

- **[ADR-079: Per-component Type Headers (amendment to ADR-051)](ADR-079-per-component-state-settings-headers.md)** 🆕  
  One `<Component>types.h` file per subsystem bundles state + settings + enums; supersedes ADR-051's single-file struct layout (struct + Hungarian-prefix conventions retained).

- **[ADR-081: Types Merge into `<Component>stuff.h` When Both Headers Exist](ADR-081-types-merge-into-stuff-when-both-exist.md)** 🆕  
  Amendment to ADR-079: when a `<Component>stuff.h` already exists, types fold into it instead of creating a separate `<Component>types.h`. Prevents file-count bloat for components that already have a stuff sibling.

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

- **[ADR-035: RESTful API Compliance Strategy](ADR-035-restful-api-compliance-strategy.md)** 🆕  
  Expand v2 API with RESTful-compliant endpoints: consistent JSON errors, proper status codes, resource naming, and CORS headers.

- **[ADR-039: Real-Time OTGraph Charting Architecture](ADR-039-otgraph-real-time-charting.md)** 🆕  
  5-grid ECharts-based charting module with dynamic Dallas sensor registration, dual-theme palettes, LTTB sampling, and 24h data buffer for real-time OpenTherm monitoring.

- **[ADR-041: Just-In-Time Home Assistant MQTT Discovery](ADR-041-jit-ha-discovery.md)** 🆕  
  JIT discovery emits HA configs only after a real OpenTherm message is observed for the corresponding msgID; reduces broker noise and avoids advertising entities the boiler does not actually report.

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

- **[ADR-029: Simple XHR-Based OTA Flash (KISS Principle)](ADR-029-simple-xhr-ota-flash.md)** 🆕  
  Simplified firmware flash mechanism using XHR with backend confirmation, eliminating WebSocket complexity and Safari bugs. Reduces code by 68.5% while improving reliability.

- **[ADR-067: SSD1306Ascii Text-Only OLED Library](ADR-067-ssd1306ascii-oled-library.md)** 🆕  
  Use the lighter SSD1306Ascii library for OLED rendering instead of the full Adafruit GFX stack; saves flash and avoids heap allocation. *(Lives here because OLED is bound to the OTA/diagnostics surface; OTGW32-only.)*

### OTGW32 and Dual Platform

- **[ADR-063: OTGW32 Hardware Support — Dual Build Targets with Runtime Feature Detection](ADR-063-otgw32-hardware-support.md)** 🆕  
  Dual build targets (ESP8266 and ESP32) with runtime detection of hardware capabilities (W5500 Ethernet, OLED, BLE) so a single firmware binary boots correctly on both platforms.

- **[ADR-064: OT-Direct Operating Mode Architecture](ADR-064-ot-direct-operating-mode-architecture.md)** 🆕  
  ESP32 GPIO-direct OpenTherm bus communication using the Phunkafizer OT library; bypasses the PIC controller entirely for boards without an OTGW PIC.

- **[ADR-068: OT-Direct Schedule Tuning Constants](ADR-068-ot-direct-schedule-tuning-constants.md)** 🆕  
  Calibrated polling intervals and burst rules for OT-Direct mode, mirroring the PIC-driven cadence so consumer code (MQTT, WebSocket) sees the same shape regardless of underlying transport.

- **[ADR-087: Frame Bridge Pattern: Raw OT Frames to PIC-Format Text](ADR-087-frame-bridge-pattern.md)** 🆕  
  In OT-Direct mode, raw OT frames are bridged into the same text format the PIC emits, so downstream parsing logic in `processOT()` and beyond stays unchanged. Renumbered from earlier ADR-065 to resolve duplicate numbering.

- **[ADR-072: SAT Platform Compatibility Layer — ESP8266 vs OTGW32](ADR-072-sat-platform-compatibility-layer.md)** 🆕  
  ESP8266 vs OTGW32 differences are encapsulated in the SAT subsystem so the controller code stays platform-neutral; drives buffer sizing, peripheral availability, and timing parameters per platform.

- **[ADR-115: Per-Board Numeric Constants and Typedefs Live in boards.h](ADR-115-per-board-constants-in-boards-h.md)** 🆕  
  Amends ADR-061's home-assignment: per-board compile-time numeric tuning constants (buffer/ring sizes, heap floors, timing windows) and the typedefs whose width depends on them live in `boards.h`, not `platform_*.h` and not inline behind raw `#if defined(ESP8266)`. Dividing line: `platform_*.h` = behaviour, `boards.h` = capacity. Documents the TASK-743 Tier 3a choice.

- **[ADR-125: Combo ESP32-S3 Board: One Binary, Runtime PIC/OTDirect Selection](ADR-125-combo-esp32-pic-otdirect-board-boot-detection.md)**  
  **Superseded by ADR-126** (2026-06-10); design revived by ADR-127 (2026-06-12). Original combo board class with PIC-probe-first boot detection persisted to `settings.iBoardMode`; withdrawn after field failures that ADR-127 later root-caused to a pin-plumbing bug.

- **[ADR-126: Fixed esp32-classic Build Supersedes Combo Runtime Detection](ADR-126-fixed-esp32-classic-build-supersedes-combo-runtime-detection.md)**  
  **Superseded by ADR-127** (2026-06-12). Dropped runtime detection for three fixed compile-time builds; established the field-verified Classic-on-S3 pin map (`BOARD_NODOSHOP_ESP32_CLASSIC`) and the WiFi-portal-first boot order, both retained by ADR-127.

- **[ADR-127: Combo ESP32-S3 Single Binary Revived: Runtime PIC/OTDirect Boot Detection](ADR-127-combo-esp32-s3-single-binary-revived-runtime-boot-detection.md)** 🆕  
  Accepted (2026-06-12). **Supersedes ADR-126**, revives the ADR-125 design with all four field objections root-caused and fixed (portal-first boot order, OTGWSerial constructor pin fix TASK-862/bug-119, dual-path 0x26+TWDT watchdog, bounded runtime indirection). Transitional fourth build target `esp32-combo`; fixed S3 targets remain until field validation. Amends ADR-113 §1 (runtime `hardware_type` slug on the combo class only). Guideline-level per ADR-080.

### SAT Subsystem

- **[ADR-085: SAT (Smart Autotune Thermostat) Integration](ADR-085-sat-smart-autotune-thermostat-integration.md)** 🆕  
  Top-level decision to fold SAT into the firmware as an optional, opt-in controller (renumbered from earlier ADR-062 to resolve duplicate numbering, content unchanged).

- **[ADR-069: SAT PID v3 Controller Implementation](ADR-069-sat-pid-v3-implementation.md)** 🆕  
  Centralised PID controller implementation for the Smart Autotune Thermostat with anti-windup and adaptive-gain support.

- **[ADR-070: SAT Memory Allocation Strategy](ADR-070-sat-memory-allocation-strategy.md)** 🆕  
  File-static buffers for SAT working state with ring-buffer sizing tuned per platform (smaller on ESP8266, expanded on ESP32) to fit the heap envelope on each target.

- **[ADR-071: SAT Heating Curve Algorithm](ADR-071-sat-heating-curve-algorithm.md)** 🆕  
  Adaptive heating-curve advisor (INCREASE/DECREASE/HOLD) driving setpoint adjustments based on outdoor and indoor data with stalled-ignition adaptation.

- **[ADR-073: SAT MQTT Topic Structure](ADR-073-sat-mqtt-topic-structure.md)** 🆕  
  Topic layout for SAT inputs (room sensor, humidity), outputs (computed setpoint, mode), and configuration; integrates with the broader OT-bus topic strategy in ADR-084.

- **[ADR-076: SAT OPV (Optimal Valve Position) Calibration](ADR-076-sat-opv-calibration.md)** 🆕  
  Calibration procedure that determines the optimal valve position by observing flow and temperature response; persisted to LittleFS for cross-boot retention.

### ADR Governance

- **[ADR-080: Binding ADR Rules Must Have a CI Gate](ADR-080-binding-adr-rules-must-have-a-ci-gate.md)** 🆕  
  Pattern-level ADRs that claim to be binding must either ship with a CI verification gate or be explicitly labelled guideline-level. Sets the bar for what "binding" means in this corpus and the basis for the guideline-vs-binding distinction used by ADR-088, ADR-089, and ADR-090.

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

- Static buffer allocation (ADR-004, ADR-053)
- PROGMEM for strings (ADR-009)
- No HTTPS/TLS (ADR-003)
- Client connection limits
- Heap monitoring and adaptive throttling (ADR-030, ADR-089)
- Re-entrancy guards on shared scratch state (ADR-090)

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

- MQTT Auto-Discovery (ADR-006, ADR-077)
- Standard HA entity patterns
- Climate control integration
- Zero-configuration setup
- Retained-discovery self-heal (ADR-062)

### Cooperative Multitasking

Single-core ESP8266 requires careful task management:

- Timer-based scheduling (ADR-007)
- Non-blocking operations (ADR-047, ADR-048, ADR-058)
- Watchdog feeding
- No delay() calls
- Time-boundary single-caller contract (ADR-086)

### Arduino Ecosystem

Maintaining Arduino compatibility for community contributions:

- Arduino framework (ADR-001, ADR-013)
- Modular .ino files (ADR-002)
- Arduino IDE support
- Standard Arduino libraries where possible
- PlatformIO as primary build for dual-target firmware (ADR-083)

### Dual-Platform Reach

The 2.0.0 line supports both ESP8266 and ESP32:

- Unified platform abstraction (ADR-061)
- OTGW32 hardware variant (ADR-063)
- OT-Direct (PIC-less) mode on ESP32 (ADR-064, ADR-068, ADR-087)
- Generic OT-bus state topics (ADR-084)
- LTS Core 2.7.4 pin on the ESP8266 line for stability (ADR-082)

## Architectural Dependencies

**Foundation Layer** (all other ADRs depend on these):

```text
ADR-001 (ESP8266) ──┬──> Establishes: 40KB RAM, no HTTPS, single-core
                     │
                     ├──> ADR-004 (Static Buffers) ──> Referenced by 8+ ADRs
                     ├──> ADR-007 (Timers) ──────────> Referenced by 6+ ADRs
                     └──> ADR-013 (Arduino) ─────────> Foundation for all
```

**Most Referenced ADRs** *(advisory; counts are estimates from earlier audits and are not re-counted on every commit)*:

- **ADR-004:** Static Buffer Allocation (~8 references)
- **ADR-001:** ESP8266 Platform (~7 references)
- **ADR-007:** Timer-Based Scheduling (~6 references)
- **ADR-008:** LittleFS Persistence (~5 references)
- **ADR-051:** Dual Encapsulating Structs (referenced by ADR-079, ADR-081, etc.)
- **ADR-080:** Binding ADR rules CI gate (referenced by ADR-088, ADR-089, ADR-090)

**Decision Timeline** (earliest to latest):

1. 2016: ADR-001 (ESP8266), ADR-013 (Arduino)
2. 2018: ADR-002 (Modular), ADR-003 (HTTP-only), ADR-007 (Timers)
3. 2019: ADR-005 (WebSocket), ADR-012 (PIC upgrade), ADR-020 (Sensors)
4. 2020: ADR-004 (Static buffers), ADR-008 (LittleFS migration)
5. 2021: ADR-015 (NTP + AceTime)
6. 2024: ADR-019 (API v2)
7. 2026 Q1: ADR-025 (Safari WebSocket fix), ADR-026 (Cache-busting), ADR-027 (Version warnings), ADR-036 (Boot sequence), ADR-037 (Gateway mode), ADR-038 (Data flow), ADR-039 (OTGraph)
8. 2026 Q1: ADR-040 (MQTT source topics), ADR-041 (JIT HA discovery), ADR-042 (No ArduinoJson), ADR-043 (Triple-reset WiFi), ADR-044 (Global state header definition), ADR-045 (PS=1 summary parsing)
9. 2026 Q1: ADR-054 (Optional HTTP Basic Auth), ADR-055 (Webhook HTTP integration), ADR-056 (Protected admin contract), ADR-057 (Webhook delivery + test endpoint policy)
10. 2026 Q1-Q2: ADR-058 (Non-blocking PIC PR=), ADR-059 (Ser2net queue awareness), ADR-060 (PIC availability guard pattern)
11. 2026 Q2: ADR-061 (Platform abstraction), ADR-062 (Retained discovery verification), ADR-063 (OTGW32 hardware), ADR-064 (OT-Direct mode), ADR-065 (otgw-pic subtree), ADR-066 (Thermostat auto-detect), ADR-067 (SSD1306 OLED), ADR-068 (OT-Direct schedule tuning)
12. 2026 Q2: ADR-069 to ADR-073 (SAT PID, memory, heating curve, platform compat, MQTT topics), ADR-074 (ADR audit SAT phase), ADR-075 (WiFi reconnect timeout tuning), ADR-076 (SAT OPV calibration)
13. 2026 Q2: ADR-077 (Streaming HA discovery), ADR-078 (MQTT sub-command dispatch), ADR-079 (Per-component type headers), ADR-080 (Binding ADR rules CI gate), ADR-081 (Types merge into stuff)
14. 2026 Q2: ADR-082 (Core 2.7.4 LTS pin), ADR-083 (PlatformIO primary), ADR-084 (Generic OT-bus topics), ADR-085 (SAT integration, renumbered), ADR-086 (Time-boundary single caller, renumbered), ADR-087 (Frame Bridge, renumbered)
15. 2026 Q2: ADR-088 (MQTT status burst windowing), ADR-089 (Heap tier machine contract), ADR-090 (Re-entrancy guard pattern)
16. 2026 Q2: ADR-093 (HA discovery retained-config orphan cleanup), ADR-097 (MQTT publish gating by source and slave-echo, renumbered from ADR-066)

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
- **Binding vs. guideline:** Per ADR-080, pattern-level rules that claim to be binding must ship with a CI gate. ADRs without a CI gate are guideline-level and should be labelled as such.

## Implementation Notes

**Heap Tier Thresholds (per ADR-089, amends ADR-030):**

- `HEAP_CRITICAL` — less than 1536 bytes (emergency mode)
- `HEAP_WARNING` — 1536 to 3072 bytes (throttle aggressively)
- `HEAP_LOW` — 3072 to 5120 bytes (reduce message rates)
- `HEAP_HEALTHY` — greater than 5120 bytes
- Fragmentation-aware promotion: `HEAP_FRAG_PROMOTE_MAXBLOCK` = 1536

**Verifying memory measurements:**

```bash
# Build and check binary size
python build.py --firmware
size build/OTGW-firmware.elf

# Monitor heap at runtime via telnet (port 23)
> s  # Show status including free heap
```

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

ADR maintenance in this repository is driven by the **adr-kit** plugin (skill: `/adr-kit:adr`, subagent: `adr-generator`). The plugin lives in the Claude Code plugin cache (`~/.claude/plugins/cache/rvdbreemen-adr-kit/`) and is configured per project via `.adr-kit.json` in the repo root. There is no in-tree skill copy at `.github/skills/adr/`; that earlier GitHub Copilot skill location is deprecated and any links pointing there should be replaced with this section.

The ADR skill helps you:

- Create well-structured ADRs using the canonical sections (Context, Decision, Alternatives, Consequences, Related)
- Lint the ADR corpus for canonical structure (`bin/adr-lint` from the plugin)
- Check code changes against existing ADRs before merging
- Distinguish binding (CI-gated) ADRs from guideline-level ADRs per ADR-080
- Maintain this README as the navigation hub when ADRs are added, superseded, or renumbered

**To use the skill from Claude Code:**

```text
/adr-kit:adr               # interactive ADR workflow (create / supersede / lint)
```

Or invoke the subagent directly for a concrete creation task:

```text
Use the adr-generator agent to draft ADR-NNN for <decision>
```

See `.adr-kit.json` for project-specific configuration (lint scope, naming policy, ignore lists). The plugin's documentation at `~/.claude/plugins/cache/rvdbreemen-adr-kit/README.md` covers slash commands, workflow, and CLI usage.

## Resources

- **adr-kit plugin** (`/adr-kit:adr` skill, `adr-generator` subagent): the active ADR workflow tooling for this repo, configured via `.adr-kit.json`.
- **adr-lint CLI**: `bin/adr-lint` from the plugin cache, run by the `/adr-kit:adr` lint flow.
- **ADR Best Practices:** <https://adr.github.io/>
- **Michael Nygard's ADR Template:** <https://github.com/joelparkerhenderson/architecture-decision-record>
- **Evaluation Framework:** `evaluate.py` (enforces decisions like PROGMEM usage)

## Maintenance

ADRs are living documentation:

- Review ADRs when onboarding new developers
- Reference ADRs in code reviews
- Update ADR status when decisions change
- Link from code comments to relevant ADRs
- Use ADRs to inform agent and copilot instructions
- Keep this README in sync with the ADR corpus (TASK-427 closed the gap from ADR-058 through ADR-088 on 2026-04-26; future additions should land here at the time the ADR is accepted)

---

*For questions about these ADRs or to propose new ones, please open an issue on GitHub.*
