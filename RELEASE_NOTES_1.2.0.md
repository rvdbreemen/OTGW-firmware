# Release Notes — v1.2.0-beta

**Release date:** 2026-02-17  
**Last updated:** 2026-02-22 (post OpenTherm v4.2 MQTT/HA audit corrections)<br>
**Release branch:** `dev-branch-v1.2.0-beta`  
**Comparison target:** `dev`  
**Base commit (merge-base):** `a279664`

---

## Overview

Version `1.2.0-beta` is a focused release on top of the `dev` branch that improves:

- OpenTherm protocol map coverage, direction correctness, and v4.2 type/byte semantics (including reserved-ID handling compatibility).
- Runtime safety around message-map indexing.
- MQTT/Home Assistant topic correctness and discovery consistency.
- Documentation quality for OpenTherm v4.2 analysis and migration.

Compared with `dev`, this branch adds protocol-centric changes and discovery corrections rather than broad platform features.

---

## Delta vs dev (high-level)

### Commit delta (excluding merges)

`dev-branch-v1.2.0-beta` contains functional changes mainly in:

- `1c6fe19` — OpenTherm v4.2 protocol compliance analysis + implementation changes.
- `43b94c5` — MQTT auto-configuration logic refactor and version bump.
- `700fc79` — removal of accidental artifact file.

Remaining unique commits in this branch are CI/version metadata updates.

### File impact summary

Compared to `dev` before this release-note update:

- **30 files changed**
- **1292 insertions / 172 deletions**
- Main touched areas:
  - OpenTherm core map and parser
  - REST API value retrieval path
  - MQTT auto-discovery and HA template config
  - Documentation (`docs/reviews/2026-02-15_opentherm-v42-compliance/`)

---

## Detailed Changes

## 1) OpenTherm v4.2 Message-Map Alignment

### Added missing message IDs

The following message IDs were added to the OT map and wired into runtime handling:

| ID | Label | Type | Direction |
| --- | --- | --- | --- |
| 39 | `TrOverride2` | `f8.8` | `OT_READ` |
| 93 | `Brand` | `u8/u8` | `OT_READ` |
| 94 | `BrandVersion` | `u8/u8` | `OT_READ` |
| 95 | `BrandSerialNumber` | `u8/u8` | `OT_READ` |
| 96 | `CoolingOperationHours` | `u16` | `OT_RW` |
| 97 | `PowerCycles` | `u16` | `OT_RW` |

Code paths updated:

- Map entries in `src/OTGW-firmware/OTGW-Core.h`
- Parsing/printing switch in `src/OTGW-firmware/OTGW-Core.ino`
- Value export in `getOTGWValue()` (`src/OTGW-firmware/OTGW-Core.ino`)

### Direction corrections for existing IDs

The following map direction flags were corrected:

| ID | Label | `dev` | `1.2.0-beta` |
| --- | --- | --- | --- |
| 4 | `Command` | `OT_RW` | `OT_WRITE` |
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

### Type/unit corrections

- ID 35 (`FanSpeed`) is now handled as `u8/u8` (`print_u8u8`) with unit `Hz` instead of `rpm`.
- ID 38 (`RelativeHumidity`) is now handled as `f8.8` (v4.2) instead of `u8/u8`.
- IDs 71, 77, 78, and 87 now use v4.2 single-byte semantics (correct HB/LB selection) instead of generic `u8/u8` handling.
- IDs 98 and 99 now use v4.2-special decoding with semantic MQTT topics, while keeping raw byte aliases for compatibility.
- ID 19 (`DHWFlowRate`) unit string updated to `l/min`.

### Legacy pre-v4.2 ID compatibility (IDs 50-63)

Firmware now applies reserved-ID rules for IDs `50-63` using a compatibility profile:

- `AUTO` (default): keep legacy decoding until a v4.x OpenTherm version is detected (`OpenThermVersionMaster` or `OpenThermVersionSlave` >= `4.0`), then suppress IDs `50-63` because they are reserved in OpenTherm v4.2+.
- `V4X_STRICT`: always suppress IDs `50-63`.
- `PRE_V42_LEGACY`: always decode/publish IDs `50-63` (legacy behavior).

Current implementation note: the profile exists in firmware code and defaults to `AUTO`; it is not yet exposed as a user setting.

### Completeness fixes

- Added missing `getOTGWValue()` mappings for:
  - ID 113 (`BurnerUnsuccessfulStarts`)
  - ID 114 (`FlameSignalTooLow`)

---

## 2) Runtime Safety and Defensive Handling

### OT map bounds checks

To prevent out-of-bounds map reads:

- `processOT()` now checks `OTdata.id <= OT_MSGID_MAX` before reading `OTmap`.
- Unknown IDs get a safe fallback lookup object (`label="Unknown"` etc.).

Files:

- `src/OTGW-firmware/OTGW-Core.ino`

### REST value lookup hardening

- `sendOTGWvalue(int msgid)` now validates range before reading `OTmap[msgid]`.

File:

- `src/OTGW-firmware/restAPI.ino`

---

## 3) MQTT and Home Assistant Discovery Corrections

### Topic and label typo fixes

Corrected typos in MQTT publications and HA discovery config:

| Old | New |
| --- | --- |
| `eletric_production` | `electric_production` |
| `solar_storage_slave_fault_incidator` | `solar_storage_slave_fault_indicator` |
| `Diagonostic_Indicator` | `Diagnostic_Indicator` |

Files:

- `src/OTGW-firmware/OTGW-Core.ino`
- `src/OTGW-firmware/data/mqttha.cfg`

