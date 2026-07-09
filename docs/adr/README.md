# Architecture Decision Records (ADRs)

This directory contains Architecture Decision Records (ADRs) that document significant architectural choices made in the OTGW-firmware project.

## What are ADRs?

Architecture Decision Records capture important architectural decisions along with their context, alternatives considered, and consequences. They serve as historical documentation to help current and future developers understand why the system is built the way it is.

## Quick Navigation

**By Topic:**

- [Platform & Build System](#platform-and-build-system) (5 ADRs)
- [Network & Security](#network-and-security) (7 ADRs)
- [Memory Management](#memory-management) (13 ADRs)
- [Integration & Communication](#integration-and-communication) (9 ADRs, plus MQTT subsection of 32)
- [System Architecture](#system-architecture) (27 ADRs)
- [Hardware & Reliability](#hardware-and-reliability) (8 ADRs)
- [Development & Build](#development-and-build) (6 ADRs)
- [Core Services](#core-services) (6 ADRs)
- [Features & Extensions](#features-and-extensions) (10 ADRs)
- [Browser & Client](#browser-and-client-compatibility) (7 ADRs)
- [OTA & Updates](#ota-and-firmware-updates) (2 ADRs)
- [OTGW32 & Dual Platform](#otgw32-and-dual-platform) (15 ADRs)
- [SAT Subsystem](#sat-subsystem) (16 ADRs)
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

- **[ADR-128: Drop ESP8266 Support from the 2.0.0 Line (Supersedes ADR-082)](ADR-128-drop-esp8266-support-from-2-0-0.md)** 🆕  
  Accepted (2026-06-12). Drops ESP8266 support from the 2.0.0 line so it targets ESP32-S3 only; supersedes ADR-082. Unblocks the FreeRTOS + async concurrency model (ADR-123).

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

- **[ADR-143: Telnet + ser2net transport on AsyncTCP (AsyncSimpleTelnet) to remove loop-task socket-write blocking](ADR-143-async-telnet-ser2net-transport-asynctcp.md)** 🆕  
  Accepted. Moves the Telnet + ser2net transport onto AsyncTCP (AsyncSimpleTelnet) so loop-task socket writes no longer block; part of the ADR-123 async migration.

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

- **[ADR-089: Heap Tier Machine Contract](ADR-089-heap-tier-machine-contract.md)** *(Proposed for supersession by ADR-167)*  
  Amends ADR-030: re-baselines tier thresholds to 1536/3072/5120 bytes (Crashevans field log evidence, TASK-344), adds fragmentation-aware promotion (`HEAP_FRAG_PROMOTE_MAXBLOCK` = 1536), adds tier-entry counters (`iEnteredLowCount`/`Warning`/`Critical`, TASK-346) with hourly retained MQTT publication. Three sub-rules CI-gated, two explicitly labelled guideline-level. Defense-in-depth peer with ADR-088. ADR-167 (Proposed) documents that this tier machine is ESP8266-era overhead unnecessary on the ESP32-S3-only `dev` branch, per TASK-956 soak evidence; removal is a deferred Phase-3 task, not yet accepted.

- **[ADR-090: Re-entrancy Guard Pattern for Shared Scratch Buffers](ADR-090-re-entrancy-guard-pattern-shared-scratch-buffers.md)** 🆕  
  Guideline-level ADR (per ADR-080: 2 instances in 1 file is below the recurrence bar for binding pattern-level enforcement; no CI gate). Documents the acquisition contract for file-scope or function-local-static mutable scratch state shared across re-entrant call paths in the cooperative ESP8266 model. Two existing exemplars in `MQTTstuff.ino`: RAII `MQTTAutoConfigSessionLock` (preferred for new code) and the `publishToSourceTopic` function-local `inUse` flag.

- **[ADR-121: Per-Consumer Heap Gating for the WebSocket Live-Log vs MQTT Publish](ADR-121-per-consumer-heap-gating-websocket-vs-mqtt.md)** *(Proposed for supersession by ADR-167)*  
  Accepted (binding amendment to ADR-089). Splits heap gating per consumer: the WebSocket live-log yields under heap pressure while MQTT publishing keeps a lower floor, so the OT-hot path degrades gracefully instead of starving MQTT. ADR-167 (Proposed) documents that this per-consumer split is likewise ESP8266-era overhead unnecessary on the ESP32-S3-only `dev` branch; removal is a deferred Phase-3 task, not yet accepted.

- **[ADR-167: Retire the ESP8266-Era Heap Tier Machine and Per-Consumer Gating on the ESP32-S3-Only Dev Branch](ADR-167-esp32s3-heap-tier-gating-removal-supersedes-adr089-adr121.md)** 🆕 *(Proposed)*  
  Documents the maintainer-authorized decision to retire ADR-089's tier machine and ADR-121's per-consumer WS/MQTT gating on `dev` (ESP32-S3-only), backed by TASK-956 soak evidence: a 6.7h synthetic-extreme run (alpha.286) and a 10h representative-load run (alpha.331+a82bfda) with `hd_min_max_block` never below 10,740 bytes and zero tier-entry/drop events in either run. Actual removal (and retiring the four `evaluate.py` gates) is deferred to a Phase-3 follow-up task (TASK-956 AC#4, not yet created). Status intentionally stays Proposed: evidence limitations (10h vs the 24-72h originally scoped; no MQTT discovery republish traffic) are disclosed openly, and acceptance is the maintainer's own call.

- **[ADR-141: Adopt ArduinoJson v7 for JSON on the ESP32-S3 2.0.0 line](ADR-141-adopt-arduinojson-v7-esp32s3.md)** *(Superseded by ADR-146)*  
  Short-lived decision to adopt ArduinoJson v7 for JSON on the ESP32-S3 2.0.0 line (reversing ADR-042); reverted to a hand-rolled streaming writer in ADR-146.

- **[ADR-145: Serve REST JSON via a chunked, pull-based response (no whole-response buffer)](ADR-145-chunked-pull-json-response-streaming.md)** *(Superseded by ADR-146)*  
  Served REST JSON via a chunked, pull-based response re-serialized per TCP window; retired when ArduinoJson was reverted in ADR-146.

- **[ADR-146: Revert ADR-141 — remove ArduinoJson, return to a hand-rolled streaming JSON writer (JsonEmit) on the ESP32-S3 REST path](ADR-146-revert-adr141-streaming-jsonemit-rest-esp32s3.md)** 🆕 *(Supersedes ADR-141, ADR-145)*  
  Accepted. Reverts ADR-141 and ADR-145: removes ArduinoJson and returns to a hand-rolled streaming JSON writer (JsonEmit) on the ESP32-S3 REST path.

- **[ADR-107: Emergency Heap Recovery Actions (2.0.0 platform variant)](ADR-107-emergency-heap-recovery-actions.md)** 🆕  
  Defines what `emergencyHeapRecovery()` does at HEAP_CRITICAL on the 2.0.0 platform: a rate-limited scheduler `yield()` for SDK free-pool housekeeping.

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

- **[ADR-124: HA Discovery Seven-Device Topology — Dedicated OT-Core and Sensors Devices, Gateway via_device Hub](ADR-124-ha-discovery-seven-device-topology.md)**  
  **Superseded by ADR-140** (2026-06-15). **Supersedes ADR-122.** Expands the HA discovery device set to seven — `{Boiler, Thermostat, Gateway, Esp, Pic, Sat, Sensors}` — giving the OpenTherm-core (PIC `HAS_PIC` / OTDirect `HAS_DIRECT_OT`, named per hardware) and the physical hardware sensors (Dallas + S0) their own devices, and adding a Gateway `via_device` hub. The `via_device` addition is a deliberate divergence from HA core 2024.10 (which sets none, the reason ADR-122 §8 omitted it). Carries forward ADR-122's `haDeviceForEntity()` routing, two-axis legacy model, and migration design. Binding under ADR-080: `check_ha_discovery_device_routing` + golden-file test updated to seven devices in TASK-826.

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

- **[ADR-157: SAT Heating-Source Selection Is Manual Configuration; Auto-Detect Is a Non-Control Hint](ADR-157-sat-heating-source-manual-config-auto-detect-is-non-control-hint.md)** 🆕 *(Accepted)*  
  Accepted (2026-06-29, TASK-943; Decision Maker: Robert, prompted by George on #dev-sat-mqtt). OpenTherm has no device-class field; the only "heat pump?" proxy is MsgID 3 HB bit2 "cooling supported" (spec v4.2:1606), which is lossy (heating-only heat pumps never set it; hybrid is invisible on one bus). So heating-source selection is **manual config** (`settings.sat.iHeatingSource`), authoritative for control; `AUTO` resolves to a safe gas-boiler profile (`satGetEffectiveHeatingSource`, SATcontrol.ino:204). Auto-detect is demoted to a **non-control telemetry hint** (`heating_source_detected`), read from the correct MsgID 3 bit2. Supersedes the TASK-891.8 auto-detect-drives-control behaviour (no prior ADR). Behaviour-neutral for the common case (MsgID 74 was absent, so AUTO already behaved as gas boiler). Guideline-level, no `evaluate.py` gate (ADR-080); related ADR-150/156, TASK-892/641 (out-of-band detection, deferred).

- **[ADR-156: Unified Off/Heat/Cool Home Assistant Climate Entity Derived from OpenTherm Status Bits](ADR-156-ha-climate-heat-cool-from-ot-status-bits.md)** 🆕 *(Accepted)*  
  Accepted (2026-06-27, TASK-939; 2.0.0 port landed commit `94890099`/alpha.279; 1.x origin `1.7.1-beta.2`, commit `7b2d3cdd`, GitHub #665). Replaces the heating-only `["off","heat"]` HA `climate` entity with a unified **`off`/`heat`/`cool`** entity driven by two new firmware-computed topics: `<pub>/hvac_mode` (`cool` when the master `cooling_enable` bit MsgID 0 HB bit 2 is set, else `heat` when the thermostat is connected, else `off`) and `<pub>/hvac_action` (`cooling`/`heating`/`idle` from the slave cooling/central-heating bits, **not** flame; `off` when disconnected). Mode is **reflective** (no `mode_command_topic`): the thermostat owns heat/cool switching (MsgID 3 HB bit 7) and re-asserts every ~7 s, so the OTGW mirrors it rather than fighting it. `modes` static `["off","heat","cool"]`, `mode_stat_t`/`action_topic` pass-through, `max_temp` 28→30; the single setpoint stays `TrSet` (MsgID 16). Rejects dual-setpoint `HEAT_COOL` (infeasible: OT carries one room setpoint + percentage cooling demand MsgID 7, never two bounds), mode-from-`ch_enable` (flickers heat↔off on demand cycles), and action-from-flame (false `heating` during DHW draws). Deliberately diverges from HA's `opentherm_gw` integration (single-element `_attr_hvac_modes` never reset + HEATING gated on `ch_active AND flame_on` freeze a heatpump on `cool`). Heating-only/gas-boiler users unaffected (cooling bits never set). Structural decision, no `evaluate.py` gate (ADR-080 structural split); references ADR-077/098/101/084/140/031.

- **[ADR-142: HA Discovery — Mark Unsupported MsgID Entities Unavailable via Availability List](ADR-142-ha-discovery-mark-unavailable-unsupported-msgids.md)** 🆕 *(Proposed)*  
  Marks HA discovery entities for MsgIDs the boiler does not support as unavailable via an availability list, instead of advertising dead entities.

- **[ADR-094: Home Assistant Discovery State Reconciliation on OTA Upgrade (feature-2.0.0 port of ADR-067)](ADR-094-ha-discovery-state-reconciliation-on-ota-upgrade.md)** 🆕  
  Guideline-level per ADR-080. Per-device, firmware-driven removal of stale retained HA discovery configs across an OTA firmware transition; complements ADR-093's per-entity user-driven removal.

- **[ADR-095: bSeparateSources Makes Base and Source-Variant Entities Mutually Exclusive (feature-2.0.0 port of ADR-068)](ADR-095-bseparatesources-mutually-exclusive-base-and-source-variants.md)** *(Superseded by ADR-097)*  
  Flipped `bSeparateSources` from additive to mutually-exclusive base-vs-source entities to end duplicate HA entities; superseded by ADR-097.

- **[ADR-096: MQTT Source-Subtopic Worldview Semantics](ADR-096-mqtt-source-topic-worldview-semantics.md)** *(Superseded by ADR-103)*  
  Adopts the worldview model (each source subtopic shows what that device sees); superseded by ADR-103, which adds the proxy-answer (no-B) routing case.

- **[ADR-098: MQTT Discovery Topic Sibling-Suffix Shape (Supersedes ADR-097)](ADR-098-mqtt-discovery-topic-sibling-suffix-shape.md)** 🆕  
  Discovery topic identifiers adopt the sibling-suffix shape to satisfy HA's `discovery.py` TOPIC_MATCHER; supersedes ADR-097's nested-discovery carve-out.

- **[ADR-099: HA Discovery Friendly-Name Format](ADR-099-ha-discovery-friendly-name-format.md)** 🆕  
  Drops the hostname prefix and renders human-readable entity names (spaces, consistent acronym casing) instead of raw internal identifiers.

- **[ADR-100: JIT HA Discovery with Smart Reconnect (Port of dev ADR-073)](ADR-100-jit-ha-discovery-smart-reconnect.md)** 🆕  
  Stops the bulk republish of all 256 configs at every boot/reconnect; republishes only after an offline-threshold reconnect (broker-restart heuristic).

- **[ADR-103: MQTT Source-Topic Worldview Routing — Proxy-Answer (no-B) Refinement](ADR-103-mqtt-source-topic-proxy-answer-routing.md)** 🆕 *(Supersedes ADR-096)*  
  Distinguishes a proxy-answer `A` (no preceding `B`) from an answer-override `A`; a proxy `A` publishes to canonical, `_boiler`, and `_thermostat`. Supersedes ADR-096.

- **[ADR-104: MQTT Status Fan-Out — Drop Global Rate-Gate, Keep Per-Slot Heartbeat (2.0.0)](ADR-104-mqtt-status-fanout-drop-global-rate-gate.md)** 🆕  
  Removes the 250 ms global publish rate-gate that starved status slots; keeps the per-slot 60 s heartbeat with immediate change-detect publishing.

- **[ADR-105: MQTT — Publish HA-Core-Style Self-Describing Aliases for Capability, State, Type, and Fault Bits under `settings.mqtt.bPublishHaCoreAliases`](ADR-105-mqtt-ha-core-capability-flag-aliases.md)** *(Superseded by ADR-106)*  
  Default-off dual-publish of HA-core-style alias topics alongside OT-spec labels; superseded by ADR-106's mutually-exclusive naming toggle.

- **[ADR-106: MQTT Topic Naming Mode — New Self-Describing Names by Default, Legacy OT-Spec Names via `settings.mqtt.bUseLegacyOtTopics` (Mutually Exclusive)](ADR-106-mqtt-topic-naming-mode-new-default-legacy-toggle.md)** 🆕  
  Self-describing topic labels become the 2.0.0 default; `settings.mqtt.bUseLegacyOtTopics` flips back to legacy OT-spec names (mutually exclusive, breaking change).

- **[ADR-108: MQTT connect() Socket Timeout Accepted as Known Main-Loop Sync-Blocker (2.0.0)](ADR-108-mqtt-connect-socket-timeout-as-accepted-sync-blocker.md)** *(Superseded by ADR-131)*  
  Accepted PubSubClient's synchronous `connect()` bounded by `setSocketTimeout(5)`; superseded by ADR-131 after espMqttClient made connect async.

- **[ADR-112: Pure JIT MQTT Discovery (2.0.0 sibling of dev ADR-073)](ADR-112-pure-jit-mqtt-discovery.md)** 🆕  
  Removes the remaining `markAllMQTTConfigPending()` boot/reconnect paths so discovery is emitted strictly just-in-time per observed MsgID.

- **[ADR-116: MQTT On-Change Publishing as the Default with One-Time Interval Migration](ADR-116-mqtt-on-change-publishing-default.md)** 🆕  
  Makes on-change publishing the default with a one-time migration of the legacy fixed-interval setting.

- **[ADR-118: Surface Gateway Overrides as Distinct Override State](ADR-118-surface-gateway-overrides-as-distinct-override-state.md)** 🆕  
  Publishes user-injected gateway overrides (`OT=`/`TT=`/`SW=`/`MM=`) to a distinct override topic so they stay visible even though the worldview gate keeps them out of canonical.

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
  Accepted (2026-06-13). Lifts the PIC UART runtime byte I/O onto a dedicated FreeRTOS task pinned to the app core, the sole runtime owner of `OTGWSerial` read/write; deletes the four-lines-per-call bound (`kMaxLinesPerDrain`, TASK-671). RX assembles lines onto the ADR-129 frame queue; TX uses a new value-copy `otTxQueue`; RX errors are detected in the task but reported loop-side to keep `OTGWState` single-writer. Task parks during a PIC flash (ADR-012 handshake) and on an OTDirect boot (ADR-127), gating on `isOTDirectEnabled()` so ADR-060 banner recovery survives. Refines ADR-123 §model-1's "high-priority task" to equal-to-loop priority for the Phase-1 (still-synchronous-networking) environment. First consumer of ADR-129; supersedes nothing. New CI gate `check_pic_uart_task_owns_serial`.

- **[ADR-132: HTTP Stack on ESPAsyncWebServer with an Imperative-Push to Async-Pull Bridge (ADR-123 Phase 3)](ADR-132-async-web-server-and-imperative-push-bridge.md)** 🆕 *(Supersedes ADR-109)*  
  Accepted (2026-06-14). Moves the web stack off the cooperative `loop()` onto ESPAsyncWebServer (over AsyncTCP): one `AsyncWebServer server(80)` instantiated once (ADR-044) and exposed `extern` for WebSocket (seq10) and OTA (seq11); the four-deep `handleClient()` drain (TASK-817) and the `OTGWWebServer = WebServer` alias are deleted. Removes the sat-slider stall at the root (parallel sockets served on the AsyncTCP task, not one-per-loop-turn). New `webServerCompat.h` bridges the imperative-push JSON builders (`sendStartJsonMap` → N×`sendJsonMapEntry` → `sendEndJsonMap`) to the async pull/callback API via a file-static per-request context (safe under single-AsyncTCP-task serialization), send-exactly-once helpers, merged `argCompat()` semantics, and a body-capture hook. Bounded JSON streams via `AsyncResponseStream` (retiring ADR-109's `sTxBuf`); static files stream straight from LittleFS; the ~39 KB index streams chunked. ADR-056 auth/CSRF preserved verbatim. OTA is intentionally dark across the 865.9→865.11 seam. Supersedes ADR-109 (whose "do NOT migrate to AsyncWebServer" premise fell with ADR-128 dropping ESP8266). Proposed Phase-3 CI gate forbids `httpServer.`/`handleClient(`/`OTGWWebServer` in app files.

- **[ADR-133: WebSocket Live-Log on AsyncWebSocket at /ws on Port 80 (ADR-123 Phase 3)](ADR-133-websocket-live-log-on-asyncwebsocket-port-80.md)** 🆕 *(Supersedes ADR-005)*  
  Accepted (2026-06-14). The WebSocket half of ADR-123 Phase 3 (seq10): retires the dedicated port-81 `WebSocketsServer` (Links2004 / `WebSockets @ 2.3.6`) and streams the OpenTherm live-log over a single `AsyncWebSocket otLogWs("/ws")` attached to the shared port-80 `AsyncWebServer server` from ADR-132 (`server.addHandler(&otLogWs)`). No dedicated WebSocket port, no second TCP listener, no per-loop `webSocket.loop()` poll: the socket is serviced on the AsyncTCP task and `handleWebSocket()` becomes a 1 s timer doing only `cleanupClients()` plus the 30 s ADR-025 keepalive. `startWebSocket()` is now attach-once idempotent (guarded by `wsInitialized`) because it is called on every WiFi/Ethernet transition without tearing the server down. The parallel `wsClientCount` is deleted in favour of `otLogWs.count()`, and the connect cap flips to `count() > MAX_WEBSOCKET_CLIENTS` since AsyncWebSocket inserts the client before firing `WS_EVT_CONNECT`. All transmit paths route through `otLogWs.textAll()`; `sendLogToWebSocket()` keeps its `&& canSendWebSocket()` OT-hot-path heap gate verbatim (ADR-121 preserved). The Web UI swaps `WEBSOCKET_PORT = 81` / `ws://host:81/` for `WEBSOCKET_PATH = '/ws'` / `ws://host/ws` from `window.location.host`; ADR-025 Safari guard retained. Supersedes ADR-005 (dedicated port 81 + Links2004 server); the streaming concept survives, re-homed. Proposed Phase-3 CI gate forbids `WebSocketsServer`/`webSocket.loop(`/`broadcastTXT` in app files and `WEBSOCKET_PORT` in index.js.

- **[ADR-134: OTA Upload Handler on AsyncWebServer onUpload (ADR-123 Phase 3)](ADR-134-async-ota-upload-handler-on-asyncwebserver.md)** 🆕  
  Accepted (2026-06-14). The OTA half of ADR-123 Phase 3 (seq11), and the sub-phase that **closes the OTA seam ADR-132 opened** (which left firmware-over-WiFi intentionally dark across the 865.9→865.11 seam). Rewrites the ESP32 firmware-update upload handler (`OTGW-ModUpdateServer-esp32.h`) from the synchronous `HTTPUpload` `UPLOAD_FILE_START/WRITE/END/ABORTED` lifecycle onto the AsyncWebServer `onUpload(filename,index,data,len,final)` callback, and attaches `/update` once to the shared port-80 `AsyncWebServer server` from ADR-132. `index==0` is start, `final==true` is end (the final chunk carries data + final, so write-then-finalize); the absolute `index` simplifies the merged-binary skip/limit math. Abort has no callback on async, so `request->onDisconnect()` drives the cleanup, guarded by `_updateFinalized` so a clean close does not double-act. Target detection moves off the (now-absent) multipart field name onto the `?cmd=0`/`?cmd=100` query param the form already posts (ADR-029 contract), with a `.littlefs.bin` filename-suffix fallback. The `/update` route is registered attach-once in `setupFSexplorer()` (guarded by `_routesRegistered`, the OTA analog of ADR-133's `wsInitialized` + ADR-044); `startWiFi()` keeps only `updateCredentials()` current on WiFi/Ethernet transitions. Carried over unchanged: merged-binary app-slot extraction (skip `0x10000`, limit `0x1E0000`), the I2C `0x26`/`0xA5` hardware-watchdog feed per write chunk (ADR-011) now on the AsyncTCP task, the `U_SPIFFS`/`U_FLASH` target split + LittleFS remount + `writeSettings`, the deferred-reboot-from-`loop()` contract (`requestDeferredReboot`, never `doRestart()` in the callback), and the four `logBootSignature` probes. Supersedes nothing; preserves ADR-029 (frontend XHR flow) and ADR-011 (watchdog). Proposed Phase-3 CI gate forbids `HTTPUpload`/`upload.status`/`_server->on(` scoped to `OTGW-ModUpdateServer-esp32.h` only.

- **[ADR-137: Defer Outbound HTTP Initiated From an Async Handler to loop() Context](ADR-137-defer-outbound-http-from-async-handlers.md)** 🆕 *(Amends ADR-132)*  
  Accepted (2026-06-14). Generalizes ADR-132's "no blocking work on the AsyncTCP task" rule from response generation to **outbound HTTP**. After the 865.9 async web migration, two PIC seams still ran blocking `HTTPClient` calls to `otgw.tclcode.com` on the AsyncTCP task: the **auto-firing** `/api/v2/pic/update-check` HEAD (fired on PIC-tab open) and the `/pic?action=refresh` GET+`writeToStream` hex download. Because that one task services all HTTP plus the ADR-133 live-log WebSocket and shares lwIP, a multi-second (or unreachable-host timeout) outbound call froze the whole web stack, worse than the pre-865.9 sync server where it stalled only one loop turn. Both are now **deferred to loop() context** via the existing `handlePendingUpgrade()` single-slot bridge (ADR-134's pattern): the async handler validates + QUEUES (`queuePicUpdateCheck`/`queuePicRefresh`, one-at-a-time `pendingPicHttpJob`) and returns immediately (`status:"checking"`/`"started"`, or `409 busy`); the loop()-context worker `handlePendingPicHttp()` performs the `HTTPClient` work off the network task and caches the result in `state.pic` (`sLatestFw`/`iUpdateCheck`); the frontend re-polls (`pollPICUpdateCheck`/`pollPICRefresh`). Timeouts are bounded (`PIC_HTTP_TIMEOUT_MS` 5 s, `PIC_HTTP_DOWNLOAD_TIMEOUT_MS` 15 s) under the 30 s TWDT (ADR-135), `feedWatchDog()`-bracketed. ADR-132's body/Status/Enforcement gate untouched; amends ADR-132, mirrors ADR-134, depends on ADR-135. Enforcement is `llm_judge:true` (the rule is call-graph reachability — blocking primitives stay legal inside the loop worker — not a line-symbol). Chose loop-defer over a transient FreeRTOS worker task (ADR-136's webhook shape) for KISS, since the PIC seams are rare, manual, one-at-a-time.

- **[ADR-138: PIC-UART Control-Method Park Handshake, TX Requeue-to-Front Ordering, and Progress-Path Heap-Gate Decision](ADR-138-pic-uart-control-method-park-and-tx-ordering.md)** 🆕 *(Amends ADR-130 and ADR-133)*  
  Accepted (2026-06-14). ADR-130 follow-up closing residual PIC-UART task-coupling gaps the sole-owner byte-I/O gate cannot catch by construction. **(A) Control-method UART access must run parked.** Two vendored `OTGWSerial` control methods drive the UART data line: `resetPic()` (direct `GW=R` write + reset-GPIO toggle + `delay(100)`) and `detectPIC()`'s `find(ETX)` (a `Stream` read loop). Called loop-side while the PIC task is **unparked** (`resetOTGW()` via ser2net `GW=R` and MQTT `resetgateway`, and `detectPIC()` via the manual `'p'` telnet debug command), the loop becomes a second concurrent UART owner. A new unconditional `g_picResetInProgress` flag is OR-ed into `picSerialTaskShouldPark()`; `resetOTGW()` and `detectPIC()` now raise it and `waitForPICTaskParked()` before the direct access (the whole `resetPic()`+`find(ETX)` span for `detectPIC`), so as of TASK-865.15 there is **no remaining unparked direct-UART loop path**. Boot calls run before `startPICSerialTask()`, so the park is a no-op there. **(B) TX requeue-to-front ordering invariant.** A flow-control short write re-queued the popped chunk to the **back** of `otTxQueue`; the loop-side ser2net relay enqueues 1 byte per `OTTxMsg` concurrently, so a relay byte could jump ahead and interleave a command's bytes (silent corruption). Now re-queued to the **front** via a new `platformQueueSendToFront()` shim (over `xQueueSendToFront`), restoring strict FIFO. **(C)** sim-mode entry parks before the loop-side `picSerialFlushRx()`. **(D)** `fwupgradestep()` dedupes the progress WebSocket on integer-pct change (~101 frames/flash, not a per-callback firehose) and is **consciously NOT routed through the ADR-121 heap gate** (progress is operation state not a droppable log line; `fwupgradedone()` sends completion independently): the note this ADR adds to ADR-133. ADR-130's byte-I/O gate and both amended ADRs' bodies/Status are untouched and still pass; `platformQueueSendToFront` follows ADR-120 (shim first, called unguarded). Enforcement is `llm_judge:true` (the park/ordering/gate invariants are call-graph/semantic, not line-symbols).

- **[ADR-168: PIC Serial Link Binds Native UART0 on GPIO43/44; IDF Console Muted on the PIC Path](ADR-168-pic-serial-native-uart0-and-idf-console-mute.md)** 🆕 *(Proposed)*  
  Proposed (2026-07-09). On the PIC path (esp32-classic / combo-with-PIC) the vendored `OTGWSerial` driver now binds the ESP32-S3's **native UART0** on GPIO43/44 (`HardwareSerial(0)`, commit ad7334846 / alpha.336) instead of UART1 via the GPIO matrix, which had ESP->PIC TX structurally broken on this hardware (every `PR=`/`GW=` command silently unanswered; `GW=R` could not even reset the PIC) while PIC->ESP RX worked, masked by the RX-only `fwreportinfo` banner callback. Second finding: the Arduino core's prebuilt sdkconfig keeps the primary IDF console on UART0 (`ARDUINO_USB_CDC_ON_BOOT=1` only moves Arduino's `Serial`), so esp_log/ROM output transmitted straight into the PIC (spurious `SE` responses; fatal inside the PIC-flash bootloader STX window). A new `platformMuteUart0Console()` shim (null vprintf + no-op putc1) is called on the PIC path before `OTGWSerial.begin()` (commit 8118d29c / alpha.337). Verified on-device: `PR=A` answers <50ms, `GW=R` resets the PIC (banner +57ms). Accepted residuals disclosed: eFuse-level ROM boot log still reaches the PIC at reset (harmless, `detectPIC` resets the PIC after), and the PIC-flash bootloader handshake STILL fails (deterministic non-STX byte <130ms after ETX; hardware measurement pending, TASK-972) -- this ADR fixes the command channel, it does not claim the flash works. Related: ADR-130, ADR-138, ADR-158. Enforcement: forbid `HardwareSerial(1)` in `OTGWSerial.cpp` + `llm_judge` for the mute-before-begin ordering.

- **[ADR-139: ETag + Bounded max-age as the Project-Wide Cache-Busting Standard for Web UI Static Assets (Retire ?v= Query Versioning and the Chunked Index Rewriter); Align AsyncTCP Task Config with the EMS-ESP32 Blueprint](ADR-139-etag-cache-busting-standard-and-asynctcp-task-config.md)** 🆕 *(Amends ADR-132; Supersedes ADR-026)*  
  Accepted (2026-06-15), guideline-level (ADR-080: no automated gate planned). Generalizes ADR-132's body-size routing into a **project-wide cache-busting standard for ALL web UI static assets**, and supersedes ADR-026's `?v=<fsHash>` conditional cache-busting mechanism. A field capture (`esp32-combo`, `alpha.192`) showed `GET /` hanging with no response while telnet, the loop, and REST (`curl /api/v2/device/info` returned JSON) were all alive, isolating the hang to the root-page path: the former hand-rolled `beginChunkedResponse` pull-callback that read the 39,791-byte `index.html` line-by-line and rewrote 8 versioned-asset tokens to carry `?v=<fsHash>` (residual bytes in `IndexEmitState`), novel flash-I/O-plus-rewrite code on the AsyncTCP task and the leading hang suspect. **Decision (primary):** make **stable plain-file URLs + ETag (= `getFilesystemHash()`) + `Cache-Control: public, max-age=60`** the standard for the shell AND the 8 JS/CSS sub-resources, retiring `?v=` query versioning on both sides (the chunked shell rewriter and `serveVersionedAsset`'s `?v=`-match logic). Assets are served by **library-managed file streaming** (`beginResponse(LittleFS, path)` / `AsyncFileResponse`, which reads `len`-sized chunks on demand, `WebResponses.cpp:817-819`: ADR-132's "never buffer the whole 39 KB on the fragmented heap" guarantee without the hand-rolled code and **without gzip**). **No gzip (maintainer directive):** the LittleFS image holds plain readable files, the build ships no `.gz` archives, and TASK-433's gzip disable is **affirmed, not reversed**; the heap concern is met by incremental streaming alone. **Critical nuance:** ETag is paired with a **bounded `max-age` (60 s, EMS precedent), NOT `no-cache`**: `no-cache` forces a conditional GET per asset every load, maximizing the parallel sub-resource requests behind the documented OTGW32 starvation (**bug-113**, the sat-slider stall); `?v=` immutable URLs had zero revalidation, so bounded `max-age` (zero requests within the window, then one cheap bodiless 304) is the mitigated middle ground. Trade-offs recorded: more sub-resource revalidation than immutable `?v=` (bounded 304 burst, knob = N; Alternative C build-time `?v=` is the fallback) and a 60 s shell-staleness window after reflash (the bounded version of ADR-026's Safari concern). The shell stays a **thin handler preserving `checkHttpAuth()`** (ADR-056; `serveStatic` has no auth hook); JS/CSS stay unauthenticated. **Decision (secondary, NOT the fix):** `-D CONFIG_ASYNC_TCP_RUNNING_CORE=1` added to the **global `[env].build_flags`** so `esp32`/`esp32-classic`/`esp32-combo` all inherit it (combo/classic rebuild flags from the global, not `[env:esp32]`; the hang was on combo). **Implemented** (TASK-865.9 follow-up): `serveVersionedAsset` rewritten to ETag + `max-age=60` plain-file-only with a `LittleFS.exists()` 404 guard; `sendIndex` now a thin `webBeginRequest`->`checkHttpAuth`->`serveVersionedAsset("/index.html")` handler with `IndexEmitState` + chunked rewriter deleted; `webSendFile` got the `LittleFS.exists()`->404 guard. Amends ADR-132 (immutable), supersedes ADR-026 (immutable); references ADR-123 (concurrency, Phase 3), ADR-056 (admin security preserved), ADR-128 (single-target). Accepted 2026-06-15: implementation done (the out-of-scope REST_STREAM_BUFFER_SIZE change pulled out as a separate decision); hardware confirmation of the `/`-hang fix + the 304-burst behaviour is field-validation, tracked via the curl root-page probe in `scripts/capture-mqtt-debug.bat` (TASK-866).

- **[ADR-163: Web UI Static Assets Use Cache-Control: no-cache (Always-Revalidate via ETag); Amends ADR-139's Bounded max-age](ADR-163-web-asset-cache-control-no-cache-amends-adr139.md)** 🆕 *(Amends ADR-139)*  
  Accepted (2026-06-30). Changes the `serveVersionedAsset()` `Cache-Control` value from `public, max-age=60` to `no-cache` (browser may store but MUST revalidate via the existing `ETag = getFilesystemHash()` every load); everything else in ADR-139 (ETag standard, stable URLs, no `?v=`, AsyncFileResponse streaming, AsyncTCP config) stays. Fixes TASK-958 ("OTA filesystem update succeeds but serves old assets"): bench repro on OTGW32 @192.168.88.39 proved the OTA write is correct (served `version.hash` flipped `a46e95a -> 03591d5`), but `max-age=60` let the browser serve cached old assets for up to 60 s without revalidating after the post-reboot reload. ADR-139's reason for choosing `max-age` over `no-cache` (per-load revalidation burst on the then single-connection server, bug-113) is mitigated: the 2.0.0 stack is async, and the 304 revalidation path is UNGATED by the ADR-147 file-serve gate (verified: 8 concurrent matching-ETag GETs -> 24/24 `304`, zero `503`). Also lands two TASK-958 defensive fixes: post-OTA reload cache-bust (`updateServerHtml.h`) and a post-write `/version.hash` existence check (`OTGW-ModUpdateServer-esp32.h`). Amends ADR-139 (immutable); references ADR-147, ADR-029/134.

- **[ADR-140: Single-Device HA Discovery Topology with Seven Categories in One Device (align 2.0.0 with the 1.6.x single-device model)](ADR-140-single-device-ha-topology-entity-category-clustering.md)** 🆕 *(Supersedes ADR-124)*  
  Accepted (2026-06-15), guideline-level (ADR-080: payload shape is field-validated). Reverts the 2.0.0 multi-device HA topology (ADR-124 seven-device split with `via_device` hub) back to **ONE device per hardware OTGW** after field testing found the multi-device layout confusing. **Mental model: 1 hardware = 1 IP = 1 MQTT device in HA.** The seven former device groupings (Boiler, Thermostat, Gateway, ESP, OT-Core, SAT, Sensors) survive as seven **categories** inside the single device, rendered as an entity-name prefix (the retained `deviceForOTId` classification repurposed from device-selection to category-selection); HA's native within-device sectioning is only three buckets (primary/Config/Diagnostic via `entity_category`), so seven native sections are impossible and the categories are a naming-prefix grouping, with `entity_category` kept as an orthogonal secondary layer. The seven-DEVICE emission is hard-removed (the seven `dev` blocks, `via_device`, per-device metadata, and the `deviceIntroduced[]` array); the `HaDevice` enum + `deviceForOTId` are kept and repurposed. Removes review finding F1 (the `deviceIntroduced[]` MEASURE-pass two-pass-determinism bug) by adopting the driver-set first-entity gate, and folds in F5 (escape hostname/manufacturer/model). Supersedes ADR-124 (immutable); ADR-106 topic-naming is orthogonal and untouched. Implemented via TASK-871 (collapse + F1/F5) and TASK-872 (category prefix). *(Amended by ADR-148, BLE probes only.)*

- **[ADR-148: BLE Sensors as Separate Home Assistant Child Devices via via_device (amend ADR-140's single-device rule for BLE probes only)](ADR-148-ble-sensors-as-ha-child-devices-via-device.md)** 🆕 *(Amends ADR-140)*  
  Accepted (2026-06-20), guideline-level (ADR-080: payload shape is field-validated). Narrows ADR-140's **single-device** rule for **BLE probes only**: where ADR-140 §1 forbids `via_device` and §2 puts BLE sensors as `sat_`-prefixed entities INSIDE the one OTGW device, the shipped BLE discovery (`satBLEPublishOneDiscovery`, `MQTTstuff.ino:2963`/`:2989`, TASK-871/872, commit 7e3da75d) instead emits **each BLE probe as its own HA child-device** linked to the main OTGW device by `via_device`. The maintainer (2026-06-20) decided to KEEP this: a BLE probe is a physically separate wireless sensor with its own battery/RSSI/identity, so a per-probe child device linked by `via_device` is truer to the physical topology and lets HA group/track each probe independently. **All non-BLE entities (`esp_`/`pic_`/`otd_`/`sat_` engine, `sensors_`) stay inside the single device exactly as ADR-140 specifies.** Does NOT revive ADR-124's seven-device split (which stays superseded); the `via_device` target equals the main device's `identifiers` (`uniqueId` = `NodeId` = `settings.mqtt.sUniqueid`), verified by a 2026-06-20 live-broker capture (105 entities under one device id, zero `via_device` with no probes present). Amends ADR-140 (immutable); references ADR-077 (streaming discovery, untouched).

- **[ADR-136: Retire / Simplify the WiFi-Reconnect, Webhook, and PIC PR= State Machines (ADR-123 Phase 4)](ADR-136-retire-simplify-wifi-webhook-pic-state-machines.md)** 🆕 *(Supersedes ADR-048; Amends ADR-047)*  
  Accepted (2026-06-14). The Phase-4 cleanup ADR-123 §Rollout named: retire the manual non-blocking FSMs that existed only because the cooperative loop forbade blocking. The three are NOT uniformly redundant. **ADR-058 (PIC `PR=`) is satisfied with no code change** — it is already fire-and-forget (`addCommandToQueue(...,true)` + async `handlePRresponse()`), and that queue is now the PIC task's TX ingress (ADR-130); `queryNextPICsetting()`/`handlePRresponse()` are byte-for-byte unchanged. **ADR-047 (WiFi) is amended and RETAINED in the loop** — `loopWifi()` does not block (`WiFi.begin()` is async, the poll is sub-ms), so per ADR-123 §model-3 it stays in `loop()`; it hosts four pieces of real product behaviour that survive untouched (bounded-retry-then-`doRestart()`, BETA `_VERSION_PRERELEASE` captive-AP fallback, `NET_ETHERNET` preemption early-return, and the issue-#525/TASK-432 no-`platformRestartDHCP()` rule). **ADR-048 (webhook) is superseded** — the one genuine blocker (synchronous `HTTPClient`) moves onto a dedicated `webhook` FreeRTOS task that may block; `evalWebhook()` keeps loop-side `evalTriggerBit()` edge detection + latest-state-supersedes and enqueues a self-contained value-copy `WebhookJob`; the lock-free sender runs the 3-attempt/30s backoff as real `platformTaskDelay`, expands `{vars}` at enqueue time (loop context, or a brief `OTStateLock` for the AsyncTCP test endpoint) so it never holds `OTStateLock` across the HTTP round-trip. `testWebhook()` now enqueues instead of blocking the AsyncTCP task; `webhookInFlight` preserves ADR-057 §5 one-pending-at-a-time. ADR-057 re-validated (isLocalUrl per send, ADR-004 error path, ADR-056 test-endpoint boundary all hold).

- **[ADR-123: 2.0.0 Concurrency Model — FreeRTOS PIC Task + Event-Driven Async Networking (ESP32-S3 only, ESP8266 dropped)](ADR-123-2-0-0-concurrency-model-async-modernization.md)** 🆕 *(Accepted)*  
  Accepted (2026-06-12). The 2.0.0 concurrency model: a dedicated FreeRTOS PIC-serial task plus event-driven async networking on the ESP32-S3; ESP8266 dropped (depends on ADR-128). Umbrella for the phased async modernization (ADR-129/130/131/132/133/134).

- **[ADR-129: OT-Frame Queue and OTGWState Mutex Foundation (ADR-123 Phase 1)](ADR-129-ot-frame-queue-and-state-mutex-foundation.md)** 🆕  
  Accepted (2026-06-14). Phase 1 foundation: an OT-frame queue plus an OTGWState mutex so the PIC task and loop can share state safely. First primitive the later phases build on.

- **[ADR-131: MQTT Engine on espMqttClient: Async Connect, Single Publish Chokepoint (ADR-123 Phase 2)](ADR-131-mqtt-engine-espmqttclient-async.md)** 🆕 *(Supersedes ADR-108)*  
  Accepted (2026-06-14). Phase 2: moves the MQTT engine to espMqttClient with async connect and a single publish chokepoint. Supersedes ADR-108 (sync connect blocker) on the ESP32 side.

- **[ADR-144: Move the AsyncTCP service task to core 0 (amend ADR-139's core-1 pin)](ADR-144-asynctcp-task-core-affinity-move-to-core-0.md)** *(Rejected)*  
  Rejected (2026-06-18). Proposed moving the AsyncTCP service task from core 1 to core 0 (amend ADR-139) to fix an under-load TWDT reboot; rejected after on-device testing. Kept for the decision trail.

- **[ADR-109: ESP32 REST Response Coalescing Buffer](ADR-109-esp32-rest-response-coalescing-buffer.md)** *(Superseded by ADR-132)*  
  Coalescing buffer on the synchronous ESP32 WebServer to cut REST chunk latency; obsolete once ESP8266 was dropped and the web stack moved to ESPAsyncWebServer. Superseded by ADR-132.

### Hardware and Reliability

- **[ADR-011: External Hardware Watchdog for Reliability](ADR-011-external-hardware-watchdog.md)**  
  I2C hardware watchdog chip that automatically resets the ESP8266 if firmware hangs or crashes.

- **[ADR-012: PIC Firmware Upgrade via Web UI](ADR-012-pic-firmware-upgrade-via-web.md)**  
  Safe PIC microcontroller firmware flashing through the Web UI with WebSocket progress streaming.

- **[ADR-060: PIC Availability Guard Pattern](ADR-060-pic-availability-guard-pattern.md)** 🆕  
  Central `isPICEnabled()` guard that disables all PIC-dependent code paths when no PIC is detected, with auto-recovery via banner detection. Enables single firmware binary for both PIC and non-PIC hardware variants.

- **[ADR-061: Unified ESP8266/ESP32 Platform Abstraction](ADR-061-unified-esp8266-esp32-platform-abstraction.md)** 🆕  
  Single source codebase compiles for both ESP8266 (PIC-based OTGW) and ESP32 (OTGW32, OT-Direct, SAT) via `platform.h` / `platform_esp{8266,32}.h` shims; SDK differences encapsulated behind a thin abstraction.

- **[ADR-149: Accept the LWIP TCP-pcb Connection Ceiling on the ESP32-S3](ADR-149-accept-lwip-tcp-pcb-connection-ceiling-esp32s3.md)** 🆕 *(Accepted)*  
  Keep the application-level connection mitigations (WebSocket cap, heap reject, REST inflight gate) and accept that an adversarial concurrent-connection flood exhausts the 16-entry LWIP pcb pool and triggers a TWDT reset; do not raise `CONFIG_LWIP_MAX_ACTIVE_TCP`. Grounded in an alpha.227 on-device A/B/C experiment (TASK-884). Complements ADR-147.

- **[ADR-135: ESP32 TWDT is the Primary Watchdog, External 0x26 Chip Demoted to Optional Secondary (Amends ADR-011)](ADR-135-twdt-primary-watchdog-external-0x26-optional-secondary.md)** 🆕  
  Accepted (2026-06-14). The ESP32 TWDT becomes the primary watchdog; the external I2C 0x26 chip is demoted to an optional secondary. Amends ADR-011.

- **[ADR-147: ESP32-S3 platform limits for concurrent webui static-file serving — keep LittleFS, guard the file-serve path, adopt the AsyncTCP config block](ADR-147-esp32s3-platform-limits-concurrent-webui-static-serving.md)** 🆕  
  Accepted. Accepts the ESP32-S3 platform limits for concurrent webui static-file serving: keep LittleFS, guard the file-serve path, and adopt the AsyncTCP config block. Complements ADR-149.

- **[ADR-159: Symmetric Presence-Gating of the External 0x26 Watchdog (Amends ADR-135)](ADR-159-symmetric-0x26-watchdog-presence-gating.md)** 🆕 *(Accepted)*  
  Proposed, guideline-level (ADR-080). Presence-gates the external 0x26 watchdog symmetrically (feed/arm only when present) instead of the unconditional feed. Amends ADR-135.

- **[ADR-165: Optimal request parallelism on ESP32-S3 v2 Web UI/REST: N*=2 confirmed by two-phase load test (TASK-1015)](ADR-165-optimal-request-parallelism-esp32s3-webui-rest.md)** 🆕 *(Proposed)*  
  Proposed, not yet Accepted (maintainer sign-off pending). Documents the TASK-1015 two-phase load-test study (capacity curve + policy confirmation) on `esp32-classic`: N*=2 is the highest offered concurrency with zero incidents AND zero nominal-load 503s. Does NOT yet authorize changing the shipped `REST_MAX_INFLIGHT`/`WEB_FILE_MAX_INFLIGHT` defaults, adding a client `MAX_INFLIGHT` knob, or updating the CLAUDE.md single-flight rule. Complements ADR-149, ADR-147.

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

- **[ADR-152: Coexisting v2 Web UI selected by a device-wide setting](ADR-152-coexisting-v2-web-ui-device-wide-default-setting.md)** 🆕 *(Accepted)*  
  The full multi-page v2 Web UI redesign (Home with three concepts, Monitor, Settings, connectivity strip) ships **alongside** the classic `index.html` rather than replacing it; a device-wide setting (`settings.ui.bUseV2`) selects which UI is served, so the new design can be field-tested without forcing it on every user. The two asset sets (`v2.html`/`v2.css`/`v2.js` next to `index.html`/`index.js`/`components.css`) coexist on LittleFS. ADR-155 builds the connectivity model inside this v2 UI.

- **[ADR-155: v2 Web UI Connectivity Model — Two-Link OT Bus, MODE Separated from HEALTH, Five-State Vocabulary](ADR-155-v2-webui-connectivity-two-link-ot-bus.md)** 🆕 *(Accepted)*  
  Accepted (2026-06-26), structural (ADR-080: no automated gate). The v2 Web UI models the OT bus as two independent links (thermostat vs boiler) instead of a single `bOnline` flag, renders gateway MODE (gateway/monitor) as a separate blue chip never a green/red health light, and uses one five-state-plus-mode vocabulary (connected/degraded/disconnected/off/unknown) everywhere as colour+icon+text. Firmware `/api/v2/health` gains `thermostatconnected`/`boilerconnected`/`otcommandinterface`/`otgwmode` additively (mirroring `/device/info`); on OT-Direct hardware the UI falls back to `bOnline` for both links since OTDirect does not populate the sub-states. Implemented alpha.268/alpha.274 (TASK-933). Complements ADR-152 (v2 Web UI); REST/UI counterpart to ADR-084 (OT-bus state MQTT topics); depends on ADR-031 (two-MCU).

- **[ADR-091: Design-System Class Drift Gated by evaluate.py](ADR-091-design-system-class-drift-gated-by-evaluate-py.md)** 🆕  
  Promotes the design-system class-drift detector into `evaluate.py` so HTML/JS class names emitted without a matching `components.css`/`ds-tokens.css` selector fail the gate (WARN→FAIL, TASK-480).

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

- **[ADR-158: Combo board: add the LOLIN S3 Mini Pro as a third boot-detected Classic variant](ADR-158-combo-s3-mini-pro-classic-variant-boot-detection.md)** 🆕 *(Accepted)*  
  Accepted (2026-06-29). Adds the LOLIN S3 Mini Pro as a third boot-detected Classic variant on the combo board. Amends ADR-127; field verification on real hardware pending.

- **[ADR-160: Combo board: AUTO hardware detection re-probes every boot and never persists its verdict](ADR-160-combo-auto-board-detect-per-boot-no-persist.md)** *(Superseded by ADR-164)*  
  Superseded by ADR-164. Combo-board AUTO detection was specified to re-probe every boot and never persist its verdict; that never-persist rule was reverted to persist-once in shipped code (see ADR-164). Amends ADR-127.

- **[ADR-164: Combo AUTO board detection persists its PIC verdict once (supersedes ADR-160)](ADR-164-combo-auto-board-detect-persist-once-supersedes-adr160.md)** 🆕 *(Accepted)*  
  Accepted (2026-07-04). Combo AUTO detection persists its PIC verdict once (with a 3x `detectPIC` retry) instead of re-probing every boot; supersedes ADR-160's never-persist rule after it hung boot on-device and was reverted to persist-once. Related to ADR-126, ADR-127, ADR-158.

- **[ADR-113: Hardware-Type as Codepath-Selection Contract](ADR-113-hardware-type-codepath-selection-contract.md)** 🆕  
  Guideline-level (ADR-080). `hardware_type` selects code paths (deprecating `picavailable`); amended by ADR-125 so the combo board resolves it at boot detection (runtime).

- **[ADR-114: OLED Runtime Detection Decoupled from the Board Flag](ADR-114-oled-runtime-detection-decoupled-from-board-flag.md)** 🆕  
  Guideline-level (ADR-080). Removes the `HAS_OLED_CAPABLE` compile gate around the OLED path in favour of a runtime I2C probe, so one binary drives boards with or without an OLED.

- **[ADR-120: Platform Abstraction Headers Promoted to the src/libraries/Platform Library](ADR-120-platform-abstraction-promoted-to-library.md)** 🆕  
  Moves the `platform*.h` abstraction headers into a dedicated `src/libraries/Platform` library; extends ADR-061/ADR-115's abstraction-file organization.

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

- **[ADR-150: SAT Per-Heating-System COLD_SETPOINT (Active Boiler-Off Cutoff on Low Demand)](ADR-150-sat-per-heating-system-cold-setpoint-boiler-off-cutoff.md)** 🆕  
  *Accepted.* When SAT is the active controller and the requested setpoint falls below a per-heating-system cutoff (radiators 28.2C, underfloor 21C), SAT commands the boiler off (CS=10, CH=0, MM=100) and gates the PWM auto-switch and flame-off hold. Departs from the prior pure-pass-through stance while SAT is enabled; the SAT-disabled handover (CS=0 to thermostat) is unchanged. Supersedes the deliberate-omission code comment; ports thermo-nova `heating_control.py`.

- **[ADR-153: SAT BLE Adds Plaintext Xiaomi MiBeacon (0xFE95); Encrypted Deferred to a Gated Phase 2](ADR-153-sat-ble-mibeacon-plaintext-defer-encrypted.md)** 🆕  
  Accepted (TASK-930). The SAT BLE roster (`SATble.ino`) gains a third advertisement decoder for plaintext Xiaomi MiBeacon (`0xFE95`) alongside the existing ATC `0x181A` and BTHome `0xFCD2` paths, so stock Mijia/LYWSD03MMC sensors are ingested without a custom firmware flash. Encrypted MiBeacon (which needs a new mbedtls dependency plus per-sensor secret storage) is explicitly deferred to a gated Phase 2 (ADR-154).

- **[ADR-154: Encrypted Xiaomi MiBeacon (v4/v5) via mbedtls AES-CCM with Per-Roster-Slot Bindkey Secret Storage](ADR-154-mibeacon-encrypted-aesccm-per-slot-bindkey.md)** 🆕 *(Phase 2 of ADR-153)*  
  Accepted (TASK-930 Phase 2). Decrypts encrypted MiBeacon v4/v5 frames using mbedtls AES-CCM with a per-roster-slot bindkey stored as a device secret, so stock LYWSD03MMC sensors at default (encrypted) settings are decoded. Adds the mbedtls dependency and the per-slot secret storage that ADR-153 deferred; the plaintext path stays the fallback.

- **[ADR-151: BLE Name Capture Stays on the Passive-Continuous Scan (Active-Scan Burst Rejected)](ADR-151-ble-hybrid-passive-continuous-with-on-demand-active-burst.md)** 🆕 *(Accepted)*  
  Accepted, guideline-level (ADR-080). BLE name capture stays on the passive-continuous scan; an on-demand active-scan burst was evaluated and rejected as a runtime radio characteristic, not a code gate.

- **[ADR-161: Dedicated REST Endpoint for the SAT BLE Sensor Roster with a Write-Only Per-Slot Secret](ADR-161-ble-roster-rest-endpoint-write-only-secret.md)** 🆕 *(Accepted)*  
  Proposed, guideline-level (ADR-080). Dedicated REST endpoint for the SAT BLE sensor roster with a write-only per-slot secret; establishes the indexed-collection endpoint and write-only-secret contract patterns.

- **[ADR-162: Sanctioned SAT Force-Boiler Test Hook in Production Firmware](ADR-162-sat-force-boiler-test-hook.md)** 🆕 *(Accepted)*  
  Proposed. A sanctioned SAT force-boiler test hook shipped in production firmware for bench/emulator validation (option A from the SAT sim options analysis).

- **[ADR-092: Adopt NimBLE-Arduino over Bluedroid for SAT BLE Scanner](ADR-092-adopt-nimble-arduino-over-bluedroid-for-sat-ble-scanner.md)** 🆕  
  Switches the SAT BLE temperature-sensor scanner from the Bluedroid-based Arduino BLE library to NimBLE-Arduino, saving roughly 400 KB of flash on the ESP32-S3.

- **[ADR-110: SAT PV-Surplus Setpoint Boost](ADR-110-sat-pv-surplus-setpoint-boost.md)** 🆕  
  Adds a solar-PV-surplus setpoint boost to the SAT controller (TASK-640).

- **[ADR-111: SAT MQTT Publish — On-Change + Jittered Heartbeat](ADR-111-sat-mqtt-publish-on-change-and-jittered-heartbeat.md)** 🆕  
  Replaces unconditional per-cycle publishing of the ~74 `sat/*` topics with change-detection plus a jittered heartbeat to cut MQTT chatter.

- **[ADR-117: SAT Simulation Contract — Bus Isolation, Boiler-Absence Availability, Command Trace](ADR-117-sat-simulation-contract.md)** 🆕  
  Defines the SAT simulation-mode contract: OT-bus isolation, boiler-absence availability handling, and a command trace for bench/emulator validation.

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
