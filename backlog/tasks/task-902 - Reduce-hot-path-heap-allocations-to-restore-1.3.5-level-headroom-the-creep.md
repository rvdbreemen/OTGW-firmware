---
id: TASK-902
title: Reduce hot-path heap allocations to restore 1.3.5-level headroom (the 'creep')
status: Done
assignee:
  - '@claude'
created_date: '2026-06-22 13:37'
updated_date: '2026-06-22 15:13'
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

FINAL (advisor-vetted): NO significant reducible code-level creep. The 'TOP' verify-buffer resize is GATED on maxFreeBlock>=1280 -> only fires when headroom EXISTS -> cannot cause a low floor -> demoted (option c, defer). Remaining items are tens-to-low-hundreds of bytes (bundle 22 heapdiag publishes->1 JSON; trim otOverrideStore[11]->9; pack bitmaps; char[]-ify 2 per-request String helpers) = NOT floor-movers. Bench head-to-head: 1.3.5 ~6624 vs 1.6/1.7 builds ~4700-5200 = ~1.3x (NOT the field 2.5x) -> code isn't 2.5x heavier; field gap = acute fragmentation ALREADY FIXED in 1.7.0 + inherent feature cost (HA discovery/entities/WS/REST). DO NOT run a bench experiment loop (reducers fire daily/hourly; 30-min soak measures noise). RECOMMENDATION: (a) STOP - beta.6 resolves the reported crash, creep = diminishing returns + regression risk on stability line [advisor: arguably best]; OR (b) one-shot low-risk tidy-up PR (heapdiag bundle + [11]->9), field-validate over days, no loop/bench; (c) defer verify-buffer. Awaiting user pick. Not closing pending decision.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Analyzed 1.3.5->1.7.0-beta.6 (281 commits, 6-agent fan-out). Conclusion (advisor-vetted): NO significant reducible code-level heap creep. Discovery is streamed; static RAM is net-leaner than 1.3.5 (sLine[1200] removed); OT-core hot path heap-alloc-free + leaner. The field 2.5x headroom gap = acute fragmentation ALREADY FIXED in 1.7.0 (beta.6 loop-cap + TASK-837/841/843 HTTP gates) + inherent cost of features 1.3.5 lacked (HA discovery, ~370 entities, WS live-log, REST). Bench head-to-head was ~1.3x not 2.5x. Reducible items are tens-to-low-hundreds of bytes (non-floor-movers) and the top candidate self-limits (gated on maxFreeBlock>=1280). User decision: (a) STOP - beta.6 resolves the reported crash; creep = diminishing returns + regression risk on a stability line. No code change. Tidy-up candidates (heapdiag-bundle, override[11]->9) recorded if ever wanted.
<!-- SECTION:FINAL_SUMMARY:END -->
