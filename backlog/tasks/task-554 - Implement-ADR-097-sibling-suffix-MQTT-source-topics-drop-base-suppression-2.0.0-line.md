---
id: TASK-554
title: >-
  Implement ADR-097: sibling-suffix MQTT source topics + drop base-suppression
  (2.0.0 line)
status: Done
assignee:
  - '@rvdbreemen-claude'
created_date: '2026-05-07 07:57'
updated_date: '2026-05-07 15:48'
labels:
  - feat-mqtt-suffix-shape
  - mqtt
  - impl
  - ha-discovery
  - 2.0.0
  - port-from-dev
dependencies:
  - TASK-553
  - TASK-552
priority: high
ordinal: 20000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the dev-side Task B (TASK-552) implementation to the 2.0.0 line, applying ADR-097.

Differences from dev's TASK-552:
- Discovery file is MQTTHaDiscovery.cpp (not mqtt_configuratie.cpp).
- Different line numbers in MQTTstuff.ino (publish path at 1594, 1598 vs dev's 1242, 1246).
- 2.0.0 supports BOTH ESP8266 and ESP32 (OTGW32) targets — verification must cover both.
- ADR cite changes from ADR-070 → ADR-097.

Current shape (after TASK-550 / ADR-096):
  <base>/value/<id>            canonical
  <base>/value/<id>/thermostat thermostat-side (nested)
  <base>/value/<id>/boiler     boiler-side (nested)

New shape (per ADR-097):
  <base>/value/<id>            canonical (unchanged)
  <base>/value/<id>_thermostat thermostat-side (sibling)
  <base>/value/<id>_boiler     boiler-side (sibling)

Surgical changes:
1. MQTTstuff.ino:1594, 1598 — PSTR literal swaps.
2. MQTTHaDiscovery.cpp:2169 — separator swap in stat_t builder.
3. MQTTHaDiscovery.cpp:1958-1960, 2043-2045 — drop base-suppression branches.
4. MQTTHaDiscovery.cpp:2589-2594 — drop canonical row from source-variants table.
5. Comment updates citing ADR-097.
6. docs/api/MQTT.md — same migration note as dev.

Discovery topic identifiers stay nested (HA-internal; keeping stable supports in-place updates).

Dependencies:
- TASK-553: ADR-097 must be Accepted.
- TASK-552: dev impl ships first so this can do parity review (AC #14).

CRITICAL: must be executed from inside the 2.0.0 worktree shell (CLAUDE.md cross-tree CLI rule). The backlog MCP server must NEVER be used (CLAUDE.md backlog rule).

Push policy: 2.0.0 feature branch is NOT auto-approved by CLAUDE.md (dev-only standing). Ask before pushing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTTstuff.ino:1594 PSTR "%s/thermostat" → "%s_thermostat" (2.0.0 line)
- [x] #2 MQTTstuff.ino:1598 PSTR "%s/boiler" → "%s_boiler" (2.0.0 line)
- [x] #3 Comment block at MQTTstuff.ino:1530-1551 updated for sibling-suffix shape; cites ADR-097
- [x] #4 MQTTHaDiscovery.cpp:2169 separator '/' → '_' between label and source segment in composeSensorPayload's stat_t builder
- [x] #5 MQTTHaDiscovery.cpp:2157 doc comment updated for sibling-suffix shape
- [x] #6 MQTTHaDiscovery.cpp:1958-1960 and :2043-2045: bSeparateSources base-suppression branches removed; canonical entity emitted unconditionally; replacement comment cites ADR-097 dropping ADR-095
- [x] #7 MQTTHaDiscovery.cpp:2589-2594: canonical row dropped from source-variants table; only thermostat and boiler rows remain
- [x] #8 MQTTHaDiscovery.cpp:2556-2580 expandAndStreamSensorSources comment block updated; cites ADR-097
- [x] #9 python build.py --firmware exits 0 for both ESP8266 and ESP32 targets in 2.0.0
- [x] #10 python evaluate.py --quick shows no new failures (relative to baseline)
- [x] #11 grep -rn '%s/thermostat\|%s/boiler' src/OTGW-firmware/MQTTstuff.ino (in 2.0.0 worktree) returns nothing
- [x] #12 docs/api/MQTT.md (2.0.0 copy) updated with the same migration note as TASK-552
- [ ] #13 Hardware-pending verification: dual-target test (one ESP8266 with PIC, one OTGW32/ESP32 if available) with active CS= override
- [x] #14 Parity review: diff vs TASK-552 is content-equivalent — only differences are filenames, comment line numbers, and ADR cite (ADR-097 vs ADR-070)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. ADR-097 must be Accepted (verified via TASK-553 Done state). Dev impl TASK-552 reference for parity.
2. Edit MQTTstuff.ino:1594,1598 PSTR literals (slash -> underscore).
3. Edit MQTTstuff.ino:1525-1551 comment block; cite ADR-097.
4. Delete msgIdHasAnySourceEntry helper at lines 1906-1925 (dead code); leave one-liner comment citing ADR-097 superseding ADR-095.
5. Collapse if/else at MQTTstuff.ino:1953-1966 (doAutoConfigure) and 2039-2049 (doAutoConfigureMsgid) so canonical entity always emits.
6. Edit MQTTHaDiscovery.cpp:2169 separator slash -> underscore.
7. Update MQTTHaDiscovery.cpp:2157 doc comment.
8. Update MQTTHaDiscovery.cpp:2556-2580 expandAndStreamSensorSources comment block; cite ADR-097.
9. Drop canonical row from source-variants table at MQTTHaDiscovery.cpp:2583-2598; table now has 2 rows.
10. Update docs/api/MQTT.md "Source-Separated Topics" + "Source-Separated Discovery" + developer-doc references in three places.
11. Bump version.h _VERSION_PRERELEASE to alpha.4.
12. python build.py --firmware (both ESP8266 and ESP32 targets must exit 0).
13. python evaluate.py --quick.
14. Commit on feature branch. PUSH NOT AUTO-ALLOWED on feature-* branches per CLAUDE.md; ask user before pushing.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-07: Implementation shipped locally on feature-dev-2.0.0-otgw32-esp32-sat-support as commit 4a2a5b9a. Push to origin pending maintainer approval (CLAUDE.md push policy: feature-* branches require explicit per-instance confirmation). All code-verifiable ACs (1-12, 14) checked. AC #13 (hardware verification) cannot self-verify; documented as exception per CLAUDE.md autonomous-completion policy. Build green for both ESP8266 and ESP32 targets (2.0.0-alpha.4+7af27b6). Evaluator: 59 passed, 2 pre-existing warnings, 0 failures. Health 97.1%.

AC #14 parity check vs dev TASK-552 (commit 7c33e0c9):
- Same routing change in publishToSourceTopic (PSTR literal swap).
- Same separator change in composeSensorPayload stat_t builder.
- Same suppression-branch collapse and canonical-row removal.
- Same MQTT.md migration note structure.
- Differences are exactly: file paths (MQTTHaDiscovery.cpp vs mqtt_configuratie.cpp), line numbers (1594/1598 vs 1242/1246), ADR cite (ADR-097 vs ADR-070), supersession chain (ADR-095 vs ADR-068), refines chain (ADR-096 vs ADR-069), and 2.0.0-specific consequence note about ESP32 RAM headroom. Content-equivalent — verified by manual diff comparison.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implements ADR-097 (2.0.0 mirror of dev ADR-070) on the feature-2.0.0 line.

Identical decisions to dev TASK-552 — sibling-suffix shape (<id>_thermostat, <id>_boiler) replaces nested children; ADR-095 mutual-exclusion rule dropped; ADR-096 worldview routing preserved. See TASK-552 Final Summary for problem statement and root-cause investigation.

2.0.0-specific notes:
- Discovery file is MQTTHaDiscovery.cpp (separate translation unit per ADR-077 streaming architecture); changes mirror dev mqtt_configuratie.cpp edits.
- Publish path lives at MQTTstuff.ino:1594/1598 (line offsets differ from dev 1242/1246).
- ESP32 (OTGW32) targets unaffected aside from same source-tree edits — both ESP8266 and ESP32 build clean.
- msgIdHasAnySourceEntry helper removal (originally TASK-528 / ADR-095) recovers ~32 bytes static RAM; on ESP32 the saving is negligible but on ESP8266 (which 2.0.0 still supports) it is meaningful.

Changes (commit 4a2a5b9a, NOT YET PUSHED):
  1. MQTTstuff.ino:1525-1551 — comment block refreshed; cites ADR-097.
  2. MQTTstuff.ino:1594,1598 — PSTR literal swaps.
  3. MQTTstuff.ino:1906-1925 — msgIdHasAnySourceEntry helper deleted; one-liner ADR-097 cite remains.
  4. MQTTstuff.ino:1953-1966, 2039-2049 — base-suppression branches collapsed.
  5. MQTTHaDiscovery.cpp:2157-2173 — composeSensorPayload stat_t builder; separator slash->underscore.
  6. MQTTHaDiscovery.cpp:2556-2598 — expandAndStreamSensorSources comment + canonical-row removal.
  7. docs/api/MQTT.md — Source-Separated Topics + Source-Separated Discovery + developer-doc references updated in three places.
  8. docs/adr/ADR-097-mqtt-source-topic-sibling-suffix-shape.md (new, Accepted 2026-05-07).
  9. docs/adr/ADR-095-... status flipped to Superseded by ADR-097.
 10. version.h bumped to 2.0.0-alpha.4.

Verification:
  - python build.py --firmware: exit 0.
  - python evaluate.py --quick: 59 passed, 2 pre-existing warnings, 0 failures.
  - grep invariants confirmed.
  - HA migration automatic via ADR-094 boot-time discovery republish.

Push blocker: CLAUDE.md feature-* push policy requires explicit per-instance maintainer approval. Awaiting.
<!-- SECTION:FINAL_SUMMARY:END -->
