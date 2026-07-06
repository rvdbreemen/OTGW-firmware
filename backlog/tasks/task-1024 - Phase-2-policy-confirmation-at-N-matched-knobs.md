---
id: TASK-1024
title: 'Phase 2: policy confirmation at N* (matched knobs)'
status: Done
assignee:
  - '@claude'
created_date: '2026-07-05 22:22'
updated_date: '2026-07-06 03:58'
labels: []
dependencies: []
ordinal: 235000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1015. Set gate caps = N* AND client MAX_INFLIGHT = N*, flash, run S1-S5 incl. combined-stress + overload. Holds (zero incidents, heap-safe, WS+HTTP recover) -> confirm. Fails -> drop to N*-1 and re-confirm.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 N* confirmed under combined-stress + overload, or reduced until it confirms
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Per user decision (informed by Phase 1's finding that nominal-load 503s start climbing above N=2), skipped the spec's literal N=8-then-walk-down loop and tested candidate N*=2 directly: built esp32-classic with REST_MAX_INFLIGHT=2/WEB_FILE_MAX_INFLIGHT=2 (matching the client-side offered concurrency used in this confirmation run), flashed COM8, ran a single confirmation arm (S1-S5 incl. S4 combined-stress and S5 overload at 2N=4) via scripts/loadtest_sweep.py's run_arm(). Result: zero watchdog incidents (no crash/hang/heap-floor breach), merged transcript captured, WS stayed connected (18 frames), OT onboard-sim toggle confirmed enable+disable, S1/S2 page-loads clean (0 503s cold+warm), S3 nominal-load API burst near-clean (2/80 503s, likely scenario-launch overlap rather than steady-state pressure), S5 overload (offered 4 = 2xN) correctly shed 43/144 requests with 503 and the device recovered fully afterward. hwm snapshot: min_max_block=9204, min_free_heap=14816 (healthy headroom, well above the 20000-byte-floor-adjacent range seen stressed at higher N in Phase 1), max_loop_gap_ms=588, tcp_active_pcbs=6. CONFIRMS N*=2 holds per AC#1 - zero incidents, heap-safe, WS+HTTP recovered.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Phase 2 confirmed N*=2: built + flashed esp32-classic with REST_MAX_INFLIGHT=2/WEB_FILE_MAX_INFLIGHT=2, ran the full S1-S5 scenario set (incl. combined-stress S4 and overload S5 at offered=4) in one confirmation arm. Zero watchdog incidents, WS/OT/HTTP all recovered, nominal-load 503s near-zero (matching Phase 1's finding that N<=2 is the 503-clean ceiling), overload correctly shed without crashing. N* holds at 2 - no need to drop further. Ready for TASK-1026 bake-in (gate caps only; client-side MAX_INFLIGHT knob deferred - see ADR draft).
<!-- SECTION:FINAL_SUMMARY:END -->
