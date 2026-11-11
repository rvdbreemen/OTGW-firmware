---
id: TASK-1822
title: >-
  Remove the separate 'OTmonitor log' download button again (Robert: keep the
  plain log format; OTmonitor 715d3c0 reads it)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T21:17:00Z'
updated_date: '2026-11-11T23:18:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 912000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert, 2026-11-11 ~22:40 in session d3: 'Voor de duidelijkheid ik wil GEEN aparte OT-log download knop. Sterker nog het probleem is al opgelost. Ik wil het oude format behouden, want daar zit alle informatie in de ook in een normaal OTmonitor format in zit. En Schelte heeft zijn otmonitor al aangepast aan ons.' Schelte's OTmonitor master 715d3c0 (case 35, otmonitor.vfs/otmonitor.tcl:1857-1862) reads our plain 2.x log download: all 162 frames of a real .61 download sit at column 35. Revert the TASK-1813 feature from beta.179 (caf868c7c): logDownloadOtmon button in v2.html, otmonLogText/downloadOtmonLog/logBufDev in v2.js, the h_mon.log_download_otmon(_title) i18n keys (EN/NL/DE/EL), tests/webui/otmonitor-log-export.test.mjs + lib/otmon_reader.tcl, the log-autoscroll stub change, the manual paragraphs (en ch03, nl h03). The plain Download button and its format stay exactly as before 1813.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The OTmonitor-log button, its code, i18n keys, test and manual text are gone; the plain log download is byte-identical in format to before TASK-1813
- [x] #2 Full web UI suite green (one file per run), evaluate --quick no new FAIL
- [ ] #3 Build, OTA on .61, screenshot of the Monitor log toolbar without the extra button; plain download still works
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Reverse the TASK-1813 feature of beta.179 (caf868c7c) for exactly its files (v2.html, v2.js, i18n de/el/nl, the two web UI tests, the reader harness, the manual paragraphs en ch03 / nl h03), 3-way applied onto origin/dev; keep downloadLog() and downloadBlob() (both pre-date 1813). Verify: no symbol left, downloadLog() byte-identical to caf868c7c^, web UI suite one file per run, evaluate. Bench (bench tester): Monitor log toolbar without the button, plain download still works.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Reverted locally on lane-1815 (to be ordered first at integration, beta.188): reverse diff of caf868c7c for its 10 files applied cleanly (-241/+6 lines). No otmonLogText/downloadOtmonLog/logDownloadOtmon/logBufDev/log_download_otmon left in src, i18n, docs/manuals or tests. downloadLog() in v2.js is byte-identical to caf868c7c^ (the pre-1813 version); downloadBlob() pre-dates 1813 and stays. Web UI suite: 63 files, one per run, 63 pass, 0 fail. evaluate --quick 86/0. AC3 (build, OTA, screenshot) goes to the bench tester after the build.
<!-- SECTION:NOTES:END -->
