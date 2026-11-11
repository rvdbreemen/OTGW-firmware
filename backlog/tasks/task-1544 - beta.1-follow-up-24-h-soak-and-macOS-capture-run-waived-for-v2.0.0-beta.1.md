---
id: TASK-1544
title: 'beta.1 follow-up: 24 h soak and macOS capture run (waived for v2.0.0-beta.1)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-01T15:47:00Z'
updated_date: '2026-11-11T05:24:00Z'
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
- [x] #1 24 h soak of a beta build on the Classic + S3 Mini without an unexplained reboot; crashlog and reboot_log quoted
- [ ] #2 Capture run from the release zip on macOS by a named tester, with transcript
- [ ] #3 Capture runs from the beta zip on Windows and Linux, each with a transcript (TASK-1504 AC2 asked for all three OSes)
- [ ] #4 Update over WiFi with the release zip through the /update page (the path the release notes give users), then device/info shows the new version and settings are kept
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: verification only (24 h soak + macOS capture). Release gate for the next beta.

2026-11-04 Robert: keep as the 2.0.0 stable-release gate (24 h soak + macOS/Linux capture runs); no bench soak now.

2026-11-06 00:33: soak .63 at 9 h 31 min, boot 148 (no restart), freeheap drifting ~500 B/h (54,176 -> 51,572 B since 19:13); filed TASK-1669.

2026-11-06 15:00 .63 (Classic + S3 Mini, beta.95+1fbd5cf): uptime 1(d)-00:03, bootcount 148 (unchanged since the soak start), mqttconnected true, crashlog available:false, reboot_log newest entries '2026-11-05 14:56:34 - reboot cause: Software reset (3)' and '2026-11-05 14:56:01 - ... [OTA] Rebooting...' (the beta.95 flash), nothing after: 24 h without a reboot.

2026-11-11 (comms): capture-tool volunteer call (one volunteer per OS, beta.158 zip, 10-15 min run) posted in #alpha-testing on 2026-11-09 06:28 UTC, msg 1569237211789000751. No direct reply or volunteer to that post as of 2026-11-11 (checked the 68 later #alpha-testing messages via the bot REST API: no message references it, no reactions). Capture uploads that did arrive afterwards, not as replies to the call: tjfs 2026-11-09 (acknowledged in msg 1569333602137935873), thaghostnl 2026-11-10 two uploads on beta.170 (TASK-1786), gruijter 2026-11-10 20261110-100556-upload.zip on beta.167 (TASK-1787). The OS of those runs was not checked here, so this note does not close the per-OS check; macOS still has only the earlier run (#beta-testing 2026-11-08).
<!-- SECTION:NOTES:END -->
