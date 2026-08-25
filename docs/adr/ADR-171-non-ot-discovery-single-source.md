---
id: "ADR-171"
title: "Boot and Republish Discovery Queues Share One Non-OT ID Set"
status: "Proposed"
date: "2026-07-26"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
---
# ADR-171 Boot and Republish Discovery Queues Share One Non-OT ID Set

## Status

Proposed. Date: 2026-07-26.

## Status History

```yaml
status_history:
  - date: 2026-07-26
    status: Proposed
    changed_by: Agent
    reason: Records the single-helper fix for a boot/republish discovery asymmetry found while mapping the 1.7.2-beta.4 port (TASK-1037). 2.0.0-only; no 1.x sibling.
    changed_via: manual
```

## Context

MQTT HA discovery configs reach the broker by two routes:

- **JIT (ADR-100/112)** — an OT MsgID publishes its config the first time that ID
  is seen on the bus.
- **Explicit queueing** — `publishNonOTDiscoveryConfigs()` at boot / top-topic
  change / broker restart, and `markAllMQTTConfigPending()` on force-republish,
  settings save, and topology migration.

Faux ("pseudo") IDs in the 242-255 block are **never bus-seen**, so JIT can never
reach them. An ID absent from the boot list is therefore announced only when
something else calls `markAllMQTTConfigPending()`.

The two functions had drifted. `markAllMQTTConfigPending()` walks
`readSensorIndex()` / `readBinSensorIndex()` across 0..255 and marks every ID
present in the discovery tables, then explicitly adds a few that the tables do not
index. `publishNonOTDiscoveryConfigs()` does **no** walk — it is a hand-maintained
list, and that list was missing seven IDs:

| ID | Symbol | Content |
|---|---|---|
| 243 | `OTGWotdirectid` | OTDirect flame metrics (ADR-124) |
| 245 | `OTGWs0dataid` | S0 pulse counters |
| 251 | `OTGWdiag200id` | SAT / flame diagnostics |
| 252 | `OTGWsatcoreid` | SAT control / PID / cycle / statistics |
| 253 | `OTGWsatweatherid` | SAT BLE / pressure / weather |
| 254 | `OTGWsatbinaryid` | SAT binaries + flame status |
| 255 | `OTGWsatzoneid` | dynamic SAT zone discovery |

Consequence: **on a clean boot with MQTT enabled, SAT never announced itself to
Home Assistant.** OTDirect flame metrics and the S0 counters were in the same
position. The entities appeared only after a settings save, a long-outage
reconnect above the republish threshold, or a manual force-republish — which is
why this went unnoticed: any of those events masks it, and developers trigger them
constantly.

This is a shipping bug independent of the 1.7.2-beta.4 port. It was found while
mapping 1.x TASK-1035 (which queued the connection-status sensors at boot on the
1.x line) onto 2.0.0 and discovering that 2.0.0 already had that particular fix
but had the same *class* of bug seven times over.

## Decision

Introduce one private helper, `queueNonOTDiscoveryIds()` in `MQTTstuff.ino`, as
the single definition of "which non-bus-seen IDs need a discovery config queued".
Both `publishNonOTDiscoveryConfigs()` and `markAllMQTTConfigPending()` call it.

Placement details that matter:

- In `markAllMQTTConfigPending()` the helper runs **after** the
  `memset(MQTTautoCfgPendingMap, ...)` and after the LUT walk.
  `setMQTTConfigPending()` is an idempotent `bitSet`, so the overlap between the
  helper's list and the walk's output is a no-op.
- `dripDeviceInfoPending = true` (ADR-140: the first drip entity carries the full
  single-device block) moves **into** the helper, so neither caller can forget it.
- The TASK-648 topology-migration early return at the top of
  `publishNonOTDiscoveryConfigs()` is untouched. It delegates to
  `markAllMQTTConfigPending()` and returns; since that now also calls the helper,
  its comment ("it will have queued the non-OT IDs too") becomes enforced rather
  than merely asserted.

## Alternatives Considered

### A: Add the seven missing IDs to the boot list
The minimal fix. Rejected: it restores parity today and does nothing to stop the
next divergence. The two lists have already drifted once, silently, for as long as
SAT discovery has existed.

### B: Give `publishNonOTDiscoveryConfigs()` the same 0..255 LUT walk
Would make the two functions structurally identical. Rejected: the walk also
marks bus-seen OT IDs, which is exactly what the boot path must *not* do — boot
deliberately leaves OT IDs to JIT (ADR-112). Reusing the walk would undo pure-JIT
discovery.

### C: Enumerate the ID list in a shared PROGMEM array
Slightly less code than a helper function. Rejected: it buys nothing over a
function call here, and it separates each ID from its explanatory comment, which
is the part that keeps this list correct.

## Consequences

**Benefits**
- SAT, OTDirect flame metrics and the S0 counters announce at boot, which is what
  a user reasonably expects after flashing and configuring MQTT.
- Boot set and republish set cannot drift again: there is only one list.

**Trade-offs**
- Boot discovery grows by 7 drip ticks (~14 s at the healthy 2 s cadence) and
  roughly 60 entities. This is **not a new peak** — it is exactly what every
  settings-save republish already does — but it is newly reached at boot, on the
  most fragmented heap of the device's life (WiFi + BLE + MQTT + web server all
  just came up). This is the main thing to watch in validation.
- `iPublishedTopicCount` rises on boot by the same ~60. Both ADR-062 counter gates
  inspect the stream helpers and the reset path, neither of which changes.
- SAT discovery is now announced at boot on hardware without SAT. This does not
  extend the existing TASK-543 gating decision (`OTGW-firmware.h`) that SAT
  discovery is unconditional on this dual-target branch — it applies it
  consistently. Runtime publishers still decide whether an entity has live data.

**Risks and mitigations**
- *Risk*: the larger boot announce pushes a PSRAM-less S3 into drip slow-mode or
  worse. *Mitigation*: `loopMQTTDiscovery()` is heap-gated and paced; validation
  requires a fresh-boot test against a wiped broker on a no-PSRAM board.

## Related Decisions

- **ADR-100 / ADR-112** — pure JIT discovery for bus-seen OT IDs; this ADR covers
  only the IDs JIT structurally cannot reach.
- **ADR-140** — single-device topology; `dripDeviceInfoPending`.
- **ADR-170** — the daily heal, which republishes this same set.
- **1.x TASK-1035** — the sibling-line fix whose mapping surfaced this.

## Enforcement

`evaluate.py::check_non_ot_discovery_single_source` asserts that both queueing
entry points call `queueNonOTDiscoveryIds()`, and that `setMQTTConfigPending(` does
not appear directly in either of them.
