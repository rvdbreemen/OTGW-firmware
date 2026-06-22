---
id: TASK-902
title: Reduce hot-path heap allocations to restore 1.3.5-level headroom (the 'creep')
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-22 13:37'
updated_date: '2026-06-22 14:08'
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
