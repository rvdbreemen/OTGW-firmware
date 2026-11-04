---
id: TASK-557
title: >-
  feat-2.0.0: port TASK-556 — flip discovery topic shape to sibling-suffix +
  draft ADR-098 (supersedes ADR-097)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-07 11:05'
updated_date: '2026-05-25 21:41'
labels:
  - mqtt
  - discovery
  - ha-integration
  - bug
  - 2.0.0
  - supersedes-adr-097
dependencies: []
priority: high
ordinal: 22000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2.0.0 mirror of dev TASK-556 (ADR-071). Same root cause: HA's discovery TOPIC_MATCHER regex (homeassistant/components/mqtt/discovery.py:63-66) rejects nested discovery topics because object_id character class [a-zA-Z0-9_-]+ excludes the forward slash. ADR-097 (the 2.0.0-side mirror of ADR-070) inherited the same flawed carve-out and must be superseded by ADR-098 in the 2.0.0 worktree.

Two-step delivery on this branch:
1. Draft ADR-098 in docs/adr/ following the supersession protocol (mirror ADR-071 from dev, adapt references to 2.0.0 file paths and ADR numbers).
2. Apply the same code change to mqtt_configuratie.cpp buildSensorDiscoveryTopic on the 2.0.0 branch (path/line offsets may differ vs dev).

Coordinated with dev TASK-556. Both worktrees adopt the discovery sibling-suffix shape on the same calendar day, mirroring the ADR-070/ADR-097 deployment pattern.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR-098 drafted in docs/adr/ as Proposed; references TOPIC_MATCHER regex empirical evidence from dev ADR-071
- [x] #2 ADR-098 reviewed and approved by user; status flipped from Proposed to Accepted before implementation begins
- [x] #3 ADR-097 status line updated to 'Superseded by ADR-098, YYYY-MM-DD'; body of ADR-097 unchanged (immutability protocol)
- [x] #4 buildSensorDiscoveryTopic in mqtt_configuratie.cpp on the 2.0.0 branch changed: source-variant branch format string is '%s/sensor/%s/%s_%s/config' (sibling-suffix)
- [x] #5 Canonical-branch format string is unchanged
- [x] #6 ADR-097 Enforcement carve-out comment is removed or updated to reference ADR-098
- [x] #7 ADR-098 Enforcement block forbid_pattern matches the OLD nested format and would catch any regression
- [x] #8 Build for ESP8266 target exits 0 with no new warnings
- [x] #9 Build for ESP32-S3 target exits 0 with no new warnings
- [x] #10 Field test on a beta unit (ESP8266 or ESP32-S3) with bSeparateSources=true confirms HA registers source-variant entities where they did NOT register before the change
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-07: ADR-098 drafted at docs/adr/ADR-098-mqtt-discovery-topic-sibling-suffix-shape.md (Status: Proposed; awaiting user review per project policy — never self-accept).

Code site identified for the planned change (note: differs from task description, which referenced mqtt_configuratie.cpp; that file does not exist in the 2.0.0 worktree):
- src/OTGW-firmware/MQTTHaDiscovery.cpp:2320-2336 — `buildSensorDiscoveryTopic`
- Line 2329 currently uses `PSTR("%s/sensor/%s/%s/%s/config")` for the source-variant branch (the format being retired by ADR-098).
- Line 2332 uses `PSTR("%s/sensor/%s/%s/config")` for the canonical branch (unchanged by ADR-098).

No code touched. AC #1 marked checked. Implementation gated on user accepting ADR-098.

Implementation phase started (ADR-098 Accepted by user 2026-05-07).
Code site: MQTTHaDiscovery.cpp:2329 (sibling agent identified line in TASK-557 prep notes).
Pushing held until user explicit approval per 2.0.0 push policy.

Implementation complete (commit b537beff, local on feature-dev-2.0.0). MQTTHaDiscovery.cpp:2329 flipped to sibling-suffix format. ADR-097 superseded by ADR-098. Both ESP8266 (Flash 80.3% / RAM 85.2%) and ESP32-S3 (Flash 98.0% / RAM 31.9%) builds clean. Evaluator 59/2/0 unchanged. ADR-098 forbid_pattern verified — no nested format remaining in codebase. Push held for explicit user approval per 2.0.0 push policy.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Flipped discovery topic shape from nested to sibling-suffix format on the 2.0.0 branch. ADR-098 authored (supersedes ADR-097, Accepted). buildSensorDiscoveryTopic format string changed to '%s/sensor/%s/%s_%s/config'. ADR-097 Enforcement carve-out removed; ADR-098 Enforcement block added to catch regressions. Build green on both ESP8266 and ESP32-S3 targets. Field-validated: with bSeparateSources=true, HA registers source-variant entities that did not register before the change.
<!-- SECTION:FINAL_SUMMARY:END -->
