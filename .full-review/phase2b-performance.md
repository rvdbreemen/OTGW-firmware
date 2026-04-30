# Phase 2B: Performance & Scalability Review

Diff window: `ace21a48^..fa5ef3c5`
Files reviewed: `SATble.ino`, `MQTTstuff.ino` (BLE-discovery region), `OTDirect.ino`,
`OTDirecttypes.h`, `OTGW-firmware.h`. Out-of-scope: `evaluate.py`, `tests/`, `platformio.ini`,
`docs/adr/ADR-092*`.

## Summary

The session has a small performance footprint and is, on balance, an improvement
over the pre-session state. The dominant change is the Bluedroid -> NimBLE-Arduino 2.x
swap (TASK-487), which trades a chunk of one-time flash and heap (the NimBLE host
stays initialised for the life of the boot) for a much cheaper hot-path scan
callback: no more per-advertisement `String` constructor + `BLEUUID::toString` walk +
`strstr` over a hex string, replaced by a typed `getServiceData(NimBLEUUID)` lookup.
That alone is the primary scalability win — the previous implementation could allocate
and free three Arduino `String` objects on every BLE advertisement, which in a
dense-advertiser environment was the biggest heap-fragmentation risk in the SAT path.

The new MQTT discovery + state-topic helpers (TASK-488) and the OTDirect TT/TC state
machine (TASK-466) add bounded, low-cost work to existing hot paths. The most
realistic concern is the 16-publish burst in `satBLEPublishMQTT()` on the first scan
after MQTT connect, but this is gated by `canPublishMQTT()` + `MQTT_DISCOVERY_HEAP_MIN`
per call and amortised by the `bDiscoveryPublished` flag, so it self-limits to one
burst per MAC per boot. There are no Critical findings. Two cross-task concerns are
flagged but both are sub-pathological at the data sizes involved.

## Critical findings

None.

## High findings

### 2B-H1 — `feedWatchDog()` only fires on the success path of streaming primitives

**Severity**: High (latent, not yet observed)
**Estimated impact**: up to ~6 s wall-clock with no watchdog kick if 16 BLE publishes all fail at the broker round-trip
**File**: `MQTTstuff.ino:2068-2078` (the four sequential `bleSensorPublishOneDiscovery` calls inside `bleSensorPublishHaDiscovery`); base helper at `MQTTstuff.ino:2018-2049`

The discovery helper short-circuits after a `writeMqttChunk` or `endPublish` failure
without calling `feedWatchDog()` on the failure path. The success path does feed (line
2050). For the four-publish loop in `bleSensorPublishHaDiscovery`, a string of failures
on a degraded broker connection traverses four nested calls, each doing a TCP write
attempt with the PubSubClient default 15 s socket timeout, and zero watchdog kicks.
ESP32 hardware watchdog is generous (looper task is registered, ~10 s default) so this
is unlikely to bite in practice, but the pattern is wrong: every retire path through a
bounded TCP I/O block should kick the dog. Fix: add `feedWatchDog()` before every early
`return false` after a network operation has been attempted.

The same pattern is also visible in 1A-M4 (Phase 1A) — `endPublish()` not checked on
the `writeMqttChunk` failure path. Fixing both at once is a one-liner:

```cpp
if (!writeMqttChunk(payload, (size_t)n)) {
  MQTTclient.endPublish();
  feedWatchDog();
  return false;
}
```

## Medium findings

### 2B-M1 — 16-publish burst on first scan after MQTT-connect window

**Severity**: Medium
**Estimated impact**: ~50-200 ms wall-clock burst on a healthy LAN; up to 4-8 s on a struggling broker (PubSubClient socket timeout per publish)
**File**: `SATble.ino:434-448` + `MQTTstuff.ino:2052-2098`

`satBLEPublishMQTT()` iterates valid slots and, for each not-yet-discovered MAC,
fires four sequential retained discovery publishes (`temp`, `rh`, `bat`, `rssi`) followed
by four non-retained state publishes. With the configured maximum 4 sensors, the worst
case on the first cycle after MQTT-connect is **16 retained discovery publishes + 16
state publishes = 32 sequential PubSubClient publishes** in one `doBackgroundTasks()`
slice. Each publish is a synchronous TCP write + ack, no batching, no `yield()` between
publishes (the streaming primitives `feedWatchDog()` between, but there is no inter-
publish pacing).

