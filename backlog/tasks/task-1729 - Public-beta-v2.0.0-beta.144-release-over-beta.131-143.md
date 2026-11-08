---
id: TASK-1729
title: Public beta v2.0.0-beta.144 (release over beta.131-143)
status: To Do
assignee: []
created_date: '2026-11-08T11:33:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 837000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-08 ~12:20: public beta now. Candidate beta.143+6c7d1ca (code = origin/dev b5ed03e23), soak on .63 (Classic + S3 Mini, PIC) from 12:26. Release commit: notes docs/releases/RELEASE_NOTES_2.0.0-beta.144.md, CHANGELOG rollover, bump beta.144. Builds use UV_OFFLINE=1 during the GitHub incident of 2026-11-08 (unauthenticated timeouts).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Candidate ran 1 h on .63 with MQTT connected, no restart, no crashlog
- [ ] #2 Release commit built (esp32-combo), OTA on .63 healthy
- [ ] #3 Tag v2.0.0-beta.144 on the release commit; GitHub prerelease with --latest=false, assets incl. .elf and safeboot, SHA256SUMS match; latest stays the 1.x stable
- [ ] #4 #alpha-testing zip post and #beta-testing announcement
<!-- AC:END -->
