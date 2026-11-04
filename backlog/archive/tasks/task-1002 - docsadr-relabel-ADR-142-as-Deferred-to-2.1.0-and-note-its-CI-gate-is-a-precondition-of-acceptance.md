---
id: TASK-1002
title: >-
  docs(adr): relabel ADR-142 as Deferred to 2.1.0 and note its CI gate is a
  precondition of acceptance
status: Done
assignee: []
created_date: '2026-07-04 15:55'
updated_date: '2026-07-09 20:34'
labels:
  - docs
  - adr
dependencies: []
ordinal: 214000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-142 (mark unsupported-msgID HA entities unavailable via a 2-entry availability list + availability_mode:all) is the only genuinely-unimplemented Proposed ADR: no availability_mode anywhere in src/, and its self-named CI gate check_ha_discovery_msgid_availability_list does not exist in evaluate.py. It self-labels Milestone 2.1.0. Sitting in plain Proposed makes it look like it is awaiting an acceptance sign-off it is not ready for. Relabel its Status to make the deferral explicit, and record that per ADR-080 the named gate must land with the implementation before it can flip to Accepted.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR-142 Status line reads Deferred to 2.1.0 (or equivalent) rather than plain Proposed
- [x] #2 A note records that check_ha_discovery_msgid_availability_list must exist in evaluate.py as a precondition of acceptance (ADR-080)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-09: ADR-142 Status relabelled Proposed -> Deferred (frontmatter status: Deferred; Status body now reads 'Deferred to Milestone 2.1.0' with the explicit ADR-080 precondition that check_ha_discovery_msgid_availability_list must land in evaluate.py in the same commit as the feature before it can flip to Accepted; Status History entry appended). README index badge updated to '(Deferred to 2.1.0)'. Decision content (Approach B) left intact.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ADR-142 relabelled from plain Proposed to Deferred (2.1.0) so it no longer reads as awaiting a review sign-off it is not ready for, and records per ADR-080 that its named CI gate is a precondition of acceptance. Docs-only.
<!-- SECTION:FINAL_SUMMARY:END -->