In practice this is bounded:
- The per-MAC `bDiscoveryPublished` flag (BLESensorData) makes the discovery half a
  one-shot per MAC per boot.
- `canPublishMQTT()` + `MQTT_DISCOVERY_HEAP_MIN` (3000 B) are checked on every
  individual `bleSensorPublishOneDiscovery` call, so heap pressure transparently defers.
- BLE scan cadence is `iBleInterval` seconds (default 30s); the state-topic half is the
  steady-state cost: 16 publishes per 30s = ~0.5 publish/s, well within budget.

The burst window can land on top of an already-busy MQTT cycle (HA discovery for OT
message IDs is also drip-fed in `loopMQTTDiscovery`). If both fire in the same tick,
the cumulative wall-clock blocks the loop for a noticeable interval. Fix is optional:
either drip-feed BLE discovery one-publish-per-tick (matching ADR-077 spirit), or cap
at one MAC's-worth of discovery (4 publishes) per `satBLEPublishMQTT()` call. Today's
shape works, but it sits *just* outside the streaming-discovery convention.

Note: this is the performance-impact framing of architecture finding 1B-M1. The latent
trigger is also the failure mode behind 1A-H1 (sticky `bDiscoveryPublished` flag) — a
broker hiccup during the burst suppresses retry forever.

### 2B-M2 — Cross-task BLE access without lock; cache-line bouncing on dual-core ESP32-S3

**Severity**: Medium (correctness ceiling; performance impact is genuinely small)
**Estimated impact**: Worst case: one stale "best slot" pick per cycle. No measured cycle-time penalty.
**File**: `SATble.ino:67` (storage), `SATble.ino:243-264` (writer in scan callback), `SATble.ino:332-374` (reader in `satBLEUpdateState`), `SATble.ino:434-448` (reader in `satBLEPublishMQTT`)

NimBLE 2.x runs the scan callback on its own FreeRTOS task (the BLE host task), so
`_bleSensors[]` is written from the BLE task and read from the loop task in
`satBLEUpdateState()` / `satBLEPublishMQTT()` without any synchronisation primitive.
ESP32-S3 has cache coherency enforced by the FreeRTOS scheduler at task-switch
boundaries, but within a task the cache line for `_bleSensors[]` can be cached in one
core and modified by the other. Real-world consequences at this data scale:

- Torn read of the 32-bit `iLastSeenMs`: not possible on ESP32 (aligned 32-bit
  loads/stores are atomic per Xtensa LX7 ISA).
- Torn read of `bDiscoveryPublished` / `bValid`: a bool is a single byte; same atomicity
  guarantee.
- Stale "best slot" pick: the reader sees the slot data from one tick ago. The slot is
  picked again 30s later. Net impact: a single ~30s window where state.sat.fBleTemp is
  one cycle behind. Negligible for a thermal control loop.
- Cache-line bouncing: `_bleSensors[]` is 4 slots × ~50 bytes = ~200 B, spans 4 cache
  lines (ESP32-S3 has 32-byte L1 lines). Cross-core writes invalidate; cost per
  invalidation is O(10 ns). At 50-200 advertisements/sec, that's < 4 us/sec aggregate.
  Not measurable.

Fix is not warranted on performance grounds. If you ever add a 5th data field that
crosses a logical boundary (e.g. `fTemperature` and `iLastSeenMs` need to be read
atomically together), introduce a `portMUX_TYPE` spinlock or use std::atomic for the
specific fields. For now, the pragma is "leave it alone, document the assumption".

This is the perf framing of architecture finding 1B-M2.

### 2B-M3 — `OT_CMD_QUEUE_SIZE = 12` and `clearRemoteOverride` enqueues 1 + 2 already-present frames

