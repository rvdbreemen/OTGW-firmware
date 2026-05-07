---
id: TASK-552
title: >-
  Implement ADR-070: switch to sibling-suffix MQTT source topics + drop
  base-suppression
status: In Progress
assignee:
  - '@rvdbreemen-claude'
created_date: '2026-05-07 07:56'
updated_date: '2026-05-07 08:24'
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
- [ ] #1 MQTTstuff.ino:1242 PSTR "%s/thermostat" → "%s_thermostat"
- [ ] #2 MQTTstuff.ino:1246 PSTR "%s/boiler" → "%s_boiler"
- [ ] #3 Comment block at MQTTstuff.ino:1178-1199 updated to describe sibling-suffix shape; cites ADR-070
- [ ] #4 mqtt_configuratie.cpp:2011 separator '/' → '_' in composeSensorPayload's stat_t builder (between label and source segment)
- [ ] #5 mqtt_configuratie.cpp:1999 doc comment updated to '"stat_t":"<mqttPubTopic>/[otgw-pic/]<label>[_<sourceTopicSegment>]"'
- [ ] #6 mqtt_configuratie.cpp:1488-1489 and :1554-1558: bSeparateSources base-suppression else if arms removed; canonical entity emitted unconditionally; replacement comment cites ADR-070 dropping ADR-068
- [ ] #7 mqtt_configuratie.cpp:2406-2408: canonical row {src_suffix_canonical, src_name_canonical, src_seg_canonical} removed from source-variants table; only thermostat and boiler rows remain
- [ ] #8 mqtt_configuratie.cpp:2367-2386 comment block above expandAndStreamSensorSources updated for sibling-suffix shape; cites ADR-070
- [ ] #9 python build.py --firmware exits 0 on dev branch
- [ ] #10 python evaluate.py --quick shows no new failures or warnings (relative to baseline)
- [ ] #11 grep -rn '%s/thermostat\|%s/boiler' src/OTGW-firmware/MQTTstuff.ino returns nothing (only the _thermostat/_boiler literals remain)
- [ ] #12 docs/api/MQTT.md 'Source-Separated Topics' section updated: new topic table + one-paragraph migration note covering orphan retained values at old nested topics
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
