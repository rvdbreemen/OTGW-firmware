---
id: TASK-428
title: 'Write ADR: Heap tier-machine contract (amendment to ADR-030)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-25 22:34'
updated_date: '2026-04-26 08:56'
labels:
  - adr
  - memory
  - documentation
dependencies:
  - TASK-346
  - TASK-358
references:
  - src/OTGW-firmware/helperStuff.ino
  - src/OTGW-firmware/OTGW-firmware.h
  - evaluate.py
  - docs/adr/ADR-030-heap-memory-monitoring-emergency-recovery.md
  - docs/adr/ADR-080-binding-adr-rules-must-have-a-ci-gate.md
  - docs/adr/ADR-088-mqtt-status-burst-windowing-and-cooldown.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-030 (Heap Memory Monitoring and Emergency Recovery, 2026-02-07) introduced a 4-state heap health system with adaptive throttling. The live code has since drifted in three load-bearing ways that need an explicit amendment ADR:

1. Threshold values were re-tuned: ADR-030 documents 3072/5120/8192 (CRITICAL/WARNING/LOW), the live code uses 1536/3072/5120 (per TASK-344 / Crashevans field log evidence). ADR-030 still claims the old values verbatim.
2. A fragmentation-aware promotion mechanism was added (HEAP_FRAG_PROMOTE_MAXBLOCK in helperStuff.ino:890): when freeHeap is in the LOW band but the largest contiguous block is below 1536 bytes, getHeapHealth() promotes to WARNING. ADR-030 mentions neither this mechanism nor the rationale.
3. Tier-entry counters (iEnteredLowCount, iEnteredWarningCount, iEnteredCriticalCount in helperStuff.ino:917-919, TASK-346) instrument tier transitions for telemetry. ADR-030 has no entry for these.

The original 4-state idea and the canSendWebSocket / canPublishMQTT consumer gates remain valid, so this is an amendment, not a supersession. ADR-030 stays as historical context; ADR-089 captures the current binding contract and adds CI gates per ADR-080.

Discovered as candidate B in the ADR discovery pass on 2026-04-25; route 1 (amendment) selected on 2026-04-26.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 docs/adr/ADR-NNN-heap-tier-machine-contract.md exists with Status: Accepted, Date: 2026-04-26, and 'Amends: ADR-030' explicit in Status block
- [x] #2 Decision section names the contract in one declarative sentence and unpacks the five sub-rules with explicit pattern-level vs guideline-level annotation per ADR-080
- [x] #3 Sub-rule 1 (tier ordering and range-bounds): HEAP_CRITICAL_THRESHOLD < HEAP_WARNING_THRESHOLD < HEAP_LOW_THRESHOLD, with current values cited (1536/3072/5120) and TASK-344 / Crashevans evidence linked. CI gate: new check_heap_tier_thresholds_ordered
- [x] #4 Sub-rule 2 (fragmentation-aware promotion): getHeapHealth() must consult platformMaxFreeBlock() in the LOW band and promote to WARNING when maxBlock < HEAP_FRAG_PROMOTE_MAXBLOCK (1536). CI gate: new check_heap_fragmentation_promotion
- [x] #5 Sub-rule 3 (tier-entry counter instrumentation): getHeapHealth() increments iEnteredLowCount / iEnteredWarningCount / iEnteredCriticalCount on entry into stricter tiers (TASK-346). CI gate: new check_heap_tier_entry_counters
- [x] #6 Sub-rule 4 (consumer-side mandatory gate): WebSocket sends pass through canSendWebSocket(), MQTT publishes pass through canPublishMQTT(). Labelled guideline-level (call-graph enforcement is impractical; code review is the right layer). Cross-references TASK-358 / TASK-370 as historical examples
- [x] #7 Sub-rule 5 (drop-counter and heapdiag visibility): drop counters and tier-entered counters remain queryable via telnet status and the REST /api/v2/health endpoint. Labelled guideline-level
- [x] #8 Alternatives Considered: (a) keep ADR-030's original 3072/5120/8192 thresholds, (b) time-based hysteresis instead of fragmentation-promotion, (c) move thresholds to OTGWSettings runtime config. Each with explicit 'why not chosen'
- [x] #9 Related Decisions cross-links ADR-030 (amends), ADR-001 (RAM budget), ADR-005 / ADR-006 (consumer paths), ADR-080 (gate governance), ADR-088 (status-burst windowing cooperates with tier promotion). Implementation history references TASK-344, TASK-346, TASK-358, TASK-370
- [x] #10 Three new CI gates land in evaluate.py and are wired into the orchestration block; gates pass on the current source tree
- [x] #11 ADR-030 receives a single-line update in its Status block: 'Amended by ADR-NNN' under the existing Status line. ADR-030's decision text is not modified
- [x] #12 docs/adr/README.md updated: ADR-089 added to 'Memory Management' category. Per the 2026-04-26 scope clarification, no category counts updated and no other index drift addressed (TASK-427 still owns the broader refresh)
- [x] #13 All four /adr-kit:adr verification gates pass: Completeness, Evidence, Clarity, Consistency
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Decision sentence (proposal)

