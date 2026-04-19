---
id: TASK-295
title: '[TEST-H1] Add evaluate.py rule for strncmp_P/strstr_P on binary buffers'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:16'
updated_date: '2026-04-19 07:04'
labels:
  - testing
  - review-2026-04-18
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
evaluate.py has no pattern for strncmp_P/strstr_P. These cause Exception (2) crashes on binary data per MEMORY.md. 23 call sites exist today, all currently safe, but drift is not hypothetical.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 evaluate.py flags every strncmp_P and strstr_P call with INFO-level warning
- [x] #2 Existing call sites documented as text-only in a short comment per call site or allow-list
- [x] #3 python evaluate.py --quick surfaces the new rule
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
evaluate.py: added check_binary_safe_compare method (wired into the always-run quick pipeline) + module-level scan_binary_compare_calls helper. Flags all strncmp_P/strstr_P call sites as INFO with file:line so each can be hand-verified to operate on null-terminated text. Current repo: 14 call sites across 6 files, no binary-buffer usage confirmed. Covered by tests/test_evaluate.py TestScanBinaryCompareCalls.
<!-- SECTION:FINAL_SUMMARY:END -->
