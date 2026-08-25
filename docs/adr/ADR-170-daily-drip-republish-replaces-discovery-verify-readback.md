---
id: "ADR-170"
title: "Unconditional Daily Drip Republish Replaces the Automatic Discovery-Verify Readback"
status: "Proposed"
date: "2026-07-26"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes:
  - "ADR-062"
superseded_by: null
---
# ADR-170 Unconditional Daily Drip Republish Replaces the Automatic Discovery-Verify Readback

## Status

Proposed. Date: 2026-07-26.

Port of `otgw-1.x.x` ADR-087 onto the 2.0.0 line. Acceptance is the
maintainer's determination; see "Evidence limitations" below, which are
materially different on this branch than on 1.x.

## Status History

status_history:
  - date: 2026-07-26
    status: Proposed
    changed_by: Agent
    reason: Ports the 1.x ADR-087 decision (TASK-1048) to the 2.0.0 line as part of TASK-1037. Maintainer directed porting the removal regardless of whether the 1.x root cause reproduces here.
    changed_via: manual

## Context

ADR-062 introduced an automatic retained-discovery verification: subscribe to
the node-scoped wildcard `<haprefix>/+/<nodeId>/#` for a ~15 s window, count the
retained configs received, compare against `iPublishedTopicCount`, and call
`markAllMQTTConfigPending()` (a full re-announce via the drip) when
`received < expected`.

On the 1.x line this mechanism killed a field device. Transcript
`transcript-20260721-225026-1.7.2-onset.1+bc067cc` (device `OTGW-48E72958B013`)
shows: boot 22:50, heap flat for ~70 min; `00:00:00.998 [verify] started:
expected=124`; `00:00:16.109 [verify] done: expected=124 received=26 orphans=0
missing=98 outcome=2`. The partial readback was misread as 98 missing, which
triggered a full republish. Because `iLastVerifyEpoch` never reached a success
value, 1.x's hourly first-run retry re-armed every hour. Verify → false-missing →
republish repeated and leaked heap until the device died at 00:12 (free 7080 /
block 4032). Boot→death: 82 minutes.

The underlying argument is platform-independent: **a count-comparison readback is
an unreliable primitive.** It cannot distinguish "the broker does not have this
config" from "the broker did not deliver it inside our window", and it treats the
latter — the normal outcome under a retained flood competing with live traffic —
as a trigger for expensive corrective work.

### Evidence limitations (read before accepting)

The 1.x failure is **not known to reproduce on this branch**, and several of its
preconditions are absent here:

- The MQTT engine is espMqttClient (ADR-131), not PubSubClient. The
  `setBufferSize`/restore dance and the max-block preflight that 1.x blamed were
  already deleted in TASK-865.8.
- Inbound is chunk-aware: `handleDiscoveryVerifyMessage()` is called from the
  chunked path (`MQTTstuff.ino`, `index == 0 && ... total`), so a large retained
  message is not silently truncated.
- The ESP32-S3 has roughly an order of magnitude more DRAM than the ESP8266.
- **There is no hourly first-run retry block on this line.** The daily trigger was
  the only automatic caller, so the specific runaway feedback loop that produced
  the 82-minute death cannot arm here in the same shape.

So this ADR removes a mechanism whose failure mode has been demonstrated on a
*sibling* platform, not on this one. The maintainer directed porting the removal
regardless, on the grounds that the readback's unreliability is inherent and that
behavioural coherence between the two lines has its own value. That is a
defensible call, but it is a **judgement**, not a measurement, and this ADR should
not be read as claiming the 2.0.0 readback was observed to misbehave.

## Decision

Remove the **automatic** discovery-verify readback. The daily trigger performs an
**unconditional drip republish** instead, guarded only by cheap local
preconditions (`OTGW-firmware.ino`, `doTaskMinuteChanged()` daily block):

```cpp
if (settings.mqtt.bDiscoveryAutoVerify
    && settings.mqtt.bEnable
    && state.mqtt.bConnected
    && isNTPtimeSet()
    && state.uptime.iSeconds > 3600
    && !isDiscoveryVerificationActive()
    && countPendingDiscoveryIds() == 0
    && discoveryDripHeapHealthy()) {
  markAllMQTTConfigPending();
  state.discovery.iLastDailyHealEpoch = (uint32_t)time(nullptr);
}
```

`markAllMQTTConfigPending()` only sets pending bits; `loopMQTTDiscovery()` drains
one ID per heap-gated tick (2 s healthy / 10 s under pressure). The heal is
therefore bounded, self-throttling and outbound-only: no wildcard subscribe, no
count, no false-missing, no retry storm.

Three sub-decisions worth stating explicitly:

