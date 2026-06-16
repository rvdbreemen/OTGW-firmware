---
id: ADR-142
title: HA Discovery — Mark Unsupported MsgID Entities Unavailable via Availability List
status: Proposed
date: 2026-06-04
tags: [mqtt, ha-discovery, availability, opentherm, boiler, unsupported-msgid, observability]
supersedes: []
superseded_by: []
related: [ADR-077, ADR-088, ADR-093, ADR-102, ADR-122, ADR-080]
deciders: [Robert van den Breemen]
---

# ADR-142: HA Discovery — Mark Unsupported MsgID Entities Unavailable via Availability List

## Status

Proposed. **Binding** (per ADR-080): the CI gate is an extension of the
golden-file discovery test in `tests/` (introduced by ADR-122 / TASK-648) to
assert, for at least one OT msgID whose `isBoilerMsgIdUnsupportedRead()` or
`isBoilerMsgIdUnsupportedWrite()` returns true, that the emitted discovery
payload includes (a) an `availability` list with two entries — the device LWT
and the per-msgID availability topic — and (b) `availability_mode: all`. The
gate must land in the same implementation commit as the feature and must not
be deferred. Name: `check_ha_discovery_msgid_availability_list` (to be added
to `evaluate.py` and/or the golden-file test harness).

The three-boiler field validation (AC#6) is a *verification* acceptance
criterion, not the CI gate; it is not automated.

**Decision Maker:** Robert van den Breemen.
**Milestone:** 2.1.0

## Status History

status_history:
  - date: 2026-06-04
    status: Proposed
    changed_by: Claude (TASK-687)
    reason: Documents Approach B (publish discovery + mark-unavailable via availability list) as decided by maintainer on 2026-06-01. Cross-references ADR-102 to reconcile capability-gating vs liveness-gating concerns.
    changed_via: adr-kit

## Context

### The unsupported-msgID noise problem

The OT protocol (OT spec v4.2) defines msg-type 7 (Unknown-Data-Id) as the
slave's authoritative signal that it does not implement a given msgID. TASK-684
and TASK-685 record these per-msgID, per-direction refusals in two 256-bit
bitmaps in `OTGW-Core.ino`:

```
boilerUnsupportedRead[32]   // set bit N when slave returns Unknown-Data-Id on a Read
boilerUnsupportedWrite[32]  // set bit N when slave returns Unknown-Data-Id on a Write
```

Query hooks are exported in `OTGW-firmware.h` lines 218-219:

```cpp
bool isBoilerMsgIdUnsupportedRead(uint8_t id);
bool isBoilerMsgIdUnsupportedWrite(uint8_t id);
```

TASK-686 surfaced the aggregated list via REST, MQTT (`otgw-firmware/boiler/unsupported_msgids`
CSV, `MQTTstuff.ino:1589`), and WebUI.

Despite this authoritative data, the discovery publisher (`MQTTHaDiscovery.cpp`)
still emits HA discovery configs for every msgID the firmware knows how to
decode, regardless of whether the actual boiler implements it. Concrete
consequences observed in crashevans's beta.20 capture:

- `Texhaust` sensor emitted; boiler never returns a value; HA card shows
  "unavailable" indefinitely.
- `Toutside` entity emitted; boiler ignores the msgID; automations silently
  lose their data source.
- Read-direction `Tr` / `Tdhw` / `ElectricalCurrentBurnerFlame` entities
  emitted but never updated for that boiler model.

The dashboard noise breeds "why is my OTGW broken?" support questions.

### Candidate approaches considered

Four approaches were evaluated in the TASK-687 description and session notes:

**A. Skip discovery entirely** for unsupported msgIDs. Cleanest HA dashboard
(entity never appears). Most stateful: requires publishing a zero-byte retained
payload (ADR-093 orphan cleanup) on suppress, and a full re-stream of the
300-1200 byte discovery config on recovery. False positives (a thermostat that
pre-flights every msgID once, gets Unknown-Data-Id, then never asks again)
result in a permanently missing entity with no indication in HA that it was
suppressed.

**B. Publish discovery, mark entity unavailable via an additional availability
topic** (decided). HA renders the entity as unavailable while offline; flips
to available automatically when the boiler answers. No retained-payload
management on suppress; recovery is a single small MQTT publish on a
dedicated topic. Non-destructive on upgrade.

**C. Publish discovery with a non-standard `state_class` marker.** Introduces
a non-standard HA convention that does not render usefully in stock HA cards
and may break HA's entity registry checks. Rejected.

**D. Hybrid: suppress on clean install, mark-unavailable on upgrade.** Avoids
entity-disappear surprise on upgrade for existing users, but adds stateful
install-context detection and doubles the code paths. The simpler Approach B
already avoids the entity-disappear surprise on upgrade (the entity stays
registered, just marked unavailable). D's extra complexity buys nothing over B
on this codebase. Rejected.

### Relationship to ADR-102 (HA availability reflects MQTT link, not OT bus)

ADR-102 (Accepted, 2026-05-16) corrected a defect where
`publishOTGWConnectedState()` wrote the OT-bus liveness signal (a 30-second
wall-clock window derived from `state.otBus.bOnline`) to the **base** LWT
availability topic, causing transient OT-bus gaps to flip all entities
unavailable in HA. ADR-102's decision: the base availability topic reflects
only MQTT-link liveness (the device LWT), not OT-bus activity.

Approach B does NOT reopen that defect. The distinction is:

- **ADR-102 prohibition:** a *transient liveness* signal (did the OT bus carry
  traffic in the last 30 seconds?) must NOT overwrite the *base* availability
  topic, because it causes spurious unavailability storms on bursty buses.
- **This ADR:** a *persistent capability fact* (the boiler has explicitly said
  it does not implement msgID N, in msg-type 7, at least once) is expressed on
  a *separate, additional* availability topic specific to that (msgID,direction)
  pair. The base LWT availability is untouched.

Capability is not liveness. Liveness is transient (recovered by the next OT
frame). A capability fact persists until the boiler contradicts it by returning
a READ_ACK or WRITE_ACK on the same msgID — an explicit protocol reversal, not
a timing artefact.

The two availability entries compose under `availability_mode: all`: an entity
is available in HA if and only if the device is online on MQTT **and** the
boiler has not refused that msgID. ADR-102's device-LWT semantics are
preserved; this ADR adds a second, orthogonal axis.

### Recovery gap

The TASK-687 session notes (line 85) found that the unsupported bitmaps are
SET on Unknown-Data-Id (OTGW-Core.ino:4214-4224) but are never CLEARED when a
previously-unsupported msgID later returns a READ_ACK or WRITE_ACK. The
automatic recovery claim for Approach B therefore depends on a new clear-on-ack
path that must be added at OTGW-Core.ino:4204-4213 as part of the
implementation. See Decision section, point 5.

### ESP8266 footprint risk

The 2.0.0 branch already carries ~300+ OT-map entities after ADR-122's
five-device topology. Adding an `availability` list to every entity grows each
discovery payload by approximately 100-150 bytes (two `avty_t`/`avty_tpl`
fields + `availability_mode`). Adding one retained availability topic per
(msgID,direction) pair that is CURRENTLY unsupported adds up to ~276 retained
topics in the worst case (138 bilateral msgIDs × 2 directions). Both costs
compound an already-tight ESP8266 build (flash ~85%, RAM ~91.5% after TASK-648).

This is a binding implementation constraint: the implementation MUST measure
`python build.py --firmware` flash and discovery-burst heap on ESP8266 before
merging. A fallback is defined in the Decision section if the footprint does
not fit.

## Decision

### 1. Approach B — availability list with per-msgID availability topic

The firmware publishes HA discovery configs for all OT-map entities as today.
Entities whose msgID is currently flagged as unsupported (read or write
direction, depending on the entity) additionally carry a second availability
entry in the HA discovery payload, expressed as an `availability` **list** (not
a scalar `avty_t`):

```json
"availability": [
  { "topic": "<pub>/value/<nodeid>", "payload_available": "online", "payload_not_available": "offline" },
  { "topic": "<pub>/avail/<msgid><dir>", "payload_available": "online", "payload_not_available": "offline" }
],
"availability_mode": "all"
```

Where:
- `<pub>/value/<nodeid>` is the device LWT base topic (the existing `ctx.mqttPubTopic`
  used today by every entity as its single `avty_t`). Its semantics are defined by
  ADR-102: it reflects MQTT-link liveness only. **Unchanged by this ADR.**
- `<pub>/avail/<msgid><dir>` is the new per-(msgID,direction) availability topic.
  `<msgid>` is the decimal msgID (0-255). `<dir>` is `r` for read-direction
  entities (gated by `isBoilerMsgIdUnsupportedRead`) or `w` for write-direction
  entities (gated by `isBoilerMsgIdUnsupportedWrite`). Bilateral entities (where
  the same msgID has both read and write directions in the OTmap) use the direction
  that matches the entity's `msgcmd` field.
- `availability_mode: all` is mandatory. The default for an `availability` list in
  HA's MQTT integration is `latest` (the most recently received topic wins), which
  would silently defeat the AND semantics. `all` requires both topics to be online.

**Entities that are currently SUPPORTED** (bit not set in the unsupported
bitmap) emit the existing single-entry availability (scalar `avty_t` pointing to
the LWT topic) and are unaffected by this ADR.

### 2. Per-msgID availability topic publication

The firmware publishes the per-msgID availability topic:

- **"online"** when `isBoilerMsgIdUnsupportedRead(id)` / `isBoilerMsgIdUnsupportedWrite(id)`
  returns **false** (boiler has never refused this msgID, or has since answered it).
- **"offline"** when the function returns **true** (boiler refused this msgID at
  least once and has not since answered it).

Publication is **retained** (the availability state must survive a firmware or HA
restart). These availability topics are published in the same discovery drip pass
that emits the discovery config for an entity, and are re-published on any bitmap
change (see point 5, recovery).

Topics are published **only for entities that exist in the OTmap**. No extra
topics are published for msgIDs the firmware does not decode.

### 3. Discovery payload modification

The change is confined to `MQTTHaDiscovery.cpp`. The per-entity compose
functions (`doAutoConfigureMsgid` call chain) are modified to:

1. Determine the entity's OT direction from its `msgcmd` field.
2. Call `isBoilerMsgIdUnsupportedRead(id)` or `isBoilerMsgIdUnsupportedWrite(id)`.
3. If unsupported: emit the `availability` list (two entries) + `availability_mode: all`.
4. If supported: emit the existing single-entry `avty_t` (no list, no `availability_mode`).

`MQTTstuff.ino`'s drip loop (`loopMQTTDiscovery`, `MQTTautoCfgPendingMap`) is
**not modified**. ADR-088 burst-windowing and ADR-077 streaming are untouched.

### 4. Runtime override (escape hatch for false positives)

A corner case exists: a thermostat that pre-flights every msgID once at boot,
receives Unknown-Data-Id from the boiler (which would have answered if asked
properly), and then never asks again. The bitmap marks the msgID as unsupported,
but it is a false positive.

To handle this, the firmware provides a runtime override via MQTT command:

```
<pub>/set/discovery-force-available <msgid>[r|w]
```

Where `<msgid>` is the decimal msgID and `r`/`w` is the direction. When received,
the firmware:

1. Clears the corresponding bit in `boilerUnsupportedRead` or `boilerUnsupportedWrite`.
2. Publishes "online" to `<pub>/avail/<msgid><dir>`.
3. Marks the msgID's discovery config pending re-publish (via the existing
   `markMQTTConfigPending(id)` mechanism) so the discovery payload is re-emitted
   without the availability list on the next drip cycle.

