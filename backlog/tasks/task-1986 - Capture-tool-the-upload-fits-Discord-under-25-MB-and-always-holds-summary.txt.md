---
id: TASK-1986
title: >-
  Capture tool: the upload fits Discord (under 25 MB) and always holds
  summary.txt
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T21:10:00Z'
updated_date: '2026-11-15T22:20:00Z'
labels:
  - capture
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1014000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter's beta.250 upload (2026-11-15) held 7 files: he packed it by hand because the zip was too big for Discord (his remark 2026-11-11), so mqtt.log (1,284,817 messages), summary.txt, crashlog.log, browser.log and curl-probes.log were missing. TASK-1988 (own topics only) makes mqtt.log much smaller; this task makes the upload itself fit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The upload the tool builds stays under Discord's attachment limit (25 MB) for a 24 h capture: xz/zip compression choice, and when still too big a split or a smaller second archive, measured on a 24 h-size sample
- [x] #2 summary.txt, the transcript and crashlog.log are always in the upload; the tool says which files it left out and why
- [x] #3 Host test on the packing logic; README
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: build_upload_zip() keeps the zip when it fits UPLOAD_LIMIT_BYTES (24 MB, one Discord attachment), else packs the same files as <run>-upload.tar.xz, and when that is still too big puts the transcript, summary, crash log, core dump and device state in the first .tar.xz and the rest in <run>-upload-part2.tar.xz; the console then says 'Upload these two files'. Measured on gruijter's real 24 h capture (local only): transcript as uploaded 280 MB = 24.2 MB deflate / 11.4 MB xz; with TASK-1988's own-topics-only MQTT 229 MB = 19.5 MB deflate / 10.1 MB xz; otframes.log 2.4 / 1.5 MB. So deflate alone stays near or over the limit, xz fits with room. AC2: summary.txt and crashlog.log go into every upload (the old tool's short-run upload held only the transcript: checked on origin's build_upload_zip, summary.txt and crashlog.log absent); every pack or split decision and the files moved to part 2 are written to the summary. AC3: tests/test_capture_upload_size.py, the three paths with a small limit (fits as zip with summary and crash log; too-big zip becomes .tar.xz and the zip is removed; split keeps the essentials first and reports a part that still does not fit): origin errors on all 3, the fix passes; the analyze-capture skill reads .tar.xz uploads (and -part2), verified on gruijter's real archive; README updated; sweep of the 10 capture test files -j 1, 19/19 PASS.
<!-- SECTION:FINAL_SUMMARY:END -->
