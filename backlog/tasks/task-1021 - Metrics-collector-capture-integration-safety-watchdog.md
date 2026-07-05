---
id: TASK-1021
title: Metrics collector + capture integration + safety watchdog
status: Done
assignee:
  - '@claude'
created_date: '2026-07-05 22:21'
updated_date: '2026-07-05 22:59'
labels: []
dependencies: []
ordinal: 232000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1015. Per-arm: poll /api/v2/stats + telnet 'z' reset + crashlog watch via capture-mqtt-debug.bat (one transcript per arm). Watchdog: telnet unreachable / crashlog / heap below floor -> abort arm, log incident, esptool recovery ready.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 One merged transcript per arm with stats+heap+503+crashlog
- [x] #2 Watchdog aborts an arm on incident and records it
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Wrote scripts/loadtest_arm_runner.py: telnet 'z' reset before the arm, wraps capture-mqtt-debug.bat for the arm duration (one transcript, -SkipBrowserCapture since the combined orchestrator (TASK-1020) already exercises the browser separately), watchdog thread polling telnet reachability / heap floor (via /api/v2/stats once TASK-1017 lands, falls back to telnet-banner minFree parse) / crashlog endpoint every --poll-interval seconds. Also added /api/v2/stats to capture-mqtt-debug.bat's curl-probes list (small, unrelated to the ADR-154 MiBeacon-crypto surface the file also touches) so the merged transcript picks up TASK-1017's stats once merged. Found + fixed a real bug during live testing: Popen.terminate()/kill() on a launched .bat only signals the top-level cmd.exe wrapper, not the PowerShell child process tree the .bat spawns — a watchdog abort against an unreachable host hung for the full duration+60s communicate() timeout instead of aborting. Fixed with taskkill /PID <pid> /T /F (kill_process_tree helper). Verified live: (1) normal arm against 192.168.88.64 produces one merged transcript under a walked subfolder; (2) abort path against an unreachable host (192.168.99.99) now aborts in 5.5s with incident.json recorded (previously hung ~100s pre-fix).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
scripts/loadtest_arm_runner.py: per-arm capture wrapper for the TASK-1015 sweep. One merged transcript per arm via capture-mqtt-debug.bat (AC#1, verified live) with /api/v2/stats now in its curl-probe list. Concurrent watchdog aborts the arm and writes incident.json on telnet-unreachable/heap-below-floor/crashlog-present (AC#2, verified live including a real process-tree-kill bug fix — plain Popen.terminate() doesn't kill a .bat's PowerShell child on Windows, taskkill /T /F does).
<!-- SECTION:FINAL_SUMMARY:END -->
