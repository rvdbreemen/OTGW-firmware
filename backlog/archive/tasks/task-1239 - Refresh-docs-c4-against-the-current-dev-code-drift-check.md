---
id: TASK-1239
title: Refresh docs/c4 against the current dev code (drift check)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-16 03:55'
updated_date: '2026-10-16 04:19'
labels:
  - docs
  - c4
dependencies: []
priority: medium
ordinal: 377000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer request 2026-10-16 (/c4-architecture, option: refresh the existing docs/c4 set, no new C4-Documentation/ dir). Several C4 files date from April/May 2026 and predate the v2-only UI (ADR-192), safeboot OTA (ADR-191), combo board detection, the SAT PIC CS keepalive (TASK-1236) and more. Check each file against the code and update only what drifted.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every docs/c4 file is checked against the current src/ and stale statements are corrected (file:line or symbol references verified)
- [x] #2 c4-component.md and c4-context.md reflect the updated component files
- [x] #3 No new parallel C4 set is created; links between docs/c4 files resolve
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Four parallel c4 agents, each owns a disjoint file set and edits in place, no commits/builds: (A) sensors + settings/configuration-state + utilities; (B) SAT code + smart-thermostat component; (C) web-interface component + otgw-core/otdirect code + opentherm-core component; (D) mqtt/rest-api code + integration-layer + network code/component + container. Then the main thread updates c4-component.md and c4-context.md, checks links, commits.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Refreshed all 20 docs/c4 files in place against alpha.427 (no new C4 set). Four parallel c4 agents checked their file groups against the source; the main thread rewrote c4-component.md and c4-context.md, cleaned the ESP8266 remnants and the invented API table in c4-code-web-assets.md, fixed the opentherm-core topic list, and grep-checked the whole set for stale terms (ESP8266 framing, PubSubClient, port 81, OPV, old SAT topics, ArduinoOTA, GitHub one-click OTA). Evidence: link check 0 broken links; 0 em dashes; spot checks of agent claims against source (MAX_WEBSOCKET_CLIENTS 3, REST heap clamp <16000, MQTT backoff 3 s x attempt then 42 s, MQTT_HA_SENSOR_COUNT 390) all match; tests/test_adr_governance.py OK. Also fixed the stale addOTWGcmdtoqueue() name in CLAUDE.md and AGENTS.md (the function is addCommandToQueue()). Two code findings went to TASK-1240. Not verified: some counts the agents marked approximate (about 150 SAT topics), and docs/api/MQTT.md was not cross-checked line by line.
<!-- SECTION:FINAL_SUMMARY:END -->
