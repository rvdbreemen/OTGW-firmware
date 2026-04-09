---
id: TASK-222
title: 'SAT: Port PID state persistence across restarts'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-09 05:29'
updated_date: '2026-04-09 05:36'
labels:
  - audit-fix
  - sat
  - pid
dependencies: []
references:
  - backlog/docs/sat-feature-completeness-matrix.md
  - backlog/docs/sat-python-cpp-mapping.md
  - src/OTGW-firmware/SATpid.ino
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python persists PID state (integral accumulator, last error, last timestamp) to HA Store and restores it on startup via async_get_last_state(). The firmware currently starts with a zeroed PID state on every reboot.

Impact: After a power cycle or OTA update, the integral term starts from 0. On cold mornings this means the system undershoots for the first ~15 minutes while the integral re-accumulates. For homes with slow thermal mass (underfloor heating) this can mean 30+ minutes of suboptimal heating.

Complexity: Low. The integral, last_error and last_timestamp must be stored to LittleFS (settings struct or a separate state file). The most critical value is the integral term; derivative and proportional terms can safely restart from 0.

Risk: If stale state is restored after an unusually long downtime (e.g. summer mode), the integral may be wildly wrong for current conditions. A staleness check (e.g. discard if last_saved > 30 min ago) should guard against this.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 PID integral term is written to LittleFS when it changes (debounced, not every second)
- [ ] #2 PID state is restored on startup if saved timestamp is within 30 minutes of current time
- [ ] #3 If saved state is stale or missing, PID starts from zero (current behavior)
- [ ] #4 State is stored in settings.sat struct or a dedicated SAT state file in LittleFS
- [ ] #5 No regression on existing PID behavior in the normal (non-restart) path
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. The integral (fPidI) IS already saved in /sat_pid_state.json as field "i"
2. Missing: no timestamp saved, so stale-state detection is impossible
3. Add Unix timestamp "ts" to satSavePidState() using time(nullptr)
4. In satLoadPidState(): read "ts", compare to current time(nullptr), discard if >1800s (30 min)
5. If time(nullptr) returns 0 (NTP not yet synced on boot), skip restore - start from zero
6. No changes to the periodic save interval (every 5 min) or satDisable() call path
<!-- SECTION:PLAN:END -->
