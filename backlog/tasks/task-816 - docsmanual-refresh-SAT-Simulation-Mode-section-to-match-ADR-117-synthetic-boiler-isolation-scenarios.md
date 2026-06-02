---
id: TASK-816
title: >-
  docs(manual): refresh SAT Simulation Mode section to match ADR-117 (synthetic
  boiler, isolation, scenarios)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-02 22:32'
updated_date: '2026-06-02 22:32'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Manual SAT Simulation sections (en ch05 5.8, nl h05 5.7) only describe the pre-TASK-795 room-temperature model (fSimHeatRate/fSimCoolRate). They omit the current synthetic boiler model (flame state machine, modulation, return temp), bus-tx isolation + boiler-absence requirement, diurnal outdoor-temperature curve (TASK-796), scenario injection (TASK-797), and command trace. Bring both language manuals in line with docs/adr/ADR-117-sat-simulation-contract.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 EN 5.8 and NL 5.7 describe the synthetic boiler model (flame/modulation/return), bus isolation + auto-disable when a real boiler is detected, diurnal outdoor curve, scenario injection, and command trace
- [ ] #2 Both sections cross-reference ADR-117
- [ ] #3 Docs-only; no firmware/version change
<!-- AC:END -->
