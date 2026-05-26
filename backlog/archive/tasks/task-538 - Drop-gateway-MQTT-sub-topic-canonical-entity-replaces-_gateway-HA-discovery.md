---
id: TASK-538
title: Drop /gateway MQTT sub-topic; canonical entity replaces _gateway HA discovery
status: Done
assignee:
  - '@claude'
created_date: '2026-05-05 05:09'
updated_date: '2026-05-05 05:11'
labels:
  - mqtt
  - ha-discovery
  - beta11
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Beta tester report (Obe, Discord): with bSeparateSources=true, OTGW publishes redundant /gateway MQTT sub-topics and a _gateway HA entity whose unique_id collides with the canonical slot — only the canonical entity (e.g. OTGW_Control_setpoint) appears in HA, and per-source children don't materialize. Drop the /gateway sub-topic; the third per-source variant becomes the canonical entity (empty suffix, empty name, empty topic segment) so HA gets {canonical, _thermostat, _boiler}. Also performed an audit of which OT IDs emit MQTT values vs which have HA discovery templates: every emitted OT ID has a discovery entry; IDs 40-47/64-69 are OT_UNDEF in OTmap[] and are never emitted, so no template gap. Implementation: edits in src/OTGW-firmware/MQTTstuff.ino (resolveSourceIndex drops OTGW_REQUEST_BOILER, mqttSourceKeys[] shrunk to 2 entries with MQTT_SOURCE_KEY_COUNT constant) and src/OTGW-firmware/mqtt_configuratie.cpp (expandAndStreamSensorSources renamed gateway -> canonical, name/segment empty so streamSensorDiscovery emits canonical unique_id/name/stat_t).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Gateway-source frames (OTGW_REQUEST_BOILER) no longer publish to a /gateway MQTT sub-topic
- [x] #2 HA discovery emits {canonical, _thermostat, _boiler} for 0x07-flagged sensors; no _gateway entity
- [x] #3 Canonical entity stat_t resolves to <topic>/<label> (no /gateway segment)
- [x] #4 bSeparateSources=false code path is unchanged
- [x] #5 Build (./build.sh) produces firmware + filesystem artifacts
- [x] #6 Audit documented: every OT ID that emits MQTT values has a HA discovery entry; 40-47/64-69 are OT_UNDEF and never published
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Audit publish path and HA discovery for /gateway suffix
2. Confirm canonical topic semantics (latest from any source, not gateway-only)
3. Edit MQTTstuff.ino: drop OTGW_REQUEST_BOILER from resolveSourceIndex, shrink mqttSourceKeys[] to 2 entries with MQTT_SOURCE_KEY_COUNT
4. Edit mqtt_configuratie.cpp: rename gateway->canonical in expandAndStreamSensorSources (empty suffix/name/segment)
5. Build firmware + filesystem via ./build.sh
6. Run python3 evaluate.py --quick — confirm no new failures
7. Document audit results in plan file
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
**Dropped redundant /gateway per-source MQTT sub-topic and renamed the _gateway HA discovery variant to a canonical (empty-suffix) entity.**

With `bSeparateSources=true` the firmware previously published four topics per source-flagged OT ID — canonical + /thermostat + /boiler + /gateway — and emitted a HA discovery entity for `<label>_gateway` whose `unique_id` collided with the base entity slot. As a result, only the base/canonical HA entity (e.g. `OTGW_Control_setpoint`) materialized; the per-source children never rendered correctly. Beta tester confirmed via Discord screenshot.

## Changes

- `src/OTGW-firmware/MQTTstuff.ino`
  - `mqttSourceKeys[]` shrunk to two entries (`thermostat`, `boiler`); added `MQTT_SOURCE_KEY_COUNT` constant.
  - `resolveSourceIndex()` no longer maps `OTGW_REQUEST_BOILER`; gateway-source frames now reach only the canonical topic via the existing `sendMQTTData(topic, _msg)` call that precedes every `publishToSourceTopic()` site.
  - `copySourceTableEntry()` bound check uses the new constant.
- `src/OTGW-firmware/mqtt_configuratie.cpp`
  - `expandAndStreamSensorSources()` third variant renamed `gateway` → `canonical`. Suffix, source name, and topic segment are empty PROGMEM strings, so `streamSensorDiscovery` already short-circuits at lines 1906-1909 / 1924-1927 and emits a canonical entity (unique_id `<nodeId>-<label>`, stat_t `<topic>/<label>`).
  - Used `kSourceVariantCount` from sizeof for clarity.

## Audit (item 2 of request)

Every OT data ID that publishes MQTT values has a corresponding HA discovery entry. IDs 40-47, 64-69, 92, 128-130 are `OT_UNDEF` in `OTmap[]` (`OTGW-Core.h`) and never reach `processOT`/`sendMQTTData` — so no template gap. Pseudo-IDs 245-247 (S0, Dallas, stats) are covered. No new templates required for OT-message values. (Diagnostic topics like `otgw-firmware/stats/*`, `reboot_count`, `reboot_reason` remain raw-published without HA discovery — out of scope, candidate for a follow-up.)

## Tests

- `./build.sh` → firmware + filesystem produced (1.5.0-beta.12+537dd5d, 0.70 MB / 1.98 MB).
- `python3 evaluate.py --quick` → 31 pass / 2 warn / 1 fail (unchanged from baseline; failure is unrelated ADR-reference resolution).

## Risk / migration

Users with HA template sensors or scripts referencing `<topic>/.../gateway` sub-topics or `<label>_gateway` HA entities will break. Recommend a release-note line and a one-shot retained-message clear for `homeassistant/sensor/<device>/<label>_gateway/config` if HA hangs onto orphaned entities. The firmware's force-discovery cycle (`F` debug command) already clears+republishes its own discovery payloads.

## Follow-ups

- TASK-539: port to `feature-dev-2.0.0-otgw32-esp32-sat-support` branch.
<!-- SECTION:FINAL_SUMMARY:END -->
