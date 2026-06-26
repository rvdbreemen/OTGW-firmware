---
id: TASK-937
title: >-
  refactor(heap): Phase 3 — remove the dead heap-frag PREVENTIVE gating, keep
  cheap floors + diagnostics (ESP32-S3)
status: To Do
assignee:
  - '@claude'
labels:
  - heap
  - esp32-s3
  - refactor
  - adr
dependencies:
  - TASK-934
  - TASK-935
ordinal: 151000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 3 of the heap-frag investigation. The TASK-935 soak (ESP32-S3, instrumented alpha.275) showed the largest contiguous block stays >=16 KB under sustained load and the preventive gating never fires (drip_slow_mode / ws_drops / mqtt_drops / entered_* all 0; min_max_block 31.7 KB; max_loop_gap 76 ms). So the COMPLEX preventive machinery — inherited from the ESP8266 (1.x) umm_malloc collapse, which does NOT happen on the ESP32-S3 multi-heap allocator — is dead weight on the 2.0.0 line.

### Decision (engineering judgment — NOT "delete everything")
The 30-min soak invalidates the COMPLEX preventive gating, not the value of cheap insurance or observability. Split:

REMOVE (the dead preventive machinery):
- Per-consumer heap ladders (ADR-121 Option B): WS_HEAP_*/MQTT_HEAP_* thresholds, getHeapHealthForWebSocket()/getHeapHealthForMQTT(), heapTierWithThresholds().
- The throttling gates: canPublishMQTT() / canSendWebSocket() (and the throttle timing constants + drop logging).
- maxBlock-promotion: HEAP_FRAG_PROMOTE_MAXBLOCK + the promotion branch inside getHeapHealth().
- The discovery-drip slow-mode throttle: discoveryDripHasHeapPressure()/discoveryDripIsHeapHealthyForRestore(), the slow/normal mode state machine, the drip skip counters.
- emergencyHeapRecovery() + its loop call site (OTGW-firmware.ino:820-822).

KEEP (cheap insurance + observability — near-zero cost when healthy):
- All TASK-934 observability: iMinMaxBlock watermark, aMaxBlockBucket histogram, sampleHeapWatermark()/resetHeapWatermark(), the 'z' key, and the MQTT stats min_max_block/maxblock_*/free_heap/max_block/frag_pct/max_loop_gap + getHeapFragmentation().
- A SIMPLIFIED getHeapHealth() that ONLY classifies the tier (free-heap thresholds, NO maxBlock-promotion) and increments the diagnostic entered_low/warning/critical counters, called once per loop. It no longer gates anything — it just records whether the field heap ever gets low. This preserves the enter_* field telemetry and keeps the evaluate.py check_heap_tier_entry_counters gate green.
- The cheap direct last-resort floors as defense-in-depth: SATcontrol.ino:1975 (maxBlock<8192 -> 503) and restAPI.ino:1861, and the MQTT_DISCOVERY_HEAP_MIN floor. These are 1-line, cost nothing when the heap is healthy, and prevent a NULL-alloc crash in a genuine future OOM the 30-min soak cannot rule out for every future load/feature. Use literal thresholds (not the removed promotion constant).
- TASK-342/347 Status-burst quiesce + cooldown — INDEPENDENT of heap gating (rate-limits burst allocations); unaffected.

DROP the now-sourceless gating counters: iWsDropsTotal, iMqttDropsTotal, iDripActiveBurstSkipCount, iDripCooldownSkipCount, iDripSlowModeCount (and their sendMQTTheapdiag topics + HA discovery sensors + telnet/REST lines). Keep iEntered* (still fed by the simplified getHeapHealth).

### Removal surface (from the Phase-3 map — verify line numbers at edit time)
- helperStuff.ino: WS_HEAP_*/MQTT_HEAP_* defines (~899-904), HEAP_FRAG_PROMOTE_MAXBLOCK (~939), throttle timing consts (~915-922), heapTierWithThresholds() (~984-994), getHeapHealthForWebSocket/MQTT() (~1004-1016), canSendWebSocket() (~1031-1080), canPublishMQTT() (~1085-1134), emergencyHeapRecovery() (~1169-1209). Simplify getHeapHealth() (drop the maxBlock-promotion branch, keep tier+counters). Simplify resetHeapWatermark() (drop the gating-counter resets, keep TASK-934 lines). Simplify logHeapStats() (drop removed counters).
- MQTTstuff.ino: discoveryDripHasHeapPressure/IsHeapHealthyForRestore (~2098-2125), drip mode state machine + skip/slow counters (~2136-2165), sendMQTTheapdiag drip/drop topic publishes (~1691-1700). **canPublishMQTT() FAN-OUT: exhaustively grep every call site (agents counted 6-16: MQTTstuff.ino 1392/1435/1774/2307/2392/3124/3246/3268/3373, MQTTHaDiscovery.cpp 2931) and convert each `if(canPublishMQTT())` to unconditional.**
- webSocketStuff.ino:274 — drop the canSendWebSocket() condition in sendLogToWebSocket().
- OTGW-firmware.ino:820-822 — remove the emergencyHeapRecovery gate block.
- handleDebug.ino:183-198 — drop the removed-counter dump lines, keep TASK-934 lines.
- restAPI.ino / networkStuff.ino banner — drop removed-counter fields.
- OTGW-firmware.h — drop the removed HeapDiagSection counter fields + the removed function prototypes; KEEP HeapHealthLevel enum (used by the simplified getHeapHealth).
- MQTTHaDiscovery.cpp — remove HA discovery configs for the dropped stats topics (else HA shows them "unavailable").

