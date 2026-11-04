---
id: TASK-1022
title: 'Phase 1: capacity-curve sweep (offered-N 1..8, bounded gate 8)'
status: Done
assignee:
  - '@claude'
created_date: '2026-07-05 22:22'
updated_date: '2026-07-05 23:44'
labels: []
dependencies: []
ordinal: 233000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1015. One instrumented build, bounded safety gate cap=8. Sweep offered-concurrency 1,2,3,4,6,8 in software (no reflash). Run S1-S5 per step. Produce curve: latency + incidents + firmware hwm vs offered-N. Identify device ceiling + candidate N*.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Capacity curve (latency+incidents+hwm vs offered-N) produced
- [x] #2 Candidate N* = highest offered-N with zero incidents identified
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Built esp32-classic with PLATFORMIO_BUILD_FLAGS=-DREST_MAX_INFLIGHT=8 -DWEB_FILE_MAX_INFLIGHT=8 (bounded safety gate, above every test-N), flashed COM8 (192.168.88.64, --update, WiFi preserved). Wrote scripts/loadtest_sweep.py driving S1-S5 per arm across offered-N in {1,2,3,4,6,8}, no reflash between steps (software concurrency sweep). Found and fixed TWO real bugs during the first two live runs: (1) /api/v2/device/info nests all fields under a top-level 'device' key — my hwm_snapshot()/watchdog code read top-level and silently got None/false-positives; (2) the safety watchdog's heap-floor check used hd_min_free_heap, which is the ESP32 allocator's all-time-low watermark and is NOT resettable (confirmed non-resettable by design, unlike the other TASK-1017 counters) — once it dipped once in an earlier run, it stayed low forever, so the watchdog false-aborted EVERY arm at ~5.7s before a transcript could even form. Fixed to poll 'freeheap' (current, live) instead. Re-ran clean (v3): all 6 arms completed with zero watchdog incidents (no crash/hang/heap-floor breach) and a full merged transcript per arm.\n\nCapacity curve (v3, scenario-seconds=8 per arm):\nN=1: min_max_block=8692 max_loop_gap_ms=779 rest_503=1 webfile_503=6 (nominal-load S3 503=0)\nN=2: min_max_block=7668 max_loop_gap_ms=491 rest_503=5 webfile_503=19 (nominal-load S3 503=0)\nN=3: min_max_block=9204 max_loop_gap_ms=579 rest_503=26 webfile_503=39 (nominal-load S3 503=3 — FIRST nominal-load 503s appear here)\nN=4: min_max_block=7668 max_loop_gap_ms=742 rest_503=85 webfile_503=80 (nominal-load S3 503=31)\nN=6: min_max_block=7668 max_loop_gap_ms=644 rest_503=84 webfile_503=69 (nominal-load S3 503=35)\nN=8: min_max_block=6644 max_loop_gap_ms=613 rest_503=143 webfile_503=92 (nominal-load S3 503=69)\n\nS5 (2xN overload, by design meant to shed): 503s appear from N=2 onward (14,28,77,71,104) — working as intended, the gate correctly sheds overload traffic and the device recovers every time (zero incidents).\n\nKEY FINDING for Phase 2 (TASK-1024): per the AC's literal 'zero incidents' criterion (crash/hang/heap-floor breach), candidate N* = 8 — the bounded gate (cap=8) never let the device get bricked across the WHOLE tested range, confirming the Phase 1 safety design worked exactly as intended. HOWEVER a second, independent signal matters for Phase 2's real robustness bar: 503s under NOMINAL (non-overload) load start appearing at N=3 and climb steeply (3->31->35->69 at N=3/4/6/8) — meaning above N=2, real users would see visible request failures even without any overload, well before the device is in any danger of crashing. N=1 and N=2 are the only two arms with ZERO nominal-load 503s.\n\nRecommendation flagged to maintainer (not yet actioned): Phase 2 should NOT blindly start its confirmation loop at N=8 and walk down one by one (spec's literal method would cost ~6 build/flash/test cycles) — the Phase 1 curve already shows the real 'clean' ceiling sits at N=2. Suggest Phase 2 test N=8 (survivability, likely fails the SOFT robustness bar via visible 503s) and N=2 (the last nominal-zero-503 arm) side by side, rather than a blind linear walk.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Phase 1 capacity-curve sweep complete on the bounded-gate-8 instrumented build (esp32-classic, COM8, 192.168.88.64). AC#1: full curve produced (latency/incidents/hwm vs offered-N 1..8, scripts/loadtest_sweep.py, raw data in build/phase1_sweep_result_v3.json + per-arm transcripts under build/loadtest-sweep-phase1-v3/). AC#2: N* identified = 8 per the literal 'zero incidents' criterion (no crash/hang/heap-floor breach at any tested N — the bounded safety gate held). Found and fixed two real bugs in the measurement pipeline along the way (device/info JSON nesting, non-resettable heap-watermark false-positiving the watchdog) — both are load-test tooling issues, not firmware defects. Important nuance for Phase 2: nominal-load (non-overload) 503s start appearing at N=3 and climb steeply, so N=1-2 are the only 503-clean arms under normal traffic even though N=8 never crashes — flagged as a maintainer decision point before Phase 2 build/flash cycles are spent confirming N=8.
<!-- SECTION:FINAL_SUMMARY:END -->
