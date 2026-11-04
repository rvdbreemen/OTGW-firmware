---
id: TASK-1488
title: >-
  build.py: machine-wide build lock so only one build runs at a time across all
  sessions and worktrees
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T21:50:00Z'
updated_date: '2026-10-29T21:54:00Z'
labels:
  - tooling
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 647000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-29: 'zorg ervoor dat er nooit meer dan 1 build sessie tegelijk is'. Today the one-build rule (CLAUDE.md Build Commands) depends on sessions coordinating by message. Make it mechanical: build.py takes an exclusive lock file in %LOCALAPPDATA%/OTGW-build/build.lock (same path for every worktree and session of this user) before any PlatformIO work and releases it on exit; a second build waits (polling, prints the holder: pid, worktree, start time) until the lock is free; a lock whose pid is dead is taken over. OTGW_BUILD_LOCK_WAIT=<minutes> bounds the wait (0 = refuse at once, exit non-zero).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Two concurrent build.py runs (any worktrees): the second waits and starts only after the first releases (test with a stubbed build step, red without the lock)
- [x] #2 A stale lock (dead pid) is taken over; OTGW_BUILD_LOCK_WAIT=0 refuses with exit code != 0 and names the holder
- [x] #3 CLAUDE.md/AGENTS.md Build Commands mention the lock
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
build.py acquire_build_lock()/release_build_lock(): exclusive create of %LOCALAPPDATA%/OTGW-build/build.lock (pid, worktree, start time) right after argument parsing, released at exit (atexit); a second build polls every 10 s and prints the holder; a dead holder (Windows OpenProcess/GetExitCodeProcess, POSIX kill 0) is taken over; OTGW_BUILD_LOCK_WAIT bounds the wait (default 120 min, 0 = refuse with exit 3). Evidence: tests/test_build_lock.py runs the real functions in child processes: Ran 3 OK (second build starts only after the first ended and printed 'Waiting for the running build'; wait 0 exits 3 naming the holder; stale lock taken over); against HEAD's build.py (no lock) FAILED (failures=3). tests/test_build_jobs.py OK. CLAUDE.md/AGENTS.md Build Commands describe the lock. Not verified: a real two-build run on this machine (worktrees get the lock after rebasing onto this commit).
<!-- SECTION:FINAL_SUMMARY:END -->
