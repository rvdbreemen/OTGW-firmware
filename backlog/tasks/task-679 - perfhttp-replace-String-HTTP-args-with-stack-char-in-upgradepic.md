---
id: TASK-679
title: 'perf(http): replace String HTTP args with stack char[] in upgradepic'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-23 16:08'
updated_date: '2026-05-23 16:10'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
src/OTGW-firmware/OTGW-Core.ino:4983-4985 declares three const String locals (action, filename, version) populated from httpServer.arg(). Per ADR-004 and the recent TASK-673 pattern for pendingUpgradePath, replace with stack char[80] buffers populated via strlcpy. Update downstream usages to use strcmp_P/strlen/raw char* (no String API calls inside the handler scope). The String-taking helpers refreshpic() are outside handler scope and unchanged.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 action/filename/version are char[80] populated via strlcpy from httpServer.arg().c_str()
- [x] #2 isEmpty checks use [0]=='\\0'; comparisons use strcmp_P + PSTR
- [x] #3 All .c_str() within the handler scope are removed
- [x] #4 python build.py --firmware exits 0
- [x] #5 python evaluate.py --quick shows no new failures
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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced const String action/filename/version locals in upgradepic() with char[80] stack buffers populated via strlcpy from httpServer.arg().c_str(). Updated all downstream usages within the handler scope: isEmpty() -> [0]=='\\0'; == F() -> strcmp_P + PSTR; dropped all .c_str() calls (chars decay to char*). The refreshpic() call site retains its String signature (outside handler scope per ADR-004); the temporary String it constructs at the call boundary is unchanged from prior behaviour. Verified: python build.py --firmware exits 0; python evaluate.py --quick 34 passed, 0 failed (no delta vs baseline).
<!-- SECTION:FINAL_SUMMARY:END -->
