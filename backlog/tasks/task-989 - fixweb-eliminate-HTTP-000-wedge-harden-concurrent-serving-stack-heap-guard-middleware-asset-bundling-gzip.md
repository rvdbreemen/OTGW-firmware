---
id: TASK-989
title: >-
  fix(web): eliminate HTTP-000 wedge + harden concurrent serving (stack,
  heap-guard middleware, asset bundling, gzip)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-03 04:11'
updated_date: '2026-07-03 05:58'
labels: []
dependencies: []
ordinal: 201000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Load-ramp testing proved two failure modes: HTTP 503 = app backpressure gate (by design); HTTP 000 = async_tcp task wedged PERMANENTLY by ASYNC_TCP_STACK_SIZE=4096 overflow in the low-heap error path under concurrent bursts (browser opens ~9 parallel asset connections; AsyncTCP accepts unbounded; heap collapses 22K->3.8K). Fix iteratively with on-device A/B evidence per step.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Step 1: ASYNC_TCP_STACK_SIZE=16384 (ADR-139 blueprint value) restores post-burst recovery — A/B ramp evidence captured
- [ ] #2 Step 2: server-level heap-guard middleware rejects/aborts requests when maxblock below floor, BEFORE response/file allocs; ramp shows no heap collapse below 8K and no wedge
- [ ] #3 Step 3: v2 asset count reduced (bundle ds-tokens.css into v2.css or equivalent); ramp shows measurably fewer concurrent-burst timeouts
- [ ] #4 Step 4: static assets served gzipped; transfer sizes drop and burst behaviour improves or holds
- [ ] #5 Field validation: real browser (desktop+mobile) loads v2 UI fully styled on first paint
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
STEP 1 EVIDENCE (stack 4096->16384, A/B load-ramp on bench S3 .88.64): BEFORE: conc=2 already 3/6 timeouts, conc>=4 all-timeout, and after the ramp port 80 stayed DEAD (sequential 000, reboot-only; telnet stayed alive -> async_tcp task wedged, board fine -> stack overflow in low-heap error path, NOT core affinity). AFTER 16384: conc=2 clean (3x200+3x503, 0 timeouts), conc=4 partial, conc>=6 still times out during burst BUT board recovers ~6s after load drops (200s + port80 accepts). Permanent wedge eliminated. 16384 = ADR-139/EMS-ESP32 blueprint value; 4096 was drift. Transient overload remains -> steps 2-4.

ROOT CAUSE (definitive, on-device A/B 2026-07-03): BLE-on-by-default (NimBLE, TASK-975) consumes ~64K internal DRAM -> steady-state 24K free / 14K maxblock is BELOW the async web stack's working minimum. With SATbleenable=false: 88K free / 47K maxblock and the identical load ramp serves clean through conc=12 (23x200+13x503 orderly gate sheds, zero wedges, instant recovery) vs total collapse at conc=4 with BLE on. All observed failure modes (transient 000, API-dead-while-static-alive, total netif death, erratic recovery) are downstream of this one memory pressure. Also: middleware approach falsified and removed (bug-145..147); fonts were unrouted (now immutable-cached routes); asset loader serialized + API polls staggered (assets now load 4/4 first-shot vs 5-7-of-9 503 before). NimBLE trim flags (TASK-978) did NOT reclaim enough - needs verification whether they take effect at all. DECISION NEEDED (maintainer): BLE default OFF (revert TASK-975) vs deeper NimBLE trim vs PSRAM-only-default.
<!-- SECTION:NOTES:END -->