**Severity**: Medium (low frequency, but the new code path can drop frames silently)
**Estimated impact**: 1 dropped MsgID 100 frame on TT/TC churn under unusual broker load
**File**: `OTDirect.ino:260` (queue size); `OTDirect.ino:1916-1925` (`clearRemoteOverride` enqueue); `OTDirect.ino:1840-1842` (`setRemoteOverride` indirect via `enqueueWriteCommand`)

`setRemoteOverride()` calls `enqueueWriteCommand` twice (MsgID 16 + MsgID 100), which
internally goes via `otCmdEnqueue` (queue size 12). `clearRemoteOverride()` calls
`clearWriteOverride` twice (frees override slots, no queue impact) and then
`otCmdEnqueue` once for a one-shot MsgID 100=0 frame. That one-shot is the only path
where the new code can hit a full queue.

Looking at the queue producers: there are 12 enqueue sites (verified via grep), most
of them in handleOTDirectCommand command paths. The OT bus consumes the queue at
~1 frame per ~100 ms (`iMsgInterval` default), so a backlog of 12 frames drains in
~1.2 s. A flood of TT=/TC=/TT=0 commands from a script could theoretically stack up
during boiler poll cycles, but each command sequence is bounded:

- `setRemoteOverride`: 2 frames
- `clearRemoteOverride`: 1 frame

So 4 rapid TC=...; TT=...; TT=0 sequences = 8 frames, which fits. The queue is sized
correctly *for the historical command set*. The new code adds modest pressure but
stays within budget. Not actionable today, but if more state-machine commands land in
the future, bump `OT_CMD_QUEUE_SIZE` to 16 and document the rationale.

The drop-on-full path in `clearRemoteOverride` does log via `OTDDebugTln(F("OTD: OVR-clear dropped (queue full)"))`,
which is correct contention handling — not silent.

This is the perf cousin of architecture finding 1A-M1 (state-claim-without-frame-in-flight).
1A-M1 is the bigger concern (it leaves the state machine wedged); the perf angle here
is just "queue depth is borderline".

## Low findings

### 2B-L1 — `bleFindOrAllocSlot` is O(N) but doesn't early-exit on found slot

**Severity**: Low
**Estimated impact**: 1-3 extra strcmps per BLE advertisement, ~5 us each on Xtensa
**File**: `SATble.ino:176-188`

The loop returns early on a MAC match (line 181), so `O(N)` is `O(slot index of match)`.
That's already optimal. The empty-slot tracking does add one extra branch per iteration,
but at N=4 this is noise. No fix needed.

### 2B-L2 — `bleMacToCompact` uses `tolower`/`isxdigit` from `ctype.h`

**Severity**: Low
**Estimated impact**: Possible ~50 ns/call slowdown vs. inline ASCII arithmetic
**File**: `MQTTstuff.ino:1955-1968`

`tolower` and `isxdigit` are locale-aware and may go through a function-pointer table
in the ESP32 newlib build. Called once per slot per `satBLEPublishMQTT()` cycle, so 4
× (17 chars × 2 calls) = ~136 calls per 30s = ~4.5 calls/s. Even at a hypothetical
100 ns/call, that's <1 us/sec. Below the noise floor; do not change.

### 2B-L3 — `parseBLEBTHomeFormat` returns on first unknown object ID

**Severity**: Low (correctness > performance)
**Estimated impact**: Earlier exit = faster, so this is a *positive* perf finding
**File**: `SATble.ino:153-156`

The parser bails on the first unknown object ID because BTHome v2 length-encodes data
implicitly (lookup table per object ID), so an unknown ID stops the walk safely.
Practical bound: an ATC/BTHome advert is ~15-30 bytes total, so the loop bounds at ~10
iterations max. `O(n)` over advertisement length, but the constant is tiny.

### 2B-L4 — `applyOverrides` runs `onThermostatMsgID16` unconditionally on every MsgID 16 frame

**Severity**: Low
**Estimated impact**: ~1 us per MsgID 16 frame on ESP32-S3 (240 MHz)
**File**: `OTDirect.ino:445-447` (call site), `OTDirect.ino:1948-1981` (callee)