**`isNTPtimeSet()` is load-bearing, not defensive.** `dayChanged()`
(`helperStuff.ino`) compares `time(nullptr)` against a `static int8_t lastday =
-1` and is not itself NTP-guarded, so it fires a second time when the clock jumps
from 1970 to real time at first sync. Without this guard the "daily" heal would
fire once shortly after every boot.

**The heap precondition delegates rather than declaring a threshold.** 1.x used
`ESP.getMaxFreeBlockSize() >= 8000`. That symbol does not exist on ESP32 (the
mandated shim is `platformMaxFreeBlock()`), and 8000 encodes an ESP8266
"20 % of a 40 KB heap is contiguous" bar that means nothing on a device with
~300 KB DRAM. Instead `discoveryDripHeapHealthy()` forwards to the drip's own
`discoveryDripIsHeapHealthyForRestore()`. The question the guard wants answered
is "would the drainer run this job at full speed right now?", and that question
already has an implementation. It also means ADR-167, if accepted, cannot leave a
stale literal behind in this path.

**Telemetry gains a field.** Under automatic operation `iLastVerifyEpoch` now only
moves on a *manual* verify, so the retained `disc_last_verify_epoch` topic would
freeze and read as "verification is broken" to anyone watching Home Assistant. A
new `state.discovery.iLastDailyHealEpoch` is published as
`otgw-firmware/stats/disc_last_daily_heal_epoch`.

The manual, user-initiated verify path (`POST /api/v2/discovery`, telnet) is
retained unchanged. It fires once per explicit request and cannot self-retrigger.

## Alternatives Considered

### A: Raise the buffer and widen the verify window
Rejected. Buffer growth consumes DRAM at the worst moment, the expected count
scales with entity growth, and a slow broker can still time out the window. It
makes the false-missing rarer, not impossible.

### B: Republish only genuinely-missing IDs from a *complete* readback
Rejected. Still depends on a reliable full readback, which is the unavailable
primitive; and per-topic identity tracking needs a reverse topic→msgid map that
ADR-062 itself rejected as fragile across source-separation modes.

### C: Keep ADR-062's automatic path on this line
This is the alternative with the strongest case here, precisely because the 1.x
failure preconditions are largely absent (see Evidence limitations). It was
rejected on the maintainer's direction in favour of cross-line coherence and of
not shipping a primitive known to be unsound on a sibling platform.

### D: Keep the readback but bound the blast radius
Cap republishes per day and never re-arm on a failed verify. Preserves verified
gap detection while removing the runaway. Rejected as a middle path that keeps the
unreliable primitive and diverges from 1.x without removing the underlying flaw.

## Consequences

**Benefits**
- No wildcard subscribe, no inbound retained-flood processing, no count, and no
  path by which a partial read causes corrective work.
- Simpler to reason about: the auto-heal is a bounded, heap-gated outbound drip.
- Nothing becomes dead code. The 2.0.0 verify state machine lives in its own
  translation unit (`mqtt_discovery_verify.cpp`) reached through the
  `verifyAccessorXxx()` bridge, and every symbol stays live for the manual path.

**Trade-offs**
- The daily heal republishes unconditionally rather than only on a detected gap.
  Cost is one drip pass per day; retain-overwrite of an identical config is a
  broker no-op.
- **Automatic detection of broker-side retained loss is gone.** Recovery is now
  time-based (next daily pass, ≤24 h) rather than verified. For a home HVAC
  gateway that latency is acceptable; for anyone who was relying on the verify
  statistics as a health signal, it is a real regression.
- `disc_verify_runs` / `disc_last_missing` / `outcome` stay at their last values
  under automatic operation.

**Risks and mitigations**
- *Risk*: the manual verify path still contains the readback and could leak if
  invoked repeatedly. *Mitigation*: user-initiated only, cannot self-retrigger.
- *Risk*: the daily drip collides with heavy traffic. *Mitigation*:
  `loopMQTTDiscovery()` is already heap-gated and paced, and the trigger also
  requires no drip in progress plus a healthy restore-level heap.

## Related Decisions

- **Supersedes ADR-062's automatic mechanism** and retains its manual endpoint.
  ADR-062 stays Accepted: its counters, REST/telnet endpoint and two CI gates all
  survive.
- **ADR-171** — the non-OT queueing set this heal now republishes.
- **ADR-088** — status-burst windowing; the drip gates on it.
- **ADR-167** (Proposed) — if accepted, changes what
  `discoveryDripIsHeapHealthyForRestore()` resolves to, automatically.
- **1.x ADR-087** — the sibling decision this ports.

## Enforcement

`evaluate.py::check_discovery_autoheal_shape` asserts that
`startDiscoveryVerification` has no call site in `OTGW-firmware.ino` and that the
daily block calls `markAllMQTTConfigPending()` behind `discoveryDripHeapHealthy()`.
