---
id: TASK-1553
title: >-
  build.py machine lock treats a reused PID as a live holder; a killed build
  blocks every later build
status: To Do
assignee: []
created_date: '2026-11-01T21:22:00Z'
updated_date: '2026-11-02T04:37:00Z'
labels:
  - tooling
dependencies: []
priority: medium
ordinal: 706000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2026-11-01: a build killed at 21:19 (TaskStop) left %LOCALAPPDATA%/OTGW-build/build.lock with PID 14916. Windows reused PID 14916 for a node.exe at 21:19:17, so the next build waited ~20 min on a 'live' holder that was not a build (TASK-1488 lock checks liveness by PID only). Removed the stale lock by hand. Fix idea: also check that the holder process is a python build.py (command line or process start time vs the lock timestamp).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A lock whose PID now belongs to a non-build process (or started after the lock time) is taken over, host test red on the old check
- [ ] #2 Waiting message names the holder process name so a stale lock is visible
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Related, same evening: in a fresh worktree (wt-1552, no build/ dir) build.py compiled firmware + safeboot ([SUCCESS] 250 s) and then crashed in build_safeboot() at build.py:2298 shutil.copy2(image, out) with FileNotFoundError WinError 3, because build/ did not exist yet; exit 1. Workaround mkdir build. The rerun then recompiled everything (no incremental gain). Fix: os.makedirs(out_dir, exist_ok=True) before the copy.

Third trap, same evening: running ../OTGW-firmware/.build-python/python.exe build.py from another worktree (wt-1552) imports the MAIN tree's config.py, because .build-python is an embedded Python whose python312._pth puts '.'(=its own dir parent? no: the main tree) on sys.path and does not insert the script dir. Verified: sys.path[2] = D:/.../OTGW-firmware, config.__file__ = OTGW-firmware/config.py. Effect: BUILD_DIR = main-tree build/, which build.py cleaned (beta.1 release artifacts removed; they are still on the GitHub release) and filled with the wt-1552 beta.2 parts; the worktree build/ stayed empty and merged/zip failed ('Firmware binary not found'), exit 0. Workaround: python.exe -c "import sys,runpy; sys.path.insert(0, r'<wt>'); sys.argv=['build.py']; runpy.run_path(r'<wt>/build.py', run_name='__main__')". The /release-2x skill (Phase 4) recommends the main tree's interpreter for a worktree: fix that text too.

Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: build.py _pid_alive (~:3021) checks the PID only, no process name or start time. Agent-doable, tooling only.
<!-- SECTION:NOTES:END -->
