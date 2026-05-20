---
id: TASK-641
title: 'Modbus TCP client for heat pump integration (ESP32-only, experimental)'
status: To Do
assignee: []
created_date: '2026-05-20 18:31'
labels:
  - feature
  - esp32-only
  - experimental
  - modbus
  - heatpump
  - energy
  - milestone-v2.1.0
  - deferred-from-v2.0.0
dependencies:
  - TASK-640
priority: low
ordinal: 44000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTGW-firmware is OpenTherm-centric by design, and that scope is deliberate. However, the heat pump market is moving to Modbus as the primary control and telemetry protocol, while OpenTherm coverage for heat pumps remains thin. This task adds a minimal, optional, ESP32-only Modbus TCP client that polls a configurable register map from a heat pump on the local network. The client publishes register values to MQTT (with HA auto-discovery) and exposes a small write surface for setpoint changes. RTU/RS485 is deliberately out of scope for this iteration: it requires hardware (MAX485 transceiver, wiring) and substantially expands testing surface. TCP-only keeps the scope to "another network client", same as MQTT or NTP.

The feature is positioned as experimental and opt-in:
- Disabled by default
- ESP32 only (compile-time excluded on ESP8266 due to memory pressure)
- Vendor-agnostic: users supply their own register map via a JSON config file on LittleFS
- No vendor-specific code in firmware; vendor profiles live as community-contributed JSON files in docs/features/modbus-profiles/
- Polling read + small write surface only; no flow control, no master mode, no scan/discovery

The strategic intent is to acknowledge the heat-pump direction without committing to heat-pump-first redesign. OpenTherm remains the primary control path. Modbus is a side channel for telemetry and limited write access on systems where OpenTherm coverage is insufficient.

This task does NOT wire Modbus into SAT's control loop. SAT continues to operate purely on OpenTherm and external sensor inputs. A future task may introduce Modbus-derived inputs into SAT, but that decision is deferred until this base feature is mature.

Pairs with TASK-640 (PV-surplus boost): typical user flow becomes "P1 meter → HA → MQTT publishes pv_surplus_w to SAT → SAT boosts target → heat pump runs harder on PV", while OTGW separately polls the heat pump via Modbus for visibility and exposes a "DHW setpoint boost on surplus" write that HA can trigger.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New compile-time guard: entire Modbus subsystem is wrapped in #if defined(ESP32). ESP8266 build produces identical binary as without this task (verify via flash/RAM size diff)
- [ ] #2 New settings in a dedicated ModbusSection struct (separate from SAT): bEnabled (bool, default false), sHost (char[64]), iPort (uint16, default 502), iUnitId (uint8, default 1, range 0-247), iPollIntervalS (uint16, default 30, range 5-3600), iTimeoutMs (uint16, default 1000, range 100-5000), iMaxConsecutiveFailures (uint8, default 5, range 1-20)
- [ ] #3 Register map loaded from /modbus.json on LittleFS at boot. JSON schema documented in docs/features/modbus-tcp-client.md. Schema fields per register: name, address, type (u16/s16/u32/s32/float/bool), byte_order (AB/BA/ABCD/BADC/CDAB/DCBA, default AB), function (holding/input, default holding), scale (default 1.0), offset (default 0.0), unit, device_class (optional), writable (default false)
- [ ] #4 Register map limits: maximum 32 registers per map. JSON parse failure -> Modbus subsystem stays disabled at runtime with clear error in debug log, MQTT publishes modbus/status = config_error. No crash, no boot loop
- [ ] #5 Modbus library selection: use eModbus or emelianov/modbus-esp8266 (despite the name, supports ESP32). Decision documented in ADR (see AC #18). Choose based on: maintenance activity, TCP client support, license (MIT/BSD), code size on ESP32
- [ ] #6 Polling loop: ESP32 task (separate FreeRTOS task, stack ~4KB) polls all configured registers at iPollIntervalS interval. Connection lifecycle: open at start of poll cycle, read in batches, close. Persistent connections deferred to future task
- [ ] #7 Connection failure handling: after iMaxConsecutiveFailures consecutive failed polls, mark subsystem failed, publish modbus/status = failed, back off to 5-minute retry. Successful poll resets failure counter
- [ ] #8 MQTT subscribe for writable registers: set/<nodeId>/modbus/<register_name> accepts numeric payload. Written via function code 6 or 16. Result published to modbus/<register_name>/write_result (ok / failed / not_writable / range_error)
- [ ] #9 MQTT publish in new modbusPublishMQTT() function: each register as <topPrefix>/value/<nodeId>/modbus/<register_name> with scaled+offset value. Publish on change + heartbeat per existing publish-interval setting (reuse gating, do not invent new)
- [ ] #10 MQTT status topics: modbus/status (disabled/config_error/connecting/ok/failed), modbus/last_poll_ms, modbus/consecutive_failures
- [ ] #11 REST API: GET /api/v2/modbus/status, GET /api/v2/modbus/registers (all current values as JSON map), POST /api/v2/modbus/register/<name> (write)
- [ ] #12 REST API: GET /api/v2/modbus/config returns loaded map JSON. POST /api/v2/modbus/config accepts new JSON map, validates, persists, reloads
- [ ] #13 HA auto-discovery: each register as sensor (or binary_sensor for type=bool). Writable registers additionally as number entities with unit/device_class. Subsystem status as sensor
- [ ] #14 WebUI: new 'Modbus' page (visible only on ESP32 builds), with connection settings section and register map editor (read-only view + file upload). Status panel shows connection state, last poll, failures, per-register value table updated via REST poll
- [ ] #15 OpenAPI 3.0 spec updated with new endpoints
- [ ] #16 Documentation: new file docs/features/modbus-tcp-client.md with schema for modbus.json, byte-order explanation, polling/connection model, security considerations (LAN-only, no auth), supported data types, worked example for generic heat pump
- [ ] #17 Community register map collection: new directory docs/features/modbus-profiles/ with README.md explaining contribution flow. Seeded with one example profile labeled 'community-contributed, not vendor-endorsed'. License of profile JSON files: MIT
- [ ] #18 ADR: new ADR (next free number after TASK-640's ADR-104) 'Modbus TCP client as optional ESP32 side channel' documenting: why Modbus given OpenTherm-first scope, why TCP not RTU, why vendor-agnostic register map not vendor-specific code, why no SAT integration this iteration, library choice rationale, security model
- [ ] #19 NL manual chapter docs/manuals/nl/h11-modbus-tcp.md (or next free chapter number) and EN equivalent describing feature, configuration steps, heat pump use case. Both manuals clearly mark as experimental
- [ ] #20 Default behavior: with bEnabled = false (default), Modbus task not started, no FreeRTOS task, no LittleFS file read, no MQTT topics published. Zero runtime cost when disabled
- [ ] #21 Resource budget when enabled: total RAM footprint (task stack + register state + connection buffer) < 12KB on ESP32. Flash impact < 50KB. Validate before merge
- [ ] #22 Safety: no register write occurs without explicit user action (MQTT publish, REST call, WebUI button). No autonomous writes from firmware logic. SAT may NOT write to Modbus registers in this task (deferred to future task)
<!-- AC:END -->
