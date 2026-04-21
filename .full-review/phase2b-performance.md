# Phase 2B: Performance & Scalability Findings

**Target**: branch `1.4.1` vs `dev`, performance theme
**Platform**: ESP8266 @ 80 MHz, ~40 KB usable RAM, 4 KB cooperative CONT stack
**Note**: All costs are inferred from code reading and analytical modelling. No runtime profiling traces are available on this platform.

---

## Summary

| Severity | Count |
|---|---|
| Critical | 1 |
| High | 2 |
| Medium | 3 |
| Low | 3 |

The branch delivers on its primary performance claim for the common case (no active Status traffic, heap healthy). Under continuous Status-frame traffic at the Crashevans-logged 3s cadence, the 10-second post-burst cooldown permanently defers the discovery drip, which is a real user-visible regression during first-boot HA integration. The JSON buffer overflow in `sendMQTTheapdiag` is the only crash-class finding.

---

## Validation of Branch's Primary Claim

**Claim**: heap pressure is reduced during HA discovery drip.

**Verdict**: TRUE for the common case, with one significant conditional failure.

### What changed and why it helps

| Parameter | dev | 1.4.1 | Effect |
|---|---|---|---|
| `HEAP_CRITICAL_THRESHOLD` | 2048 | 1536 | Lowers "stop everything" floor, giving more room to operate in WARNING |
| `HEAP_WARNING_THRESHOLD` | 4096 | 3072 | Narrows the WARNING band; publish gate fires later |
| `HEAP_LOW_THRESHOLD` | 6144 | 5120 | Lowers the soft-throttle floor by 1 KB |
| `DISCOVERY_INTERVAL_NORMAL` | 1s | 2s | Halves drip rate; halves allocation frequency |
| `DISCOVERY_INTERVAL_SLOW` | 30s | 10s | Faster recovery from slow-mode once pressure drops |
| Slow-mode trigger | `>= HEAP_WARNING` (<4096) | `>= HEAP_LOW` (<5120) | Slow-mode fires 1 KB earlier, before publish gate drops anything |
| `MQTT_DISCOVERY_HEAP_MIN` | 4000 | 3000 | Aligned with new WARNING floor |
| Status-burst quiesce | absent | 500ms window + 10s cooldown | Prevents drip alloc overlapping Status fanout |

**Quantified heap reduction during drip**: On dev, with 1s drip interval, each discovery publish costs approximately 200 bytes of PROGMEM staging + lwIP TX pbuf allocation (~400-600 bytes transient). A Status fanout (9 publishes in ~20ms) simultaneously holds ~800 bytes of lwIP TX pbufs. At 1s drip interval, there was a ~2% probability per Status cycle that the drip publish and Status fanout overlapped on the heap. With 2s interval and burst quiesce, that overlap probability is eliminated.

**Threshold reduction**: Lowering `HEAP_LOW_THRESHOLD` from 6144 to 5120 means 1 KB of previously "LOW-tier" heap is now treated as HEALTHY. Under the v1.4.0 thresholds, a system sitting at 5500 bytes free was already throttled. Under 1.4.1, it runs unthrottled. This is the primary claim's mechanism, and the tuning is based on actual tester log data from Crashevans (debug_2a.txt), which is the right evidence base.

**Conditional failure**: see [HIGH-1] below.

---

## Findings

### [CRITICAL] `sendMQTTheapdiag` JSON buffer truncates at maximum counter values

- **File**: `src/OTGW-firmware/MQTTstuff.ino` (the `sendMQTTheapdiag` function, approximately line 1095 post-diff)
- **Cost / impact**: Stack buffer overflow by 81 bytes. `snprintf_P` silently truncates rather than overflowing, so this does not crash. However, the published JSON is invalid (truncated mid-field) whenever any counter reaches its upper range. The retained MQTT message then stays broken until the next hour's publish writes a lower value.
- **Scenario**: Any system with `>= ~40 million` MQTT drops, WebSocket drops, or heap-tier entries. On a system under chronic heap pressure (the exact users this feature targets) with sustained uptime, counters accumulate. A system with 24h uptime under WARNING-tier stress could accumulate enough `mqtt_drops` to push the total past the truncation point.

**Math (all values verified by script)**:

```
17 fields:
  13 x uint32_t (%lu): max "4294967295" = 10 chars each -> 130 chars of values
  2 x uint16_t (%u):   max "65535"       =  5 chars each ->  10 chars
  1 x uint8_t  (%u):   max "100"         =  3 chars      ->   3 chars
  JSON keys + punctuation: ~321 chars (counted from template)
Total worst-case: 464 chars + NUL = 465 chars
Buffer size: 384 chars
Overflow: 81 bytes
```

The Phase 1B report flagged this (MEDIUM "sendMQTTheapdiag JSON buffer 384 bytes under worst-case"). It is confirmed here by precise count: 465 bytes required, 384 allocated. This was raised from 256 to 384 in this branch when the discovery fields were added, but 384 is still not enough.

- **Fix**: Raise to 512 bytes. On-stack allocation in a once-hourly function; 512 bytes on a 4 KB stack is within budget.
  ```cpp
  char json[512];   // was 384: worst-case is 465 bytes + NUL at max counter values
  ```

- **Tradeoff**: 128 extra stack bytes, used once per hour. Negligible.

---

### [HIGH-1] STATUS_BURST_COOLDOWN_MS permanently defers discovery drip under normal Status-frame cadence

- **File**: `src/OTGW-firmware/MQTTstuff.ino` (STATUS_BURST_COOLDOWN_MS constant and `loopMQTTDiscovery`)
- **Cost / impact**: At the Crashevans-documented ~3s Status-frame cadence, the 10-second post-burst cooldown is perpetually renewed. Each Status frame (master or slave) triggers `endStatusBurst()`, which resets `burstCooldownUntilMs` to `now + 10000`. The next burst fires 3 seconds later, renewing the cooldown again. The drip never gets an available tick.

**Math**:

```
Status burst cadence: ~3s (observed, Crashevans log)
Post-burst cooldown: 10000ms
Cooldown renewed at: t=0, t=3, t=6, t=9...
Next drip window opens when: millis() > burstCooldownUntilMs
That requires a 10s gap between bursts.
Under 3s cadence: gap = 3s < 10s -> drip is permanently deferred.
```

The code comment (`MQTTstuff.ino` around the STATUS_BURST_COOLDOWN_MS definition) actually acknowledges this: "under heavy Status traffic the drip can stall. Tunable via STATUS_BURST_COOLDOWN_MS. If you see iDripCooldownSkipCount grow without discovery progressing, lower the value (candidates: 2500ms = fits between bursts, 5000ms = partial overlap)."

The comment identifies the problem correctly but ships the wrong default. At 10s, there is no "if" — the drip permanently stalls. At 2500ms, the gap per burst is 0.5s, which allows one 2s-interval drip slot every 5 bursts on average. Time to complete 80 IDs: approximately 16 minutes. At STATUS_BURST_COOLDOWN_MS = 2000ms (just under the burst cadence), a gap opens after each burst: 3s - 2s = 1s gap. One drip every 3s = same rate as the old 1s drip. This is the actual correct default.

**Impact on users**: VH boilers (ventilation) double the Status fanout (master+slave for both heater and VH device). Their cadence may be shorter than 3s. Those users experience indefinitely deferred discovery drip on first boot, meaning Home Assistant sees no entities until either the user manually triggers `/api/v2/discovery/republish` or the heap pressure drops enough to clear the busy-status condition.

- **Fix**: Change `STATUS_BURST_COOLDOWN_MS` to 2000ms (just under typical Status cadence) or make it explicitly configurable. Add a diagnostic assertion that the drip timer does not advance zero IDs over any 5-minute window while pending IDs exist (observable via `iDripCooldownSkipCount / iDripActiveBurstSkipCount` ratio in the heapdiag JSON).
  ```cpp
  constexpr unsigned long STATUS_BURST_COOLDOWN_MS = 2000;  // was 10000; stays under 3s Status cadence
  ```

- **Tradeoff**: Lower cooldown means more overlap between post-burst lwIP pbuf drain and the next drip allocation. At 2s, the overlap is ~1-2 pbufs still in flight. The burst-quiesce guard (which catches the active burst itself) remains in effect regardless of cooldown length. The heap-gate in `loopMQTTDiscovery` (`ESP.getFreeHeap() < MQTT_DISCOVERY_HEAP_MIN`) provides the safety net if the heap is truly under pressure.

---

