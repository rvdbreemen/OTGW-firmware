---
id: TASK-1002
title: >-
  docs(adr): relabel ADR-142 as Deferred to 2.1.0 and note its CI gate is a
  precondition of acceptance
status: To Do
assignee: []
created_date: '2026-07-04 15:55'
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
- [ ] #1 ADR-142 Status line reads Deferred to 2.1.0 (or equivalent) rather than plain Proposed
- [ ] #2 A note records that check_ha_discovery_msgid_availability_list must exist in evaluate.py as a precondition of acceptance (ADR-080)
<!-- AC:END -->
