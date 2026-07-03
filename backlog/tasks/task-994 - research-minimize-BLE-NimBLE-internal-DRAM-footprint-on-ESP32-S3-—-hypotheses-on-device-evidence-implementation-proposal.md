---
id: TASK-994
title: >-
  research: minimize BLE (NimBLE) internal-DRAM footprint on ESP32-S3 —
  hypotheses, on-device evidence, implementation proposal
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-03 07:49'
updated_date: '2026-07-03 07:49'
labels: []
dependencies: []
ordinal: 206000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
BLE-on-by-default costs ~64K internal DRAM (24K free with BLE vs 88K without, measured 2026-07-03 under TASK-989). Research NimBLE-Arduino memory tuning, controller-level knobs, PSRAM alloc mode, and alternative minimal-host approaches; form 3-8 hypotheses with pros/cons; adversarial review of test plan; empirically test viable hypotheses on the bench S3; deliver an implementation proposal with alternatives for maintainer decision.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 3-8 hypotheses documented with pros/cons
- [ ] #2 Test plan adversarially reviewed before execution
- [ ] #3 Each executed hypothesis has on-device heap + ramp + BLE-functional evidence
- [ ] #4 Implementation proposal written with alternatives and trade-offs
- [ ] #5 Maintainer decision recorded (yes/no per option)
<!-- AC:END -->
