---
id: "ADR-187"
title: "Track upstream espMqttClient in the vendored copy instead of pinning 1.7.2"
status: "Accepted"
date: "2026-10-05"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "mqtt"
  - "espmqttclient"
  - "vendored-library"
aliases:
  - "espMqttClient upstream update"
  - "vendored espMqttClient version"
components:
  - "vendored espMqttClient version policy"
symbols:
  - "ClientSync::write"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-187 Track upstream espMqttClient in the vendored copy instead of pinning 1.7.2

## Status

Accepted, 2026-10-05.

## Status History

```yaml
status_history:
  - date: 2026-10-05
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-05
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: "Maintainer accepted 2026-10-05 after the alpha.416 bench test (TASK-1214): max loop gap 124 ms under a 500 ms broker delay, 398 configs, three builds green"
    changed_via: adr-kit lifecycle
```

## Context

ADR-186 vendored the MQTT (Message Queuing Telemetry Transport) client espMqttClient into
`src/libraries/espMqttClient`. Its decision contract says that directory holds "the 1.7.2
sources unchanged except `ClientSync::write()`".

Upstream has moved on since 1.7.2.
- v1.7.3 ("Fix memory issue", upstream PR #188) re-anchors the Outbox iterator after
  `_remove()`, so it no longer leaves a dangling pointer.
- After 1.7.3, `main` also fixes a parser desync when an incoming PUBLISH topic exceeds
  `EMC_MAX_TOPIC_LEN`, and adds `NetworkClient` support for Arduino-ESP32 3.x.
- The dangling pointer is a heap-corruption risk on our line, which runs at low free heap
  under load (TASK-1036, TASK-1162).

The ADR-186 patch is offered upstream as bertmelis/espMqttClient PR #191. Until it is
merged and released, the vendored copy stays.

## Decision

The vendored copy follows upstream: release, or upstream `main` when a needed fix is not yet
in a release. The only local change is the ADR-186 `ClientSync::write()` patch, re-applied on
every update. This replaces ADR-186's "1.7.2 sources" wording; the rest of ADR-186 stands.
The first update takes upstream `main` at commit de31cb2 (library.json 1.7.3), which carries
both fixes above.

## Decision Contract

### Must

- `src/libraries/espMqttClient` equals an upstream release or upstream `main` at a recorded
  commit, plus only the ADR-186 `ClientSync::write()` patch, marked in the code with a comment
  naming ADR-186 and upstream PR #191.
- Every update repeats the TASK-1213 slow-broker bench test and builds the three targets.

### Must Not

- Must not carry any other local change to the library without a new ADR.

### Exceptions

- None.

### Verification

- Bench, 2026-10-05, OTGW32 combo alpha.416 with upstream `main` de31cb2 plus the patch.
  `tc netem delay 500ms` on the rig broker, reboot, then a discovery republish
  (`t1213_netem.py`). Result: max loop gap 124 ms, 0 gaps over 200 ms, 398 retained configs,
  MQTT connected.
- Builds: esp32, esp32-classic and esp32-combo SUCCESS (alpha.416); `evaluate.py --quick`
  0 failures.

## Alternatives Considered

- **Stay on 1.7.2 as ADR-186 wrote.** It keeps the known Outbox dangling pointer and the
  parser desync in a firmware that runs at low free heap.
- **Wait for a release that contains both fixes.** The parser fix is only on `main` today, and
  the Outbox fix is already released, so waiting buys nothing for that one.

## Consequences

**Positive:**

- The firmware gets the upstream memory-safety and parser fixes, with the stall fix kept:
  max loop gap 124 ms under a 500 ms broker delay.

**Negative:**

- Tracking `main` can pull unreleased changes. Mitigation: record the commit and repeat the
  bench test on every update.

## Open Questions

- None.

## Related Decisions

- ADR-186 (vendor espMqttClient with a non-blocking socket write). This ADR amends its version
  wording.
- ADR-131 (MQTT engine on espMqttClient), unchanged.

## References

- TASK-1214 (this update), TASK-1213 (the stall fix and its bench test).
- Upstream: bertmelis/espMqttClient PR #188 (Outbox fix, v1.7.3), PR #191 (our patch), `main`
  commit de31cb2.
