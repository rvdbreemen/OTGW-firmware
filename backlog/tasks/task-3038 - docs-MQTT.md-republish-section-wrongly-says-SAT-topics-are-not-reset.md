---
id: TASK-3038
title: 'docs: MQTT.md republish section wrongly says SAT topics are not reset'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-23T21:23:59Z'
updated_date: '2026-11-23T21:24:45Z'
labels:
  - docs
  - mqtt
dependencies: []
priority: low
ordinal: 1146000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
docs/api/MQTT.md (Republish on Reconnect and Home Assistant Restart) says 'SAT topics are not part of this reset'. Since TASK-1284 requestMQTTRepublishAll() (OTGW-Core.ino:2203) also calls satRepublishCoreState() (SATcontrol.ino:5303), which re-arms the SAT core shadows, the HA climate bridge topics and bumps g_satShadowEpoch so every other SAT shadow republishes too. Docs-only fix.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTT.md states that requestMQTTRepublishAll() also re-publishes the SAT topics, with the code reference
- [x] #2 No other doc in docs/api or docs/c4 repeats the stale claim
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: docs/api/MQTT.md republish section now says requestMQTTRepublishAll() (OTGW-Core.ino:2203) calls satRepublishCoreState() (SATcontrol.ino:5303); topic names checked against satPublishCoreState() SATcontrol.ino:5263-5296. AC2: grep for the stale claim over docs/ (excluding the private decisions submodule) found only MQTT.md:1669, now replaced. Docs-only, no build/evaluator gate.
<!-- SECTION:FINAL_SUMMARY:END -->
