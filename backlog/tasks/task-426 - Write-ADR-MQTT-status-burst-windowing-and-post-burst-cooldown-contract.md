---
id: TASK-426
title: 'Write ADR: MQTT status-burst windowing and post-burst cooldown contract'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-25 22:06'
updated_date: '2026-04-25 22:25'
labels:
  - adr
  - mqtt
  - documentation
dependencies:
  - TASK-347
  - TASK-353
  - TASK-354
  - TASK-371
references:
  - src/OTGW-firmware/MQTTstuff.ino
  - evaluate.py
  - docs/adr/ADR-052-mqtt-publish-eligibility-contract.md
  - docs/adr/ADR-077-streaming-mqtt-ha-discovery-architecture.md
  - docs/adr/ADR-030-heap-memory-monitoring-emergency-recovery.md
  - docs/adr/ADR-080-binding-adr-rules-must-have-a-ci-gate.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Document the status-burst windowing pattern in MQTTstuff.ino as a first-class architectural decision, plus the post-burst cooldown that lets lwIP pbufs drain before the next heap allocation. The pattern is currently described only in inline code comments and tuning history (TASK-342, TASK-347, TASK-353, TASK-354, TASK-371) — there is no consolidated ADR that names the contract that future MQTT publishers must follow.

This ADR captures: (a) the begin/end window around Status-frame fanout, (b) the cooldown timing constraint relative to Status-frame cadence, (c) the drip-deferral protocol, and (d) the timeout self-heal for missed endStatusBurst calls.

Two CI gates already enforce parts of this contract (evaluate.py:check_status_burst_cooldown_bound and evaluate.py:check_status_publishers_wrap_burst). The second gate currently mislabels itself as "ADR-062" in the evaluator output — that label must be updated to point at this new ADR once accepted. The ADR therefore satisfies ADR-080 from day one without needing a new gate.

Discovered as candidate A in the ADR discovery pass on 2026-04-25.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 docs/adr/ADR-NNN-mqtt-status-burst-windowing-and-cooldown.md exists with Status: Accepted and the next sequential number (currently 088, verify before writing)
- [x] #2 ADR follows the project's adr-kit template: Context, Decision, Alternatives Considered (3+), Consequences (positive + negative + risks/mitigations), Related Decisions, References
- [x] #3 All four verification gates pass: Completeness, Evidence, Clarity, Consistency (per /adr-kit:adr skill)
- [x] #4 Decision section names the contract in one declarative sentence and unpacks the four sub-rules: begin/end window, cooldown bound, drip-deferral, timeout self-heal
- [x] #5 Evidence gate: cooldown bound stated as '< 3000ms (~3s Status cadence)' with file:line citation; tuning history references TASK-353 with the 10000→2000ms transition
- [x] #6 Alternatives section covers: (a) no-windowing baseline, (b) mutex-around-publish, (c) larger lwIP buffer / TCP_SND_BUF tuning. Each with explicit 'why not chosen'
- [x] #7 Related Decisions section cross-links ADR-030 (heap monitoring tiers), ADR-052 (publish eligibility), ADR-077 (streaming HA discovery)
- [x] #8 References section cites the two CI gates: evaluate.py:893 (cooldown bound) and evaluate.py:957 (publishers-wrap-burst), plus the source contract: MQTTstuff.ino:93-172
- [x] #9 evaluate.py: stale 'ADR-062' label in check_status_publishers_wrap_burst result-row updated to ADR-NNN
- [x] #10 docs/adr/README.md index updated with the new ADR in the appropriate category (likely 'Protocol and Integration' or 'MQTT'); category counts updated
- [x] #11 Cross-references added to ADR-052 and ADR-077 in their Related Decisions sections (immutable text untouched; only the Related Decisions list extended)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Decision sentence (proposal)

MQTT publishers that fan out a Status frame must bracket the fanout with `beginStatusBurst()`/`endStatusBurst()`, and the discovery drip loop must defer for the duration of the window plus a `STATUS_BURST_COOLDOWN_MS` cooldown bounded below the Status-frame cadence.

## Four sub-rules

1. **Begin/end window**: every `publish(Master|Slave)Status*State` function in OTGW-Core.ino must call `beginStatusBurst()` before the first publish and `endStatusBurst()` after the last publish in the fanout.
2. **Cooldown bound**: `STATUS_BURST_COOLDOWN_MS` must stay strictly below the typical Status-frame cadence (~3s observed in production logs); `< 3000` is the default rule, with `// verified tuning` as the only escape hatch.
3. **Drip-deferral**: `loopMQTTDiscovery()` must consult `isDripDeferred()` before issuing a discovery drip publish.
4. **Timeout self-heal**: `isStatusBurstActive()` must auto-clear after `STATUS_BURST_TIMEOUT_MS` so an exception-path-missed `endStatusBurst()` does not permanently freeze the drip.