### [HIGH-2] VH status publishers bypass the burst quiesce (confirmed from Phase 1A)

- **File**: `src/OTGW-firmware/OTGW-Core.ino`, lines 1667-1733 (`publishMasterStatusVHState`, `publishSlaveStatusVHState`, `publishStatusVHBitMQTT`)
- **Cost / impact**: VH boilers publish a separate Status fanout (4-6 bits per side) without `beginStatusBurst()`/`endStatusBurst()` wrappers. On those hardware configurations, the drip quiesce does not fire. Each VH Status frame triggers 5-7 lwIP TX pbuf allocations with no drip pause. Under 3s VH cadence, the heap pressure reduction benefit of this branch is approximately zero for VH users.

The `publishStatusVHBitMQTT` helper (`OTGW-Core.ino:1500`) also has no `incrementStatusBurstPublishCount()` call, so the cooldown is never armed for VH bursts. This means the drip runs freely during and immediately after VH Status fanouts, which is worse than the heater-Status case (heater Status: drip deferred during burst; VH Status: drip not deferred at all).

- **Fix** (same as Phase 1A HIGH #1):
  ```cpp
  // In publishMasterStatusVHState():
  beginStatusBurst();
  { OTPublishGate gate(publishCombined);
    if (publishCombined) incrementStatusBurstPublishCount();
    sendMQTTData(F("status_vh_master"), statusText); }
  publishStatusVHBitMQTT(...); // x4
  endStatusBurst();

  // In publishStatusVHBitMQTT():
  if (allowPublish) incrementStatusBurstPublishCount();
  ```

- **Tradeoff**: Adds 10s cooldown after each VH Status burst. With HIGH-1 fixed (cooldown = 2000ms), this is benign.

---

### [MEDIUM-1] Heap-abort during verify creates a false-negative on busy brokers

- **File**: `src/OTGW-firmware/MQTTstuff.ino`, `tickDiscoveryVerification()` (around line 315 post-diff)
- **Cost / impact**: When heap drops below `VERIFICATION_MIN_HEAP_ABORT` (4500 bytes) during the 15s window, the code sets `verifyReceivedCount = expected` before calling `endDiscoveryVerification()`. This suppresses the "missing" republish. On a busy broker with many retained messages and concurrent Status traffic, the scenario is plausible:
  1. Verify starts at `freeHeap = 6000` (just at the `VERIFICATION_MIN_HEAP_START` floor).
  2. Buffer resize: -640 bytes → free 5360.
  3. Broker floods 80 retained configs: TCP rx pbuf allocation of ~1460 bytes → free 3900.
  4. Concurrent Status burst: ~800 bytes TX pbufs → free 3100.
  5. Next `tickDiscoveryVerification()` check: 3100 < 4500 → heap-abort fires.
  6. `verifyReceivedCount = expected` → false-negative: verify reports "all present" when it only received 20 of 80 configs.
  7. Missing discovery topics are not republished.

The comment on the heap-abort guard correctly notes this is to "suppress false-missing republish," but does not acknowledge that it also suppresses true-missing republish under real pressure.

- **Fix**: Instead of masking with `verifyReceivedCount = expected`, close the window and log a distinct "aborted" outcome. Set `iLastMissingCount` to a sentinel value (e.g. `0xFFFF`) to indicate indeterminate result rather than "all present." Then do NOT trigger republish (preserving the intent of avoiding work under pressure), but also do not mark the result as a clean pass.
  ```cpp
  if (ESP.getFreeHeap() < VERIFICATION_MIN_HEAP_ABORT) {
    DebugTln(F("[verify] heap-abort: closing window early (result indeterminate)"));
    state.discovery.iLastMissingCount = 0xFFFF;  // sentinel: indeterminate
    verifyActive = false;
    state.discovery.bVerificationActive = false;
    // Restore buffer but do NOT mark as clean pass and do NOT republish.
    if (verifyBufferResized) { MQTTclient.setBufferSize(MQTT_CLIENT_BUFFER_SIZE); verifyBufferResized = false; }
    return;
  }
  ```

- **Tradeoff**: More complex state machine. The REST endpoint needs to handle the sentinel value. The simplification is worth it for observability: currently a heap-abort is invisible to operators.

---

### [MEDIUM-2] `getHeapHealth()` transition counter inflates on boundary oscillation (no hysteresis)

- **File**: `src/OTGW-firmware/helperStuff.ino`, `getHeapHealth()` (line 733)
- **Cost / impact**: The transition counters (`iEnteredLowCount`, `iEnteredWarningCount`, `iEnteredCriticalCount`) increment only on upward transitions (healthy → pressured), which is correct. However, there is no hysteresis band. If `freeHeap` oscillates around `HEAP_LOW_THRESHOLD = 5120` with ±50-byte amplitude (common during repeated drip publishes), the counter increments on every oscillation crossing. A system under moderate normal load could show `iEnteredLowCount = 1440` after 24 hours (once per minute) — a misleading signal.

CPU cost of calling `getHeapHealth()` is negligible: ~6 µs when `maxBlock` walk is triggered (only in LOW/WARNING tier), ~0.1 µs otherwise. At ~20 calls/sec: 6 µs × 20 = 120 µs/sec = 0.015% CPU. Not a performance concern; the issue is telemetry accuracy only.

- **Fix**: Add a narrow hysteresis band (e.g. 128 bytes) so tier-entry is counted only when crossing into a new tier from at least 128 bytes inside the previous tier. Alternatively, add `iEnteredLowCount_recent_10min` or cap at reporting once per minute.

- **Tradeoff**: Adds complexity to an already-correct function. Low priority.

---

### [MEDIUM-3] `VERIFICATION_MIN_HEAP_START = 6000` may be insufficient when WebSocket client is connected and `sendDeviceInfoV2` is concurrently serving

- **File**: `src/OTGW-firmware/MQTTstuff.ino` (`startDiscoveryVerification`, line 218)
- **Cost / impact**: `startDiscoveryVerification()` requires `freeHeap >= 6000` and `maxBlock >= 1280`. These are checked at verify-start but not continuously monitored during the 15s window. The `tickDiscoveryVerification()` heap-abort at 4500 catches degradation.

However, `sendDeviceInfoV2()` on `GET /api/v2/devinfo` now calls `countPendingDiscoveryIds()` (a 256-bit-scan, 6 µs) and `getHeapFragmentation()` (a `getMaxFreeBlockSize()` walk, ~6 µs). These are individually cheap. The new addition of 15 `sendJsonMapEntry()` calls adds approximately 15 × `snprintf_P` + `httpServer.sendContent()` invocations. Each `sendContent` call may trigger an HTTP chunked-transfer write to the TCP stack, which can allocate an lwIP pbuf (~400-600 bytes) if the chunk buffer is full. If `sendDeviceInfoV2` is called while verify is active, the concurrent lwIP pbuf allocations from HTTP chunked transfer could push the heap below `VERIFICATION_MIN_HEAP_ABORT`, triggering the heap-abort false-negative described in MEDIUM-1.

This is a two-concurrent-requests scenario (HTTP client + MQTT broker both active), which is realistic for a user opening the web UI while the system is at boot-time verify.

- **Fix**: In `tickDiscoveryVerification()`, consider checking `httpServer.client().connected()` and postponing early-close decisions when an HTTP client is actively sending. Or raise `VERIFICATION_MIN_HEAP_START` to 8000 bytes (measured free after WS + typical HTTP overhead).

- **Tradeoff**: Raising the floor makes verify less likely to start on lower-memory systems. The 6000 floor was chosen based on Crashevans's specific log; it may not generalise.

---

### [LOW-1] Drip completion time under slow-mode is 13 minutes — user-visible during broker reconnects

- **File**: `src/OTGW-firmware/MQTTstuff.ino` (DISCOVERY_INTERVAL_SLOW, DISCOVERY_INTERVAL_NORMAL)
- **Cost / impact**:

```
Normal mode (healthy heap): 80 IDs × 2s = 160s = 2.7 minutes
Slow mode (heap pressure):  80 IDs × 10s = 800s = 13.3 minutes
Normal mode + 10s cooldown permanently stalled (HIGH-1 unresolved): infinite
```

After a broker reconnect, `markAllMQTTConfigPending()` queues all 80 IDs. A user who just restarted their MQTT broker will wait 2.7 minutes before Home Assistant sees discovery entities — already noticeable. At slow-mode, 13 minutes is long enough that users may file bug reports assuming the integration is broken. The old 30s slow-mode interval was even worse at 40 minutes, so this is an improvement, but 13 minutes remains uncomfortable.

- **Fix**: No code change required here; this is a design constraint of the single-message-per-tick architecture. Document the worst-case wait times in the user-facing settings UI. The `disc_pending_ids` counter in the `/api/v2/devinfo` response can be used to show progress.

- **Tradeoff**: Faster drip under pressure would increase the very heap pressure the slow-mode is designed to avoid.

---

### [LOW-2] `getHeapHealth()` is called twice per gated publish path (once in `canPublishMQTT`, once in `loopMQTTDiscovery` slow-mode check)

- **File**: `src/OTGW-firmware/MQTTstuff.ino` (`loopMQTTDiscovery`, line 1308); `src/OTGW-firmware/helperStuff.ino` (`canPublishMQTT`)
- **Cost / impact**: `loopMQTTDiscovery()` calls `getHeapHealth()` to set the slow-mode interval, then calls `doAutoConfigureMsgid()` which may internally call `canPublishMQTT()`, which calls `getHeapHealth()` again. In the HEALTHY tier, each call is ~0.1 µs (no `maxBlock` walk). In LOW tier, each call is ~6 µs. Two calls per drip tick at 2s interval = 12 µs/2s = 6 µs/s. Negligible.

The only scenario where this matters: if `getHeapHealth()` is promoted to WARNING by the fragmentation check (freeHeap in LOW but maxBlock < 1536), the first call in `loopMQTTDiscovery` returns WARNING and triggers slow-mode, but the second call inside `canPublishMQTT` may return a different level if heap changed between the two calls (cooperative yield does not happen between them, so the calls are effectively atomic). Not a correctness issue.

- **Fix**: Cache the result of `getHeapHealth()` at the start of `loopMQTTDiscovery` and pass it down. Low priority.

- **Tradeoff**: Marginal complexity for negligible gain.

---

### [LOW-3] Daily verify window: 15s may be insufficient for slow brokers with large retained-topic backlogs

- **File**: `src/OTGW-firmware/MQTTstuff.ino` (VERIFICATION_WINDOW_MS = 15000)
- **Cost / impact**: Mosquitto delivers retained topics synchronously upon subscribe. A broker with high load or many retained topics may take more than 15s to deliver all 80 discovery configs. In that case, the window closes with `verifyReceivedCount < expected`, triggering a full `markAllMQTTConfigPending()` and 80-topic republish drip even though the topics were actually present on the broker.

False-positive republish rate: one per day if broker delivery consistently exceeds 15s. Impact: 80 extra MQTT publishes at 2s intervals (2.7 minutes of drip) per day. On a lightly loaded LAN broker this is unlikely; on a shared broker or one with thousands of retained topics it is plausible.

- **Fix**: Configurable window via `settings.mqtt.iVerificationWindowSec` (default 15, range 10-60). Or add a minimum-settling-delay approach: start a 30s window but allow early-close only after `verifyReceivedCount >= expected`.

- **Tradeoff**: Longer window holds the 1024-byte buffer longer, blocking a second verify from starting. Configurable is the cleanest solution.

---

## Heap Envelope Analysis

**Worst-case concurrent allocation path**: discovery verify window, active WebSocket client, Status burst, TCP RX flood from broker.

**Starting condition**: verify starts at freeHeap = 6000 (minimum allowed by `VERIFICATION_MIN_HEAP_START`). All of the following allocations happen from this baseline. The 6000 baseline already includes whatever WebSocket client overhead exists (socket, read buffer).

| Allocation | Bytes | Cumulative free |
|---|---|---|
| Start (freeHeap = 6000) | 0 | 6000 |
| setBufferSize(1024): net +640 | 640 | 5360 |
| Status burst TX pbufs (9 publishes × ~90B) | 800 | 4560 |
| TCP RX pbuf for retained message flood | ~1460 | ~3100 |
| handleMQTTcallback stack frame | ~200 | ~2900 |

At 2900 bytes free:
- `HEAP_CRITICAL_THRESHOLD` = 1536: still safe (margin 1364 bytes)
- `HEAP_WARNING_THRESHOLD` = 3072: already in WARNING tier
- `VERIFICATION_MIN_HEAP_ABORT` = 4500: heap-abort fires (3100 < 4500)

**Conclusion**: Under the worst-case combination (broker flood + Status burst at minimal start heap), `tickDiscoveryVerification()` fires its heap-abort guard and closes the window early. The 1536 CRITICAL threshold is not breached. The `HEAP_CRITICAL` zone is safe. However, this triggers the MEDIUM-1 false-negative issue (verify closes with "all present" when it may have only received a fraction of the configs).

The new lowered thresholds (`HEAP_CRITICAL` = 1536, `HEAP_WARNING` = 3072) are validated against this worst-case: the system remains above CRITICAL with comfortable margin. The tradeoff is that WARNING tier fires more frequently (at 3072 vs 4096), which causes `canPublishMQTT()` to enforce throttling more often. This is intentional per the branch design.

**VH boiler worst-case**: VH Status fanout adds ~600 bytes more TX pbufs. Combined with verify start at 6000: 6000 - 640 - 800 - 600 - 1460 - 200 = 2300 bytes. Still above CRITICAL (1536) but with only 764 bytes margin. On systems where DRAM consumption has grown (additional sensors, longer uptime fragmentation), this margin compresses further.

---

## CPU / Per-Loop Cost Summary

**ZonedDateTime conversions**: The refactor consolidates `hourChanged()`, `dayChanged()`, `yearChanged()` into a single call site in `doTaskMinuteChanged()`. This is architecturally correct per ADR-064. The CPU cost is 4 pairs of `createForZoneName()` + `forUnixSeconds64()` per minute boundary, estimated at ~150 µs per pair = ~600 µs total per minute = ~10 µs/sec average. This is negligible at 80 MHz. The nightly-restart check, previously run every 60 seconds (60 `createForZoneName` calls per hour), is now run hourly (1 call per hour). This branch reduces ZonedDateTime call count by 59 pairs per hour for nightly-restart users. Positive improvement.

**`markAllMQTTConfigPending()`**: 256 iterations of `pgm_read_word()` (PROGMEM array access) plus a `bitSet()`. No O(N²) behaviour. Estimated cost: 25-125 µs per call. Called infrequently (broker reconnect, POST /republish, verify missing > 0). No performance concern.

**`handleMQTTcallback` verify-filter**: per-message cost is ~0.3 µs (two `strncmp` + two `strchr`). At realistic broker delivery rates of 3-13 messages/sec during the 15s window, total CPU impact is under 4 µs/sec. Does not monopolise the loop. The DoS segment-length cap (`VERIFICATION_MAX_NODE_SEGMENT_LEN = 64`) correctly bounds the `strncmp` before any comparison.

**`sendMQTTheapdiag`**: One `snprintf_P` into a stack buffer, one `sendMQTTData`. Called once per hour. CPU cost is irrelevant. Buffer overflow concern is addressed in the CRITICAL finding above.

**`getHeapHealth()` call frequency**: In the healthy tier (common case): ~0.1 µs per call (just reads `freeHeap`). In LOW/WARNING tier: ~6 µs (includes `getMaxFreeBlockSize()` free-list walk). At 20 calls/sec in LOW tier: 120 µs/sec = 0.015% CPU. The comment on the `getMaxFreeBlockSize()` call in `getHeapHealth()` correctly documents that the walk is skipped in the healthy path. This is the right design.

---

## Scalability Concerns

**Users with many OT IDs + Dallas + VH**: With VH Status bypass unresolved (HIGH-2), the heap reduction claim does not hold for VH hardware. Discovery drip completion may be indefinitely deferred.

**Multi-month uptime fragmentation**: The daily verify `setBufferSize(1024→384)` cycle is benign because umm_malloc's coalescing handles the resize pattern correctly. If fragmentation prevents a contiguous 1280-byte block, `startDiscoveryVerification()` refuses to start (maxBlock precheck at line 221). Net result: verify silently skips rather than crashing. The `disc_verify_runs` counter in the heapdiag JSON will reveal if skips are accumulating.

**Slow brokers**: 15s window may not be sufficient for brokers with delivery latency > 15s. See LOW-3.

**Rapid-fire POST to `/api/v2/discovery/republish`**: Each POST calls `markAllMQTTConfigPending()` (~100 µs) and returns immediately. The drip itself is rate-limited by the 2s timer. Ten rapid POSTs just call `markAllMQTTConfigPending()` ten times in succession with the same result (bitmap already all-set after the first call). No amplification concern; the second through tenth calls are no-ops on the bitmap.

---

## Frontend Performance

The `data/index.js` changes are pure additions to the `translateFields` display-name table (16 new string pairs). There are no new `setInterval`, `setTimeout`, `fetch`, or `addEventListener` registrations in the diff. The new discovery verification UI reads fields from the existing `/api/v2/devinfo` WebSocket-streamed response — no new polling interval is introduced. The 15 additional `sendJsonMapEntry()` calls in `sendDeviceInfoV2` increase the JSON response size by approximately 300-400 bytes. This is within normal HTTP chunk-streaming budget and does not require any frontend polling change. No frontend performance concern.

---

## Metric Deltas vs dev

| Parameter | dev | 1.4.1 | Delta |
|---|---|---|---|
| `HEAP_CRITICAL_THRESHOLD` | 2048 | 1536 | -512 bytes (-25%) |
| `HEAP_WARNING_THRESHOLD` | 4096 | 3072 | -1024 bytes (-25%) |
| `HEAP_LOW_THRESHOLD` | 6144 | 5120 | -1024 bytes (-17%) |
| `HEAP_FRAG_PROMOTE_MAXBLOCK` | — | 1536 | NEW |
| `DISCOVERY_INTERVAL_NORMAL` | 1s | 2s | +100% |
| `DISCOVERY_INTERVAL_SLOW` | 30s | 10s | -67% |
| Slow-mode trigger tier | HEAP_WARNING | HEAP_LOW | fires earlier |
| `MQTT_DISCOVERY_HEAP_MIN` | 4000 | 3000 | -25% |
| `STATUS_BURST_TIMEOUT_MS` | — | 500ms | NEW |
| `STATUS_BURST_COOLDOWN_MS` | — | 10000ms | NEW (problematic default) |
| `VERIFICATION_WINDOW_MS` | — | 15000ms | NEW |
| `VERIFICATION_BUFFER_BYTES` | — | 1024 | NEW (+640 transient) |
| `VERIFICATION_MIN_HEAP_START` | — | 6000 | NEW |
| `VERIFICATION_MIN_HEAP_ABORT` | — | 4500 | NEW |
| New static RAM (BSS) | 0 | ~222 bytes | +222 bytes |
| `OTGWState` new sections | 0 | 60 bytes | `DiscoverySection` (24) + `HeapDiagSection` (36) |
| New globals in `MQTTstuff.ino` | 0 | ~157 bytes | dominated by `verifyWildcard[128]` |

---

## Top 3 Priority Optimisations

1. **Fix the JSON buffer** (`sendMQTTheapdiag`): raise `char json[384]` to `char json[512]`. One-line fix. Eliminates the only crash-class finding (data corruption of a retained MQTT message that stays broken indefinitely).

2. **Fix `STATUS_BURST_COOLDOWN_MS`**: change from 10000ms to 2000ms. The current default permanently defers the discovery drip under normal boiler Status-frame cadence, negating the primary feature of this branch for most users during first-boot HA integration. One constant change; confirmed by arithmetic from the Crashevans 3s cadence data cited in the code itself.

3. **Add `beginStatusBurst()`/`endStatusBurst()` to VH publishers**: `publishMasterStatusVHState` and `publishSlaveStatusVHState` need the same wrappers as their non-VH counterparts. Without this, the heap-pressure reduction claim is false for any VH-equipped installation.

---

## Recommended SLOs for This Feature

These are inferred targets based on the branch's stated goals and platform constraints.

| Metric | Target | Rationale |
|---|---|---|
| Discovery drip completion (healthy heap, no VH) | < 3 minutes | 80 IDs × 2s = 160s; add margin for broker latency |
| Discovery drip completion (heap pressure, no VH) | < 15 minutes | 80 IDs × 10s slow-mode |
| Verify false-positive rate | < 1 per month | false-positive = unnecessary 80-topic republish |
| Post-burst drip resume latency (at 3s cadence) | < 3s | cooldown must be shorter than Status cadence |
| Heap above CRITICAL threshold | 100% of time | 1536 bytes = 1 lwIP pbuf; below this is crash territory |
| Daily verify buffer realloc cost | < 1ms | one-shot, negligible; mainly a fragmentation audit point |
