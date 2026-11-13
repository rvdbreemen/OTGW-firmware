---
id: TASK-1898
title: >-
  build.bat :find_python runs a python.cmd shim without call: silent exit 1 in a
  fresh worktree
status: Done
assignee:
  - '@claude'
created_date: '2026-11-13T13:22:00Z'
updated_date: '2026-11-13T13:33:00Z'
labels:
  - build
  - tooling
dependencies: []
priority: medium
ordinal: 967000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
In a worktree without .build-venv, build.bat goes through :bootstrap_build_venv -> :find_python, which runs 'py -3 -c ...' and then 'python -c ...' without 'call'. On this machine the first 'python' on PATH is C:\tools\Codex\python.cmd (a 2-line shim to C:\Python312\python.exe, 2026-07-08). A batch file that runs another batch file without 'call' never returns, so build.bat ends there with exit 1 and prints nothing: no build, empty log. Seen 2026-11-13 on wt-ba-1885 (TASK-1885 build attempts 2 and 3); an echo-on trace stops right after the 'python -c' line. Workaround used: C:\tools\Codex left out of PATH for that one process. Related: TASK-1894 (0-byte build logs from the agent context); this explains the fresh-worktree silent exit, not necessarily every 0-byte log (the TASK-1871 build in a worktree with a .build-venv also gave a 0-byte log through PowerShell '*>' but built fine). Decision (Robert, DM 1570788215567159421, relayed by the coordinator): option A, fix build.bat only (use 'call' for the python probes in :find_python, or prefer the worktree's own .build-venv/.build-python first); the PATH stays as it is.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 build.bat :find_python runs every interpreter probe through 'call' (or the probe order no longer reaches a PATH shim before the worktree's own Python), so a .cmd/.bat python on PATH cannot end build.bat
- [x] #2 Evidence in a fresh worktree (no .build-venv) with C:\tools\Codex\python.cmd first on PATH: an echo-on trace before the fix stops after 'python -c' with exit 1 and no output; after the fix the trace continues and the build log arrives
- [x] #3 A regression check (test or evaluate gate) fails when :find_python calls an interpreter probe without 'call'; red on the old build.bat, green on the fix
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Prefix every interpreter probe in build.bat :find_python (py -3, python, python3) and the venv creation in :bootstrap_build_venv (%BASE_PYTHON% -m venv) with CALL, so a .cmd/.bat shim returns to build.bat. Static regression check tests/test_build_bat_call_probes.py. Evidence: echo-on traces of the old and the fixed build.bat in a fresh worktree, cut off before build.py.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 evidence in fresh worktree wt-ba-buildbat (no .build-venv/.build-python; 'where python' first = C:\tools\Codex\python): echo-on copies of build.bat with the build.py line replaced by an echo. OLD: trace (4817 B) ends at the line 'python -c "import sys; ..." 1>nul 2>nul' and never reaches build.py. FIX: trace (4941 B) shows 'call python -c ...' returning, .build-venv created on it, then 'TRACE: reached build.py with ...\wt-ba-buildbat\.build-venv\Scripts\python.exe'. Logs kept in RvdB/OTGW-worktree-leftovers/evidence-1898/. tests/test_build_bat_call_probes.py: FAILED on the old build.bat (4 probes without CALL: py -3, python, python3, %BASE_PYTHON% -m venv), OK on the fix; run_tests 1/1. evaluate.py --quick 85 PASS 0 FAIL.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: build.bat :find_python runs py -3, python and python3 through CALL, and :bootstrap_build_venv runs %BASE_PYTHON% -m venv through CALL (commit f7c5c7a37, rebased). AC2: echo-on traces in the fresh worktree wt-ba-buildbat with C:\tools\Codex\python first on PATH: old build.bat stops after the 'python -c' probe and never reaches build.py; fixed build.bat creates .build-venv and reaches build.py (logs in RvdB/OTGW-worktree-leftovers/evidence-1898/). AC3: tests/test_build_bat_call_probes.py FAILED on the old build.bat (4 probes without CALL), OK on the fix, run_tests 1/1 after the rebase onto 827fb519f; evaluate.py --quick 85 PASS 0 FAIL. Not verified: a full build through the fixed build.bat in a fresh worktree (the traces stop before build.py by design); the next fresh-worktree build exercises it. TASK-1894 keeps the other 0-byte-log case (PowerShell '*>' on a working build).
<!-- SECTION:FINAL_SUMMARY:END -->
