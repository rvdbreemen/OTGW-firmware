---
id: TASK-1598
title: Public beta v2.0.0-beta.62 (08:00 slot 2026-11-04)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-04T05:12:00Z'
updated_date: '2026-11-04T05:13:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 741000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Morning public beta: Robert 2026-11-04 'next public beta around 8am', soak starts ~06:30 with the newest beta then available, 60 min MQTT soak, release at 08:00. Candidate beta.62 (TASK-1447, TASK-1462).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes + CHANGELOG rollover committed (delta since beta.60, TASK-1462 HA entity change flagged)
- [ ] #2 Release-commit build OTA on the Classic+S3 unit, 60 min MQTT soak: boot = flash+2, no crashlog, flat heap
- [ ] #3 Tag, prerelease --latest=false, assets + SHA verified, latest stays 1.x
- [ ] #4 #beta-testing announcement sent
<!-- AC:END -->
