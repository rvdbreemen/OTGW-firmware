---
id: TASK-1657
title: >-
  Public beta v2.0.0-beta.95 (beta.94 code + release commit; replaces the
  cancelled 08:00 beta.80)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T11:19:00Z'
updated_date: '2026-11-05T11:40:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 781000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-05 ~12:30: 'This morning you skipped the public beta, do it now'. Candidate beta.94 (c474756, benched on .61). Release commit beta.95 (docs-only): notes bundle beta.78..94 since v2.0.0-beta.77, CHANGELOG folds the never-published [2.0.0-beta.80] section, RELEASE_NOTES_2.0.0-beta.80.md replaced. Soak 1 h on the Classic .63 with MQTT before publishing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes beta.95 checked against code, no em dashes/TODO, CHANGELOG rollover with the beta.80 section folded in
- [ ] #2 esp32-combo build of the release commit green; OTA on .63 (Classic + PIC), settings unchanged apart from intended new keys
- [ ] #3 1 h soak on .63 with MQTT: bootcount = flash + 2, no crashlog, heap not trending down
- [ ] #4 Tag + GitHub prerelease (--latest=false), assets and SHA256SUMS verified, latest stays 1.x
- [ ] #5 #alpha-testing zip post and #beta-testing announcement sent and read back
<!-- AC:END -->
