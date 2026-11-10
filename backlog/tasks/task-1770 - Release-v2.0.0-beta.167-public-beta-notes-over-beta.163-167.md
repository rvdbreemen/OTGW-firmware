---
id: TASK-1770
title: Release v2.0.0-beta.167 public beta (notes over beta.163-167)
status: To Do
assignee: []
created_date: '2026-11-10T03:09:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 867000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public beta of beta.167 (e79e49522) at ~04:15-05:00 on 2026-11-10 (12 h rule waived by Robert for this one, long overnight soak). Bundles TASK-1749, 1752, 1762, 1764, 1768 since beta.162. Run via /beta-release-2x.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release commit with notes, CHANGELOG rollover, README links; lint clean (no em dash/TODO/relative link/board name)
- [ ] #2 Soak of both bench units read and recorded (bootcount, crashlog, MQTT, heap)
- [ ] #3 esp32-combo build from the release commit with both SUCCESS lines, under the commit-charge limit
- [ ] #4 Tag pushed, prerelease published with --latest=false, assets equal build/, download SHA matches
- [ ] #5 #beta-testing text to 97 and posted; #alpha-testing covered by 80's zip post
<!-- AC:END -->
