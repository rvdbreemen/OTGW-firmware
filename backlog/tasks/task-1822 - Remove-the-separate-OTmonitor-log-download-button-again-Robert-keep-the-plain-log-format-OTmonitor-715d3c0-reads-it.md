---
id: TASK-1822
title: >-
  Remove the separate 'OTmonitor log' download button again (Robert: keep the
  plain log format; OTmonitor 715d3c0 reads it)
status: To Do
assignee: []
created_date: '2026-11-11T21:17:00Z'
updated_date: '2026-11-11T21:20:00Z'
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
- [ ] #1 The OTmonitor-log button, its code, i18n keys, test and manual text are gone; the plain log download is byte-identical in format to before TASK-1813
- [ ] #2 Full web UI suite green (one file per run), evaluate --quick no new FAIL
- [ ] #3 Build, OTA on .61, screenshot of the Monitor log toolbar without the extra button; plain download still works
<!-- AC:END -->
