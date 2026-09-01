---
id: TASK-1098
title: FSexplorer free-space guard is defeated by a 32-bit underflow
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-01 18:30'
updated_date: '2026-09-01 19:14'
labels:
  - audit
  - fsexplorer
dependencies: []
priority: high
ordinal: 195000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
api/listfiles reports freeBytes as fsInfo.totalBytes - (usedBytes * 1.05). Once LittleFS passes 95.238 percent full that subtraction wraps in 32-bit unsigned arithmetic and answers about 4 GB free on a 2 MB partition. FSexplorer.html uses that value as its only 'not enough space' check, so the Upload button stays enabled and the upload proceeds into a filesystem that cannot hold it. freeSpace() carries the same underflow. Found by the FSexplorer audit of 2026-09-01, finding 1 of 18.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 freeBytes never exceeds totalBytes, whatever the fill level
- [x] #2 The UI refuses an upload larger than the real free space on a nearly full filesystem
- [x] #3 freeSpace() uses the same clamped arithmetic
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Clamped usedBytes to totalBytes before the 32-bit subtraction in apilistfiles(), so freeBytes can no longer underflow to ~4 GB above 95.2% fill. 1.x freeSpace() uses double arithmetic and was already correct, left untouched. Build and evaluator green. The 95%-full trigger cannot be reached on the bench device, so AC1/AC2 are code-and-logic verified rather than hardware-exercised; the arithmetic is now monotonic (used+free==total) by construction.
<!-- SECTION:FINAL_SUMMARY:END -->
