# OT-Thing-OTGW32 Feature Parity — OTGW-firmware 2.0.0

**Audit date:** 2026-05-08  
**Audit scope:** Full 5-domain comparison of OT-Thing-OTGW32 (Seegel Systeme) vs OTGW-firmware 2.0.0  
**Verdict:** OTGW-firmware 2.0.0 is a comprehensive superset of OT-Thing across all domains.

---

## How to read this document

Each domain section lists:
- **COVERED** — equivalent or better feature present in 2.0.0
- **GAP** — feature in OT-Thing not yet in 2.0.0 (linked to backlog task)
- **ADVANTAGE** — feature in 2.0.0 that OT-Thing does not have
- **BOTH GAP** — absent in both projects

---

## Domain 1: MQTT + HA Discovery

| Feature | Status | Notes |
|---------|--------|-------|
| Core control topics (outsideTemp, chSetTemp, dhwSetTemp, modes) | COVERED | Full parity |
| MQTT status / LWT | COVERED | Full parity |
| HA discovery sensor count | ADVANTAGE | 2.0.0: 385 sensors vs OT-Thing: ~10 |
| HA discovery binary sensors | ADVANTAGE | 2.0.0: 58 explicit vs OT-Thing: ~16 implicit |
| HA discovery metadata (icons, entity_category, templates) | ADVANTAGE | 2.0.0 far richer |
| Streaming memory model (no ArduinoJson heap pressure) | ADVANTAGE | 2.0.0 PROGMEM + chunked publish |
| Discovery verification system (ADR-062) | ADVANTAGE | OT-Thing has none |
| SAT MQTT commands (12+ switches, 40+ sensors) | ADVANTAGE | OT-Thing pre-dates SAT |
| OTDirect MQTT commands | ADVANTAGE | OT-Thing: no OT-Direct concept |
| Ventilation/VH sensor discovery (30+ sensors) | ADVANTAGE | OT-Thing: not covered |
| Dallas 1-Wire HA discovery | ADVANTAGE | OT-Thing: not covered |
| S0 meter HA discovery | ADVANTAGE | OT-Thing: not covered |
| Firmware diagnostics topics (heap, drops, reboots) | ADVANTAGE | OT-Thing: not covered |
| Bypass control topics (openBypass, autoBypass, freeVentEnable) | BOTH GAP | Stubbed in OT-Thing; absent in 2.0.0 |

---

## Domain 2: OT Protocol + Control

| Feature | Status | Notes |
|---------|--------|-------|
| OpenTherm MsgID polling (all essential IDs) | COVERED | 2.0.0 polls ~60 additional IDs beyond OT-Thing |
| Status polling at 800ms (MsgID 0) | COVERED | Parity |
| CH/DHW setpoint writes | COVERED | 2.0.0: uniform 15s; OT-Thing: scattered 10-30s |
| MASTER / BYPASS / LOOPBACK / REPEATER modes | COVERED | Full parity |
| GATEWAY mode | ADVANTAGE | Not in OT-Thing |
| MONITOR mode (read-only passive) | ADVANTAGE | Not in OT-Thing |
| 3-strike UNKNOWN_DATA_ID auto-blacklist | ADVANTAGE | OT-Thing ignores unknown IDs |
| UI/KI operator blacklist control | ADVANTAGE | Not in OT-Thing |
| TT=/TC= remote override state machine | ADVANTAGE | Not in OT-Thing |
| DHW push emergency heating state machine | ADVANTAGE | Not in OT-Thing |
| Flame ratio tracking (180-min window) | COVERED | Parity |
| Thermostat disconnect + setback | COVERED | Parity (OTDirect adds more than OT-Thing) |
| Ventilation polling interval | GAP — TASK-583 | OT-Thing: 10s when vent active; 2.0.0: 60s |
| OTDirect hysteresis + CH suspension | GAP — TASK-582 | OT-Thing: per-channel deadband; 2.0.0: none in OTDirect |
| Room temperature low-pass filtering in PI loop | GAP (minor) | OT-Thing: 0.1/0.9 filter; 2.0.0 OTDirect: none |
| Ventilation override persistence | GAP — TASK-584 | OT-Thing persists; 2.0.0 runtime-only |

---

## Domain 3: UI + Web Portal

| Feature | Status | Notes |
|---------|--------|-------|
| Device status page | COVERED | 2.0.0 equivalent |
| Live OT log viewer | COVERED | 2.0.0 has streaming OT Monitor tab |
| OpenTherm statistics (sortable table) | ADVANTAGE | Not in OT-Thing |
| OpenTherm graph (time-window chart) | ADVANTAGE | Not in OT-Thing |
| SAT dashboard (full thermostat control) | ADVANTAGE | Not in OT-Thing |
| File system explorer | ADVANTAGE | Not in OT-Thing |
| Crash log viewer | ADVANTAGE | Not in OT-Thing |
| Multi-zone / area control | ADVANTAGE | Not in OT-Thing |
| BLE sensor roster (discovery, labels, forget) | ADVANTAGE | Not in OT-Thing |
| REST API scope | ADVANTAGE | 2.0.0: 50+ endpoints; OT-Thing: 13 |
| WiFi network scan REST + UI dropdown | GAP — TASK-585 | OT-Thing: /scan endpoint; 2.0.0: type SSID manually |
| Heating curve marker system | GAP — TASK-586 | OT-Thing: calibration points on graph; 2.0.0: none |
| MQTT topic listing endpoint | GAP (minor) | OT-Thing: /topics; 2.0.0: not exposed |
| Two-step OTA (check + install) | GAP (minor) | OT-Thing: GitHub API check; 2.0.0: manual upload |

