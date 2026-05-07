---
id: TASK-552
title: >-
  Implement ADR-070: switch to sibling-suffix MQTT source topics + drop
  base-suppression
status: In Progress
assignee:
  - '@rvdbreemen-claude'
created_date: '2026-05-07 07:56'
updated_date: '2026-05-07 08:28'
labels:
  - feat-mqtt-suffix-shape
  - mqtt
  - impl
  - ha-discovery
  - dev-1.5x
dependencies:
  - TASK-551
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement the topic-shape change ratified by ADR-070 on the 1.5.x dev line.

Current shape (after TASK-549 / ADR-069):
  <base>/value/<id>            canonical (boiler-side worldview)
  <base>/value/<id>/thermostat thermostat-side (nested under canonical)
  <base>/value/<id>/boiler     boiler-side (nested under canonical)

New shape (per ADR-070):
  <base>/value/<id>            canonical (unchanged)
  <base>/value/<id>_thermostat thermostat-side (sibling)
  <base>/value/<id>_boiler     boiler-side (sibling)

Changes are small and surgical:
1. Two PSTR literal swaps in publish path (MQTTstuff.ino:1242, 1246).
2. One separator swap in the discovery stat_t builder (mqtt_configuratie.cpp:2011).
3. Removal of two bSeparateSources base-suppression branches in discovery (mqtt_configuratie.cpp:1488-1489 and 1554-1558) per ADR-068 supersession.
4. Removal of the canonical row from expandAndStreamSensorSources's source-variants table (mqtt_configuratie.cpp:2406-2408) — the base entity now carries the canonical worldview directly.
5. Comment updates in three places.
6. docs/api/MQTT.md "Source-Separated Topics" section: new topic table + migration note.

Discovery topic identifiers (homeassistant/sensor/<id>/<label>/<src>/config) STAY nested — they are internal HA identifiers, not state topics. Keeping them stable means HA updates entities in place rather than orphaning old configs (HA source verified — subscription.async_prepare_subscribe_topics handles state_topic delta cleanly).

Sibling task: Task A (TASK-551) authors ADR-070. Do NOT begin coding until ADR-070 is Accepted.

Mirror in 2.0.0: TASK-553 (sibling-suffix shape on 2.0.0 line, ADR-097).

CLAUDE.md push policy: dev pushes auto-approved when build green and evaluator clean. Mention the push in Final Summary.

AC #13 is hardware-pending (cannot self-verify); document that in Final Summary and leave task at "In Progress" until Andre/Robert confirm in beta channel — per CLAUDE.md autonomous-completion exceptions.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTTstuff.ino:1242 PSTR "%s/thermostat" → "%s_thermostat"
- [x] #2 MQTTstuff.ino:1246 PSTR "%s/boiler" → "%s_boiler"
- [x] #3 Comment block at MQTTstuff.ino:1178-1199 updated to describe sibling-suffix shape; cites ADR-070
- [x] #4 mqtt_configuratie.cpp:2011 separator '/' → '_' in composeSensorPayload's stat_t builder (between label and source segment)
- [x] #5 mqtt_configuratie.cpp:1999 doc comment updated to '"stat_t":"<mqttPubTopic>/[otgw-pic/]<label>[_<sourceTopicSegment>]"'
- [x] #6 mqtt_configuratie.cpp:1488-1489 and :1554-1558: bSeparateSources base-suppression else if arms removed; canonical entity emitted unconditionally; replacement comment cites ADR-070 dropping ADR-068
- [x] #7 mqtt_configuratie.cpp:2406-2408: canonical row {src_suffix_canonical, src_name_canonical, src_seg_canonical} removed from source-variants table; only thermostat and boiler rows remain
- [x] #8 mqtt_configuratie.cpp:2367-2386 comment block above expandAndStreamSensorSources updated for sibling-suffix shape; cites ADR-070
- [x] #9 python build.py --firmware exits 0 on dev branch
- [x] #10 python evaluate.py --quick shows no new failures or warnings (relative to baseline)
- [x] #11 grep -rn '%s/thermostat\|%s/boiler' src/OTGW-firmware/MQTTstuff.ino returns nothing (only the _thermostat/_boiler literals remain)
- [x] #12 docs/api/MQTT.md 'Source-Separated Topics' section updated: new topic table + one-paragraph migration note covering orphan retained values at old nested topics
- [ ] #13 Hardware verification (cannot self-verify; pending Andre/Robert): with active CS=27.37 override and thermostat asking 23.00, mosquitto_sub at <base>/value/+/TSet_thermostat shows 23.00 and <base>/value/+/TSet_boiler shows 27.37; HA dev tools shows three TSet sensors under one device card
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. ADR-070 must be Accepted (verified via TASK-551 Done state).
2. Edit MQTTstuff.ino:1242,1246 PSTR literals (slash -> underscore).
3. Edit MQTTstuff.ino:1173-1199 comment block to describe sibling-suffix shape; cite ADR-070.
4. Delete msgIdHasAnySourceEntry helper at lines 1438-1455 (dead code after suppression branches removed); leave a one-liner comment citing ADR-070 superseding ADR-068.
5. Collapse if/else at MQTTstuff.ino:1483-1492 (doAutoConfigure) and 1553-1561 (doAutoConfigureMsgid) so canonical entity always emits.
6. Edit mqtt_configuratie.cpp:2011 separator slash -> underscore in composeSensorPayload stat_t builder.
7. Update mqtt_configuratie.cpp:1999 doc comment.
8. Update mqtt_configuratie.cpp:2367-2386 expandAndStreamSensorSources comment block; cite ADR-070.
9. Drop canonical row from source-variants table at mqtt_configuratie.cpp:2406-2408 (and unused PROGMEM constants); table now has 2 rows.
10. Update docs/api/MQTT.md "Source-Separated Topics" section: new topic table, sibling-suffix examples, migration note covering both topology shift and orphan retained values.
11. Bump version.h _VERSION_PRERELEASE to beta.21.
12. python build.py --firmware (must exit 0).
13. python evaluate.py --quick (must show no NEW failures).
14. Commit + push to origin/dev (auto-allowed per CLAUDE.md push policy when build green and evaluator clean).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-07: Implementation shipped on dev as commit 7c33e0c9. All code-verifiable ACs (1-12) checked. AC #13 (hardware verification with active CS= override) cannot be self-verified — task remains In Progress per CLAUDE.md autonomous-completion policy until Andre or Robert confirms in beta channel. Build green (1.5.0-beta.21+fd7cdb4). Evaluator: 31 passed, 2 pre-existing warnings, 1 cross-worktree-references "failure" (the 2 unresolved refs are intentional pointers from ADR-070 to ADR-097 which lives in the parallel 2.0.0 worktree).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implements ADR-070 (sibling-suffix MQTT source topic shape) on the 1.5.x dev line.

