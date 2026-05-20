-----

id: TASK-636
title: ‘Modbus TCP client for heat pump integration (ESP32-only, experimental)’
status: To Do
assignee: []
created_date: ‘2026-05-20 17:10’
updated_date: ‘2026-05-20 17:25’
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
- TASK-635
  priority: low

-----

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->

OTGW-firmware is OpenTherm-centric by design, and that scope is deliberate. However, the heat pump market is moving to Modbus as the primary control and telemetry protocol, while OpenTherm coverage for heat pumps remains thin (cooling msgIDs are present, but COP/defrost/compressor data and write access to setpoints often only exist on Modbus registers). This creates a coverage gap for users running modern heat pumps (Ochsner, Kronoterm, NIBE, Mitsubishi, Daikin Altherma, etc.) who still want OTGW + SAT as their control intelligence layer.

This task adds a minimal, optional, ESP32-only Modbus TCP client that polls a configurable register map from a heat pump on the local network. The client publishes register values to MQTT (with HA auto-discovery) and exposes a small write surface for setpoint changes. RTU/RS485 is deliberately out of scope for this iteration: it requires hardware (MAX485 transceiver, wiring) and substantially expands testing surface. TCP-only keeps the scope to “another network client”, same as MQTT or NTP.

The feature is positioned as **experimental and opt-in**:

- Disabled by default
- ESP32 only (compile-time excluded on ESP8266 due to memory pressure and lack of strategic value)
- Vendor-agnostic: users supply their own register map via a JSON config file on LittleFS
- No vendor-specific code in firmware; vendor profiles live as community-contributed JSON files in `docs/features/modbus-profiles/`
- Polling read + small write surface only; no flow control, no master mode, no scan/discovery

The strategic intent is to **acknowledge the heat-pump direction without committing to heat-pump-first redesign**. OpenTherm remains the primary control path. Modbus is a side channel for telemetry and limited write access on systems where OpenTherm coverage is insufficient. Users keep SAT and OTGW for control intelligence and OpenTherm boilers/heat pumps; Modbus closes the gap for the non-OpenTherm devices in their setup.

This task does **not** wire Modbus into SAT’s control loop. SAT continues to operate purely on OpenTherm and external sensor inputs. A future task may introduce Modbus-derived inputs into SAT (e.g. heat pump outdoor unit temperature as outdoor sensor source), but that decision is deferred until this base feature is mature.

Pairs naturally with TASK-635 (PV-surplus boost): a typical user flow becomes “P1 meter → HA → MQTT publishes pv_surplus_w to SAT → SAT boosts target → heat pump runs harder on PV”, while OTGW separately polls the heat pump via Modbus for visibility and exposes a “DHW setpoint boost on surplus” write that HA can trigger.

<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria

<!-- AC:BEGIN -->

