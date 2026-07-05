---
id: TASK-966
title: 'v2 parity: Debug-info / crashlog page'
status: To Do
assignee: []
created_date: '2026-06-30 23:07'
updated_date: '2026-07-01 19:34'
labels: []
dependencies: []
ordinal: 178000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verified gap (grep 0 hits in v2): Classic Advanced has Debug Information (device info, crash log display, debug console commands). v2 has no crashlog/debug page. Support-critical — needed to diagnose field crashes. Endpoints: /api/v2/device/info, /crashlog (or /api/v2/device/crashlog).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 v2 Advanced surface shows device/debug info + the crash log (if present)
- [ ] #2 Verified on-device: crashlog endpoint content renders in v2
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Duplicate of TASK-967 (identical title + description, created one minute earlier). TASK-967 is the implemented+verified copy (Debug sub-tab, commit 91cd2364e, both ACs verified on .39). Archiving this orphan; no unique content lost.
<!-- SECTION:NOTES:END -->