---

## Domain 4: Sensors + BLE + SAT

| Feature | Status | Notes |
|---------|--------|-------|
| BLE BTHome v2 (UUID 0xFCD2) | COVERED | Full parity |
| BLE ATC/pvvx (UUID 0x181A) | ADVANTAGE | Not in OT-Thing |
| BLE scan config (100ms/50ms, continuous) | COVERED | Identical params, 2.0.0 uses NimBLE 2.x |
| DS18B20 1-Wire support | COVERED | 2.0.0 adds configurable GPIO, web labels, simulation |
| Multi-source room temp priority stack | ADVANTAGE | OT-Thing: single fixed source |
| Multi-area weighted averaging (4 zones) | ADVANTAGE | Not in OT-Thing |
| Thermal comfort adjustment (humidity → setpoint) | ADVANTAGE | Not in OT-Thing |
| Fallback thermal estimation | ADVANTAGE | Not in OT-Thing |
| Full SAT control loop (PID, heating curve, auto-gains) | ADVANTAGE | Not in OT-Thing |
| Multi-zone PID (up to 4 zones) | ADVANTAGE | Not in OT-Thing |
| Modulation suppression + overshoot protection | ADVANTAGE | Not in OT-Thing |
| Cycle tracking and classification | ADVANTAGE | Not in OT-Thing |
| Solar gain compensation | ADVANTAGE | Not in OT-Thing |
| Named DS18B20 → SAT area mapping | GAP — TASK-587 | Neither has UI; REST-only workaround in 2.0.0 |
| Direct MQTT room-temp → OT bus (TC= injection) | BOTH GAP | Absent in both; requires explicit design decision |

---

## Domain 5: Settings + Config + Hardware

| Feature | Status | Notes |
|---------|--------|-------|
| Core settings (hostname, WiFi, MQTT, NTP) | COVERED | 2.0.0 richer (configurable NTP server, IANA timezone) |
| HTTP Basic Auth | ADVANTAGE | Not in OT-Thing |
| Ethernet static IP | ADVANTAGE | OT-Thing: DHCP only |
| S0 pulse counter | ADVANTAGE | Not in OT-Thing |
| GPIO relay output | ADVANTAGE | Not in OT-Thing |
| Webhooks | ADVANTAGE | Not in OT-Thing |
| Dark mode + UI preferences | ADVANTAGE | Not in OT-Thing |
| Nightly scheduled reboot | ADVANTAGE | Not in OT-Thing |
| SAT settings (30+ fields) | ADVANTAGE | Not in OT-Thing |
| Heap pressure diagnostics (8 metrics) | ADVANTAGE | Not in OT-Thing |
| Discovery verify system | ADVANTAGE | Not in OT-Thing |
| OLED UI (5-page display, button pagination, 30s timeout) | GAP — TASK-580 | OT-Thing: full OLED UI; 2.0.0: detects but does not display |
| Dynamic Ethernet↔WiFi failover (5s polling) | GAP — TASK-581 | OT-Thing: runtime; 2.0.0: boot-time only |
| Auto firmware version check (GitHub API) | GAP (minor) | OT-Thing: built-in; 2.0.0: external checker required |
| Heating curve config (exponent/gradient/offset — static) | DIFFERENT | OT-Thing: static curves; 2.0.0: SAT autotune replaces this intentionally |

---

## Gap summary

### Backlog tasks created from this audit

| Task | Title | Priority |
|------|-------|----------|
| TASK-579 | feat(ui): highlight active heating curve — mute reference curves | medium |
| TASK-580 | feat(oled): implement OLED UI driver with multi-page display | medium |
| TASK-581 | feat(network): dynamic Ethernet-to-WiFi failover at runtime | medium |
| TASK-582 | feat(otdirect): add hysteresis and CH suspension logic to OTDirect | medium |
| TASK-583 | feat(otdirect): increase ventilation MsgID polling frequency from 60s to 10s | low |
| TASK-584 | feat(settings): persist ventilation overrides across reboots | low |
| TASK-585 | feat(ui): WiFi network scan REST endpoint and UI selector | low |
| TASK-586 | feat(sat): heating curve marker system for manual calibration points | medium |
| TASK-587 | feat(sat): named DS18B20 sensor to SAT area mapping in settings UI | low |

### Items absent in both projects (not tracked as gaps)

- **Direct MQTT room-temp → OT bus routing** — neither OT-Thing nor 2.0.0 forward
  external room temperature directly as a TC= OpenTherm command. Requires an explicit
  design decision (ADR candidate) before implementing.
- **Bypass/vent stubs** — `openBypass`, `autoBypass`, `freeVentEnable` are empty `case`
  blocks in OT-Thing's MQTT handler. Not implemented in either project.

---

## Migration risk assessment

| User type | Risk | Notes |
|-----------|------|-------|
| MQTT-only users | Low | Core control topics have full parity |
| OLED display users | Medium | TASK-580 pending; workaround: use web UI |
| Ethernet-only users | Medium | TASK-581 pending; workaround: reboot after cable change |
| OT-Thing static heating curve users | Low | SAT autotune provides better outcome; initial tuning required |
| Ventilation control users | Low-medium | Vent polling slower (TASK-583); settings not persisted (TASK-584) |
