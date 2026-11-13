---
id: TASK-1892
title: >-
  webui test pic-flash-status: picAfterFlash harness lacks applyPicTabGate stub
  (ReferenceError); graph-room-dropout and home-dial-sat-only pass alone
status: Done
assignee:
  - '@claude'
created_date: '2026-11-13T08:31:00Z'
updated_date: '2026-11-13T09:32:00Z'
labels:
  - webui
  - test
  - bug
dependencies: []
priority: low
ordinal: 959000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by d3 via the coordinator 2026-11-13 as three pre-existing webui failures on the base commit. Reproduced one at a time in wt-09-1884 (origin/dev 220-era + TASK-1884 UI): pic-flash-status fails 1/14, 'picAfterFlash() runs the real refresh functions one request at a time': ReferenceError: applyPicTabGate is not defined (fetchPic calls it since the PIC-tab gate; the test's hand-written stub set at tests/webui/pic-flash-status.test.mjs:226-231 was not updated). graph-room-dropout 0 failures and home-dial-sat-only 5/5 pass alone with no other load, as TASK-1723 found for load-only failures.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 pic-flash-status passes 14/14; the stub is added in the test, v2.js unchanged
- [x] #2 graph-room-dropout and home-dial-sat-only re-run alone and recorded; filed separately if one fails alone
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
tests/webui/pic-flash-status.test.mjs: the hand-written stub set for the picAfterFlash() chain now defines applyPicTabGate() (fetchPic calls it since TASK-1843, commit 19eecc53b). Red before: 13/14, ReferenceError: applyPicTabGate is not defined. Green after: 14/14. v2.js unchanged. graph-room-dropout (0 failures) and home-dial-sat-only (5/5) pass when run alone with no other load (2026-11-13, one file at a time), consistent with TASK-1723's load-only finding; no separate task needed.
<!-- SECTION:FINAL_SUMMARY:END -->
