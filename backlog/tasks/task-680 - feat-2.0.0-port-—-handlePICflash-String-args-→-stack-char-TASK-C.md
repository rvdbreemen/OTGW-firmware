---
id: TASK-680
title: 'feat-2.0.0: port — handlePICflash String args → stack char[] (TASK-C)'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-23 16:11'
updated_date: '2026-05-23 16:12'
labels: []
dependencies: []
ordinal: 56000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sibling port of dev TASK-C. Replace the three 'const String' locals (action, filename, version) in upgradepic() at OTGW-Core.ino:4999-5001 (post-TASK-A line numbers) with char[80] stack buffers populated via strlcpy from httpServer.arg().c_str(). Pattern mirrors TASK-675 / pendingUpgradePath String elimination already shipped on 2.0.0.

Master plan: /root/.claude/plans/evaluate-the-found-issues-misty-frog.md
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Three 'const String' locals replaced by stack char[80] populated via strlcpy from httpServer.arg().c_str()
- [x] #2 Downstream 'action == F(...)' comparisons converted to strcmp_P(action, PSTR(...)) == 0
- [x] #3 No String in upgradepic() handler scope
- [x] #4 refreshpic(String,String) still callable: pass char-buffer .c_str() implicitly (String constructor) OR refactor refreshpic — choose minimal-blast-radius option
- [x] #5 python build.py --firmware --target esp8266 exits 0
- [x] #6 python evaluate.py --quick: no new failures
- [ ] #7 Commit pushed to claude/beta20-improvements-2.0.0-xRpVI
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Replace 'const String action/filename/version' with char[80] stack buffers; populate via strlcpy(buf, httpServer.arg(F(...)).c_str(), sizeof(buf)).
2. action == F(...) → strcmp_P(action, PSTR(...)) == 0. action.isEmpty() → action[0] == '\0'.
3. filename.isEmpty() → filename[0] == '\0'.
4. Keep refreshpic(String,String) signature unchanged; the char[] passes via implicit String() ctor at the call site. Minimal blast radius.
5. Build + evaluator.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
TASK-C-2.0.0 implementation complete.

- Replaced three 'const String' locals (action/filename/version) in upgradepic() with char[80] stack buffers.

- strlcpy(buf, httpServer.arg(...).c_str(), sizeof(buf)) populates each.

- 'action == F(...)' comparisons → strcmp_P(action, PSTR(...)) == 0.

- 'action.isEmpty()' → 'action[0] == 0'; same for filename.

- refreshpic(String,String) signature unchanged — char[] arg promotes via implicit String(const char*) ctor at the single call site. Minimal blast radius.

- Build (ESP8266) clean, evaluator 60/0 (identical to baseline).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Eliminated three 'const String' locals in upgradepic() (OTGW-Core.ino:4994+) per ADR-004. Replaced with char[80] stack buffers populated via strlcpy from httpServer.arg().c_str(). Downstream:

- 'action == F(...)' rewritten as strcmp_P(action, PSTR(...)) == 0.
- '.isEmpty()' rewritten as 'buf[0] == 0'.
- refreshpic(String,String) signature kept; char[] argument promotes via implicit String ctor at the single call site to keep the change scoped.

Build (ESP8266): clean. Evaluator: 60/0 (identical to baseline). No new String construction in upgradepic() scope.
<!-- SECTION:FINAL_SUMMARY:END -->
