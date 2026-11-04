---
id: TASK-1171
title: >-
  evaluate.py: the ADR-088 cooldown gate reads a moved boards.h and has skipped
  its check since 2026-06-02
status: Done
assignee:
  - '@claude'
created_date: '2026-09-30 08:14'
updated_date: '2026-09-30 08:18'
labels:
  - evaluate
  - ci-gate
  - adr-088
dependencies: []
priority: medium
ordinal: 294000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
check_status_burst_cooldown_bound() (evaluate.py ~:1724) looks for config.FIRMWARE_ROOT / 'boards.h' (src/OTGW-firmware/boards.h). Commit c880a0203 (2026-06-02, 'promote ESP abstraction into src/libraries/Platform library') moved that file to src/libraries/Platform/src/boards.h. Since then the gate only reports WARN 'boards.h not found' and returns, so the binding ADR-088 STATUS_BURST_COOLDOWN_MS bound is not enforced (ADR-080: a binding rule needs a working gate). Two more defects in the same gate: it stops after the FIRST declaration although boards.h defines the constant once per board (lines ~132 and ~197), and it labels findings 'MQTTstuff.ino:<line>'. Found in the full evaluate.py run for IMG-0 on 2026-09-30 (79 passed, 5 warnings, 0 failed; this is one of the warnings).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The gate reads boards.h from src/libraries/Platform/src/boards.h, defined in one place
- [x] #2 A missing boards.h or a missing STATUS_BURST_COOLDOWN_MS declaration makes the gate FAIL instead of WARN
- [x] #3 Every per-board declaration is checked, not only the first, and findings name boards.h:<line>
- [x] #4 tests/test_evaluate.py covers: the current shape passes, a second-board value >= 3000 without marker fails, a 'verified tuning' marker allows it, a missing declaration fails
- [x] #5 Old-vs-fix on the same tree: the old gate WARNs 'boards.h not found', the fixed gate PASSes; with a fault injected into the second declaration of a temporary copy, the fixed gate FAILs
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-30 evidence (all in-session, same tree):
- OLD gate (HEAD 3621a3830): 'WARN: boards.h not found', exit 0: the ADR-088 bound was skipped. Also seen in the full evaluate.py run for IMG-0 (79 passed, 5 warnings).
- FIX, real tree: 'PASS: 2 per-board declaration(s) within bound: 250 ms, 250 ms [boards.h:132, boards.h:197]'.
- FIX, fault injected into the SECOND declaration of a temporary copy (197: 5000): 'FAIL: STATUS_BURST_COOLDOWN_MS = 5000 ms (>= 3000) without verified tuning marker [...:197]', exit 1. The old gate stopped after the first declaration and would never have read line 197.
- FIX, missing file: FAIL '... not found: the ADR-088 bound cannot be checked', exit 1.
- tests/test_evaluate.py: 66 tests OK (5 new in TestStatusBurstCooldownBound). Mutation check: the 5 new tests run against the OLD evaluate.py all error out (Ran 5, FAILED errors=5), so a revert is caught.
- evaluate.py --quick: 69 passed, 0 warnings, 0 failed (was 68 passed with this gate as a warning).
Harness: %LOCALAPPDATA%/OTGW-capture/task1171-run-gate.py (runs only this gate, optional boards.h override).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The ADR-088 STATUS_BURST_COOLDOWN_MS gate had silently skipped its check since c880a0203 (2026-06-02) moved boards.h into src/libraries/Platform/src/. It now reads that path from one constant (STATUS_BURST_COOLDOWN_SOURCE), FAILs when the file or the declaration is missing instead of WARNing, checks every per-board declaration instead of only the first, and names boards.h:<line>. The logic moved into a pure helper, status_burst_cooldown_findings(), with 5 unit tests. Evidence: old gate WARN 'boards.h not found' vs fixed gate PASS on both declarations (250 ms, :132 and :197); a fault in the second declaration FAILs; a missing file FAILs; the new tests error out against the old evaluate.py; test_evaluate 66 OK; evaluate --quick 69 passed / 0 warnings / 0 failed.
<!-- SECTION:FINAL_SUMMARY:END -->
