---
id: TASK-679
title: 'perf(http): replace String HTTP args with stack char[] in upgradepic'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-23 16:08'
updated_date: '2026-05-23 16:08'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
src/OTGW-firmware/OTGW-Core.ino:4983-4985 declares three const String locals (action, filename, version) populated from httpServer.arg(). Per ADR-004 and the recent TASK-673 pattern for pendingUpgradePath, replace with stack char[80] buffers populated via strlcpy. Update downstream usages to use strcmp_P/strlen/raw char* (no String API calls inside the handler scope). The String-taking helpers refreshpic() are outside handler scope and unchanged.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 action/filename/version are char[80] populated via strlcpy from httpServer.arg().c_str()
- [ ] #2 isEmpty checks use [0]=='\\0'; comparisons use strcmp_P + PSTR
- [ ] #3 All .c_str() within the handler scope are removed
- [ ] #4 python build.py --firmware exits 0
- [ ] #5 python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Replace const String action/filename/version with char[80] + strlcpy from httpServer.arg()
2. Update isEmpty checks (== [0]) and == F() comparisons (strcmp_P)
3. Drop .c_str() calls within the handler scope
4. Confirm refreshpic call site still compiles (String implicit constructor accepts char*)
5. Build firmware
6. Run evaluator quick
<!-- SECTION:PLAN:END -->
