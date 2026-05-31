---
id: TASK-790
title: >-
  MQTT on-change publishing as default for v1.6.1 with one-time 0-to-60
  migration
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 19:46'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Re-introduce on-change MQTT publishing as the backend default (reverted in TASK-788), now that the device brick was traced to an unrelated bug (TASK-789, fixed). New stored setting MQTTonChangePublishing (field bOnChangePublishing) defaults true. One-time settings-load migration: when the flag is true and MQTTinterval==0, set interval to 60 and persist via the deferred flushSettings() path (no boot-time write). UI checkbox drives interval visibility + value (60 on tick, 0 on untick). Avoids the three defects of the original attempt: no write during readSettings, flag exposed in sendDeviceSettings, no runtime flag/interval coupling. Governed by new ADR-081 (Proposed, requires user acceptance).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Stored setting MQTTonChangePublishing / bOnChangePublishing exists, firmware default true, persisted in settings file
- [ ] #2 Fresh installs default to flag true + MQTTinterval 60
- [ ] #3 On load, when flag is true (incl. default-from-absent key) and MQTTinterval==0, interval migrates to 60 and is persisted via deferred flushSettings (no writeSettings inside readSettings)
- [ ] #4 Flag exposed by sendDeviceSettings as a 'b' setting and accepted via knownSettings/postSettings; no runtime coupling overwrites the flag on interval change
- [ ] #5 Publish gate is flag-aware: flag false = legacy publish-every-message, flag true + interval>0 = on-change with heartbeat
- [ ] #6 Web UI: ticking the setting reveals the interval row and sets 60; unticking hides it and sets 0; both persist and survive reload
- [ ] #7 ADR-081 authored (Proposed) documenting the decision; ADR-006/052 left unedited until ADR-081 Accepted by user
- [ ] #8 Docs updated: CHANGELOG, README, MQTT.md, BREAKING_CHANGES, api/README + openapi
- [ ] #9 python build.py exits 0 and python evaluate.py --quick shows no new failures
- [ ] #10 On-device (authoritative): fresh=60/on, old interval=0 config migrates to 60 and persists across reboot, UI toggle works, off=firehose
<!-- AC:END -->
