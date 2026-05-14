---
id: TASK-601
title: 'fix(mqtt): JIT discovery — gate on hasConfig so phantom IDs don''t stall drip'
status: To Do
assignee: []
created_date: '2026-05-14 16:57'
labels:
  - mqtt
  - bug
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-073 introduced pure JIT discovery for OT MsgIDs: the JIT trigger in processOT() (OTGW-Core.ino:4112-4116) marks the pending bit for any OT message whose is_value_valid() is true and whose done-bit isn't set yet, on the assumption that loopMQTTDiscovery() will later drain it via doAutoConfigureMsgid().

The drip-loop has an asymmetry with the F (force) path that defeats this assumption: doAutoConfigureMsgid() returns false for any OT ID that has no HA sensor or binsensor entry and is not 0/27/Dallas. When the drip sees that failure (MQTTstuff.ino:1475-1482) it intentionally leaves the pending bit set ("retain pending — next drip tick retries automatically"). On the next tick the bitmap-scan picks the same low-numbered ID, fails again, and never advances. Effectively the drip stalls on the first such "phantom" ID.

markAllMQTTConfigPending() (the F path) is immune: at MQTTstuff.ino:1339 it only sets pending for IDs where sIdx != MQTT_HA_INDEX_NONE || bIdx != MQTT_HA_INDEX_NONE. JIT does not apply that filter, so any OT-bus traffic for an ID without an HA discovery entry — but with a valid OTmap msgcmd — can poison the pending bitmap and freeze further drip progress.

This matches the field report from 1.5.1-beta.3: values publish (is_value_valid true), JIT-mark is logically called, but no entities appear in HA until the user runs F. After F, all 118 active IDs publish in ~5 minutes because F never marks phantoms.

Fix: in the JIT branch in processOT() add the same "hasConfig" check that markAllMQTTConfigPending() already uses, so JIT and F paths are symmetric and cannot enqueue an ID the drip cannot publish.

Scope: src/OTGW-firmware/OTGW-Core.ino:4109-4116 only. No change to drip loop, F path, value-publish layer, or settings.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 JIT branch in processOT() (OTGW-Core.ino:4109-4116) gates setMQTTConfigPending on a hasConfig predicate (readSensorIndex != NONE || readBinSensorIndex != NONE || id == 0 || id == 27)
- [ ] #2 doAutoConfigure (F path) behaviour unchanged: markAllMQTTConfigPending still publishes all known IDs
- [ ] #3 Build green: python build.py --firmware exits 0
- [ ] #4 Evaluator green: python evaluate.py --quick shows no new failures vs baseline
- [ ] #5 Version prerelease bumped (beta.3 → beta.4) via bin/bump-prerelease.sh; version.h + data/version.hash staged
- [ ] #6 Commit message follows project convention; mentions JIT phantom-ID stall + ADR-073 reference
- [ ] #7 Branch claude/fix-jit-mqtt-discovery-N7Yos pushed with draft PR open
<!-- AC:END -->
