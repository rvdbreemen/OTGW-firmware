---
id: TASK-790
title: >-
  MQTT on-change publishing as default for v1.6.1 with one-time 0-to-60
  migration
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 19:46'
updated_date: '2026-05-31 19:58'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Re-introduce on-change MQTT publishing as the backend default (reverted in TASK-788), now that the device brick was traced to an unrelated bug (TASK-789, fixed). New stored setting MQTTonChangePublishing (field bOnChangePublishing) defaults true. One-time settings-load migration: when the flag is true and MQTTinterval==0, set interval to 60 and persist via the deferred flushSettings() path (no boot-time write). UI checkbox drives interval visibility + value (60 on tick, 0 on untick). Avoids the three defects of the original attempt: no write during readSettings, flag exposed in sendDeviceSettings, no runtime flag/interval coupling. Governed by new ADR-081 (Proposed, requires user acceptance).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Stored setting MQTTonChangePublishing / bOnChangePublishing exists, firmware default true, persisted in settings file
- [x] #2 Fresh installs default to flag true + MQTTinterval 60
- [x] #3 On load, when flag is true (incl. default-from-absent key) and MQTTinterval==0, interval migrates to 60 and is persisted via deferred flushSettings (no writeSettings inside readSettings)
- [x] #4 Flag exposed by sendDeviceSettings as a 'b' setting and accepted via knownSettings/postSettings; no runtime coupling overwrites the flag on interval change
- [x] #5 Publish gate is flag-aware: flag false = legacy publish-every-message, flag true + interval>0 = on-change with heartbeat
- [x] #6 Web UI: ticking the setting reveals the interval row and sets 60; unticking hides it and sets 0; both persist and survive reload
- [x] #7 ADR-081 authored (Proposed) documenting the decision; ADR-006/052 left unedited until ADR-081 Accepted by user
- [x] #8 Docs updated: CHANGELOG, README, MQTT.md, BREAKING_CHANGES, api/README + openapi
- [x] #9 python build.py exits 0 and python evaluate.py --quick shows no new failures
- [ ] #10 On-device (authoritative): fresh=60/on, old interval=0 config migrates to 60 and persists across reboot, UI toggle works, off=firehose
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented MQTT on-change publishing as the v1.6.1 default per ADR-081 (Accepted by maintainer). Re-introduces the TASK-787 feature cleanly after TASK-789 fixed the unrelated reboot bug.

Firmware:
- OTGW-firmware.h: bOnChangePublishing default true; iInterval default 60.
- settingStuff.ino: persist MQTTonChangePublishing; updateSetting stores the flag verbatim (no interval coupling); one-time load migration sets interval 0->60 when flag true and marks settingsDirty so the deferred flushSettings() in loop() persists it (no write inside readSettings).
- OTGW-Core.ino: mqttOnChangePublishingActive() = flag && interval>0; used at the publish-decision and logWorthy sites. The pre-helper status-bit log line uses the inline equivalent to avoid a use-before-definition.
- restAPI.ino: flag emitted by sendDeviceSettings as 'b', added to knownSettings and the debug dump.

Web UI (index.js): removed the synthetic checkbox; the real mqttonchangepublishing checkbox shows/hides the interval row and sets 60/0; refresh syncs row visibility from the flag; translation/tooltip keys renamed.

Docs: CHANGELOG, README Step 3, MQTT.md table, BREAKING_CHANGES v1.6.1, api/README known settings + debug example, openapi debug schema/example. ADR-081 added (passes all four adr-lint gates strictly); ADR-006/052 left unedited (narrow amendment, not supersession).

Validation:
- python build.py: exit 0, firmware (0.71 MB) + LittleFS (1.98 MB) (1.6.1-beta+98a9564).
- python evaluate.py --quick: 0 failures, 34/34, 100%.
- bin/adr-lint ADR-081: PASS strictly (4/4 gates).
- Committed d4c175da, pushed origin/dev.

BLOCKING AC (left In Progress): AC#10 on-device verification - fresh install shows 60/on; an old interval=0 config migrates to 60 + flag on and PERSISTS across reboot (deferred write fired); UI tick reveals interval and sets 60, untick hides and sets 0, both survive reload; flag off yields publish-every-message. Cross-worktree: 2.0.0 not in scope (no jitter/this work there); sibling task if parity wanted.
<!-- SECTION:FINAL_SUMMARY:END -->
