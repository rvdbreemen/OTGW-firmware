---
id: TASK-902
title: Reduce hot-path heap allocations to restore 1.3.5-level headroom (the 'creep')
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-22 13:37'
updated_date: '2026-06-22 14:22'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-901 (beta.6/delay1) fixed the CLIFF (the delay1->yield regression that crashed 1.6.x+ and made users downgrade). It does NOT restore 1.3.5-level heap headroom. Field transcripts show the CREEP: maxBlock p05 ~10456 (1.3.5) vs ~4040 (beta.13) = 2.5x less headroom by the 1.6.0 cycle, accumulated gradually 1.3.5->1.4->1.5->1.6. The loop cap (delay1) keeps the floor from collapsing but the baseline headroom is still low. To deliver the original goal ('as stable as 1.3.5, minimize heap pressure') the hot-path allocations must be cut: String use in MQTT/WS/JSON paths, per-message/per-publish buffers, HA discovery republish churn. Use the committed bench rig (scripts/tests: sim replay + MQTT + overload + heap_sampler, tier counters hd_enter_low/critical) to measure maxBlock-floor improvement per change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Top hot-path heap allocators identified (MQTT publish, WS send, JSON build, discovery) with evidence (which alloc, how often, size)
- [ ] #2 Reduce them (String->char[]/strlcpy, reuse static buffers, avoid per-message allocs) without behaviour change
- [ ] #3 Measured maxBlock p05 floor under the sim+MQTT bench rig improves materially toward 1.3.5-level vs beta.6 baseline
- [ ] #4 evaluate.py green + field validation on a boiler-connected unit
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ANALYSIS DONE (6-agent fan-out, 20 findings). KEY: NO single big reducible heap-hog. Discovery is STREAMED (128B chunks, not a big allocator); HEAD removed 1.3.5 sLine[1200] -> static RAM NET LEANER than 1.3.5; OT-core hot path heap-alloc-free + leaner. Reducible items are SMALL: (TOP,med) discovery-verify reallocs long-lived MQTT buffer 384<->1024 daily/hourly -> relocation holes; fix=keep 384 (verify reads topic only). (low) 22 heapdiag publishes/hr -> bundle to 1 JSON. (low) minor String churn per-HTTP-request helpers; over-provisioned statics (override[11]->9, 224B bitmaps). HONEST: field 2.5x gap (1.3.5 p05 10456 vs beta.13 4040) is mostly (a) ACUTE fragmentation ALREADY FIXED in 1.7.0 (beta.6 loop-cap + TASK-837/841/843 HTTP gates) + (b) inherent cost of FEATURES 1.3.5 lacked (HA discovery, more entities, WS, REST) = steady activity not wasteful code. Reducible code != 2.5x recovery. CAVEAT: top reducers fire daily/hourly -> 30-min bench soak CANNOT measure them; they are reasoned+field-validated, not bench-A/B-able. Vetting via advisor.
<!-- SECTION:NOTES:END -->
