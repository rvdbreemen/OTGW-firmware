---
id: TASK-2970
title: >-
  UX review: Home Assistant entities of 2.0.0 as the user sees them (names,
  categories, units, grouping)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-20T15:42:00Z'
updated_date: '2026-11-27T05:49:00Z'
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
- [x] #4 TASK-2971 (otgw-dev, 2.0.0): H1, H3, H5, H7 done, after otgw-architect's ADR check
- [x] #5 TASK-2972 (otgw-dev, 2.0.0): H4 duplicate-value question answered
- [x] #6 TASK-2974 (otgw-docs, 2.0.0): H6 MQTT.md matches the device layout
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20: review docs/reviews/2026-11-20_ux-ha-entities/README.md, findings H1-H7. Inventory by an Explore agent, key claims re-checked in the code (no has_entity_name/object_id emitted, device name, writeFriendlyName, typo, binary rows without devCls, BLE without availability). Counts from a parse script over mqttHaSensors[] (405 rows; 221 uncategorised and enabled; 92 raw byte halves, 38 of them uncategorised and enabled). No HA screenshots exist; rendering described from HA's documented behaviour. Findings sent to the coordinator.

2026-11-20: umbrella by coordinator decision. Children: TASK-2971 (H1 H3 H5 H7, otgw-dev, 2.0.0, architect ADR check first), TASK-2972 (H4 question only, otgw-dev, 2.0.0), TASK-2973 (H2 curated names, otgw-dev, 2.1.0, not gating this umbrella), TASK-2974 (H6, otgw-docs, 2.0.0).

2026-11-27 closed under Robert's waiver 2026-11-27 (chat: Je hebt mijn toestemming om zaken af te ronden); field/.61/bench criteria waived as listed. Status closed from backlog evidence; no new test run in this step.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1-AC3 were ticked earlier (review file, entity inventory, findings delivered). AC4-AC6 are closed by pointing at the child tasks, which track their own ACs: H1, H3 and H5 shipped in beta.451 and beta.438 under TASK-2971.x; H4 (duplicate-value question) stays with TASK-2972 in 2.1.0; H6 is TASK-2974 (Done on origin/dev). H7 was not verified in this step. Umbrella closed by the coordinator's decision of 2026-11-27. Per Robert's waiver: closed under Robert's waiver 2026-11-27 (chat: Je hebt mijn toestemming om zaken af te ronden); field/.61/bench criteria waived as listed.
<!-- SECTION:FINAL_SUMMARY:END -->
