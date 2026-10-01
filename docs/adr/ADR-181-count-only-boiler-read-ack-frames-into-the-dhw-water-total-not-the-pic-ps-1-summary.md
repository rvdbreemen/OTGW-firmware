---
id: "ADR-181"
title: "Count only boiler Read-Ack frames into the DHW water total, not the PIC PS=1 summary"
status: "Accepted"
date: "2026-10-01"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-176"
topics:
  - "water-meter"
  - "dhw"
  - "energy-dashboard"
  - "mqtt"
  - "pic-summary"
aliases:
  - "dhw_water_total"
  - "PS=1 water total"
  - "stale MsgID 19"
  - "water meter over-count"
components:
  - "DHW water total feed (the MsgID 19 write sites that call updateDHWWaterMeter)"
symbols:
  - "updateDHWWaterMeter"
  - "updatePSSummaryFloatState"
  - "print_f88"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-181 Count only boiler Read-Ack frames into the DHW water total, not the PIC PS=1 summary

## Status

Accepted, 2026-10-01.

**Decision Maker:** User: Robert van den Breemen (maintainer), 2026-10-01, chose not
to count the PIC summary, as on the 1.x line, over documenting the over-count or
researching a freshness signal.

## Status History

```yaml
status_history:
  - date: 2026-10-01
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-01
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: "ADR-181 amends ADR-176's clause on the two MsgID 19 write sites: the PS=1 summary no longer feeds the water total"
    changed_via: adr-kit lifecycle
  - date: 2026-10-01
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Maintainer accepted in session 2026-10-01 after the acceptance packet (PIC source, red/green harness run, mutants M3/M3b)
    changed_via: adr-kit lifecycle
```

## Context

ADR-176 publishes a cumulative domestic hot water (DHW) total, `dhw_water_total`,
for the Home Assistant Energy dashboard. The firmware integrates MsgID 19 (DHW
flow rate) as elapsed time times flow. ADR-176's Decision Contract requires the
accumulator to be called from both state write sites: `print_f88()`, which handles
the boiler's Read-Ack frames, and `updatePSSummaryFloatState()`, which handles the
summary line a PIC gateway sends in PS=1 mode (ADR-176, line 150). In PS=1 mode
the PIC hides the individual frames, so the summary was the only MsgID 19 source
there.

The summary does not report a fresh reading. The PIC firmware stores the value of
every boiler response (`HandleResponse` calls `StoreValue`,
`other-projects/otgw-6.6/gateway.asm:2429` and `:2436`) and builds the summary from
those stored values (`SummaryReport` calls `PrintStoredVal`, `gateway.asm:1644`
and `:1648`). Once the boiler stops answering MsgID 19, or the thermostat stops
asking, every later summary repeats the last stored flow, and nothing in the line
says how old it is.

The host harness of TASK-1123 measured the effect on the real code
(`test/host/test_dhw_water_meter.py`, case R1): a stored 8 L/min, presented by a
summary every 30 s for 10 minutes, adds 80 L. Each interval stays under the gap
cap, so the accumulator cannot tell a stale repeat from a steady draw. In Home
Assistant's long-term statistics a decrease of a `total_increasing` sensor reads
as a meter reset, so an over-count cannot be taken back afterwards.

The 1.x line counts only `print_f88()` frames that pass its validity gate, so a 1.x
PIC gateway in PS=1 mode reports no water total (TASK-1123 notes, point 4).

## Decision

Only boiler Read-Ack frames for MsgID 19, through `print_f88()`, feed the DHW
water total. `updatePSSummaryFloatState()` keeps writing the MsgID 19 value into
`OTcurrentSystemState.DHWFlowRate`, so the flow-rate sensor still updates, but it
no longer calls `updateDHWWaterMeter()`. A PIC gateway in PS=1 mode therefore
adds no water. Booted in PS=1 mode, it decodes no MsgID 19 frame, so no total is
published and Home Assistant shows the entity as unknown (ADR-176 publishes only
after a decoded frame). Switched to PS=1 after frames were counted, it keeps
publishing the last total, which no longer grows.

This amends one clause of ADR-176: "Call the accumulator from both state write
sites" becomes "Call the accumulator from `print_f88()` only". Every other clause of
ADR-176 stays in force.

## Decision Contract

### Must

- Call `updateDHWWaterMeter()` only for a MsgID 19 boiler Read-Ack handled by
  `print_f88()`.
- Keep writing the PS=1 summary's MsgID 19 field into
  `OTcurrentSystemState.DHWFlowRate`.
- State in the user documentation that a PIC gateway in PS=1 mode adds no water to
  the total.

### Must Not

- Feed the water total from the PIC PS=1 summary, or from any other source that
  repeats a stored value without its age.

### Exceptions

- None.

### Verification

- `python test/host/test_dhw_water_meter.py`: case W2 (PS=1 summaries add 0 L
  while the flow-rate state follows them), case R1 (a stale repeated summary adds
  0 L) and case W6 (summary fields, valid or malformed, leave a restored total
  unchanged), on the real code sliced from the sources. Mutant M3, which restores
  the summary feed, must fail R1.

## Alternatives Considered

- **Keep counting the summary and document the over-count.** PS=1 users keep a
  total, but it can run away by several hundred litres an hour (8 L/min is 480 L
  per hour) whenever the stored value goes stale, and the Energy dashboard keeps
  that error for good.
- **Gate the summary on a freshness signal.** The summary carries no per-field
  age. The nearest heuristic, a value that stays exactly the same for a long time,
  also matches a steady real draw, so it would drop real water or keep counting
  stale water. Rejected as unreliable before any research effort.
- **Count the summary only outside OT-Direct mode (ADR-176 as accepted).** That is
  the behaviour this decision replaces.

## Consequences

**Positive:**

- A stale summary can no longer inflate the total: case R1 goes from 80 L to 0 L.
- Both firmware lines count the same: a PIC gateway in PS=1 mode adds no water on
  1.x either.

**Negative:**

- A PIC gateway in PS=1 mode gets no usable water total: booted in that mode it
  shows the entity as unknown. Its flow-rate sensor still updates. A user who wants
  the total runs the PIC in PS=0 mode.
- OT-Direct boards are unaffected: their summary echo was already excluded, and
  their B frames still count through `print_f88()`.

## Open Questions

- None.

## Related Decisions

- **ADR-176 (Publish a firmware-integrated cumulative DHW water total for the Home
  Assistant Energy dashboard)**: this ADR amends its clause on the two write sites;
  the rest of ADR-176 stands.

## References

- `docs/adr/ADR-176-publish-a-firmware-integrated-cumulative-dhw-water-total-for-the-home-assistant-energy-dashboard.md`,
  line 150 (the clause this ADR amends).
- `src/OTGW-firmware/OTGW-Core.ino`: `print_f88()` (the MsgID 19 call of
  `updateDHWWaterMeter()`) and `updatePSSummaryFloatState()`, case 19.
- `other-projects/otgw-6.6/gateway.asm:117` (PS=1 described), `:1644` and `:1648`
  (`SummaryReport` prints stored values), `:2429` and `:2436` (`HandleResponse`
  stores the boiler's value).
- `test/host/test_dhw_water_meter.py` and `test/host/test_dhw_water_meter.cpp`,
  cases W2, W6 and R1.
- TASK-1123 (the water total and its open R1 question) and TASK-1189 (this change).
- Home Assistant sensor entity documentation, state classes:
  <https://developers.home-assistant.io/docs/core/entity/sensor/#available-state-classes>
