---
id: TASK-1544
title: 'beta.1 follow-up: 24 h soak and macOS capture run (waived for v2.0.0-beta.1)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-01T15:47:00Z'
updated_date: '2026-11-05T16:57:00Z'
labels:
  - beta
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 700000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1504 AC2 (macOS capture run by a tester) and AC3 (24 h soak on Classic + S3 Mini) were waived by Robert for v2.0.0-beta.1 (2026-11-01). They are still needed before the next beta or stable. Longest run so far: 224 min (alpha.577, OTGW-direct board); .63 Classic ran alpha.597 6 h 24 min without reboot.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 24 h soak of a beta build on the Classic + S3 Mini without an unexplained reboot; crashlog and reboot_log quoted
- [ ] #2 Capture run from the release zip on macOS by a named tester, with transcript
- [ ] #3 Capture runs from the beta zip on Windows and Linux, each with a transcript (TASK-1504 AC2 asked for all three OSes)
- [ ] #4 Update over WiFi with the release zip through the /update page (the path the release notes give users), then device/info shows the new version and settings are kept
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: verification only (24 h soak + macOS capture). Release gate for the next beta.

2026-11-04 Robert: keep as the 2.0.0 stable-release gate (24 h soak + macOS/Linux capture runs); no bench soak now.
<!-- SECTION:NOTES:END -->