The override is documented in `docs/guides/` and the README HA section (AC#7).
The override does NOT persist across firmware restarts; if the boiler still
refuses the msgID after restart, the bitmap will be re-set on the next Unknown-Data-Id.
A persistent override is deliberately out of scope (YAGNI; the override exists for
diagnosis, not for permanent config management).

### 5. Recovery path — clear-on-ack

As noted in Context, the bitmaps currently accumulate Unknown-Data-Id facts but
have no clear path. Recovery requires adding a clear-on-ack step at
OTGW-Core.ino:4204-4213: when a READ_ACK or WRITE_ACK is received for a msgID
whose unsupported bit is currently set, clear the bit and call
`markMQTTConfigPending(id)` to trigger a re-publish of the discovery config
(now without the availability list, since the msgID is no longer unsupported)
and a "online" publish on the availability topic.

This clear-on-ack path is a **required** part of the implementation (it is the
mechanism by which Approach B's claimed automatic recovery actually works). It
must be implemented and field-tested before AC#5 can be checked off.

### 6. Availability topic lifecycle on entity removal

If a discovery entity is removed from the firmware's OTmap in a future release,
the per-msgID availability topic for that entity becomes an orphan. Per ADR-093
(orphan cleanup contract), when the firmware stops emitting a discovery entity
it MUST also publish a zero-byte retained payload to the availability topic to
clear it from the broker. This is carried forward as a contributor contract.

### 7. Composition with ADR-122 five-device topology

ADR-122 (Proposed, TASK-648) defines per-device unique_id schemes and a
`haDeviceForEntity()` routing function. The per-msgID availability list is an
**additional field** on the entity's discovery payload and is orthogonal to
device routing. The availability topic scheme uses the same base namespace
(`ctx.mqttPubTopic`) and does not depend on which of the five devices an entity
is routed to. No change to ADR-122 is required; the two compose cleanly.

### 8. ESP8266 footprint fallback

If `python build.py --firmware` flash measurement on ESP8266 shows the
availability-list additions exhaust the remaining flash headroom, the
implementation MUST apply the following ESP8266-specific fallback:

> **On ESP8266:** only attach the second availability entry (and emit the
> availability topic) for entities whose msgID is **currently** flagged as
> unsupported at discovery publish time. Entities for supported msgIDs continue
> to emit only the single-entry LWT availability (today's behaviour, zero change
> to payload size). On first detection of an Unknown-Data-Id for a given msgID,
> `markMQTTConfigPending(id)` triggers a re-publish of the discovery config
> with the availability list now present. On clear-on-ack (recovery), another
> re-publish removes the list.
>
> This means ESP8266 availability lists are dynamic (added on first unsupported
> detection, removed on recovery), while ESP32 availability lists are static
> (always emitted, just carrying different payload values). The end-user
> experience is identical: the entity is marked unavailable in HA when the
> msgID is unsupported; it becomes available again on recovery.

If the fallback is triggered, it is documented in code with a
`HAS_ESP8266_AVAIL_LAZY_ATTACH` comment at the conditional block. Whether the
fallback is actually needed is deferred to implementation measurement; it does
not affect this ADR's logical design.

## Alternatives Considered

**A. Skip discovery entirely.** Rejected on two grounds: (a) recovery on
Approach A requires publishing a zero-byte retained payload (ADR-093) plus a
full re-stream of the discovery config (300-1200 bytes) — more broker and RAM
pressure than Approach B's single small availability topic flip; (b) false
positives (the thermostat pre-flight scenario) result in a permanently hidden
entity with no indication in HA, and the only escape is the runtime override
which triggers a re-discovery stream. Approach B's false-positive path is
cheaper: just flip the availability to online.

**C. State_class marker.** Rejected: introduces a non-standard HA convention,
does not render usefully in stock HA entity cards, and risks breakage across HA
releases as `state_class` semantics evolve.

**D. Hybrid (suppress on clean install, mark-unavailable on upgrade).** Rejected:
Approach B already avoids the entity-disappear surprise on upgrade (entities
stay registered but become unavailable). D adds stateful install-context
detection and duplicate code paths for no additional user benefit over B.

**Single availability topic (expire_after instead of a second entry).** An
alternative to an explicit per-msgID availability topic is `expire_after` (HA
marks the entity unavailable if no value update is received within the window).
Rejected: `expire_after` cannot distinguish "boiler offline" from "msgID
unsupported"; it fires on any quiet period, including normal sampling intervals
for infrequently-polled msgIDs. It also requires the entity to have received at
least one value update since firmware boot before the timeout kicks in, so
entities for msgIDs the boiler has refused from the start would need the first
Unknown-Data-Id to arrive before HA marks them unavailable.

## Consequences

**Positive:**

- HA dashboard noise from never-updating entities is eliminated: unsupported
  entities are visually marked unavailable rather than silently accumulating
  stale "unknown" state.
- Recovery is automatic and cheap: a single retained availability topic publish
  flips the entity to available when the boiler answers. No re-stream of the
  discovery config required (on ESP32; see fallback note for ESP8266).
- Non-destructive on upgrade: no existing retained discovery configs are removed
  or changed. HA updates entities in place when the firmware re-publishes with the
  availability list added.
- The base LWT availability (ADR-102) is untouched: MQTT-link liveness continues
  to drive the device-level availability; per-msgID capability is an additional
  axis.
- The `unsupported_msgids` CSV already on the broker (`MQTTstuff.ino:1589`) and
  the REST surface from TASK-686 remain useful independently; this ADR does not
  replace them.

**Negative / trade-offs:**

- **Discovery payload growth.** Each unsupported entity's discovery payload grows
  by ~120-150 bytes (the availability list and `availability_mode` field). On
  ESP8266 this may trigger the lazy-attach fallback (section 8); the fallback adds
  a re-publish event on first detection and on recovery.
- **Per-msgID availability topics.** Up to ~276 additional retained topics on the
  broker in the worst case (all bilateral msgIDs reported unsupported in both
  directions). In practice most boilers implement the common msgIDs; the real
  number is far smaller.
- **New clear-on-ack code path in OTGW-Core.ino.** A previously missing recovery
  primitive must be added. This is low-risk (a bit clear + a pending-map mark) but
  adds a non-trivial test surface: the recovery path must be validated against at
  least one boiler that initially refuses a msgID then later answers it (e.g. after
  a PIC firmware swap).
- **`availability_mode: all` is easy to forget.** Without it, HA defaults to
  `latest` and the AND semantics silently break. The CI gate
  (`check_ha_discovery_msgid_availability_list`) must assert its presence on any
  entity that carries the availability list.

**Risks and mitigations:**

- *Risk:* false-positive unsupported classification (thermostat pre-flight
  scenario) permanently marks a usable entity unavailable. *Mitigation:* runtime
  override (section 4); clear-on-ack (section 5); documented in user-facing guides.
- *Risk:* ESP8266 flash overflow. *Mitigation:* mandatory build measurement before
  merge; lazy-attach fallback defined (section 8).
- *Risk:* reviewer or future contributor adds a new entity that uses the scalar
  `avty_t` form even when the discovery list is the standard. *Mitigation:* CI gate
  asserts list form on any unsupported-flagged entity; code review.
- *Risk:* clear-on-ack path is never triggered in practice for a specific boiler,
  leaving the entity permanently unavailable even after a thermostat replacement.
  *Mitigation:* runtime override plus user-facing documentation; field test AC#6
  requires at least one tester to verify the recovery path end-to-end.

## Related Decisions

- **ADR-077** (Streaming MQTT HA Discovery Architecture): the per-entity
  availability list is emitted within the existing streaming discovery path;
  this ADR does not change how streaming operates.
- **ADR-088** (MQTT Status-Burst Windowing): availability topic publishes are
  subject to the same drip-and-burst budget; the implementation must verify the
  worst-case burst (all entities marked unsupported simultaneously) fits within
  the ADR-088 window.
- **ADR-093** (HA Discovery Retained-Config Orphan Cleanup): the per-msgID
  availability topics are retained; if an entity is removed from the OTmap in a
  future release, its availability topic must be cleared per the ADR-093 orphan
  cleanup contract.
- **ADR-102** (HA Entity Availability Reflects MQTT Link, Not OT Bus): this ADR
  adds a second availability entry alongside ADR-102's LWT entry; it does NOT
  overwrite or replace the LWT entry. The reconciliation is detailed in Context.
  ADR-102 governs transient liveness; ADR-142 adds a persistent capability fact.
  Both apply, independently, under `availability_mode: all`.
- **ADR-122** (HA Discovery Five-Device Topology, Proposed): the per-msgID
  availability list composes with ADR-122's per-device unique_id routing without
  conflict. Both ADRs add fields to the same discovery payload; neither supersedes
  the other.
- **ADR-080** (Binding ADRs must have a CI gate): governs the Binding
  classification in the Status section above.

## References

- TASK-687 (this work — ADR + implementation).
- TASK-684 / TASK-685: unsupported-msgID bitmaps (`boilerUnsupportedRead`,
  `boilerUnsupportedWrite`) and query hooks (`isBoilerMsgIdUnsupportedRead`,
  `isBoilerMsgIdUnsupportedWrite`).
- TASK-686: `otgw-firmware/boiler/unsupported_msgids` MQTT CSV and REST surface.
- TASK-648: ADR-122 five-device topology; golden-file discovery test infrastructure
  (to be extended by this ADR's CI gate).
- `src/OTGW-firmware/OTGW-Core.ino` lines 4204-4213: READ_ACK/WRITE_ACK handler
  (clear-on-ack path needed here).
- `src/OTGW-firmware/OTGW-Core.ino` lines 4214-4224: Unknown-Data-Id handler
  (existing unsupported bitmap set path).
- `src/OTGW-firmware/OTGW-firmware.h` lines 218-219:
  `isBoilerMsgIdUnsupportedRead()` / `isBoilerMsgIdUnsupportedWrite()`.
- `src/OTGW-firmware/MQTTstuff.ino` line 1589: `unsupported_msgids` CSV publish.
- `src/OTGW-firmware/MQTTHaDiscovery.cpp`: per-entity compose functions (change target).
- HA MQTT discovery specification: availability list + `availability_mode` contract.
