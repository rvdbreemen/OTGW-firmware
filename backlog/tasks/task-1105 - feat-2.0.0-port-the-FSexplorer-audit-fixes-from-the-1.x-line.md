---
id: TASK-1105
title: 'feat-2.0.0: port the FSexplorer audit fixes from the 1.x line'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-01 18:39'
updated_date: '2026-09-01 19:24'
labels:
  - audit
  - fsexplorer
dependencies: []
ordinal: 267000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Ports the shared findings of the FSexplorer audit of 2026-09-01 (1.x TASK-1098 through TASK-1104) to the 2.0.0 line. Shared defects: the freeBytes underflow that defeats the upload space guard; settings.ini served without authentication while carrying the admin and MQTT passwords; upload reporting success when the open or the write failed and when the filename was silently shortened to 30 characters; unencoded href assignment for open and download links plus urlDecode applied to a multipart filename; the delete-path normalization that overwrites the terminator; and the same-origin check that runs after the file has already been written. Line numbers differ between trees, so each fix is located in this tree before it is applied.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every shared audit finding is fixed in this tree or explicitly recorded as not applicable here
- [x] #2 Firmware builds green for esp32-classic and esp32-combo
- [x] #3 Evaluator shows no new failures
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported the shared FSexplorer audit fixes to the 2.0.0 async tree: freeBytes/freeSpace underflow clamp, settings.ini gated with checkHttpAuth (including a //settings.ini alias collapse), upload now reports a 507 when the open or a write failed, delete-path terminator forced, multipart filename stored without a urlDecode pass, per-segment href encoding on open/download links, the same-origin check moved ahead of the write, and the filename-too-long refusal in the UI. Located each site against the current async code rather than the audit's stale line numbers. Build: esp32, esp32-classic and esp32-combo all SUCCESS. evaluate.py --quick 76 checks, 0 failures, 1 pre-existing unrelated warning. Not exercised on ESP32 hardware this session; the changed lines mirror the 1.x fixes that were hardware-verified.
<!-- SECTION:FINAL_SUMMARY:END -->