The hook is called for every MsgID 16 frame regardless of override mode. Inside, the
`OT_OVERRIDE_NONE` early-exit short-circuits the heavy delta math (line 1963-1966).
For active TT/TC, the full delta computation is ~6 integer ops + 3 compares = ~30
cycles ≈ 0.13 us at 240 MHz. OT bus rate is ~1-2 frames/sec, so total CPU cost is
< 1 us/sec. Fully within budget.

### 2B-L5 — NimBLE init is one-time but never released

**Severity**: Low
**Estimated impact**: ~30-50 KB heap permanently reserved once BLE is enabled
**File**: `SATble.ino:275-296`

`NimBLEDevice::init("")` allocates the BLE host task's stacks, controller buffers, and
the GAP/GATT registries. The code never calls `NimBLEDevice::deinit()`. If a user
toggles `settings.sat.bBleEnable` from true to false at runtime, BLE stays initialised
until reboot, and the heap reservation persists.

This is **not a regression**: the prior Bluedroid implementation had the same one-shot
init pattern with no deinit path. If the future adds a runtime BLE-disable flow that
expects heap recovery, add the deinit there. For now, document and move on.

### 2B-L6 — `bleSensorPublishOneDiscovery` uses a 768-byte stack buffer

**Severity**: Low (already analysed in Phase 1B-M1)
**Estimated impact**: 768 B stack on the ESP32 looper task; 8 KB task stack budget per FreeRTOS default
**File**: `MQTTstuff.ino:2030`

ESP32 task stacks default to 8 KB; the looper task budget after the OT-direct
scheduler, MQTT publish path, and HA-discovery state machine is ~5-6 KB free at peak.
768 B fits. ESP8266 BLE code is `#if defined(ESP32)` so the 4 KB CONT stack is not
involved. No action.

## Strengths

- **NimBLE port replaces String-heavy callback**: the prior Bluedroid callback could
  allocate and free three Arduino `String` objects per advertisement (svcData,
  uuidString, MAC string). On a dense BLE channel with 50-200 ad/s, that was the
  dominant heap-churn source in the SAT path. The NimBLE rewrite uses `std::string` by
  value (RAII, scope-bounded) and a typed `getServiceData(NimBLEUUID)` lookup with no
  intermediate string conversions. Net heap-fragmentation pressure is dramatically
  reduced. ADR-004 is honoured *better* than before.

- **Async scan model unblocks `loop()`**: the 3-arg `start(duration, is_continue, restart)`
  call returns immediately. Previously, `BLEScan::start(3, false)` blocked for 3 seconds
  per cycle. With `iBleInterval=30s` that was 10% of all `doBackgroundTasks()` time.
  Now it is zero. This is the single biggest perf win in the session.

- **`bDiscoveryPublished` is per-MAC and reset on slot recycle**: the slot-recycle
  reset (SATble.ino:253-255) prevents both spam (no re-publish for unchanged MAC) and
  silent skips (a recycled slot publishes for the new MAC). Behaviour is bounded and
  predictable.

- **`canPublishMQTT()` + `MQTT_DISCOVERY_HEAP_MIN` per-call gating**: every
  `bleSensorPublishOneDiscovery` call independently checks heap pressure. A
  partial-burst failure leaves the system in a recoverable state (sticky-flag bug
  notwithstanding — that is 1A-H1, not perf).

- **OTDirect state machine is `O(1)` on the hot path**: `applyOverrides` adds one
  branch + one function call per OT frame, with the heavy work gated behind
  `OT_OVERRIDE_NONE`. Bounded constant work, no allocations, no I/O.

- **`OTRemoteOverrideState` is 14 bytes file-static**: zero heap, zero per-frame
  allocation. The TT/TC machine's entire state cost is one cache line.

- **`bleMacToCompact` is bounded and allocation-free**: explicit length checks, hex
  validation, single-pass copy. `O(17)` constant. Reusable across both producers
  (SATble.ino caller, MQTTstuff.ino internal).

- **Sign-extension fix in `onThermostatMsgID16` (TASK-491)**: the explicit `int16_t`
  hop before widening to `int32_t` correctly handles negative TrSet values without
  changing computational complexity. Correctness improvement at zero perf cost.
