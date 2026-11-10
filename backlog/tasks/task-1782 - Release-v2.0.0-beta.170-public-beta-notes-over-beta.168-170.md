---
id: TASK-1782
title: Release v2.0.0-beta.170 public beta (notes over beta.168-170)
status: To Do
assignee: []
created_date: '2026-11-10T15:05:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 878000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public beta of beta.170 (2dda513da) plus the docs and two Greek strings (0b835ccc0, 75ed5ac29) on origin/dev, slot 16:21 on 2026-11-10 (12 h after beta.167). Bundles TASK-1763 (b) mitigation, TASK-1775, TASK-1777. Run via /beta-release-2x.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release commit with notes, CHANGELOG rollover, README links; lint clean
- [ ] #2 Soak of both bench units read and recorded (bootcount, crashlog, lastreset, MQTT, heap)
- [ ] #3 esp32-combo build from the release commit with both SUCCESS lines, under the commit-charge limit
- [ ] #4 Tag pushed, prerelease published with --latest=false, assets equal build/, download SHA matches
- [ ] #5 #beta-testing text to 97 and posted
<!-- AC:END -->
