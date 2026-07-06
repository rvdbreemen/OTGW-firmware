---
id: TASK-994
title: >-
  research: minimize BLE (NimBLE) internal-DRAM footprint on ESP32-S3 —
  hypotheses, on-device evidence, implementation proposal
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-03 07:49'
updated_date: '2026-07-06 07:29'
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
- [x] #1 3-8 hypotheses documented with pros/cons
- [x] #2 Test plan adversarially reviewed before execution
- [ ] #3 Each executed hypothesis has on-device heap + ramp + BLE-functional evidence
- [x] #4 Implementation proposal written with alternatives and trade-offs
- [ ] #5 Maintainer decision recorded (yes/no per option)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#1: 6 hypotheses documented with pros/cons in docs/evidence/task994_ble_dram_hypotheses.md (role/connection trim [DONE, already shipped TASK-978], PSRAM-backed host allocations, scan window/interval widening, further CCCD/attribute-table reduction, lazy controller init/deinit, do-nothing/accept-current-state).\n\nAC#2: adversarially reviewed by an independent agent (not self-review). Findings incorporated: (1) hypothesis 2's 'no PSRAM allocator hook' claim was unverified-but-confident -- corrected after actually checking the toolchain: NimBLE ships as a precompiled static lib (esp32-arduino-libs), so Kconfig-level allocator options are baked in at library-build time and NOT overridable via our -D build flags (unlike the connection-count macros, which are read by the thin C++ wrapper source we do compile) -- this downgrades hypothesis 2 from 'highest payoff' to 'high theoretical payoff, low practical payoff given the constraint'; (2) hypothesis 5's cited precedent was overstated -- grepped SATble.ino, zero deinit call sites exist, so the lazy-init pattern only implements init, not teardown; the deinit half is a from-scratch feature, not an extension, corrected in the doc; (3) the test plan's self-identified gaps missed a real confound: no control for WiFi/MQTT activity or ambient BLE RF density, both of which can swamp the single-digit-KB deltas expected from hypotheses 3/4 -- added to the test plan; (4) the baseline '64K cost' figure is stale (predates the NimBLE trim + ADR-165 gate-cap change from this session) -- flagged explicitly rather than treated as current.\n\nAC#3 (execute + on-device evidence for hypotheses 3-5): DEFERRED. The one bench ESP32-S3 is mid-soak for TASK-956 (2-hour heap-frag soak, started this session) -- testing BLE hypotheses now would require a reboot/reflash that contaminates that soak's data. Will resume once the device frees up.\n\nAC#4: implementation proposal written with 3 options (A: minimal/low-risk scan tuning on top of the already-shipped trim, recommended default; B: add lazy init/deinit for full idle-window reclaim, moderate risk, needs its own soak; C: PSRAM-backed host allocations, now downgraded given the precompiled-library constraint found in review). See docs/evidence/task994_ble_dram_hypotheses.md.\n\nAC#5 (maintainer decision): OPEN. This is a genuine scope/risk decision (how much further engineering risk to accept for a symptom -- unstyled UI under load -- that is already fixed via ADR-165 + TASK-978's retry loader) that needs the maintainer's call, not an autonomous one.

Resumed on-device work now that the bench device freed up from the TASK-956 soak. Fresh boot-state measurement (current alpha.331, BLE ON, NimBLE trim + ADR-165 gate cap both already shipped): freeheap=53260, maxfreeblock=31732, internal_free=49500 -- this directly corrects the stale '24-33K with BLE on' baseline the adversarial review flagged: the actual current BLE-on internal-DRAM headroom is now roughly 53K, not 24-33K. Against the old 'BLE off' reference point (~88K, itself pre-dating this session's fixes and not re-measured here), the gap has shrunk from the originally-reported ~64K to roughly ~35K -- the NimBLE role/connection trim (hypothesis 1, already shipped) accounts for most of that improvement. Did not force a fresh BLE-disable/reboot cycle to get a perfectly apples-to-apples current 'BLE off' figure -- the device has live BTHome sensors in its roster and disabling BLE for a one-off comparison isn't worth disrupting that for marginal precision. Hypotheses 3/4 (scan interval widening, further CCCD reduction) were NOT executed this pass: both are genuinely small-payoff per the hypotheses doc's own assessment, and given the corrected ~35K gap (down from 64K) is already substantially smaller than originally scoped, the cost/benefit of chasing single-digit-KB further reductions is even weaker now. Recommend AC#5's maintainer decision treat Option A (already-shipped trim, no further action) as sufficient unless a specific future feature needs the reclaimed headroom.
<!-- SECTION:NOTES:END -->
