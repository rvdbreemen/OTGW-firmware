---
id: TASK-2970
title: >-
  UX review: Home Assistant entities of 2.0.0 as the user sees them (names,
  categories, units, grouping)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-20T15:42:00Z'
updated_date: '2026-11-20T15:51:00Z'
labels:
  - ux
  - mqtt
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1102000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Read-only UX review by otgw-ux-designer, suggested by the coordinator: how the 2.0.0 MQTT discovery presents the gateway in Home Assistant (entity names, entity_category, device_class, units, icons, enabled-by-default, grouping per device), from the discovery code and existing HA-rig screenshots. No unit or broker touched. Deliverable: docs/reviews/2026-11-20_ux-ha-entities/README.md with findings (severity, effort, evidence file:line, proposed AC).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Review file lists every finding with heuristic, severity, effort, evidence (file:line) and a proposed AC
- [x] #2 Entity inventory read from the discovery code (counts per component and category), with what could not be verified read-only listed under Not verified
- [x] #3 Findings list delivered to the coordinator for triage
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20: review docs/reviews/2026-11-20_ux-ha-entities/README.md, findings H1-H7. Inventory by an Explore agent, key claims re-checked in the code (no has_entity_name/object_id emitted, device name, writeFriendlyName, typo, binary rows without devCls, BLE without availability). Counts from a parse script over mqttHaSensors[] (405 rows; 221 uncategorised and enabled; 92 raw byte halves, 38 of them uncategorised and enabled). No HA screenshots exist; rendering described from HA's documented behaviour. Findings sent to the coordinator.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
HA entities UX review in docs/reviews/2026-11-20_ux-ha-entities/README.md: 7 findings H1-H7 with severity, effort, file:line evidence and a proposed AC. AC1: the review file. AC2: inventory from the discovery code at origin/dev 9b6a74a85 with counts from a parse script; HA rendering, real-boiler counts and the SAT-duplicate values listed under Not verified. AC3: findings list sent to the coordinator.
<!-- SECTION:FINAL_SUMMARY:END -->
