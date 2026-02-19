# Release Notes — v1.2.0

**Release date:** 2026-02-19  
**Comparison baseline:** v1.1.0 (documented baseline commit `38f1972`)  
**Release branch:** `dev-branch-v1.2.0-beta-adr040-clean` (stabilized for v1.2.0)

---

## Overview

Version `1.2.0` is a correctness and reliability release on top of `1.1.0`.

Primary goals:

1. Align OpenTherm message handling with v4.2 map expectations.
2. Add source-specific MQTT visibility (thermostat/boiler/gateway) without breaking existing topics.
3. Harden MQTT discovery/settings/GPIO behavior to prevent silent misconfiguration and avoid extra flash wear.

High-level delta from v1.1.0:

- **55 files changed**
- **6500 insertions / 504 deletions**
- Main impact areas: `OTGW-Core.*`, `MQTTstuff.ino`, `restAPI.ino`, `settingStuff.ino`, `data/index.js`, `data/mqttha.cfg`

---

## Breaking and Behavioral Changes

## 1) MQTT topic corrections (breaking for manual topic users)

Two typo fixes changed published topic names:

| Old topic | New topic |
| --- | --- |
| `eletric_production` | `electric_production` |
| `solar_storage_slave_fault_incidator` | `solar_storage_slave_fault_indicator` |

If you configured manual MQTT sensors/automations, update topic references.

## 2) Settings POST persistence semantics

`POST /api/v2/settings` now returns `200` with queued persistence metadata:

```json
{"status":"queued","name":"<setting>","pending":true}
```

Persistence is deferred/coalesced (single-save behavior), not immediate per field.
Pending writes are still flushed on reboot before restart.

---

## Major Features

## 1) OpenTherm v4.2 message-map alignment

Added missing IDs:

| ID | Label | Type | Direction |
| --- | --- | --- | --- |
| 39 | `TrOverride2` | `f8.8` | `OT_READ` |
| 93 | `Brand` | `u8/u8` | `OT_READ` |
| 94 | `BrandVersion` | `u8/u8` | `OT_READ` |
| 95 | `BrandSerialNumber` | `u8/u8` | `OT_READ` |
| 96 | `CoolingOperationHours` | `u16` | `OT_RW` |
| 97 | `PowerCycles` | `u16` | `OT_RW` |

Direction corrections:

| ID | Label | Previous | v1.2.0 |
| --- | --- | --- | --- |
| 27 | `Toutside` | `OT_READ` | `OT_RW` |
| 37 | `TRoomCH2` | `OT_READ` | `OT_WRITE` |
| 38 | `RelativeHumidity` | `OT_READ` | `OT_RW` |
| 98 | `RFstrengthbatterylevel` | `OT_READ` | `OT_WRITE` |
| 99 | `OperatingMode_HC1_HC2_DHW` | `OT_READ` | `OT_RW` |
| 109 | `ElectricityProducerStarts` | `OT_READ` | `OT_RW` |
| 110 | `ElectricityProducerHours` | `OT_READ` | `OT_RW` |
| 112 | `CumulativElectricityProduction` | `OT_READ` | `OT_RW` |
| 124 | `OpenThermVersionMaster` | `OT_READ` | `OT_WRITE` |
| 126 | `MasterVersion` | `OT_READ` | `OT_WRITE` |

Type/unit correctness:

- ID 35 (`FanSpeed`) corrected to `u8/u8` (`Hz`).
- DHW flow unit normalized to `l/min`.
- Added missing `getOTGWValue()` handling for IDs 113 and 114.

## 2) MQTT source-specific topics (ADR-040)

New capability: publish WRITE/RW values with source suffix while keeping legacy topics.

Source topics:

- `<prefix>/value/<node>/<sensor>_thermostat`
- `<prefix>/value/<node>/<sensor>_boiler`
- `<prefix>/value/<node>/<sensor>_gateway`

Legacy topic continues unchanged:

- `<prefix>/value/<node>/<sensor>`

Enabled by setting:

