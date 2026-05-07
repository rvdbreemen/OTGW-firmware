---
id: TASK-553
title: >-
  fix(mqtt): add threshold-hysteresis (deadband + K-ticks) to drip mode
  transitions to stop ~60-90s thrash
status: To Do
assignee: []
created_date: '2026-05-07 08:45'
labels:
  - mqtt
  - heap
  - quality-of-life
  - telemetry
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field log from 1.5.0-beta.20+cbc21af (TASK-552 sibling-suffix shape, healthy heap ~12-14KB) shows the discovery drip slow/restore cycling every 60-90s during normal operation: e.g. 08:40:50 slowed -> 08:41:00 restored -> 08:41:23 slowed -> 08:41:33 restored -> 08:41:49 slowed -> 08:41:59 restored. TASK-370 added time-hysteresis (>=2s dwell in normal, >=10s dwell in slow) which stopped the same-second toggling, but a longer-cycle thrash remains because the heap recovers just barely above HEAP_LOW_THRESHOLD (5120 bytes) during the 10s slow-dwell, then the next Status-burst + WS pong + sensor publish overlap tips it back below 5120. Classic Schmitt-trigger problem: enter and exit on the same threshold.

Quality-of-life fix, not a stability concern (rate-limiter does its job either way). Reduces telnet log noise and gives more accurate iEnteredLowCount / drip_slowmode telemetry.

Recommended values (analyzed from code + field log):
- N_BYTES (deadband) = 1024: restore threshold becomes freeHeap >= 6144 (5120 + 1024). Covers a single discovery alloc footprint (~1KB transient incl. broker-side TX buffers) without over-delaying restoration. Conservative fallback: 2048 if field validation shows residual thrash.
- K_TICKS (consecutive healthy reads) = 2: require 2 successive slow-mode ticks (20s total) of confirmed-healthy heap before restoring. Defense-in-depth against transient blips that would slip through threshold-only check. Each tick happens at the existing drip cadence in slow-mode; no extra timers needed.

Builds on TASK-370 (time hysteresis), keeps that hysteresis intact.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A new constant HEAP_LOW_RESTORE_THRESHOLD = HEAP_LOW_THRESHOLD + 1024 (=6144) is defined in helperStuff.ino with a comment explaining the deadband rationale
- [ ] #2 loopMQTTDiscovery in MQTTstuff.ino uses HEAP_LOW_RESTORE_THRESHOLD (not HEAP_LOW_THRESHOLD) for the slow->normal restore decision; the normal->slow trigger remains at HEAP_LOW_THRESHOLD
- [ ] #3 loopMQTTDiscovery requires K=2 consecutive healthy reads (freeHeap >= HEAP_LOW_RESTORE_THRESHOLD) before restoring to normal mode; a counter resets to 0 on any unhealthy read
- [ ] #4 Existing time-hysteresis from TASK-370 (modeEnteredMs / canSwitch) is preserved unchanged
- [ ] #5 Block-header comment in loopMQTTDiscovery is updated to document both hysteresis layers (time + threshold + K-ticks) and reference TASK-370 plus this task
- [ ] #6 python build.py --firmware exits 0 with no new warnings
- [ ] #7 Field-log re-capture under steady healthy heap shows zero spurious slowed/restored pairs over a 5-minute window (validated against beta.20 baseline log)
- [ ] #8 Under real heap pressure (sustained freeHeap < 5120), slow-mode still engages within 1s (TASK-370 AC3 not regressed)
<!-- AC:END -->
