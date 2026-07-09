---
id: TASK-1036
title: 'Phase-3: remove ESP32-S3 heap-frag gating once ADR-167 is Accepted'
status: To Do
assignee: []
created_date: '2026-07-09 21:17'
labels: []
dependencies: []
ordinal: 245000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-956 (heap-frag soak investigation, complete). The soak evidence (10h + 30-June, 0 tier escalations) is captured and ADR-167 (Retire the ESP8266-Era Heap Tier Machine and Per-Consumer Gating on the ESP32-S3-Only Dev Branch) is drafted as Proposed. This task is the actual Phase-3 removal, GATED on the maintainer accepting ADR-167. When accepted: remove dev's preventive drip/tier gating + delay(1) loop pacing; update the evaluate.py gates that REQUIRE the removed code (check_heap_fragmentation_promotion / check_per_consumer_heap_gate / check_heap_tier_entry_counters / check_heap_tier_thresholds_ordered under ADR-089/121) together with the ADR status flips; rebuild + re-soak clean to confirm no regression. Do NOT start until ADR-167 is Accepted. Recommend one clean re-soak (no concurrent hardware testing on the same unit) either before or after removal to settle the loop-gap sub-criterion TASK-956 flagged.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ADR-167 is Accepted (precondition — do not start otherwise)
- [ ] #2 Preventive drip/tier gating + delay(1) pacing removed from dev
- [ ] #3 evaluate.py gates check_heap_fragmentation_promotion/check_per_consumer_heap_gate/check_heap_tier_entry_counters/check_heap_tier_thresholds_ordered updated to match, ADR-089/121 status flipped, evaluator green
- [ ] #4 Rebuilt + re-soaked clean (no tier escalations) to confirm no regression
<!-- AC:END -->