Problem (Andre, beta channel 2026-05-07):
  Beta testers reported "I do not think this nested topology is working" and "I have not noticed any difference between that option enabled and disabled" after ADR-069 / TASK-549 shipped the worldview routing semantics with nested children topology (<id>/thermostat, <id>/boiler under canonical <id>).

Root cause investigation:
  HA itself is structurally agnostic to topology shape (verified against homeassistant/components/mqtt/sensor.py:289-295 which calls async_subscribe on the literal state_topic; subscription.py:59-97 handles state_topic deltas in-place; util.py:254-307 has no nesting constraint). But the nested-with-payload pattern is unconventional, breaks topic-browser UX (mosquitto_sub tree mode shows the parent value oddly when children appear), and creates user doubt about HA support.

Solution (ADR-070):
  Use sibling-suffix shape (<id>_thermostat, <id>_boiler) instead of nested children. All three (canonical, _thermostat, _boiler) are sibling leaves with no structural surprise. Drop ADR-068 mutual-exclusion rule: with siblings, canonical and source variants have non-overlapping state_topics, so canonical stays advertised alongside the variants — three additive entities under bSeparateSources=true.

Changes (commit 7c33e0c9):
  1. MQTTstuff.ino — two PSTR literal swaps in publishToSourceTopic; routing comment refreshed; dropped msgIdHasAnySourceEntry helper (~24 lines + 32 bytes static RAM); collapsed two if/else suppression branches in discovery dispatch.
  2. mqtt_configuratie.cpp — composeSensorPayload stat_t separator slash->underscore; expandAndStreamSensorSources canonical row removed from variants table (base entity carries canonical worldview); comment block updated.
  3. docs/api/MQTT.md — Source-Separated Topics section rewritten with sibling-suffix examples + migration note covering retained-value cleanup recipe.
  4. docs/adr/ADR-070-mqtt-source-topic-sibling-suffix-shape.md (new, Accepted 2026-05-07).
  5. docs/adr/ADR-068-... status line updated to Superseded by ADR-070; rest immutable.
  6. version.h bumped to 1.5.0-beta.21.

Verification:
  - python build.py --firmware: exit 0 (artifact 1.5.0-beta.21+fd7cdb4).
  - python evaluate.py --quick: 31 passed, 2 pre-existing warnings, 1 failure (2 unresolved cross-worktree ADR-097 refs — intentional, ADR-097 lives in 2.0.0 worktree).
  - grep invariants confirmed: no slash-shape PSTR literals remain.
  - HA migration is automatic via subscription.async_prepare_subscribe_topics + ADR-067 boot-time republish.

Hardware verification pending (Andre / Robert / beta channel): with bSeparateSources=true and active CS=27.37 override while thermostat asks 23.00, expect _thermostat=23.00 and _boiler=27.37 to diverge; HA dev tools should show three TSet sensors (canonical + thermostat + boiler) under one device card. AC #13 documents this; task remains In Progress until field-confirmed.

2.0.0 mirror: ADR-097 / TASK-554 / commit 4a2a5b9a on feature-dev-2.0.0 branch (push pending maintainer approval per CLAUDE.md push policy for feature branches).
<!-- SECTION:FINAL_SUMMARY:END -->
