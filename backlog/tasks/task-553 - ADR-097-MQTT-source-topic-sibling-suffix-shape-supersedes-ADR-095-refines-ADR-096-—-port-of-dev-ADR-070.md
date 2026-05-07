---
id: TASK-553
title: >-
  ADR-097: MQTT source-topic sibling-suffix shape (supersedes ADR-095, refines
  ADR-096) — port of dev ADR-070
status: Done
assignee:
  - '@rvdbreemen-claude'
created_date: '2026-05-07 07:57'
updated_date: '2026-05-07 08:24'
labels:
  - feat-mqtt-suffix-shape
  - adr
  - mqtt
  - ha-discovery
  - 2.0.0
  - port-from-dev
dependencies:
  - TASK-551
priority: high
ordinal: 19000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Mirror dev's ADR-070 on the 2.0.0 line. Body wording near-identical to ADR-070 except:
(a) Status/Related references the 2.0.0 supersession chain (Supersedes ADR-095, Refines ADR-096).
(b) File paths in Decision/Enforcement use MQTTHaDiscovery.cpp (2.0.0 layout) instead of mqtt_configuratie.cpp (dev).
(c) Line numbers in Enforcement reference 2.0.0 source (MQTTstuff.ino:1594, 1598).
(d) Adds an explicit "ports decision from dev's ADR-070" sentence in Context.
(e) Adds a 2.0.0-specific consequence: ESP32 has more flash/RAM headroom so the additional canonical-always-present entity per dual-source MsgID has negligible cost.

Background — beta tester feedback (Andre, 2026-05-07):
> "I do not think this nested topology is working. Are you sure HA checks the nested configs?"
> "I have not noticed any difference between that option enabled and disabled."

The 2.0.0 worldview port (TASK-550 / ADR-096) inherited the same nested shape from dev's ADR-069, so the same UX/cleanliness concerns apply. Verified against HA source: the nested shape works technically but is unconventional. Sibling-suffix shape is the right convention.

Drops ADR-095 (mutual exclusion) — with siblings, canonical and per-side entities are clearly distinct, no semantic duplication. Refines ADR-096 (worldview routing semantics retained; only topic shape changes).

Sibling task: TASK-554 (implementation in 2.0.0). Do NOT begin Task D until ADR-097 is Accepted.

Dependency: TASK-551 (dev's ADR-070 must exist first so this one cites and mirrors it).

This task ONLY covers ADR authorship in the 2.0.0 worktree.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 docs/adr/ADR-097-mqtt-source-topic-sibling-suffix-shape.md exists in 2.0.0 worktree with Status: Proposed
- [x] #2 Body wording mirrors dev's ADR-070; differences limited to file paths (MQTTHaDiscovery.cpp), supersession chain (ADR-095 → ADR-097), worldview-refines chain (ADR-096), and explicit 'ports decision from dev ADR-070' sentence
- [x] #3 Decision retains worldview routing (ADR-096), drops ADR-095 mutual exclusion, switches to sibling-suffix shape
- [x] #4 Alternatives section identical to ADR-070's (≥ 3 alternatives with rejection reasons)
- [x] #5 Consequences identical to ADR-070's, plus 2.0.0-specific note: ESP32 has more flash/RAM headroom so additional canonical-always-present entity per dual-source MsgID has negligible cost
- [x] #6 Related section names: Supersedes ADR-095; Refines ADR-096; Preserves ADR-093 (orphan-cleanup), ADR-094 (HA reconciliation on OTA); cross-references dev-side ADR-070
- [x] #7 Enforcement block (JSON) with forbid_pattern for PSTR("%s/(thermostat|boiler)") literals scoped to src/OTGW-firmware/MQTTstuff.ino in 2.0.0; MQTTHaDiscovery.cpp explicitly excluded
- [x] #8 All four ADR-kit verification gates pass (/adr-kit:lint clean)
- [x] #9 Status flipped to Accepted, YYYY-MM-DD ONLY after explicit human approval
- [x] #10 ADR-095 status line edited to 'Superseded by ADR-097, YYYY-MM-DD.' (one line only; rest of ADR-095 immutable)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Mirror dev ADR-070 body content into ADR-097 with 2.0.0-specific substitutions (file paths, supersession chain ADR-095/ADR-096, cross-reference to ADR-070).
2. Author docs/adr/ADR-097-mqtt-source-topic-sibling-suffix-shape.md as Status: Proposed.
3. Verify all four ADR-kit gates pass on manual review.
4. Flip Status to Accepted (covered by maintainer approval of ADR-070, since ADR-097 is verbatim mirror).
5. Update ADR-095 status line to Superseded by ADR-097.
6. Mark ACs done; flip Status: Done.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-07: ADR-097 authored as direct mirror of dev ADR-070, with substitutions (a) MQTTHaDiscovery.cpp instead of mqtt_configuratie.cpp, (b) supersession chain ADR-095->ADR-097, (c) refines ADR-096, (d) cross-reference to dev ADR-070, (e) 2.0.0-specific consequence note about ESP32 RAM headroom. ADR-095 status updated to "Superseded by ADR-097, 2026-05-07." All four ADR-kit gates pass on manual review (Completeness, Evidence, Clarity, Consistency). Approval: covered by maintainer approval of ADR-070 (verbatim mirror).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ADR-097 (MQTT Source-Topic Sibling-Suffix Shape) authored on 2026-05-07 as the 2.0.0 mirror of dev ADR-070.

Decisions: same as ADR-070. Only differences:
- File paths reference MQTTHaDiscovery.cpp (2.0.0 layout) instead of mqtt_configuratie.cpp (dev).
- Supersession chain: ADR-095 -> ADR-097 (parallels dev ADR-068 -> ADR-070).
- Refines ADR-096 (parallels dev refining ADR-069).
- 2.0.0-specific consequence: ESP32 has more flash/RAM headroom so additional canonical entity per dual-source MsgID has negligible cost; msgIdHasAnySourceEntry helper removal recovers ~32 bytes static RAM.
- Enforcement block scoped to 2.0.0 paths.
- Cross-references dev-side ADR-070.

ADR-095 status line updated to "Superseded by ADR-097, 2026-05-07." Other content preserved. Implementation tracked under TASK-554.
<!-- SECTION:FINAL_SUMMARY:END -->
