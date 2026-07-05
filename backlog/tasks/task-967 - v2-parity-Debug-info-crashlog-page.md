---
id: TASK-967
title: 'v2 parity: Debug-info / crashlog page'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-30 23:08'
updated_date: '2026-07-01 19:34'
labels: []
dependencies: []
ordinal: 179000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verified gap (grep 0 hits in v2): Classic Advanced has Debug Information (device info, crash log, debug console). v2 has no crashlog/debug page. Support-critical for diagnosing field crashes. Endpoints: /api/v2/device/info, /crashlog.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 v2 Advanced shows device/debug info + the crash log
- [x] #2 Verified on-device: crashlog content renders in v2
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 parity: added a Debug sub-tab (Monitor > Debug) showing device/build/runtime info (GET /api/v2/debug: build.version/number/githash/date + runtime heap/frag/uptime) and the crash log (GET /api/v2/device/crashlog: 'No crash recorded.' when none, else summary+details). Parity with the Classic 'Debug Information' page — support-critical for diagnosing field crashes. Two mono panels (reuses .console/.set-group, no drift). Verified on .39: debug dump (6.7KB, build.version + heap etc) + 'No crash recorded.', 0 console errors.
<!-- SECTION:FINAL_SUMMARY:END -->
