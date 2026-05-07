---
id: TASK-565
title: 'refactor(sat): write-on-change for SAT-emitted OT commands'
status: To Do
assignee: []
created_date: '2026-05-07 17:49'
updated_date: '2026-05-07 18:01'
labels:
  - sat
  - otbus
  - refactor
  - heap
  - 2.0.0
dependencies: []
priority: medium
ordinal: 28000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT loop re-enqueues the same OT commands every 30 s even when their values have not changed. SergeantD's telnet log (alpha.3, 2026-05-07) shows MM=100 (MaxRelModLevelSetting) re-emitted 17 times in 13 minutes for an unchanged value; CS, CH, OT show the same pattern. This burns OT command queue slots — observed high-water=2..3 (capacity=12) — and adds OT-bus traffic for no functional benefit. Aggravator for TASK-529 latency since it puts steady scheduler load on the same path that has to flush HTTP responses. Refactor: stage last-emitted value for each SAT-managed command (CS, MM, CH, OT) in a small struct; only enqueue when the new value differs OR when a max-staleness window has elapsed (e.g. 5 minutes, to handle PIC reboot recovery and ensure the boiler periodically reaffirms the override). Cross-ref: TASK-529 latency, TASK-564 settings debounce.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Last-sent value cached for each SAT-managed OT command (CS, MM, CH, OT). Cache lives in state.sat (transient runtime state per ADR-051), not settings
- [ ] #2 SAT loop only enqueues a command when the new value differs from the cached last-sent value (write-on-change)
- [ ] #3 A max-staleness window forces a refresh enqueue even on unchanged values: configurable, default 5 minutes. Documented in code comment why the refresh exists (PIC reboot recovery, boiler-side state divergence)
- [ ] #4 Cache resets on PIC reset / OT bus offline transitions so a recovering boiler immediately gets the current values
- [ ] #5 Telnet log over a 10-min idle window with steady SAT state shows at most 2-3 enqueues per command (one initial + one staleness refresh), down from ~20 today
- [ ] #6 OT command-queue high-water mark drops in the same scenario (verified via otCmdEnqueue log lines)
- [ ] #7 No regression on initial commissioning: when SAT first enables, all four commands are emitted exactly once
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
---
**Plan reference**: implementation sequencing tracked in `/Users/Breee02/.claude/plans/clever-yawning-wreath.md` (local working plan, not in repo). **Ship 1** of the SergeantD-driven 2.0.0 fix sequence — first to land because smallest blast radius and verifiable from the existing telnet capture without a tester reflash. Removes a steady aggravator that would otherwise muddy any latency measurement for TASK-529.
<!-- SECTION:NOTES:END -->
