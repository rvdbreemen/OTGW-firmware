---
id: TASK-1191
title: Correct ADR-176's premise on an unclean reboot lowering the water total
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-01 18:26'
updated_date: '2026-10-01 18:26'
labels:
  - 2.0.0
  - adr
  - water-meter
dependencies: []
priority: low
ordinal: 313000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1123 notes, point 1: ADR-176's answered question on partial regression says 'The counter never decreases, so Home Assistant never sees a reset' and 'Same answer as the 1.x peer'. Neither holds: a power cut restores up to 10 L plus one minute of flow less than the last published value (docs/api/MQTT.md, DHW Water Total), and 1.x keeps the total in RAM only, so a reboot zeroes it. The decision itself, resume from the last persisted value and accept the undercount, stands. Maintainer decision 2026-10-01: record the correction in a short amending ADR (docs only).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 An amending ADR, authored through adr-kit and Accepted by the maintainer, states what an unclean reboot does to the published total and how Home Assistant reads it, and that the 1.x line zeroes on reboot; ADR-176's decision to resume from the last persisted value stands
- [ ] #2 The ADR is cross-referenced with ADR-176 on both sides (adr relate) and listed in the curated ADR index; adr-lint passes every gate
<!-- AC:END -->
