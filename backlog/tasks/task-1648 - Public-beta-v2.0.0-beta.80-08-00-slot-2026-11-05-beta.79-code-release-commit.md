---
id: TASK-1648
title: >-
  Public beta v2.0.0-beta.80 (08:00 slot 2026-11-05): beta.79 code + release
  commit
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T05:26:00Z'
updated_date: '2026-11-05T05:26:00Z'
labels:
  - release
  - beta
dependencies: []
priority: high
ordinal: 772000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public beta from the 12-hour slot (Robert 2026-11-02/03 rules). Candidate beta.79 (TASK-1608 HA names, TASK-1609 flow dots) + docs-only release commit bumped to beta.80. 1 h MQTT soak on .63.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release commit: notes, CHANGELOG rollover, bump; evaluate --quick 0 failed
- [ ] #2 esp32-combo build SUCCESS, OTA on .63, settings unchanged except keys unknown to beta.80
- [ ] #3 1 h soak with MQTT: bootcount = flash + 2, no crashlog, heap flat
- [ ] #4 GitHub prerelease --latest=false with matching SHA256SUMS; latest stays 1.x
- [ ] #5 #alpha-testing zip post and #beta-testing announcement
<!-- AC:END -->