- [ ] #1 New compile-time guard: entire Modbus subsystem is wrapped in `#if defined(ESP32)`. ESP8266 build produces identical binary as without this task (verify via flash/RAM size diff)
- [ ] #2 New settings in a dedicated `ModbusSection` struct (separate from SAT): `bEnabled` (bool, default false), `sHost` (char[64]), `iPort` (uint16, default 502), `iUnitId` (uint8, default 1, range 0-247), `iPollIntervalS` (uint16, default 30, range 5-3600), `iTimeoutMs` (uint16, default 1000, range 100-5000), `iMaxConsecutiveFailures` (uint8, default 5, range 1-20)
- [ ] #3 Register map loaded from `/modbus.json` on LittleFS at boot. JSON schema documented in `docs/features/modbus-tcp-client.md`. Schema fields per register: `name` (string, MQTT topic suffix), `address` (uint16), `type` (string: u16/s16/u32/s32/float/bool), `byte_order` (string: AB/BA/ABCD/BADC/CDAB/DCBA, default AB for 16-bit), `function` (string: holding/input, default holding), `scale` (float, default 1.0), `offset` (float, default 0.0), `unit` (string, for HA discovery), `device_class` (string, optional, for HA discovery), `writable` (bool, default false)
- [ ] #4 Register map limits: maximum 32 registers per map. JSON parse failure (missing file, syntax error, exceeded limit) → Modbus subsystem stays disabled at runtime with clear error in debug log, MQTT publishes `modbus/status = config_error`. No crash, no boot loop
- [ ] #5 Modbus library selection: use `eModbus` (https://github.com/eModbus/eModbus) or `emelianov/modbus-esp8266` (despite the name, supports ESP32). Decision documented in ADR (see AC #18). Choose based on: maintenance activity, support for TCP client mode, license compatibility (MIT/BSD), code size on ESP32
- [ ] #6 Polling loop: ESP32 task (separate FreeRTOS task with stack ~4KB) polls all configured registers at `iPollIntervalS` interval. Connection lifecycle: open TCP connection at start of poll cycle, perform reads in one batch (consecutive registers grouped where possible), close connection. Persistent connections deferred to future task to keep first iteration simple
- [ ] #7 Connection failure handling: after `iMaxConsecutiveFailures` consecutive failed polls, mark Modbus subsystem as failed, publish `modbus/status = failed`, back off to retry every 5 minutes. Successful poll resets failure counter and clears failed state
- [ ] #8 MQTT subscribe for writable registers: `set/<nodeId>/modbus/<register_name>` accepts numeric payload. Written via Modbus function code 6 (write single holding register) or 16 (write multiple, for 32-bit values). Write result published to `modbus/<register_name>/write_result` (`ok` / `failed` / `not_writable` / `range_error`)
- [ ] #9 MQTT publish in a new `modbusPublishMQTT()` function: each register published as `<topPrefix>/value/<nodeId>/modbus/<register_name>` with the scaled+offset value. Publish on change + heartbeat per existing publish-interval setting (reuse the MQTT publish gating logic, do not invent new gating)
- [ ] #10 MQTT status topics: `modbus/status` (disabled/config_error/connecting/ok/failed), `modbus/last_poll_ms` (ms since last successful poll), `modbus/consecutive_failures` (count)
- [ ] #11 REST API: `GET /api/v2/modbus/status` returns full subsystem state (enabled, host, port, status, last_poll_ms, consecutive_failures, register count). `GET /api/v2/modbus/registers` returns all current register values as JSON map. `POST /api/v2/modbus/register/<name>` writes a value to a writable register
- [ ] #12 REST API: `GET /api/v2/modbus/config` returns the loaded register map JSON. `POST /api/v2/modbus/config` accepts a new JSON map, validates it, persists to LittleFS, reloads. Validation enforces the 32-register limit and schema correctness
- [ ] #13 HA auto-discovery: each register published as a `sensor` (or `binary_sensor` for type=bool). Writable registers additionally exposed as `number` entities with unit/device_class from the register definition. Subsystem status as `sensor`
- [ ] #14 WebUI: new “Modbus” page (only visible on ESP32 builds), with two sections: (a) connection settings (enable, host, port, unit_id, poll_interval, timeout), (b) register map editor — read-only view of current map, file-upload button to replace `/modbus.json`, link to documentation. No inline JSON editor in first iteration (keep complexity low; users edit the file locally and upload). Status panel shows connection state, last poll time, failure count, and a per-register value table updated via REST poll
- [ ] #15 OpenAPI 3.0 spec updated with the new endpoints
- [ ] #16 Documentation: new file `docs/features/modbus-tcp-client.md` with: schema for `modbus.json`, byte-order explanation (the classic Modbus byte-order trap), polling/connection model, security considerations (Modbus TCP is unauthenticated; trust boundary is the LAN), supported data types, worked example for a generic heat pump (outdoor temp, flow temp, compressor power, DHW setpoint write)
- [ ] #17 Community register map collection: new directory `docs/features/modbus-profiles/` with `README.md` explaining the contribution flow. Seeded with one or two example profiles labeled clearly as “community-contributed, not vendor-endorsed, use at your own risk” (Ochsner Europa example if a public register doc is available; otherwise a generic placeholder). License of profile JSON files: MIT, contributors warrant they have the right to share
- [ ] #18 ADR: new ADR (next free number after TASK-635’s ADR-104) “Modbus TCP client as optional ESP32 side channel” documenting: why Modbus is added at all given OpenTherm-first scope, why TCP-only and not RTU, why vendor-agnostic register map and not vendor-specific firmware code, why no SAT integration in this iteration, library choice rationale (per AC #5), security model (LAN-only, no auth)
- [ ] #19 NL manual chapter `docs/manuals/nl/h11-modbus-tcp.md` (next free chapter number) and EN equivalent describing the feature, configuration steps, and the heat pump use case. Both manuals clearly mark the feature as experimental
- [ ] #20 Default behavior: with `bEnabled = false` (the default), the Modbus task is not started, no FreeRTOS task created, no LittleFS file read, no MQTT topics published. Existing users see no change after upgrade. Resource impact when disabled: zero runtime cost
- [ ] #21 Resource budget when enabled: total RAM footprint (task stack + register state + connection buffer) < 12KB on ESP32. Flash impact < 50KB. Validate before merge
- [ ] #22 Safety: no register write occurs without explicit user action (MQTT publish, REST call, or WebUI button). No autonomous writes from firmware logic in this iteration. Specifically, SAT may NOT write to Modbus registers in this task (deferred to future task)