### HA discovery and MQTT topic alignment fixes

- `mqttha.cfg`: fixed `vh_configuration_*` discovery entries to trigger on message ID `74` (`ConfigMemberIDVH`) instead of `70`.
- `mqttha.cfg`: fixed `Hcratio` discovery `stat_t` topic to `%mqtt_pub_topic%/Hcratio` (was incorrectly `%mqtt_pub_topic%/DHWFlowRate`).
- `mqttha.cfg`: replaced broken `FanSpeed` discovery entity with two spec-aligned sensors:
  - `FanSpeed_setpoint_hz` -> `%mqtt_pub_topic%/FanSpeed_hb_u8`
  - `FanSpeed_actual_hz` -> `%mqtt_pub_topic%/FanSpeed_lb_u8`
  - Unit corrected to `Hz` (OpenTherm v4.2 defines Hz, not rpm)
- Firmware: IDs `71`, `77`, `78`, `87` now publish canonical single-byte base topics (e.g. `RelativeVentilation`) and also publish legacy `_hb_u8` / `_lb_u8` aliases for backward compatibility.
- Firmware: IDs `98` and `99` now publish semantic decoded topics in addition to raw byte aliases.
- Firmware: ID `38` (`RelativeHumidity`) now publishes canonical `f8.8` value on `RelativeHumidity` instead of split `u8/u8` topics.

Files:

- `src/OTGW-firmware/OTGW-Core.h`
- `src/OTGW-firmware/OTGW-Core.ino`
- `src/OTGW-firmware/data/mqttha.cfg`

### Auto-configuration logic cleanup

`doAutoConfigure()` now:

- skips Dallas placeholder line in central loop,
- sends lines when `!getMQTTConfigDone(lineID)` (or forced),
- marks message IDs as configured after publication attempt,
- triggers Dallas-specific configuration via `configSensors()` only when needed.

File:

- `src/OTGW-firmware/MQTTstuff.ino`

---

## 4) Documentation and Repository Hygiene

### Added OpenTherm v4.2 review docs

- `docs/reviews/2026-02-15_opentherm-v42-compliance/README.md`
- `docs/reviews/2026-02-15_opentherm-v42-compliance/OPENTHERM_V42_COMPLIANCE_PLAN.md`
- `docs/reviews/2026-02-15_opentherm-v42-compliance/OUT_OF_SCOPE_ANALYSIS.md`

### Cleanup

- Removed accidental artifact file: `tmpclaude-ecc0-cwd`.

---

## Breaking Changes and Migration Notes

### MQTT topic renames

If you use manual MQTT sensors/automations, update these topic names:

- `eletric_production` -> `electric_production`
- `solar_storage_slave_fault_incidator` -> `solar_storage_slave_fault_indicator`

### MQTT payload/topic behavior changes (OpenTherm v4.2 alignment)

The following are breaking for manual MQTT consumers and Home Assistant entities created from older discovery data:

- `RelativeHumidity` (ID `38`) no longer uses legacy `u8/u8` split topics (`RelativeHumidity_hb_u8`, `RelativeHumidity_lb_u8`); it now publishes the v4.2 `f8.8` value on `RelativeHumidity`.
- `FanSpeed` HA discovery is now split into two sensors (`FanSpeed_setpoint_hz`, `FanSpeed_actual_hz`) with `Hz` units.
- Legacy pre-v4.2 IDs `50-63` are suppressed on v4.x systems in default `AUTO` mode because those IDs are reserved in OpenTherm v4.2+.

Compatibility retained:

- IDs `71`, `77`, `78`, `87`: legacy `_hb_u8` / `_lb_u8` alias topics are still published alongside spec-correct base topics.
- IDs `98`, `99`: raw byte alias topics are still published, plus new semantic decoded topics.

### Home Assistant cleanup

After upgrade:

1. Remove stale entities tied to old typo topics and the old `FanSpeed` entity.
2. Trigger MQTT auto-discovery again.
3. Verify that `FanSpeed_setpoint_hz` and `FanSpeed_actual_hz` entities are created.
4. Verify that `electric_production` and solar storage fault entities are available.
5. If you use legacy IDs `50-63`, verify whether your thermostat/boiler is pre-v4.2 (v4.x systems now suppress them in `AUTO` mode).

Detailed migration guidance: `docs/fixes/opentherm-v42-mqtt-breaking-changes.md`

---

## Review Findings (vs dev)

### Medium

1. `doAutoConfigure()` marks discovery IDs as done even if publish fails.  
File reference: `src/OTGW-firmware/MQTTstuff.ino:823` and `src/OTGW-firmware/MQTTstuff.ino:824`  
Context: `sendMQTTStreaming()` returns `void`, so publish failure is not propagated, but `setMQTTConfigDone(lineID)` is still called.  
Impact: if MQTT is disconnected or publish fails during discovery trigger, entries can be marked configured prematurely and may be skipped on later non-forced runs.

### Low

1. Previous README text claimed an `is_value_valid()` consistency fix that is not present as a code delta vs `dev`.  
Status: corrected in current README update.

---

## Validation Status

This release-note update is based on:

- Commit and diff analysis between `dev` and `dev-branch-v1.2.0-beta`.
- Manual code-path review of:
  - `OTGW-Core.h`
  - `OTGW-Core.ino`
  - `restAPI.ino`
  - `MQTTstuff.ino`
  - `data/mqttha.cfg`

Not executed in this review:

- Hardware-in-the-loop boiler/thermostat validation
- End-to-end MQTT broker integration tests
- Full firmware compile/build pipeline