The 4-state heap health tier introduced in ADR-030 is a fragmentation-aware ordinal contract over `platformFreeHeap()` and `platformMaxFreeBlock()`, with strict range-bounds, mandatory tier-entry counter instrumentation, and a mandatory consumer-side gate for every WebSocket send and MQTT publish.

## Five sub-rules

1. **Tier ordering and range-bounds** [pattern-level binding]
   `HEAP_CRITICAL_THRESHOLD < HEAP_WARNING_THRESHOLD < HEAP_LOW_THRESHOLD`. Current values: 1536 / 3072 / 5120 (re-tuned per TASK-344 from ADR-030's original 3072 / 5120 / 8192 based on Crashevans field log). The lower bound HEAP_CRITICAL_THRESHOLD >= 1024 is sanity (ESP8266 WiFi stack baseline ~1-2 KB).
   Gate: NEW `check_heap_tier_thresholds_ordered` in evaluate.py.

2. **Fragmentation-aware promotion** [pattern-level binding]
   When `freeHeap < HEAP_LOW_THRESHOLD` (LOW band), `getHeapHealth()` must consult `platformMaxFreeBlock()` and promote to WARNING if `maxBlock < HEAP_FRAG_PROMOTE_MAXBLOCK` (1536). Without this, a fragmented but free-byte-rich heap fails the next allocation silently because umm_malloc has no compaction.
   Gate: NEW `check_heap_fragmentation_promotion` (verifies HEAP_FRAG_PROMOTE_MAXBLOCK is defined and that getHeapHealth's body references both `platformMaxFreeBlock(` and the promote constant).

3. **Tier-entry counter instrumentation** [pattern-level binding]
   `getHeapHealth()` must increment `state.heapdiag.iEnteredLowCount` / `iEnteredWarningCount` / `iEnteredCriticalCount` on transitions into stricter tiers (TASK-346). Counters give operators an observable signal for tuning the thresholds in the field.
   Gate: NEW `check_heap_tier_entry_counters` (verifies the three counter increments appear in getHeapHealth's body).

4. **Consumer-side mandatory gate** [guideline-level]
   Every WebSocket send must pass through `canSendWebSocket()`; every MQTT publish must pass through `canPublishMQTT()` or the publish-eligibility chain that wraps it. Labelled guideline-level because per-call-site call-graph enforcement is impractical; code review is the right layer. Cross-references TASK-358 (REST /verify dedupe) and TASK-370 (HEAP_LOW back-off) as historical examples of consumers consulting the tier.

5. **Drop-counter and heapdiag visibility** [guideline-level]
   Drop-counters and tier-entered counters remain queryable via telnet `s` command and the REST `/api/v2/health` endpoint. Not gated, not a recurring pattern — a property of the diagnostic surface that operators rely on but that future code is not constrained to reproduce.

## Alternatives Considered

(a) **Keep ADR-030's original 3072 / 5120 / 8192 thresholds**: rejected per TASK-344. Crashevans field log showed the firmware spent excessive time in throttled tiers under normal load; HEALTHY at >8 KB was set above realistic ESP8266 free-heap during normal multi-client operation. Halving tightens headroom to actual baseline (~1.5-2 KB for WiFi stack) without compromising safety.

(b) **Time-based hysteresis instead of fragmentation-promotion**: rejected. Fragmentation can change instantly when a large block frees; fragmentation-aware promotion is more responsive and is the actual leading indicator of allocation failure on umm_malloc (no compaction). Time-based hysteresis would lag.

(c) **Move thresholds to OTGWSettings runtime config**: rejected. Threshold tuning is firmware-engineering territory anchored to evidence (field logs, profiling), not user-facing config. Would require persistence in `OTGWSettings`, migration, validation, and exposes a footgun (user lowers thresholds to "free more memory", system crashes). ADR-051 discipline favours static `#define` for build-time invariants.

## CI gate position (per ADR-080)

- Sub-rules 1, 2, 3: NEW gates in evaluate.py (three functions), all expected to PASS on current source.
- Sub-rule 4: guideline-level (no gate, code-review enforcement).
- Sub-rule 5: guideline-level (no gate, diagnostic surface property).

## Out of scope

- Touching ADR-030's decision text (only Status block gets "Amended by ADR-089").
- Re-tuning thresholds (this ADR documents the current values, does not change them).
- Refactoring helperStuff.ino (this is documentation work).
- README index drift beyond the ADR-089 entry under Memory Management.

## Open decision points (need user call)

**Q1**: Sub-rule split confirmed (1-3 pattern-level with new gates, 4-5 guideline-level)?

**Q2**: Number 089 (next sequential after just-landed 088) — verify before write?

**Q3**: README placement: under "Memory Management" (where ADR-030 lives) vs under "Integration and Communication > MQTT" sub-section we just created? My preference: Memory Management. Tier-machine is a memory-management mechanism, even though its consumers are MQTT and WebSocket. Both consumers have their own ADRs already (006, 005).
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Wrote ADR-089 (122 lines) as an amendment to ADR-030. The 4-state heap health system from ADR-030 stays intact; this amendment captures three load-bearing changes that landed in the live code since 2025-Q4 (TASK-344 threshold re-baseline, TASK-346 tier-entry counters, fragmentation-aware promotion via HEAP_FRAG_PROMOTE_MAXBLOCK) and adds the consumer-side gate contract that ADR-030 only implied.

The decision is one declarative sentence backed by five sub-rules with explicit ADR-080 annotations: three pattern-level binding (gate-enforced) and two guideline-level (consumer-side gate enforcement, drop-counter visibility).

Three new CI gates landed in evaluate.py:
- check_heap_tier_thresholds_ordered: verifies HEAP_CRITICAL < HEAP_WARNING < HEAP_LOW and HEAP_CRITICAL >= 1024 (sanity floor for ESP8266 WiFi stack baseline). PASS at 1536/3072/5120.
- check_heap_fragmentation_promotion: verifies HEAP_FRAG_PROMOTE_MAXBLOCK is defined and that getHeapHealth() body references both the constant and platformMaxFreeBlock(). PASS.
- check_heap_tier_entry_counters: verifies getHeapHealth() body contains the three iEntered*Count increments. PASS.

All three gates reuse the strict signature regex pattern (\(\s*\)\s*\{ ) learned from ADR-088's forward-declaration trap so they target the function definition only.

ADR-030 received a single Updated: line in its Status block per the immutability rule (Decision text untouched). README received the ADR-089 entry under Memory Management with a one-line summary describing the amendment scope, defense-in-depth peer relationship with ADR-088, and the three-gates-plus-two-guidelines split.

One nit caught and fixed during self-review: the subagent had cited a counter named `iStatusBurstSkipCount` that does not exist; the actual counter is `iDripActiveBurstSkipCount` at MQTTstuff.ino:1668-1672. Fixed in the Consequences section so the Evidence gate stays clean.

Files modified:
- docs/adr/ADR-089-heap-tier-machine-contract.md (new, 122 lines after the iDripActiveBurstSkipCount fix)
- evaluate.py (three new gate functions ~190 lines, three new orchestration calls)
- docs/adr/ADR-030-heap-memory-monitoring-emergency-recovery.md (Status block: one Updated: line added, Decision text preserved)
- docs/adr/README.md (ADR-089 entry added under Memory Management; per scope clarification, no category counts updated)

Discovered as candidate B in the ADR discovery pass on 2026-04-25; route 1 (amendment) selected on 2026-04-26. Candidate C (re-entrancy guard pattern for shared scratch buffers) remains for a separate session.
<!-- SECTION:FINAL_SUMMARY:END -->