- Config key: `MQTTSeparateSources`
- API field: `mqttseparatesources`
- Default: `true`

Home Assistant:

- Source-specific discovery entities are generated and deduplicated per source sensor-id.

---

## Reliability and Correctness Improvements

## 1) MQTT discovery success gating (critical reliability)

Discovery is no longer marked complete if publish fails.

Stabilization details:

- `sendMQTTStreaming(...)` now returns `bool` success/failure.
- Source-specific discovery cache (`setSourceConfigDone`) only updates on successful publish.
- Global auto-discovery done bits (`setMQTTConfigDone`) only update on successful publish.
- `doAutoConfigureMsgid(...)` returns success only when publish succeeds.

Impact:

- transient broker outages no longer cause permanent "configured-but-never-published" states.
- discovery retries naturally on next eligible cycle.

## 2) Runtime safety hardening

- Added OT map bounds checks before map indexing in core processing and REST lookup.
- Unknown IDs use safe fallback metadata instead of unsafe map access.
- Message tracking array and hour bitmask fixes prevent silent data corruption edge cases.

## 3) Deferred settings write model preserved

- Coalesced settings writes (reduced flash wear) remain the default model.
- Reboot path explicitly flushes pending writes before restart.
- Web UI now treats queued persistence response as success and keeps user feedback clear.

## 4) GPIO conflict check cleanup

- Conflict caller switched from string token to typed owner enum.
- Warnings now depend on feature enabled-state (sensor/S0/output), reducing false positives.
- Behavior remains warn-only (no hard reject), preserving existing UX.

---

## API and Web UI Updates

## 1) REST API v2 completion and consistency

v2 REST work from this cycle includes:

- endpoint completion and frontend migration to v2 routes.
- standardized JSON error behavior and status code hygiene.
- resource naming consistency (`commands`, `messages/{id}`, `device/*`, etc.).
- CORS and method handling improvements.

## 2) UI behavior and responsiveness

- settings save flow updated for sequential/queued persistence feedback.
- PS mode (`psmode`) support exposed via device-time API and respected in UI behavior.
- OT monitor/UI behavior tuned to avoid unnecessary work in constrained states.

---

## CI / Build / Tooling

- CI setup and build reliability improved (retry logic, cache handling).
- Build/release workflow docs expanded for repeatable release packaging.
- ADR and review documentation expanded for traceability (including ADR-040 validation docs).

---

## Migration Checklist (1.1.0 -> 1.2.0)

1. Update any manual MQTT sensors/automations using corrected topic names.
2. Re-run MQTT discovery in Home Assistant and remove stale typo-topic entities.
3. If using source separation, verify new entities (`_thermostat`, `_boiler`, `_gateway`) are expected and mapped.
4. If tooling assumed immediate settings durability on POST, update logic to accept queued persistence (`status=queued`).
5. Flash both firmware and LittleFS images for this release.

---

## Validation Status

Validated in this branch:

- Firmware build passes (`python build.py --firmware` and full `python build.py`).
- Filesystem image build passes.
- Source checks confirm:
  - publish-success gating in MQTT discovery paths,
  - queued settings response path,
  - enabled-aware GPIO conflict checks,
  - UI handling for queued save responses.

Generated artifacts (example from validation run):

- `build/OTGW-firmware-1.2.0-beta+533d941.ino.bin`
- `build/OTGW-firmware.1.2.0-beta+533d941.littlefs.bin`

---

## Known Notes

- Existing compile warning remains from dual `WEBSOCKETS_MAX_DATA_SIZE` defines (`networkStuff.h` vs WebSockets library header).
- ESP8266 memory headroom remains tight under heavy feature usage; monitor heap in long-running installs.

---

## References

- `README.md`
- `RELEASE_NOTES_1.1.0.md`
- `docs/adr/ADR-040-mqtt-source-specific-topics.md`
- `docs/testing/ADR-040-source-separation-test-matrix.md`
- `docs/reviews/2026-02-15_opentherm-v42-compliance/README.md`
