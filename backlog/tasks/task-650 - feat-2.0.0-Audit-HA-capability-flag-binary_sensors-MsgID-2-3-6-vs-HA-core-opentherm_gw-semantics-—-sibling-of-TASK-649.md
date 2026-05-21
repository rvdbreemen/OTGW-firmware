---
id: TASK-650
title: >-
  feat-2.0.0: Audit HA capability-flag binary_sensors (MsgID 2/3/6) vs HA core
  opentherm_gw semantics — sibling of TASK-649
status: Done
assignee:
  - '@claude'
created_date: '2026-05-21 09:15'
updated_date: '2026-05-21 09:19'
labels:
  - audit
  - ha-discovery
  - mqtt
  - non-breaking
  - feat-2.0.0
dependencies: []
priority: medium
ordinal: 48000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sibling of TASK-649 on the feature-dev-2.0.0-otgw32-esp32-sat-support branch. Same scope, same off-limits list, mirrored against the renamed file (MQTTHaDiscovery.cpp instead of mqtt_configuratie.cpp) and the 2.0.0 layout (MqttHaBinSensorCfg has deviceClass and payload fields; entries default-init those to nullptr / HaBinaryPayload::on_off=0 respectively, so no override emitted; HA defaults apply).

Structural differences from dev:
- mqtt_configuratie.cpp -> MQTTHaDiscovery.cpp (831 lines of structural divergence between branches)
- MqttHaBinSensorCfg gained explicit deviceClass + HaBinaryPayload fields on 2.0.0 (MQTTstuff.h:277-287); none of the 13 capability entries override either, so behaviour matches dev.

Reference research and reference table: see TASK-649 description; the same 11 HA-core-matched + 2 firmware-extras entries are present at MQTTHaDiscovery.cpp:1277-1298.

On zero-fix outcome, the audit-report doc at docs/audits/2026-05-21-ha-capability-flags-feature-2.0.0.md is the deliverable.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Read the 13 binary_sensor entries in mqttHaBinSensors[] at MQTTHaDiscovery.cpp:1277-1298 and record current entity_category, device_class, icon, retain flag, and the actual ON/OFF payload values published by OTGW-Core.ino
- [x] #2 Confirm all 13 entries have entity_category=diagnostic
- [x] #3 Confirm none of the 13 entries override cfg.deviceClass (the field exists on this branch but defaults to nullptr) — none should assert PROBLEM/RUNNING/HEAT
- [x] #4 Confirm none of the 13 entries override cfg.payload — default HaBinaryPayload::on_off (=0) emits no payload keys in JSON, so HA defaults ON/OFF apply, matching sendMQTTData literals
- [x] #5 Confirm all 13 discovery configs are published with retain=true via streamBinarySensorDiscovery's beginPublish(topic, byteCount, true) at MQTTHaDiscovery.cpp:2446
- [x] #6 Spot-check the friendly ha_name_* strings for typos; fix only clear typos, not naming-style changes
- [x] #7 Add the same bit-6 ambiguity comment at OTGW-Core.ino:2327 noting pyotgw's DATA_SLAVE_REMOTE_RESET vs the OT spec's remote_water_filling_function (port from dev's de5de6de)
- [x] #8 Produce docs/audits/2026-05-21-ha-capability-flags-feature-2.0.0.md with per-entry findings table, fixes applied, and conclusion
- [x] #9 OFF-LIMITS — no ha_lbl_* (topic label) changes, no new bit decoders, no MsgID handler changes, no ADR-098 source-separated layer changes, no gating/cadence logic changes
- [x] #10 build.py --firmware exits 0; evaluate.py --quick shows no new findings
- [x] #11 _VERSION_PRERELEASE bumped via bin/bump-prerelease.sh since OTGW-Core.ino touched
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Sibling audit of TASK-649 on feature-dev-2.0.0-otgw32-esp32-sat-support. Same outcome — parity-confirmed; 1 comment-only fix applied (ported from dev `de5de6de`).

## Outcome

All 13 HA capability-flag binary_sensors (MsgID 2/3/6) match HA core's `opentherm_gw` semantics: `entity_category=diagnostic`, no `device_class` emitted, `retain=true`, HA-default `"ON"`/`"OFF"` payloads (publishers emit literal strings).

The 2.0.0 `MqttHaBinSensorCfg` struct has explicit `deviceClass` and `payload` fields (unlike dev's struct), but none of the 13 entries override either — `deviceClass` defaults to `nullptr` (composer only emits when non-null), `payload` defaults to `HaBinaryPayload::on_off = 0` (composer's switch emits nothing for that case → HA defaults apply). Net wire-level discovery output is identical to dev.

## Fix applied

`src/OTGW-firmware/OTGW-Core.ino` around line 2327 — added the same multi-line bit-6 ambiguity comment ported from dev commit `de5de6de`. Notes that pyotgw / HA core's `opentherm_gw` integration names this bit `DATA_SLAVE_REMOTE_RESET` while the OT 2.2 spec and this firmware's label call it `remote_water_filling_function`. Same wire bit (MsgID 3, HB bit 6). Comment-only; no behavioural change.

## Audit report

`docs/audits/2026-05-21-ha-capability-flags-feature-2.0.0.md` — per-entry findings table, fixes, off-limits items observed, and a comparison table to dev showing the struct-level differences with their identical net behaviour.

## Verification

- `python build.py --firmware --target esp8266` exit 0 — `OTGW-firmware-esp8266-2.0.0-alpha.46+2a67bf5.ino.bin` (0.81 MB).
- ESP32 build skipped — PlatformIO failed to download esp32 platform package due to SSL cert chain (container network policy restriction), not a project issue. The change is comment-only and cannot affect ESP32 compilation differently from ESP8266.
- `python evaluate.py --quick` 98.5% (60 passed, 1 pre-existing warning about `mqtt_configuratie.cpp` not found — that's the evaluator looking for the dev-side filename, irrelevant on 2.0.0).

## Version bump

`_VERSION_PRERELEASE` bumped `alpha.45 → alpha.46` via `bin/bump-prerelease.sh` (touched `src/OTGW-firmware/OTGW-Core.ino` triggers bump policy).

## Off-limits items observed (not addressed)

Same list as dev TASK-649 — friendly-name casing inconsistencies, "Cooling_configs" plural, firmware-extras not in HA core. All documented in the audit report's "Items noted but not addressed" section.

## Related backlog

- TASK-649 (dev sibling) — Done, commit `de5de6de`.
- TASK-648 (v2.1.0 three-device split) — To Do, milestone-v2.1.0.
<!-- SECTION:FINAL_SUMMARY:END -->