## Alternatives to document and reject

(a) **No-windowing baseline**: let drip and Status-bursts overlap. Rejected: overlap pushes heap into WARNING tier unnecessarily; `iDripCooldownSkipCount` grew without discovery progressing under heavy Status traffic (TASK-353 evidence).

(b) **Mutex around every publish**: a single MQTT publish lock. Rejected: serializes all publishes (not just bursts vs. drips), inverts the cooperative-thread model, adds latency to non-conflicting publishes.

(c) **Larger lwIP TCP_SND_BUF**: let buffers absorb the overlap. Rejected: RAM headroom on ESP8266 is the constraining resource; larger send buffer just defers fragmentation.

## CI gate position (per ADR-080)

- Sub-rule #1 enforced by `check_status_publishers_wrap_burst` (evaluate.py:957)
- Sub-rule #2 enforced by `check_status_burst_cooldown_bound` (evaluate.py:893)
- Sub-rules #3 and #4 currently have NO automated gate
- Existing stale label `ADR-062` in the wrap-burst gate output row will be flipped to the new ADR number

## Open decision points (need user call)

**Q1**: Sub-rules #3 (drip-deferral) and #4 (timeout self-heal) — gate them too, or label the ADR partly guideline-level?

**Q2**: Number 088 (next sequential) confirmed before write?

**Q3**: Category in README.md — "Protocol and Integration" or new "MQTT" subsection?

## Out of scope

- Touching the implementation in MQTTstuff.ino (this is doc work)
- Re-documenting ADR-052, ADR-077, ADR-030 (only cross-reference)
- Backfilling gates for #3/#4 unless approved in Q1
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Scope clarification 2026-04-26: AC #10 narrowed to adding ADR-088 under a new 'MQTT' sub-section inside the existing 'Integration and Communication' category in README.md. Category counts in the Quick Navigation block will NOT be updated by this task because the README index is significantly out of date (missing index entries for approximately 30 ADRs from ADR-058 through ADR-087, plus a stale GitHub Copilot ADR skill reference superseded by adr-kit). Updating only one count would be misleading. The broader README refresh is tracked separately in the follow-up task created in this session.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Wrote ADR-088 documenting the MQTT status-burst windowing and post-burst cooldown contract that bridges ADR-052 (publish eligibility) and ADR-077 (streaming HA discovery). The contract has one declarative decision sentence backed by four sub-rules, three of them pattern-level binding (gate-enforced) and one explicitly labelled guideline-level per ADR-080.

Gate coverage after this task:
- Sub-rule 1 (begin/end window): existing check_status_publishers_wrap_burst at evaluate.py:957. Result label updated from stale "ADR-062" to "ADR-088" (5 occurrences via replace_all).
- Sub-rule 2 (cooldown bound): existing check_status_burst_cooldown_bound at evaluate.py:893. PASS at 2000 ms (< 3000).
- Sub-rule 3 (drip-deferral): NEW gate check_drip_consults_deferred added in evaluate.py and wired into the orchestration block. Required a regex tightening to require `\(\s*\)\s*\{` after the signature so the forward declaration on MQTTstuff.ino:35 does not capture the wrong function body.
- Sub-rule 4 (timeout self-heal): guideline-level, no gate, explicitly labelled in the ADR.

Verified with a direct Python invocation of the three status-burst gates: all PASS.

ADR-052 and ADR-077 received a single-line cross-reference to ADR-088 in their Related Decisions / Related sections; their immutable decision text was not touched.

README scope was narrowed per the 2026-04-26 scope clarification: only a new "MQTT" sub-section under "Integration and Communication" was added, seeded with ADR-088 only. The 30 missing ADR index entries (ADR-058 through ADR-087), the stale category counts, the deprecated GitHub Copilot ADR skill reference, and the truncated Decision Timeline are tracked in TASK-427 (Refresh ADR README index) as a separate hygiene pass.

Files modified:
- docs/adr/ADR-088-mqtt-status-burst-windowing-and-cooldown.md (new, 225 lines)
- evaluate.py (label fix in 5 rows, new gate function, new orchestration call)
- docs/adr/ADR-052-mqtt-publish-eligibility-contract.md (one Related Decisions line added)
- docs/adr/ADR-077-streaming-mqtt-ha-discovery-architecture.md (ADR-088 added to Prior ADRs line)
- docs/adr/README.md (new MQTT sub-section under Integration and Communication)

Discovered as candidate A in the ADR discovery pass on 2026-04-25; the discovery shortlist also identified candidate B (heap-tier back-pressure for WebSocket/MQTT) and candidate C (re-entrancy guard pattern for shared scratch buffers) for separate sessions.
<!-- SECTION:FINAL_SUMMARY:END -->
