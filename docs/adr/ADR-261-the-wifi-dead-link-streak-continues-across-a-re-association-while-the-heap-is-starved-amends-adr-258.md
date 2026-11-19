---
id: "ADR-261"
title: "The WiFi dead-link streak continues across a re-association while the heap is starved (amends ADR-258)"
status: "Proposed"
date: "2026-11-19"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-258"
topics:
  - "wifi"
  - "dead-link supervisor"
  - "heap starvation"
  - "watchdog recovery"
aliases:
  - "dead streak reset on reconnect"
  - "starved heap re-association"
components:
  - "wifiLinkProbeOnReconnect"
symbols:
  - "wifiLinkProbeOnReconnect"
  - "wifiDeadProbes"
  - "WIFI_STARVED_MAXBLOCK"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-261 The WiFi dead-link streak continues across a re-association while the heap is starved (amends ADR-258)

## Status

Proposed, 2026-11-19. Guideline-level (ADR-080): verified by the host test named below, no evaluate.py gate. Amends
ADR-258: it adds one rule to the dead-link supervisor; the rest of ADR-258 (a starved probe counts as dead, the witness
rule, the reconnect-then-restart path and the cap of 3) stays.

User Decision: Robert chose option C for 2.0.0 (Discord DM 1572842729485111327, 2026-11-19 05:15 UTC, a reply to
the coordinator's update 1572774626625589432; relayed by the coordinator), with option B as the 2.1.0 follow-up
(TASK-2038). That choice settles the option; acceptance of this ADR is a separate yes from Robert.

## Status History

```yaml
status_history:
  - date: 2026-11-19
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal (option C, put to Robert by the coordinator, 2026-11-19)
    changed_via: adr-kit
  - date: 2026-11-19
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Related to ADR-258
    changed_via: adr-kit lifecycle
```

## Context

ADR-258 lets the WiFi dead-link supervisor (`networkStuff.ino`) count a probe that cannot run for lack of memory as a
dead probe when the largest free block is under `WIFI_STARVED_MAXBLOCK` (4096 B). Eight dead probes in a row (one per
60 s) lead to a forced reconnect, and the next eight to a restart.

The bench run of TASK-2026 on .63 (2026-11-19, beta.381 with supervisor counters, heap held starved for 30 min, REST and
telnet unreachable) showed that the supervisor never reached a decision: 14 probes were counted dead, but 0 decision
points (witness 0, reconnect 0) and no restart. In the same 30 min the access point (AP) dropped the association 7 times
(`wifi_outages` 7, reason `NOT_AUTHED`), about every 4-5 min. Every re-association calls `wifiLinkProbeOnReconnect()`,
which set the dead streak `wifiDeadProbes` to 0. So eight dead probes in a row never happened, and a starved, unreachable
unit was not recovered.

## Decision

While the heap is starved (`platformMaxFreeBlock() < WIFI_STARVED_MAXBLOCK`), `wifiLinkProbeOnReconnect()` keeps the
dead streak instead of resetting it; with a heap that is not starved it resets the streak as before.

Under a starved heap a re-association is part of the symptom, not a sign that the link recovered: the AP keeps dropping
the association because the unit cannot keep up. An answered probe (the gateway replies) still resets the streak, so a
link that really works again is not counted against.

## Decision Contract

### Must

- Keep `wifiDeadProbes` across a re-association only while the largest free block is under `WIFI_STARVED_MAXBLOCK`.
- Keep every other reset of the streak: an answered probe, a wired link, a changed gateway address, and the decision
  point itself.

### Must Not

- Must not keep the streak across a re-association when the heap is not starved (a normal reconnect starts a fresh
  streak, ADR-258 behaviour).
- Must not change the witness rule, the restart cap or the thresholds of ADR-258.

### Exceptions

- None.

### Verification

- `tests/test_wifi_deadlink.py::WifiDeadLinkStateMachine::test_starved_link_with_ap_drops_every_4_min_still_recovers`
  runs the real state machine with a starved heap, a dead link and an AP drop every 4 min for 40 min. It fails on the
  code before this ADR (0 forced reconnects, no restart; commit b0febe073 ran it as an expected failure) and passes with
  it. The other scenarios of the file stay green (36 passed with `tests/test_heaphold_hook.py`).

## Alternatives Considered

- **A: keep ADR-258 as is and document the limit.** Rejected: the bench showed the limit is reached in the very state
  ADR-258 is meant to recover from, so the fix would not act there.
- **B: judge the app layer as well (no completed REST response or MQTT acknowledgement within the window).** Not chosen
  for now: it also covers a unit whose network stack answers but whose app is starved, but it is a larger behaviour
  change with a risk of false restarts on a quiet, healthy unit. It is a separate follow-up (2.1.0).
- **Do not reset the streak on any re-association.** Rejected: a normal reconnect after a real outage would then count
  old dead probes against a link that works again.

## Consequences

**Positive:**

- A starved unit whose AP keeps re-associating reaches the forced reconnect and the restart, as ADR-258 intends.
- One condition in one function; the healthy-heap behaviour is unchanged.

**Negative:**

- A unit with a starved heap whose link really recovered through a re-association, but whose gateway does not answer
  ping yet, keeps its streak a little longer. Mitigation: the first answered probe resets it, and the witness rule
  (MQTT, REST or port 25238 traffic) still blocks an action while the LAN works.
- The probe is an ICMP echo to the gateway: it does not test a TCP round trip through the app. A unit whose network stack
  answers but whose app is starved is not covered by ADR-258 or this ADR (alternative B, follow-up).

## Open Questions

- [x] Robert's choice between A, C (this ADR) and B for 2.0.0: C, B for 2.1.0 (DM 1572842729485111327, 2026-11-19).

## Related Decisions

- **ADR-258**: amended by this ADR (one added rule for the streak reset on a re-association).

## References

- TASK-2026 notes (2026-11-19): run 3 on .63, 381+b06eeff, counters and `wifi_outages`.
- `src/OTGW-firmware/networkStuff.ino`: `wifiLinkProbeOnReconnect()`, `WIFI_STARVED_MAXBLOCK`.
- `tests/test_wifi_deadlink.py`: scenario `starved_flap`.
