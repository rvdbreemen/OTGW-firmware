---
id: TASK-791
title: >-
  feat-2.0.0: port TASK-790 — MQTT on-change publishing default with 0->60
  migration
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 20:16'
updated_date: '2026-05-31 20:29'
labels:
  - mqtt
  - settings
  - 2.0.0
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the dev-branch MQTT on-change-publishing-as-default feature to 2.0.0. Add a persisted bool setting MQTTonChangePublishing (settings.mqtt.bOnChangePublishing, default true), change the MQTTinterval fresh-install default to 60, one-time deferred 0->60 migration in readSettings, a flag-aware value-topic publish gate, and UI coupling. Must NOT disturb the TASK-400 status-heartbeat (STATUS_HEARTBEAT_INTERVAL_SEC / shouldPublishTrackedStatusBit). Document with a new ADR-116 (Proposed).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTTSettingsSection gains bool bOnChangePublishing=true; iInterval default becomes MQTT_DEFAULT_PUBLISH_INTERVAL_SEC (60) via a new define
- [x] #2 readSettings performs one-time 0->60 migration via deferred settingsDirty path only (no writeSettings inside readSettings, no service restart)
- [x] #3 Helper mqttOnChangePublishingActive() gates the value-topic publish paths (shouldPublishMQTTForID, PS-field, logWorthy x4, status-bit LOG line); flag and interval stored independently in updateSetting
- [x] #4 TASK-400 status heartbeat unchanged: STATUS_HEARTBEAT_INTERVAL_SEC and shouldPublishTrackedStatusBit untouched
- [x] #5 Flag emitted by sendDeviceSettings before mqttinterval, added to knownSettings whitelist and debug dump; persisted by writeSettings
- [x] #6 Web UI: real mqttonchangepublishing checkbox drives interval row visibility + 0/60; synthetic checkbox removed; labels MQTT Publish On-Change / MQTT Interval with expanded tooltips
- [x] #7 New ADR-116 authored (Proposed) grounded in 2.0.0 artifacts; passes bin/adr-lint
- [x] #8 python build.py succeeds with per-env SUCCESS lines; python evaluate.py --quick shows no new failures
- [ ] #9 On-device behaviour verified by maintainer (hardware gate)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. MQTTstuff.h: define MQTT_DEFAULT_PUBLISH_INTERVAL_SEC 60 above struct; add bool bOnChangePublishing=true; iInterval default=MQTT_DEFAULT_PUBLISH_INTERVAL_SEC.\n2. OTGW-Core.ino: add mqttOnChangePublishingActive() after getPackedSlotTime; replace value-topic gates (iInterval==0 -> !active at shouldPublishMQTTForID/PSField; logWorthy x4 -> active; status-bit LOG inline form). Leave STATUS_HEARTBEAT_INTERVAL_SEC + shouldPublishTrackedStatusBit untouched.\n3. settingStuff.ino: writeSettings add MQTTonChangePublishing; updateSetting add branch (no coupling); readSettings deferred migration (settingsDirty); debug dump line.\n4. restAPI.ino: emit mqttonchangepublishing before mqttinterval; add to knownSettings; add to debug dump.\n5. index.js: remove synthetic checkbox; bind real checkbox to interval row; refresh-sync; labels + tooltips (53f6c9a7 final form).\n6. ADR-116 (Proposed) grounded in 2.0.0; run bin/adr-lint.\n7. Docs: CHANGELOG/README/MQTT.md/BREAKING_CHANGES as present.\n8. build.py full + evaluate --quick; bump-prerelease; commit selectively; push.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Ported dev TASK-790 (commit d4c175da + label/tooltip follow-up 53f6c9a7) to 2.0.0. MQTTstuff.h: MQTT_DEFAULT_PUBLISH_INTERVAL_SEC(60) define + bool bOnChangePublishing=true, iInterval default 60. OTGW-Core.ino: mqttOnChangePublishingActive() helper after getPackedSlotTime; value-topic gates (shouldPublishMQTTForID, PS-field, 4x logWorthy, status-bit LOG) made flag-aware. STATUS_HEARTBEAT_INTERVAL_SEC + shouldPublishTrackedStatusBit untouched. settingStuff.ino: write/update/deferred-migration/debug-dump. restAPI.ino: settings feed + knownSettings + debug map. index.js: synthetic checkbox removed, real checkbox drives interval row, labels MQTT Publish On-Change / MQTT Interval + expanded tooltips. ADR-116 (Proposed) passes bin/adr-lint strictly. build.py: esp8266 SUCCESS + esp32 SUCCESS (full fw+fs both targets). evaluate.py --quick: 0 failed, 1 pre-existing ESP-abstraction-baseline warning (no regression). Prerelease bumped alpha.115 -> alpha.116.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ports the dev-branch MQTT on-change-publishing-as-default feature into the 2.0.0 line, adapted to its structure, without disturbing the TASK-400 status heartbeat.

What changed:
- MQTTstuff.h: new MQTT_DEFAULT_PUBLISH_INTERVAL_SEC (60) define; new persisted bool settings.mqtt.bOnChangePublishing (default true); iInterval default changed 0 -> 60.
- OTGW-Core.ino: helper mqttOnChangePublishingActive() (= bOnChangePublishing && iInterval>0) placed after getPackedSlotTime (before first use). Replaced the value-topic gates (shouldPublishMQTTForID, shouldPublishMQTTForPSField, four status-byte logWorthy lines, status-bit decision-log line) with the flag-aware helper/inline form. STATUS_HEARTBEAT_INTERVAL_SEC and shouldPublishTrackedStatusBit left completely untouched.
- settingStuff.ino: writeSettings persists MQTTonChangePublishing; updateSetting stores it verbatim (no interval coupling); readSettings runs a one-time 0->60 migration via the deferred settingsDirty/flushSettings() path (no write or restart inside readSettings); debug dump line added.
- restAPI.ino: mqttonchangepublishing emitted before mqttinterval, added to knownSettings whitelist and the debug-dump map.
- data/index.js: synthetic checkbox removed; the real mqttonchangepublishing checkbox drives the interval row (tick->show+60, untick->hide+0); refresh syncs row visibility from the checkbox; labels set to MQTT Publish On-Change / MQTT Interval with expanded tooltips.
- docs: CHANGELOG, README, MQTT.md, api/README, BREAKING_CHANGES updated (2.0.0 docs were in pre-feature state; entries added fresh, referencing ADR-116).

ADR: docs/adr/ADR-116-mqtt-on-change-publishing-default.md authored, Status Proposed (NOT Accepted - maintainer gate), grounded in 2.0.0 artifacts, passes bin/adr-lint strictly. Numbered 116 because 2.0.0 ADR-081 is a different (Accepted) decision.

Verification: python build.py -> esp8266 SUCCESS + esp32 SUCCESS (firmware + filesystem both targets, full artifacts produced). python evaluate.py --quick -> 0 failed, 1 pre-existing ESP-abstraction-baseline warning (no regression; this change adds no platform conditionals). Prerelease bumped alpha.115 -> alpha.116 (single commit per project policy).

Blocked AC: on-device behaviour verification is the maintainer's hardware gate (AC #9 left unchecked). ADR-116 remains Proposed pending maintainer acceptance.
<!-- SECTION:FINAL_SUMMARY:END -->