<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->

1. **Library evaluation spike** (½ day): compare eModbus vs emelianov/modbus-esp8266 on ESP32. Build small test sketch with each, measure flash/RAM footprint, test against a heat pump simulator (e.g. `diagslave` from libmodbus, or a Modbus TCP test server in Python). Document choice in ADR per AC #18
1. **Add ModbusSection struct** in `OTGW-firmware.h` (or a new `Modbustypes.h` if struct sizes warrant separation, consistent with the SATtypes.h pattern). Wrap in `#if defined(ESP32)`. Add settings persistence in `settingStuff.ino`
1. **Implement JSON register map loader** in new file `src/OTGW-firmware/Modbusconfig.ino`. Parse `/modbus.json` from LittleFS, validate schema, build in-memory register table. Handle all four failure modes (file missing, parse error, schema violation, register count exceeded) with clear status reporting
1. **Implement polling task** in new file `src/OTGW-firmware/Modbusclient.ino`. Create FreeRTOS task on ESP32 with `xTaskCreate`, stack 4KB, priority below WiFi. Task body: sleep `iPollIntervalS`, connect, read registers, close, publish via existing MQTT pipeline. Use FreeRTOS notifications for write requests so writes don’t block on the poll cycle
1. **Implement byte-order decoder** for the six byte-order modes per AC #3. This is where 80% of Modbus integration bugs live; cover with unit tests in `tests/` if test infrastructure permits
1. **MQTT subscribe** in `MQTTstuff.ino`: add handler for `set/<nodeId>/modbus/<register_name>` topic pattern. Look up register by name in the loaded map, validate writability, range-check, dispatch write via FreeRTOS notification to the Modbus task
1. **MQTT publish** in a new `modbusPublishMQTT()` function called from the Modbus task after a successful poll. Use the existing publish-interval gating (do not duplicate)
1. **REST endpoints** in `restAPI.ino`: five new endpoints per AC #11 and #12. Config upload reuses the existing LittleFS upload pattern (with size check)
1. **HA auto-discovery**: extend the existing discovery framework with Modbus entities. Discovery is published once at subsystem-start and on register map reload. Honor the existing retained-discovery orphan cleanup (ADR-093) when registers are removed from the map
1. **WebUI** new “Modbus” page: conditional render based on `/api/v2/device/info` reporting ESP32 platform. File upload via existing file-manager primitives. Status panel polls `/api/v2/modbus/status` every 5s
1. **OpenAPI spec, ADR, EN+NL manuals**
1. **Sample register map** (`docs/features/modbus-profiles/example-generic-heatpump.json`) with 6-8 commonly-exposed registers (outdoor temp, flow temp, return temp, DHW temp, compressor state, COP if available, DHW setpoint as writable)
1. **Manual testing**: against a Modbus TCP simulator first (Python `pymodbus` server), then against at least one real heat pump from the community (Ochsner or Kronoterm via volunteer testing). Validate the seven test scenarios in the notes below

<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->

**Why this is a separate task from TASK-635**

TASK-635 (PV-surplus boost) is purely additive to existing SAT external-input handlers; it changes target-temperature read sites and that’s it. This task introduces a whole new subsystem with its own networking, persistence, MQTT topic tree, HA discovery, and WebUI page. Mixing them would conflate two scope decisions: (1) “should SAT respond to PV?” and (2) “should OTGW speak Modbus?”. The answer to (1) is clearly yes; the answer to (2) is “carefully, optionally, with clear boundaries”. Keep them separate.

**The TASK-635 dependency declared above**

Soft dependency: TASK-635 should land first because it introduces the user-facing concept “OTGW can act on external energy signals” with a low-risk feature. Once that pattern is accepted, this task extends it. Hard dependency would be wrong; technically these can be implemented in either order.

**Why not Modbus RTU in this iteration**

RTU requires: MAX485 transceiver wiring, GPIO pin allocation conflicts with OTDirect/OLED/etc., termination resistor decisions, baud rate / parity / stop bit configuration, half-duplex timing on the RS485 bus. Each is solvable but compounds the testing surface dramatically. TCP is “another TCP client like MQTT” and ships in weeks; RTU is a hardware feature and ships in months. RTU can be a follow-up task once the TCP path is proven.

**Why vendor-agnostic register maps (not vendor-specific code)**

A heat-pump manufacturer’s register map can have 200+ registers, change between firmware versions, and be partially undocumented. Hardcoding per-vendor logic in firmware creates an endless maintenance tail. JSON profiles in `docs/features/modbus-profiles/` let the community own vendor-specific knowledge, while firmware stays the engine. Same model as e.g. Tasmota templates.

