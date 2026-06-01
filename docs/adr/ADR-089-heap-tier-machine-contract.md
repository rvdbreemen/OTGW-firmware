# ADR-089: Heap Tier Machine Contract

## Status

Accepted. Date: 2026-04-26. Amends ADR-030 (4-state heap health system) by re-baselining tier thresholds on Crashevans field-log evidence (TASK-344), adding fragmentation-aware promotion (HEAP_FRAG_PROMOTE_MAXBLOCK), and adding tier-entry counter instrumentation (TASK-346).

## Context

ADR-030 introduced a 4-state heap health system (HEALTHY, LOW, WARNING, CRITICAL) with publish-throttling consumers. The state machine has held up well in field deployments, but the live code has drifted from the original ADR text in three load-bearing ways since 2025-Q4:

1. The original threshold ladder (3072 / 5120 / 8192 bytes) was re-baselined to 1536 / 3072 / 5120 bytes after the Crashevans field log (TASK-344) showed the firmware spent excessive time in throttled tiers because HEALTHY required >8 KB free, which is above the realistic ESP8266 free-heap floor during multi-client operation.
2. `getHeapHealth()` now consults `platformMaxFreeBlock()` and promotes a sub-LOW reading to WARNING when fragmentation alone could fail the next allocation. umm_malloc has no compaction, so a heap with 4 KB free but a 600-byte largest block fails every multi-KB request silently.
3. Three tier-entry counters (`iEnteredLowCount`, `iEnteredWarningCount`, `iEnteredCriticalCount`) were added in TASK-346 and now publish hourly to retained MQTT topics, giving field operators a directly-observable signal correlating with the symptoms ADR-088 attacks at the producer side.

These three changes are real architectural commitments encoded in the firmware today, but ADR-030 still describes the original shape. Reviewers cite ADR-030 and find the cited constants do not match the source. The contract is also missing an explicit statement of the consumer-side gate, which is what every WebSocket and MQTT publisher actually depends on.

This amendment captures the current contract, anchors it to source lines, and brings the binding sub-rules under ADR-080 governance with named CI gates landing alongside the ADR.

## Decision

The 4-state heap health tier introduced in ADR-030 is a fragmentation-aware ordinal contract over `platformFreeHeap()` and `platformMaxFreeBlock()`, with strict range-bounds, mandatory tier-entry counter instrumentation, and a mandatory consumer-side gate for every WebSocket send and MQTT publish.

The contract decomposes into five sub-rules. Each sub-rule is annotated with its enforcement level per ADR-080.

### Sub-rule 1: Tier ordering and range-bounds (pattern-level binding)

The three thresholds must satisfy `HEAP_CRITICAL_THRESHOLD < HEAP_WARNING_THRESHOLD < HEAP_LOW_THRESHOLD`. The current values are 1536 / 3072 / 5120 bytes (`helperStuff.ino:853-855`), re-tuned from the ADR-030 originals (3072 / 5120 / 8192) under TASK-344 evidence. The lower sanity bound is `HEAP_CRITICAL_THRESHOLD >= 1024`, below which the ESP8266 WiFi stack baseline (~1 to 2 KB) makes the tier value meaningless: WiFi will fail before the gate can fire.

Enforcement: new gate `check_heap_tier_thresholds_ordered`, landing alongside this ADR (TASK-428).

### Sub-rule 2: Fragmentation-aware promotion (pattern-level binding)

When `platformFreeHeap() < HEAP_LOW_THRESHOLD`, `getHeapHealth()` must consult `platformMaxFreeBlock()` and promote the result to WARNING when `maxBlock < HEAP_FRAG_PROMOTE_MAXBLOCK` (currently 1536 bytes, `helperStuff.ino:890`). The promotion logic lives at `helperStuff.ino:903-908`.

The justification is that umm_malloc on the ESP8266 has no compaction step. A heap reporting several kilobytes free but a small largest block will still fail the next multi-KB allocation, and the failure is silent: malloc returns null, the publish drops, and the only visible signal is a counter rising elsewhere. Promoting the tier on a fragmentation signal lets the consumer-side gate refuse the publish before allocation is attempted.

Enforcement: new gate `check_heap_fragmentation_promotion`, landing alongside this ADR (TASK-428). The gate FAILs if `getHeapHealth()` does not reference `HEAP_FRAG_PROMOTE_MAXBLOCK` or does not call `platformMaxFreeBlock()`.

### Sub-rule 3: Tier-entry counter instrumentation (pattern-level binding)

`getHeapHealth()` must increment `state.heapdiag.iEnteredLowCount`, `iEnteredWarningCount`, and `iEnteredCriticalCount` on transitions into stricter tiers, not on every call. The increments live at `helperStuff.ino:917-919`. The counter struct is `HeapDiagSection` at `OTGW-firmware.h:316-321`.

