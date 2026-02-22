# OpenTherm v4.2 MQTT / HA Breaking Changes (v1.2.0-beta)

## Issue Description

An OpenTherm v4.2 audit found multiple MQTT output and Home Assistant discovery mismatches:

- Some message IDs were decoded with the wrong data type or wrong active byte (`u8/u8` used where v4.2 defines `f8.8`, `special`, or single-byte fields).
- `mqttha.cfg` contained discovery template errors (wrong message ID trigger and wrong state topic).
- Legacy pre-v4.2 IDs `50-63` were treated as normal IDs even on v4.x systems, although OpenTherm v4.2 reserves them.

This caused incorrect values, broken HA entities, and non-compliant behavior on v4.x systems.

## Root Cause

- Historical OTGW decoding logic favored generic `u8/u8` decoding for several IDs.
- HA discovery templates were partially copy/pasted and not revalidated against the v4.2 message table.
- There was no compatibility profile to distinguish pre-v4.2 legacy ID handling from v4.2+ reserved-ID behavior.

## The Fix

### Firmware (OpenTherm decoding and MQTT publishing)

- Added a compatibility profile for IDs `50-63`:
  - `AUTO` (default): suppresses IDs `50-63` only after detecting OpenTherm v4.x (`OpenThermVersionMaster` or `OpenThermVersionSlave` >= `4.0`)
  - `V4X_STRICT`: always suppress
  - `PRE_V42_LEGACY`: always allow legacy decoding
- Corrected v4.2 decoding for:
  - ID `38` (`RelativeHumidity`) -> `f8.8`
  - IDs `71`, `77`, `78`, `87` -> single-byte handling with correct HB/LB selection
  - IDs `98`, `99` -> `special` decoding
- Added semantic MQTT topics for IDs `98` and `99` while keeping raw byte alias topics for compatibility.
- For single-byte IDs (`71`, `77`, `78`, `87`), firmware now publishes:
  - canonical base topic (spec-aligned)
  - legacy `_hb_u8` and `_lb_u8` aliases (compatibility)

### Home Assistant discovery (`mqttha.cfg`)

- Fixed `vh_configuration_*` discovery entries to use message ID `74` (`ConfigMemberIDVH`), not `70`.
- Fixed `Hcratio` discovery `stat_t` topic from `DHWFlowRate` to `Hcratio`.
- Replaced broken `FanSpeed` discovery entity with two spec-aligned entities:
  - `FanSpeed_setpoint_hz` (`FanSpeed_hb_u8`)
  - `FanSpeed_actual_hz` (`FanSpeed_lb_u8`)
- Documented legacy pre-v4.2 IDs `50` and `58` in `mqttha.cfg` comments (reserved in v4.x).

## Breaking Changes

### Manual MQTT consumers

1. `RelativeHumidity` (ID `38`)
   - Old behavior: split byte topics (`RelativeHumidity_hb_u8`, `RelativeHumidity_lb_u8`) due to incorrect `u8/u8` decoding.
   - New behavior: canonical `RelativeHumidity` topic publishes v4.2 `f8.8` value.

2. Legacy IDs `50-63` on v4.x systems
   - Old behavior: always decoded/published.
   - New behavior: suppressed in default `AUTO` mode after v4.x is detected (reserved in v4.2+).

3. Typo-topic fixes (already breaking if manually subscribed)
   - `eletric_production` -> `electric_production`
   - `solar_storage_slave_fault_incidator` -> `solar_storage_slave_fault_indicator`

### Home Assistant discovery / entities

1. `FanSpeed`
   - Old discovery entity: single `FanSpeed` sensor (`rpm`) pointing to a topic that was not published.
   - New discovery entities:
     - `FanSpeed_setpoint_hz`
     - `FanSpeed_actual_hz`

2. `Hcratio` (ID `58`, legacy pre-v4.2)
   - Discovery topic corrected from `DHWFlowRate` to `Hcratio`.

## Compatibility Notes

- IDs `71`, `77`, `78`, `87`: legacy split alias topics are still published.
- IDs `98`, `99`: raw byte aliases are still published, plus new semantic decoded topics.
- Legacy IDs `50-63` remain supported for actual pre-v4.2 devices via `AUTO` (before v4.x is detected) and `PRE_V42_LEGACY`.

Current limitation:

- The compatibility profile is implemented in firmware code and defaults to `AUTO`, but it is not yet exposed as a UI/MQTT setting.

## Migration Steps

1. Upgrade firmware and filesystem together (recommended for `mqttha.cfg` changes).
2. Remove stale Home Assistant entities (especially old `FanSpeed` and typo-topic entities).
3. Trigger MQTT auto-discovery again.
4. Update manual MQTT sensors/automations:
   - `RelativeHumidity_*_u8` -> `RelativeHumidity`
   - `eletric_production` -> `electric_production`
   - `solar_storage_slave_fault_incidator` -> `solar_storage_slave_fault_indicator`
5. If you rely on IDs `50-63`, verify your device protocol generation:
   - pre-v4.2: legacy topics remain available
   - v4.x: reserved IDs are suppressed by default

## Testing

Validated by code review and spec cross-check against the local Markdown OpenTherm v4.2 message reference:

- `specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md`

Checks performed:

- Per-ID map/decode coverage audit (all 101 v4.2 IDs covered)
- Type/direction spot checks for corrected IDs (`4`, `38`, `71`, `77`, `78`, `87`, `98`, `99`)
- `mqttha.cfg` discovery line verification for `FanSpeed`, `vh_configuration_*`, `Hcratio`

Not yet performed:

- Hardware-in-the-loop validation on live OTGW + thermostat/boiler
- End-to-end Home Assistant auto-discovery regression test

## Impact

- Improves OpenTherm v4.2 compliance and HA discovery correctness.
- Preserves backward compatibility for legacy pre-v4.2 systems through profile-based handling and MQTT alias topics.
- Introduces documented breaking changes for some MQTT/HA consumers (mainly `RelativeHumidity`, `FanSpeed`, and legacy IDs `50-63` on v4.x systems).

## Related Files

- `src/OTGW-firmware/OTGW-Core.h`
- `src/OTGW-firmware/OTGW-Core.ino`
- `src/OTGW-firmware/data/mqttha.cfg`
- `README.md`
- `RELEASE_NOTES_1.2.0.md`

## Commit

Unreleased working tree (post-audit changes, 2026-02-22)
