---
id: "ADR-171"
title: "Boot and Republish Discovery Queues Share One Non-OT ID Set"
status: "Accepted"
date: "2026-10-15"
binding: false
gate: "check_non_ot_discovery_single_source"
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-182"
---
# ADR-171 Boot and Republish Discovery Queues Share One Non-OT ID Set

## Status

Accepted, 2026-10-15.

## Status History

```yaml
status_history:
  - date: 2026-07-26
    status: Proposed
    changed_by: Agent
    reason: Records the single-helper fix for a boot/republish discovery asymmetry found while mapping the 1.7.2-beta.4 port (TASK-1037). 2.0.0-only; no 1.x sibling.
    changed_via: manual
  - date: 2026-10-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-182
    changed_via: adr-kit lifecycle
  - date: 2026-10-15
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer accepted 2026-10-15 after the code-vs-ADR verification and refresh (TASK-1237)
    changed_via: adr-kit lifecycle
```

## Context

MQTT (Message Queuing Telemetry Transport) HA discovery configs reach the broker by two routes:

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
| 251 | `OTGWdiag200id` | SAT (smart thermostat) / flame diagnostics |
| 252 | `OTGWsatcoreid` | SAT control / PID (proportional-integral-derivative) / cycle / statistics |
| 253 | `OTGWsatweatherid` | SAT BLE (Bluetooth Low Energy) / pressure / weather |
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
  `memset(MQTTautoCfgPendingMap, ...)` and after the LUT (lookup table) walk.
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
- **ADR-182** (Accepted): amends the helper: ID 241 (`OTGWdhwmeterid`, the DHW (domestic hot water)
  water total) is queued from `queueNonOTDiscoveryIds()` only when
  `dhwWaterMeterHasData()` is true (`MQTTstuff.ino:2034`), and the LUT walk in
  `markAllMQTTConfigPending()` skips 241 so the helper alone decides
  (`MQTTstuff.ino:2100`). The helper stays the single owner of the non-OT set;
  one of its entries is now conditional.
- **1.x TASK-1035** — the sibling-line fix whose mapping surfaced this.

## Enforcement

`evaluate.py::check_non_ot_discovery_single_source` (`evaluate.py:2290`, helper
`non_ot_discovery_single_source()` at `evaluate.py:761`, run from
`evaluate.py:3754`) asserts that the helper is defined, that both queueing entry
points call `queueNonOTDiscoveryIds()`, and that the boot path
`publishNonOTDiscoveryConfigs()` contains no direct `setMQTTConfigPending(` call
(`boot_no_direct_calls`). The republish path is not checked for direct calls and
does call `setMQTTConfigPending()` itself: the 0..255 LUT walk in
`markAllMQTTConfigPending()` marks every ID the discovery tables index
(`MQTTstuff.ino:2099-2106`). That is by design (see Alternative B); the rule is
that neither entry point hand-maintains its own list of non-bus-seen IDs.

## References

- Tasks: TASK-1037 (where the asymmetry was found and fixed); 1.x TASK-1035 (the
  sibling fix); TASK-543 (SAT discovery unconditional); TASK-648 (topology
  migration early return); TASK-1123 (DHW water total, ADR-182).
- `src/OTGW-firmware/MQTTstuff.ino:2030-2050`: `queueNonOTDiscoveryIds()`.
- `src/OTGW-firmware/MQTTstuff.ino:2056-2075`: `publishNonOTDiscoveryConfigs()`
  (boot path; calls the helper at `:2073`).
- `src/OTGW-firmware/MQTTstuff.ino:2085-2114`: `markAllMQTTConfigPending()`
  (republish path; calls the helper at `:2112`).
- Commits: 24be052f2 (introduced the helper, TASK-1037), 4bbceed61 (ID 241 added
  conditionally, TASK-1123 / ADR-182), c456570ac (TASK-1037 closed with the
  fresh-boot discovery run on a wiped broker).