**Why no SAT-to-Modbus write in this task**

If SAT writes setpoints to a heat pump, you have two control authorities (OpenTherm + Modbus) potentially fighting each other on the same physical device. That requires a coordination model (which authority wins when, what happens on Modbus failure, how does PIC OT command queue interact with the Modbus task). That’s a real design problem that deserves its own task with its own ADR. This iteration deliberately keeps the Modbus subsystem read-mostly + user-triggered-write, which has no control-loop semantics.

**Security model**

Modbus TCP has no authentication. Anyone on the LAN can read and write registers. This is the protocol’s design, not a configuration choice. Document this clearly: do not enable on networks where untrusted devices are present. The firmware’s role is to be transparent about this, not to add an auth layer (would not interop with any actual heat pump). For users on the existing OTGW security model (LAN-trusted, no internet exposure), this is consistent.

**Test scenarios**

1. Heat pump online, register map loads, all registers polled successfully → MQTT topics populate, HA entities appear
1. Heat pump goes offline mid-day → failure counter increments, after 5 failures status flips to `failed`, retry every 5 min, recovery is automatic when device comes back
1. Bad `modbus.json` (syntax error) → subsystem stays disabled, clear error in debug log and `modbus/status`, no boot loop
1. Register map exceeds 32 registers → load fails with `config_error`
1. Write to a writable register via MQTT → register updates on device, value re-read on next poll matches
1. Write to a non-writable register → `not_writable` response, no Modbus write attempted
1. Subsystem disabled (default) → zero new MQTT topics, zero new FreeRTOS tasks, identical behavior to v2.0.0 without this task
1. Replace register map via WebUI upload → old HA entities cleaned up via retained-discovery orphan cleanup, new entities appear

**Library choice prediction (subject to AC #5 spike)**

`eModbus` looks more actively maintained (commits in 2025-2026, async architecture, TCP and RTU support) but has a larger footprint. `emelianov/modbus-esp8266` is smaller and simpler but the maintenance velocity is lower. My current expectation is eModbus wins on long-term viability, modbus-esp8266 wins on minimal-footprint. Decide on the spike, document in ADR.

**Relationship to OTDirect**

OTDirect uses GPIO pins for OpenTherm bus communication on ESP32. Modbus TCP uses no GPIOs (it’s pure network). So there’s no GPIO conflict. If a future RTU follow-up task happens, that DOES conflict with OTDirect on GPIO budget, and the priority will need to be: OTDirect wins because it’s core to the product.

**What this task explicitly does not solve**

- Native PV current/power measurement (out of scope, see TASK-635 notes — users supply via HA)
- SAT-controlled heat pump setpoint writes (deferred to future task, requires coordination ADR)
- Modbus RTU over RS485 (deferred, hardware feature)
- Modbus slave/server mode (different use case, deferred)
- Vendor-specific protocol extensions beyond raw Modbus (e.g. NIBE’s modbus40 framing, KNX-over-Modbus) — community territory

**Positioning in release notes**

This is an experimental opt-in feature **targeted at v2.1.0, deferred from v2.0.0** to keep the v2.0.0 scope focused on stabilizing the major platform changes already in flight (ESP32/OTGW32 support, OTDirect, BLE, Ethernet, SAT embedded port, AP fallback). v2.0.0 release notes should mention this work as “planned for v2.1.0” only if a roadmap section is added; otherwise no mention.

When this lands in v2.1.0, the release-notes message must be carefully framed: “OTGW does Modbus” must not overshadow “OTGW does OpenTherm + SAT”. One sentence in the release notes, more depth in the feature documentation page, and explicit “experimental” framing throughout.

**Rationale for deferral**

Three reasons to defer rather than rush into v2.0.0:

1. v2.0.0’s test/validation surface is already large (two MCUs, two OT paths, BLE, Ethernet, SAT, OLED, AP fallback). Adding a new networking subsystem before v2.0.0 ships would amplify launch risk for a feature that benefits a minority of users.
1. The library-evaluation spike (AC #5) needs to complete before any implementation, and the result shapes the rest of the design. Doing this under v2.0.0 timeline pressure forces a suboptimal choice.
1. TASK-635 (PV-surplus boost) is the better v2.0.0 candidate from the same feature family: smaller surface, no new dependencies, demonstrates the “OTGW responds to external energy signals” pattern. Landing it first sets the narrative; this task extends it later.

The dependency on TASK-635 is now both narrative and practical: TASK-635 in v2.0.0, this in v2.1.0.<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->

<!-- to be filled on completion -->

<!-- SECTION:FINAL_SUMMARY:END -->