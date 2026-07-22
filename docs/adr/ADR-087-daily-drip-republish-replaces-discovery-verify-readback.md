# ADR-087: Unconditional daily drip republish replaces automatic discovery-verify readback

## Status

Accepted. Date: 2026-07-22.

**Decision Maker:** User: Robert van den Breemen (maintainer) approved replacing ADR-062's automatic discovery-verify readback with an unconditional daily drip republish, after root-cause analysis of a field heap-exhaustion on device OTGW-48E72958B013.

## Status History

status_history:
  - date: 2026-07-22
    status: Proposed
    changed_by: Agent
    reason: Records the option-5 redesign (TASK-1048) that replaces ADR-062's auto-verify readback with a daily drip republish; awaiting maintainer acceptance
    changed_via: adr-kit
  - date: 2026-07-22
    status: Accepted
    changed_by: User
    reason: Maintainer accepted after review; supersedes ADR-062, ships in 1.7.2-beta.3

## Context

ADR-062 introduced an automatic retained-discovery verification: subscribe to the node-scoped wildcard `<haprefix>/+/<nodeId>/#` for a ~15 s window, count retained configs received, compare against `iPublishedTopicCount`, and trigger `markAllMQTTConfigPending()` (a full re-announce via the drip) when `received < expected`. It was tuned for ~80 configs with a temporary 1024-byte PubSubClient RX buffer.

A field transcript (`transcript-20260721-225026-1.7.2-onset.1+bc067cc`, device OTGW-48E72958B013) shows this mechanism causing a hard heap exhaustion:

- Boot 22:50, heap flat and healthy (~20000 free / 16400 max block) for ~70 min.
- `00:00:00.998 verifyAccess: [verify] started: wildcard=homeassistant/+/otgw-48E72958B013/# expected=124`.
- The device now announces **124** entities, not ~80. The retained-config flood plus concurrent traffic overruns the readback: `00:00:16.109 [verify] done: expected=124 received=26 orphans=0 missing=98 outcome=2`.
- Partial readback (26 of 124) is misread as 98 missing → `markAllMQTTConfigPending` full republish.
- Because `iLastVerifyEpoch` is never set to success, the hourly first-run retry (`OTGW-firmware.ino:344`) re-arms every hour. Verify → false-missing → republish repeats and leaks heap until the device dies at 00:12 (free 7080 / block 4032). Boot→death = 82 min.

Free heap AND max block fall together across the decline, i.e. genuine exhaustion, not pure fragmentation. Bench reproduction failed for months because the bench had auto-verify off, fewer entities, and a broker that delivered all retained messages inside the window, so `received == expected` and no runaway armed.

The readback is unreliable on ESP8266: the reduced PubSubClient buffer and short window cannot absorb a 124-message retained burst during live MQTT/WebSocket traffic, so `received < expected` is the normal outcome, not the exception. The count-comparison design therefore self-triggers an endless republish.

## Decision

Remove the **automatic** discovery-verify readback. The daily and hourly-first-run triggers no longer call `startDiscoveryVerification()`. Instead, the daily trigger performs an **unconditional drip republish** of all retained discovery configs as the Gap-A auto-heal, guarded only by cheap local preconditions:

```cpp
if (settings.mqtt.bDiscoveryAutoVerify
    && state.mqtt.bConnected
    && countPendingDiscoveryIds() == 0
    && ESP.getMaxFreeBlockSize() >= 8000) {
  markAllMQTTConfigPending();
}
```

`markAllMQTTConfigPending()` only sets pending bits; `loopMQTTDiscovery()` drains one ID per heap-gated tick (2 s healthy, 10 s under pressure). The heal is therefore bounded, self-throttling, and never subscribes to a wildcard or counts inbound messages, so no false-missing and no retry storm can occur. The hourly first-run retry block is deleted.

The manual, user-initiated verify path (`POST /api/v2/discovery`, `restAPI.ino:654`; telnet debug, `handleDebug.ino:175`) is retained unchanged so a user can still run an on-demand diagnostic; it fires once per explicit request and cannot self-retrigger.

The `bDiscoveryAutoVerify` setting is kept as the on/off switch for the daily auto-heal (default true), preserving settings-file compatibility.

## Alternatives Considered

### Alternative A: raise the PubSubClient buffer and widen the verify window
Grow the RX buffer and extend the 15 s window so `received == expected` reliably. Rejected: buffer growth consumes DRAM at the worst moment (mid-flood), the count scales with entity growth (already 124, not 80), and a slow/busy broker can still time out the window: the false-missing failure mode is only made rarer, not removed. It also leaves the runaway retry loop intact.

### Alternative B: republish only the genuinely-missing IDs, with a clean-read requirement
Only mark-pending the specific IDs absent from a *complete* readback. Rejected: it still depends on a reliable full readback, which is the unavailable primitive here; and per-topic identity tracking needs a reverse topic→msgid map that ADR-062 itself rejected as fragile across source-separation modes.

### Alternative C: do nothing (keep ADR-062)
Rejected: primary-source field evidence shows ADR-062's automatic path exhausts heap and kills the device on a real installation.

## Consequences

**Benefits**
- Eliminates the runaway feedback loop and the ~82 min field death entirely: no wildcard subscribe, no count, no false-missing, no hourly retry.
- Removes transient buffer-growth and inbound retained-flood processing from steady-state operation.
- Simpler to reason about (KISS): the auto-heal is a bounded, heap-gated outbound drip.

**Trade-offs**
- The daily heal republishes all configs unconditionally instead of only when a gap is detected. Cost is one drip pass per day (2 s per ID, retain-overwrite of identical configs is a broker no-op), which the field log shows is heap-safe (heap stayed ~17800 during the drip).
- Automatic detection of broker-side retained loss is gone; recovery is now time-based (next daily pass, ≤24 h) rather than verified. For a home HVAC gateway this latency is acceptable.
- The `disc_verify_runs` / `disc_last_missing` / `outcome` statistics stay at their last values under automatic operation (only the manual path updates them).

**Risks and mitigations**
- *Risk*: the manual verify path still contains the readback and can leak if invoked repeatedly. *Mitigation*: it is user-initiated only and cannot self-retrigger; a full fix or removal of the readback leak is tracked separately (TASK-1037).
- *Risk*: daily drip collides with heavy traffic. *Mitigation*: `loopMQTTDiscovery()` is already heap-gated and paced; the trigger also guards on `maxFreeBlock >= 8000` and `no drip in progress`.

## Related Decisions

- **Supersedes ADR-062 (Retained discovery verification via wildcard subscribe with RAM-tuned buffer resize)** replaces its automatic verification mechanism, and retains the manual diagnostic endpoint it introduced.
- **ADR-040** source-specific discovery topic variants (the wildcard shape ADR-062 targeted).
- **ADR-086** rate limit on UI-polled REST endpoints (same 1.7.2 hardening cycle).

## References

- Field transcript: `transcript-20260721-225026-1.7.2-onset.1+bc067cc OTGW-48E72958B013`; analysis in `OTGW-logs/FIELD-ROOTCAUSE-discovery-verify-runaway.md`.
- Implementation: `src/OTGW-firmware/OTGW-firmware.ino` daily/hourly trigger block; `markAllMQTTConfigPending()` `MQTTstuff.ino:1505`; `loopMQTTDiscovery()` `MQTTstuff.ino:1538`.
- Task: TASK-1048. Related leak-fix task: TASK-1037.
