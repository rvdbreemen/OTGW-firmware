---
id: TASK-1255
title: >-
  PIC flash shows 'Error: PIC upgrade was successful' and does not refresh
  banner or cached PR= values
status: To Do
assignee: []
created_date: '2026-10-19 17:32'
labels:
  - bug
  - webui
  - pic
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 387000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by gruijter in #alpha-testing 2026-10-19 with screenshot: after a successful PIC upgrade (6.6 to 6.8) the v2 Advanced > PIC page shows a red bar 'Error: PIC upgrade was successful'; the header shows 6.8 but the update-available banner still says 6.6 -> 6.8 and the cached PR= table still shows 6.6. Cause (read in code): fwupgradedone() (OTGW-Core.ino ~5796) writes the success text into state.flash.sError; /api/v2/pic/flash-status sends it as error; pollPicFlash() (v2.js ~3421) treats any non-empty error as failure. After the flash only fetchPic runs, not picUpdatePoll/renderPicBanner or fetchPicSettings.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Successful flash shows a green/neutral success state, a failed flash still shows the error (old-vs-fix evidence)
- [ ] #2 After success the update banner and the cached PR= gateway settings refresh to the new version without a page reload
<!-- AC:END -->
