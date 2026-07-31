---
id: TASK-1036
title: 'Phase-3: remove ESP32-S3 heap-frag gating once ADR-167 is Accepted'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-09 21:17'
updated_date: '2026-07-31 20:50'
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
- [x] #1 ADR-167 is Accepted (precondition — do not start otherwise)
- [x] #2 Preventive drip/tier gating + delay(1) pacing removed from dev
- [x] #3 evaluate.py gates check_heap_fragmentation_promotion/check_per_consumer_heap_gate/check_heap_tier_entry_counters/check_heap_tier_thresholds_ordered updated to match, ADR-089/121 status flipped, evaluator green
- [ ] #4 Rebuilt + re-soaked clean (no tier escalations) to confirm no regression
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ADR-167 Accepted 2026-07-31 on the maintainer's direct instruction (adr-kit quality A / 0.92). AC#1 satisfied.

Removal landed as follows. Thresholds in helperStuff.ino reverted to ADR-030's original ladder (3072/5120/8192); ADR-089's ESP8266-tuned 1536/3072/5120 is gone. HEAP_FRAG_PROMOTE_MAXBLOCK and the fragmentation-aware promotion branch in getHeapHealth() deleted. ADR-121's per-consumer split deleted entirely: getHeapHealthForWebSocket(), getHeapHealthForMQTT(), heapTierWithThresholds() and the WS_HEAP_*/MQTT_HEAP_* macro ladders, plus the now-unused WEBSOCKET_/MQTT_THROTTLE_MS_* constants and the lastWebSocketSendMs/lastMQTTPublishMs statics. canSendWebSocket() and canPublishMQTT() collapsed to a CRITICAL-only block; all ~20 call sites unchanged.

MAINTAINER DECISION during implementation: ADR-167 Decision item 1 listed the tier-entry counters for removal, but they are a published contract (otgw-firmware/stats/enter_{low,warning,critical}, three HA diagnostic entities at faux id 247, state.heap.entered_* over REST, telnet + web UI rows) and ADR-167 item 4 simultaneously requires preserving raw heap observability. Maintainer chose 'keep as pure telemetry': the counters keep counting, they are no longer a gate input, and check_heap_tier_entry_counters is retired anyway. No published topic or HA entity was broken.

evaluate.py: all four gates removed (check_heap_tier_thresholds_ordered, check_heap_fragmentation_promotion, check_heap_tier_entry_counters, check_per_consumer_heap_gate), ~11.7KB of gate code, replaced by a comment block explaining the ADR-080 reasoning. ADR-089 and ADR-121 flipped to Superseded by ADR-167 with status_history entries; both note they remain in force on otgw-1.x.x. docs/adr/README.md index rows and the heap-threshold table updated; CLAUDE.md's binding-ADR list now names ADR-167 instead of ADR-089.

Verification: python evaluate.py 90 checks / 0 failures / exit 0. python tests/test_evaluate.py 47 tests OK. build.bat --target esp32 SUCCESS for firmware AND filesystem, artifacts confirmed fresh by mtime (not trusting exit code alone).

INCIDENTAL FIX: the first build failed on 'src/OTGW-firmware/version.h:8:1: error: version control conflict marker in file', cascading into ~40 bogus AceTime 'acetime_t does not name a type' errors. Pre-existing, unrelated to this task: an unresolved stash-pop conflict left in the working tree, whose two sides were byte-identical apart from CRLF vs LF. Resolved by keeping the upstream side and stripping the markers. Logged as bug-150 in .wolf/buglog.json.

AC#4 (re-soak) is NOT done and is not self-verifiable: it needs a clean multi-hour soak on dedicated hardware with no concurrent testing on the same unit. Left unchecked deliberately.

2026-07-31 status: ACs 1-3 complete and pushed as 9c0a7e78a. AC#4 is the only thing outstanding and is BLOCKED on hardware: it needs a clean multi-hour soak on a dedicated unit with no concurrent testing on the same board (the TASK-956 10h run was already contaminated by parallel PIC-flash testing, which is what produced its 1027ms loop-gap blemish). Left In Progress rather than Done so the open field-validation is visible. UNBLOCKS WHEN: a bench unit is free for an uninterrupted soak window.
<!-- SECTION:NOTES:END -->
