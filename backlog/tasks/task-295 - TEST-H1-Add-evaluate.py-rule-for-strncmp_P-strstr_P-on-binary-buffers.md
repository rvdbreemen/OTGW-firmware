---
id: TASK-295
title: '[TEST-H1] Add evaluate.py rule for strncmp_P/strstr_P on binary buffers'
status: To Do
assignee: []
created_date: '2026-04-18 19:16'
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
- [ ] #1 evaluate.py flags every strncmp_P and strstr_P call with INFO-level warning
- [ ] #2 Existing call sites documented as text-only in a short comment per call site or allow-list
- [ ] #3 python evaluate.py --quick surfaces the new rule
<!-- AC:END -->
