---
id: "ADR-243"
title: "OT-Direct master mode keeps the thermostat interface on by default (bEnableSlave true, amends ADR-066)"
status: "Accepted"
date: "2026-11-15"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-241"
topics:
  - "otdirect"
  - "master-mode"
  - "settings-defaults"
  - "pic-parity"
  - "thermostat"
aliases:
  - "bEnableSlave default"
  - "otdenableslave"
  - "TASK-1953"
components:
  - "OT-Direct master-mode slave interface default"
symbols:
  - "bEnableSlave"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-243 OT-Direct master mode keeps the thermostat interface on by default (bEnableSlave true, amends ADR-066)

## Status

Accepted, 2026-11-15.

## Status History

```yaml
status_history:
  - date: 2026-11-15
    status: Proposed
    changed_by: Agent (d3)
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-15
    status: Proposed
    changed_by: "Agent (d3)"
    reason: Related to ADR-241
    changed_via: adr-kit lifecycle
  - date: 2026-11-15
    status: Accepted
    changed_by: "Robert van den Breemen (decision, DM 1571432519373557801), recorded by Agent (d3)"
    reason: "Accepted: decision Robert van den Breemen (DM 1571432519373557801, answer A), recorded by Agent (d3); all four gates passed"
    changed_via: adr-kit lifecycle
```

## Context

ADR-066 (Accepted) describes OT-Direct master mode, where the ESP32-S3 drives the boiler itself and, with
`bEnableSlave` on, answers a thermostat on the slave port as a virtual boiler. Its text says
"`bEnableSlave = false` (default): Pure standalone master"
(`docs/adr/ADR-066-thermostat-auto-detection-master-mode.md:71`). The code has always had the opposite default:
`bool bEnableSlave = true;` (`src/OTGW-firmware/OTDirecttypes.h:114`, unchanged since the types were extracted in
9ae6f1e6c, 2026-04-19), and the settings help says "Default on" (`src/OTGW-firmware/settingsHelp.h:90`). The
mismatch was found during the TASK-1812 design (TASK-1953). Accepted ADRs are immutable, so the record is
corrected here instead of in ADR-066.

The project rule for OT-Direct is to emulate the PIC (the gateway's PIC microcontroller) closely. The PIC always
serves the thermostat side of the bus; it has no setting that leaves a connected thermostat unanswered.

## Decision

The default of `bEnableSlave` is `true`, as the code already has it: in master mode the slave interface is on
unless the user turns it off; this amends only the default stated in ADR-066, and the rest of ADR-066 stands.

Robert decided this (option A) by Discord DM 1571432519373557801 on 2026-11-15, answering question
1571432372916850780 ("bEnableSlave: keep the code default true. The PIC always serves the thermostat side, so
true is the PIC-like value; only the ADR-066 text (which says false) is wrong").

## Decision Contract

### Must

- `OTDirectSettings::bEnableSlave` defaults to `true` (`src/OTGW-firmware/OTDirecttypes.h:114`).
- The settings help for `otdenableslave` states the default as on (`src/OTGW-firmware/settingsHelp.h:90`).
- A stored `OTDenableslave` value in the settings file keeps winning over the default.

### Must Not

- Must not change a stored user value through a migration: there is nothing to migrate, the code is unchanged.

### Exceptions

- None.

### Verification

- Source anchors `src/OTGW-firmware/OTDirecttypes.h:114` and `src/OTGW-firmware/settingsHelp.h:90` agree with
  this ADR (checked 2026-11-15). Guideline-level per ADR-080: no CI gate, reviewed at PR.

## Alternatives Considered

- **Change the code default to false, as ADR-066 says.** Rejected: a master-mode unit with a thermostat
  connected would stop answering it after an update on every unit without a stored value, which is a silent
  behaviour change, and it moves away from the PIC.
- **Leave the mismatch.** Rejected: ADR-066 would keep telling readers and agents the wrong default, and the
  TASK-1812 write relay (ADR-241) depends on the slave interface being on.

## Consequences

**Positive:**

- Code, settings help and the ADR record agree; no firmware change and no settings migration.
- A thermostat on the slave port of a master-mode unit is answered out of the box, like on the PIC.

**Negative:**

- A user who wants a pure standalone master must turn the setting off. Mitigation: the setting stays in the
  web UI and the settings help names the default.

## Open Questions

- [x] Robert: keep the code default true or change it to false? Answered A (keep true) by Discord DM
  1571432519373557801, 2026-11-15.

## Related Decisions

- **ADR-066 (Thermostat auto-detection and master mode)**: amended; only the stated default of `bEnableSlave`
  changes.
- **ADR-241 (master mode relays the thermostat's writes)**: relies on the slave interface being on.

## References

- TASK-1953 (this decision), TASK-1812 (where the mismatch was found).
- `src/OTGW-firmware/OTDirecttypes.h:114`, `src/OTGW-firmware/settingsHelp.h:90`,
  `docs/adr/ADR-066-thermostat-auto-detection-master-mode.md:71`.