The counters publish hourly via `sendMQTTheapdiag()` to retained topics `otgw-firmware/stats/enter_low`, `enter_warning`, and `enter_critical` (`MQTTstuff.ino:1392-1394`). This makes the tier machine externally observable from a Home Assistant dashboard or MQTT log, and gives ADR-088 a peer signal: rising `iEnteredWarningCount` correlates directly with the burst-overlap symptom ADR-088 prevents at the producer side.

Enforcement: new gate `check_heap_tier_entry_counters`, landing alongside this ADR (TASK-428). The gate FAILs if any of the three increment lines is missing from `getHeapHealth()`.

### Sub-rule 4: Consumer-side mandatory gate (guideline-level)

Every WebSocket send must pass through `canSendWebSocket()` (`helperStuff.ino:938`). Every MQTT publish must pass through `canPublishMQTT()` or its eligibility wrapper. A direct `mqttClient.publish()` or `webSocket.broadcastTXT()` that bypasses the gate is a regression.

This sub-rule is explicitly guideline-level. Per-call-site enforcement is impractical without static analysis that can distinguish the gated wrappers from raw calls. Code review at PR time is the right enforcement layer. Cross-references TASK-358 (WS gate) and TASK-370 (MQTT gate).

### Sub-rule 5: Drop-counter and heapdiag visibility (guideline-level)

The drop counters (`iWsDropsTotal`, `iMqttDropsTotal`) and the three tier-entered counters must remain queryable via the telnet `s` command, the REST endpoint `/api/v2/health`, and the hourly retained MQTT topics under `otgw-firmware/stats/*` (`MQTTstuff.ino:1385-1404`). This is a diagnostic-surface property, not a recurring code pattern: there is no template that future code follows. It is documented here so a refactor that drops one of the surfaces is recognised as a regression at review time.

## Alternatives Considered

### Alternative 1: Keep the original ADR-030 thresholds (3072 / 5120 / 8192)

Reject the re-tuning and revert to the values in the original ADR-030 text.

Rejected per TASK-344 evidence. The Crashevans field log showed firmware spent excessive time in throttled tiers because the HEALTHY band started above the realistic ESP8266 free-heap floor during multi-client operation. Halving the thresholds (1536 / 3072 / 5120) brings the bands in line with the actual baseline observed across the deployed fleet. Reverting would re-introduce the symptom the re-tune fixed.

### Alternative 2: Time-based hysteresis instead of fragmentation-promotion

Add a time-based hysteresis layer (e.g. require N consecutive readings below threshold before transitioning) and skip the maxBlock check.

Rejected. Fragmentation can change instantly when a single large block frees, and time-based hysteresis lags the failure signal: by the time N readings have elapsed, the publish that motivated the transition has already failed. Promotion via `platformMaxFreeBlock()` is more responsive and directly attacks the failure mode (fragmented heap fails the next allocation silently). Time-based hysteresis attacks a different problem (tier flapping) which has not been observed in field logs.

### Alternative 3: Move thresholds to OTGWSettings runtime config

Promote the three thresholds to `settings.diag.iHeap*Threshold` so they can be tuned from the web UI at runtime.

Rejected. Threshold tuning is firmware-engineering territory, anchored to log evidence and platform behaviour, not user-facing configuration. Adding runtime config requires persistence, migration, validation, and a per-platform default ladder. The footgun is concrete: a user lowers `HEAP_CRITICAL_THRESHOLD` to "free up more memory", the gate stops firing, the firmware crashes on the next fragmented allocation. ADR-051 favours static `#define` for build-time invariants of this kind, and that guidance applies cleanly here.

## Consequences

### Positive

- The contract is nameable in code review. Reviewers cite ADR-089 instead of digging through ADR-030 plus several TASK histories to reconstruct the current state.
- Three of the five sub-rules are gate-enforced by `evaluate.py` at build time, blocking accidental regression of the threshold ladder, the fragmentation-promotion logic, or the counter instrumentation.
- The three tier-entry counters become first-class observable signals. They correlate with the ADR-088 counters (`iDripCooldownSkipCount`, `iDripActiveBurstSkipCount` at `MQTTstuff.ino:1668-1672`) so a field operator can distinguish "the producer side is dropping" from "the consumer side is throttling".
- ADR-088 and ADR-089 are now documented as defense-in-depth peers: ADR-088 prevents tier transitions by coordinating producers, ADR-089 catches the residue via fragmentation-aware promotion and consumer-side gating.

### Trade-offs

