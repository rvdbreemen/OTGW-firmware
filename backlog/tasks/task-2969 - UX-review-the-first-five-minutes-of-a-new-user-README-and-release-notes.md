---
id: TASK-2969
title: 'UX review: the first five minutes of a new user (README and release notes)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-20T15:37:00Z'
updated_date: '2026-11-20T15:40:00Z'
labels:
  - ux
  - docs
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1101000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Read-only UX review by otgw-ux-designer, suggested by the coordinator: what a new visitor reads in the first five minutes on the GitHub page (README on the default branch dev, the release list, the newest release notes) and whether they end up with the right firmware. Deliverable: docs/reviews/2026-11-20_ux-first-five-minutes/README.md with findings R1-R7 (severity, effort, evidence, proposed AC); the coordinator triages them (texts: otgw-docs, release-note style: BlueFlame).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Review file lists every finding with heuristic, severity, effort, evidence (file:line or a gh query) and a proposed AC
- [x] #2 Facts checked against the repo and gh (default branch, latest release, release list, settings names); what was not checked is listed under Not verified
- [ ] #3 Findings list delivered to the coordinator for triage
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20: review docs/reviews/2026-11-20_ux-first-five-minutes/README.md, findings R1-R7. Facts: gh repo view (default branch dev, latest release v1.7.9 2026-11-04), gh release list (6 newest are 2.0.0 prereleases), settings names checked against v2.js SET_META and the firmware (no HA discovery on/off key, only mqtthaprefix). Line numbers at origin/dev 335e16db7.
<!-- SECTION:NOTES:END -->