### evaluate.py gates (must land green)
- REMOVE invocations + methods: check_heap_fragmentation_promotion (ADR-089 sub-rule 2 — getHeapHealth no longer references HEAP_FRAG_PROMOTE_MAXBLOCK/platformMaxFreeBlock), check_per_consumer_heap_gate (ADR-121), and relax/remove check_heap_tier_thresholds_ordered if the per-tier thresholds collapse.
- KEEP: check_heap_tier_entry_counters (the simplified getHeapHealth still increments enter_*) and check_json_buffer_arithmetic (sendMQTTheapdiag stays).
- CI: `.github/workflows/evaluate.yml` runs `python evaluate.py --quick` — no workflow change, just fewer gates.

### ADRs (immutable Accepted -> SUPERSEDE, do not edit)
- Write a NEW ADR: "Remove ESP8266-era heap-frag preventive gating on the ESP32-S3 line" — cite the TASK-935 soak evidence (histogram 1967/1967 >=16k, counters 0), the keep/remove split, and the defense-in-depth floors retained.
- Supersede/amend in the new ADR's Supersedes chain: ADR-030 (root tier machine — amend: tier now diagnostic-only), ADR-089 (tier contract + frag promotion), ADR-107 (emergency recovery), ADR-121 (per-consumer ladders). Keep the originals as historical rationale.

### Risks / mitigations
- canPublishMQTT/canSendWebSocket fan-out: removing without updating every caller leaves orphaned conditionals / breaks the build — grep exhaustively first (atomic multi-file symbol removal).
- Removing ALL safety would delete defense-in-depth on 30 min of gentle-load data; mitigated by keeping the cheap floors + the diagnostic tier counters.
- Future load profiles / new features could fragment differently; mitigated by the kept floors + the retained observability (min_max_block/histogram still measured, so a regression is visible).
- HA discovery: dropped stats sensors must be wiped (zero-byte retained) or they linger "unavailable".
- Re-tune note: SATcontrol/restAPI floors use literal 8192 — keep the literal when the promotion constant is deleted.

## Execution status (2026-06-27, agent on maintainer go-ahead)

LANDED (commits 5c6db138 + cosmetic follow-up on feat/heap-soak-instr; builds clean for esp32, evaluate.py 0-fail/98.6%, adversarial review found ZERO real bugs):
- Removed: per-consumer ladders (WS_HEAP_*/MQTT_HEAP_*, getHeapHealthForWebSocket/MQTT, heapTierWithThresholds), throttle state/intervals, maxBlock tier-promotion (HEAP_FRAG_PROMOTE_MAXBLOCK), emergencyHeapRecovery() + its loop gate + the orphaned doWebSocketDisconnectAll().
- canSendWebSocket()/canPublishMQTT() neutered to always-true.
- Kept: simplified diagnostic getHeapHealth() (tier + enter_* counters, called once/loop), all TASK-934 observability, the cheap maxBlock<8192 / MQTT_DISCOVERY_HEAP_MIN floors, TASK-342/347 burst/cooldown.
- evaluate.py: retired check_heap_fragmentation_promotion + check_per_consumer_heap_gate; ADR-156 written (supersedes ADR-121/107, amends ADR-030/089). Misleading comments fixed.

DEFERRED (harmless dead code per the review — compiles + runs, never fires / publishes 0):
- The discovery-drip slow-mode throttle (discoveryDripHasHeapPressure/IsHeapHealthyForRestore + the slow/normal state machine in MQTTstuff.ino loopMQTTDiscovery).
- The sourceless gating-counter FIELDS (iWsDropsTotal/iMqttDropsTotal/iDrip*) + their sendMQTTheapdiag topics + handleDebug/REST/banner lines + the HA discovery sensors (a multi-file symbol removal — do it atomically, see [[atomic-multi-file-symbol-removal]]).
- AC#1 longer confirmatory soak: SKIPPED for the landing per go-ahead; still recommended before this ships to the field. The ADR-156 "Status" notes this.
- ADR-121/107 "Superseded by ADR-156" back-references (adr-kit guardian backfill / lint hygiene).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 GATE: a longer confirmatory soak (hours/days, ideally under heavier-but-stable load) reproduces the TASK-935 result (min_max_block well above floors, gating counters 0) BEFORE any removal lands
- [ ] #2 Preventive machinery removed per the surface above: per-consumer ladders, canPublishMQTT/canSendWebSocket gates (all call sites converted), maxBlock-promotion, drip slow-mode throttle, emergencyHeapRecovery
- [x] #3 Kept: all TASK-934 observability, a simplified diagnostic getHeapHealth() (tier+enter_* counters, no promotion/gating), the cheap maxBlock<8192 / MQTT_DISCOVERY_HEAP_MIN floors, and the TASK-342/347 burst/cooldown framework
- [x] #4 evaluate.py green: frag-promotion + per-consumer gates removed, tier-entry + buffer-arithmetic gates still pass; CI `evaluate.yml` passes
- [ ] #5 A new ADR documents the removal + soak evidence and supersedes/amends ADR-030/089/107/121; HA discovery sensors for dropped stats topics cleaned up
- [ ] #6 Builds clean for esp32; re-soak confirms no regression (heap still healthy, no new drops/stalls)
<!-- AC:END -->