- The threshold values (1536 / 3072 / 5120) are tied to the ESP8266 baseline. ESP32 builds compile against the same constants but operate against different heap behaviour. ADR-061 covers the platform abstraction (`platformFreeHeap()`, `platformMaxFreeBlock()`) but does not cover value tuning. A future amendment can branch on `#ifdef ESP32` once ESP32 field data justifies a different ladder.
- Fragmentation-aware promotion fires on the LOW-to-WARNING boundary only. A heap reporting WARNING with severe fragmentation could in principle warrant CRITICAL, but the symptom that motivated the promotion (silent allocation failure under apparent free-heap headroom) is most acute at the HEALTHY-to-LOW boundary, where the consumer-side gate has not yet engaged.
- Sub-rule 4 (consumer-side gate) is guideline-level. A new direct publisher that bypasses `canSendWebSocket()` or `canPublishMQTT()` will not fail CI; it will only be caught at review.

### Risks and mitigations

- *Risk*: A reviewer lowers `HEAP_CRITICAL_THRESHOLD` below 1024 thinking it "frees more memory" for application use.
  *Mitigation*: `check_heap_tier_thresholds_ordered` FAILs the build below the 1024 sanity floor.

- *Risk*: A refactor of `getHeapHealth()` removes the `platformMaxFreeBlock()` call or drops the `HEAP_FRAG_PROMOTE_MAXBLOCK` reference.
  *Mitigation*: `check_heap_fragmentation_promotion` FAILs on the missing reference.

- *Risk*: A new counter is added to `HeapDiagSection` but `getHeapHealth()` never increments it.
  *Mitigation*: out of scope for this ADR. The three locked-down counters (`iEnteredLowCount`, `iEnteredWarningCount`, `iEnteredCriticalCount`) are gated; new counters added later need their own gate or guideline-level acceptance.

- *Risk*: ESP32 deployment with different heap behaviour silently mis-tunes the gate machine because the same constants apply.
  *Mitigation*: out of scope for this ADR. ADR-061 covers the platform abstraction. A future amendment can branch the constants on `#ifdef ESP32` once field data is available.

## Related Decisions

- **ADR-030** (Heap Memory Monitoring and Emergency Recovery): this ADR amends. The 4-state machine and the consumer-side throttling design are unchanged; only the thresholds, the fragmentation-promotion path, and the counter instrumentation are updated. ADR-030's Status block is updated to point at ADR-089; its decision text is preserved per the immutability rule.
- **ADR-001** (RAM Budget): drives the threshold ladder. The 1536 / 3072 / 5120 values are anchored to the ~40 KB usable heap budget.
- **ADR-005** (WebSocket Architecture): consumer of `canSendWebSocket()`. Sub-rule 4 names this gate as the mandatory entry point for every WS send.
- **ADR-006** (MQTT Architecture): consumer of `canPublishMQTT()`. Sub-rule 4 names this gate as the mandatory entry point for every MQTT publish.
- **ADR-061** (Platform Abstraction): defines `platformFreeHeap()` and `platformMaxFreeBlock()`. This ADR depends on both for its tier machine and fragmentation-promotion respectively.
- **ADR-080** (Binding ADR Rules Must Have a CI Gate): governance. Three sub-rules satisfy ADR-080 via new gates landing in TASK-428; two are explicitly labelled guideline-level.
- **ADR-088** (MQTT Status Burst Windowing and Cooldown): defense-in-depth peer. ADR-088 prevents tier transitions at the producer side; ADR-089 catches the residue at the consumer side via fragmentation-promotion and the gate. The `iEnteredWarningCount` counter from sub-rule 3 is the observable signal that links the two ADRs in field logs.

## References

- Source: `src/OTGW-firmware/helperStuff.ino:853-987` (thresholds, `getHeapHealth()`, `canSendWebSocket()`, fragmentation promotion, tier-entry counters, drop counters).
- Constants: `HEAP_CRITICAL_THRESHOLD = 1536` at `helperStuff.ino:853`, `HEAP_WARNING_THRESHOLD = 3072` at `helperStuff.ino:854`, `HEAP_LOW_THRESHOLD = 5120` at `helperStuff.ino:855`, `HEAP_FRAG_PROMOTE_MAXBLOCK = 1536` at `helperStuff.ino:890`.
- State: `HeapDiagSection` at `OTGW-firmware.h:316-321`.
- Telemetry: `sendMQTTheapdiag()` at `MQTTstuff.ino:1385-1404`; retained topics `otgw-firmware/stats/enter_low|warning|critical` at `MQTTstuff.ino:1392-1394`.
- CI gates: `check_heap_tier_thresholds_ordered`, `check_heap_fragmentation_promotion`, `check_heap_tier_entry_counters` land alongside this ADR in `evaluate.py` under TASK-428.
- Implementation history: TASK-344 (threshold re-baseline), TASK-346 (tier-entry counters), TASK-358 (WS gate), TASK-370 (MQTT gate).
