---
id: TASK-925
title: >-
  feat(webui): solve all v2 audit findings + gas/heat-pump source control
  (TASK-908 P9)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-24 18:50'
updated_date: '2026-06-26 22:32'
labels: []
milestone: 2.0.0
dependencies: []
ordinal: 141000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement approved plan cozy-juggling-fern: Phase1 firmware (satsource whitelist, PATCH/DELETE auth, comment scrubs); Phase2 v2 UI (REST seed, writable source toggle, steppers->sat/target, stats sort, log download, conn detail, support hover, graph chips+CSV+PNG, enum selects); Phase3 classic satsystem label fix + heating_source select; Phase4 docs (MQTT retained line, MEMORY ADR-124->140) + SAT golden regen.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Firmware: satsource settable + PATCH/DELETE auth + comment scrubs
- [ ] #2 v2 UI: all unwired controls wired + REST seed + source toggle
- [ ] #3 Classic satsystem label bug fixed + heating_source select
- [ ] #4 Docs/oracle updated; build+eval green; verified on device
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Loop assessment 2026-06-26: Phase2 (v2 UI: REST seed, source toggle, steppers, stats sort, log download, conn detail, support hover, graph chips+CSV+PNG, enum selects) = DONE per TASK-933 audit + Playwright validation. Phase3 (classic satsource 'Heat Source' select) present at index.js:3630. REMAINING/unverified: Phase1 firmware (satsource whitelist in dispatch, PATCH/DELETE auth, comment scrubs), Phase4 (docs MQTT-retained line + MEMORY ADR-124->140 + SAT json-golden regen), and AC#4 on-device verification. Recommend as next focused iteration — left In Progress.
<!-- SECTION:NOTES:END -->
