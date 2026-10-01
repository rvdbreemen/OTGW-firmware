---
id: "ADR-183"
title: "Record what an unclean reboot does to the persisted DHW water total"
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
  - "persistence"
  - "home-assistant"
  - "energy-dashboard"
aliases:
  - "partial regression"
  - "unclean reboot"
  - "water total drop"
  - "total_increasing dip"
components:
  - "Persisted DHW water total after an unclean reboot (loadDHWWaterMeter, the write-rate rule)"
symbols:
  - "loadDHWWaterMeter"
  - "dhwWaterMeterSaveDue"
  - "DHW_METER_SAVE_DELTA_L"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-183 Record what an unclean reboot does to the persisted DHW water total

## Status

Accepted, 2026-10-01.

**Decision Maker:** User: Robert van den Breemen (maintainer), 2026-10-01, asked for
the record of ADR-176 to be corrected in a short amending ADR.

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
    reason: ADR-183 amends ADR-176's answered question on partial regression with the actual unclean-reboot behaviour
    changed_via: adr-kit lifecycle
  - date: 2026-10-01
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Maintainer accepted in session 2026-10-01 after the acceptance packet
    changed_via: adr-kit lifecycle
```

## Context

ADR-176 persists the domestic hot water (DHW) total in `/dhw_water.json`. It writes
the file when at least 10 L are unsaved (`DHW_METER_SAVE_DELTA_L`), 15 minutes after
the last write when any water is unsaved, and on every orderly restart. Its
answered question on a partial regression after an unclean reboot (ADR-176, line
266) decides to "resume from the last persisted value and accept the undercount",
and gives two reasons:

- "The counter never decreases, so Home Assistant never sees a reset and the
  long-term statistic stays coherent."
- "Same answer as the 1.x peer."

Neither reason holds:

- After a power cut, or a firmware flash over USB (Universal Serial Bus) that resets
  the chip without an orderly restart, the restored total can be lower than the last published value:
  by under 10 L, plus up to one minute of flow (`docs/api/MQTT.md`, DHW Water Total,
  Persistence). The published counter does decrease.
- The 1.x line keeps the total in RAM (random-access memory) only, so every reboot
  starts it from 0 (1.x
  `dhwWaterMeter.ino`: "The total lives in RAM only"). Home Assistant reads that as a
  clean meter reset. The two lines do not answer this question the same way.

Home Assistant decides between the two readings in `reset_detected()`
(`homeassistant/components/sensor/recorder.py`): a value below 0.9 times the
previous one is a meter reset; a lower value at 0.9 times the previous one or more
is a dip, which it logs as a warning.

The TASK-1123 notes found this discrepancy (point 1). ADR-176 is Accepted, so its
text is not edited.

## Decision

ADR-176's decision stands: after an unclean reboot the firmware resumes from the
last persisted value and accepts the lost litres.

This ADR corrects the record of what that means:

- The published total can drop after an unclean reboot, by less than 10 L plus up to
  one minute of flow.
- Home Assistant reads a drop to at least 90% of the previous value as a dip, not as
  a reset. A drop below 90% is a meter reset. With a drop under 10 L that happens
  only while the total is below 100 L, so in the first days after the counter starts
  or after a user reset.
- The 1.x line differs: it starts from 0 after every reboot.

This amends ADR-176's answered question on partial regression; its two reasons are
replaced by the points above. Every other clause of ADR-176 stays in force.

## Decision Contract

### Must

- Keep `docs/api/MQTT.md` describing the drop after an unclean reboot and how Home
  Assistant reads it.

### Must Not

- State, in documentation or a decision record, that the persisted total never
  decreases.

### Exceptions

- None.

### Verification

- `docs/api/MQTT.md`, DHW Water Total, Persistence: the unclean-reboot bullet.

## Alternatives Considered

- **Leave ADR-176 as it is.** The real behaviour is documented in
  `docs/api/MQTT.md`, but a reader who follows the decision record would take the
  wrong reasons as settled.
- **Write the file on every publish so the counter never drops.** That is the flash
  wear ADR-176 rules out: one of its Must Not items forbids writing the persisted
  counter to flash on every publish.

## Consequences

**Positive:**

- The decision record matches the firmware and the API documentation again: a drop
  of under 10 L plus up to one minute of flow, read as a dip at 90% or more of the
  previous value and as a reset below it.

**Negative:**

- None. Nothing in the firmware changes, so 0 lines of code move.

## Open Questions

- None.

## Related Decisions

- **ADR-176 (Publish a firmware-integrated cumulative DHW water total for the Home
  Assistant Energy dashboard)**: this ADR amends its answered question on partial
  regression; the decision itself stands.

## References

- `docs/adr/ADR-176-publish-a-firmware-integrated-cumulative-dhw-water-total-for-the-home-assistant-energy-dashboard.md`,
  line 266 (the answered question this ADR amends).
- `docs/api/MQTT.md`, DHW Water Total, Persistence.
- `src/OTGW-firmware/dhwWaterMeter.ino`: `DHW_METER_SAVE_DELTA_L`,
  `dhwWaterMeterSaveDue()` and `loadDHWWaterMeter()`.
- The 1.x line's `dhwWaterMeter.ino` header ("The total lives in RAM only").
- Home Assistant `reset_detected()` in `homeassistant/components/sensor/recorder.py`:
  <https://github.com/home-assistant/core/blob/dev/homeassistant/components/sensor/recorder.py>
- TASK-1123 notes, point 1, and TASK-1191 (this record).
