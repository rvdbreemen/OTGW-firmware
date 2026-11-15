---
id: TASK-1986
title: >-
  Capture tool: the upload fits Discord (under 25 MB) and always holds
  summary.txt
status: To Do
assignee: []
created_date: '2026-11-15T21:10:00Z'
updated_date: '2026-11-15T21:28:00Z'
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
- [ ] #1 The upload the tool builds stays under Discord's attachment limit (25 MB) for a 24 h capture: xz/zip compression choice, and when still too big a split or a smaller second archive, measured on a 24 h-size sample
- [ ] #2 summary.txt, the transcript and crashlog.log are always in the upload; the tool says which files it left out and why
- [ ] #3 Host test on the packing logic; README
<!-- AC:END -->
