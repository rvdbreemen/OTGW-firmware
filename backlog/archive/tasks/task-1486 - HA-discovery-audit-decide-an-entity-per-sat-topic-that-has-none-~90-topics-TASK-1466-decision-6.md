---
id: TASK-1486
title: >-
  HA discovery audit: decide an entity per sat/* topic that has none (~90
  topics) (TASK-1466 decision 6)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T20:31:00Z'
updated_date: '2026-10-31T05:26:00Z'
labels:
  - sat
  - mqtt
  - ha
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 645000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-29: full audit of all sat/* MQTT topics without a Home Assistant entity (153 published, 63 discovered, 2026-10-29 count). Per topic: keep MQTT-only (diagnostic) or add an entity (type, device class, entity category), with thermo-nova's HA entities (binary_sensor.py, sensor.py) as reference. Output: a table in docs, then a task for the additions. Health/sync entities follow the ON = problem convention (TASK-1466 decision 4).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A reviewed table lists every undiscovered sat/* topic with a decision and reason; additions filed as one implementation task; flash/heap cost estimated
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29: audit written: docs/sat/sat-ha-entity-audit.md (linked from docs/api/MQTT.md SAT Discovery Entities). Method: 178 F("sat/...") literals in SAT*.ino vs 95 sat/ paths in MQTTHaDiscovery.cpp -> 97 unmatched, +3 pub-namespace topics that share a name with a switch command (solar_gain, auto_tune, simulation), -9 PV-boost topics discovered in MQTTstuff.ino:2774-2786 = 91 topics without an entity. Decisions: 13 add, 10 open for Robert (with recommendation), 68 MQTT-only. Flash measured from alpha.535 ELF via nm: sensor row 28 B, binary row 32 B, strings ~18 B each; estimate ~0.9 KB for the 13 adds, ~1.5 KB with all recommended open adds, 0 B static heap. Second reader (subagent) refuted: missed PV discovery path (fixed 100->91), setpoint_mismatch is not the undebounced setpoint_sync (fixed), sensor.py:62-65 citation, climate.py attribute list, gate wording (all fixed). Draft implementation task sent to the 2.0.0 lead, who creates tasks.

2026-10-31 maintainer decision (Robert, in session otgw-firmware-ea): the audit recommendation for the 10 open topics is accepted as written in docs/sat/sat-ha-entity-audit.md section 2: Add binary_sensor problem (diagnostic) for sat/fallback_active, sat/room_temp_lost, sat/ot_link_lost, sat/push_refused; Add switch for sat/enabled (command enabled) and select for sat/preset (command preset); MQTT-only for sat/pressure_alarm, sat/setpoint_mismatch, sat/modulation_reliable, sat/control_mode. The 6 adds join the implementation in TASK-1490. Estimated ~1.5 KB flash with all adds.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audit docs/sat/sat-ha-entity-audit.md lists every sat/* topic without an HA entity (91) with a decision and reason: 13 add, 68 MQTT-only, 10 put to Robert, who accepted the recommendation on 2026-10-31 (6 more adds, 4 MQTT-only). Flash cost measured from the alpha.535 ELF (~0.9 KB for 13 adds, ~1.5 KB with the 6 more). Implementation: TASK-1490.
<!-- SECTION:FINAL_SUMMARY:END -->
