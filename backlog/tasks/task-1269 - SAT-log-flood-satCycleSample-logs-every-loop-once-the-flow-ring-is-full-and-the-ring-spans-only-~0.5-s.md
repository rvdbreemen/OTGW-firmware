---
id: TASK-1269
title: >-
  SAT log flood: satCycleSample logs every loop once the flow ring is full, and
  the ring spans only ~0.5 s
status: To Do
assignee: []
created_date: '2026-10-20 18:16'
updated_date: '2026-10-20 19:30'
labels:
  - bug
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 399000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
First real SAT field run (gruijter, Intergas HReco 30 + Honeywell Round + PIC Classic, alpha.439/441, #alpha-testing 2026-10-20), captures in %TEMP%\discord-1020. SATcycles.ino ~862-869: once _flow_sampleCount saturates at SAT_FLOW_SAMPLE_SIZE (256), (count & 0x0F)==0 is always true, so 'SAT sample: flow=' is logged on every loop: 106,674 lines in 13 min, which garbled the whole telnet capture. Sampling runs every loop (~2 ms), so the 256-entry ring read for p90/p10 covers ~0.5 s of flow instead of a cycle.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sampling runs at a fixed cadence (e.g. 1 Hz) so the ring covers the intended window; the debug line is rate-limited (telnet line count over 5 min before vs after)
- [ ] #2 Cycle classifier p90/p10 still computed (test)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-20 cycle: skipped for now; changes SAT behaviour (control/classifier), so it needs Sergeant D's OK per the step 4 rule.
<!-- SECTION:NOTES:END -->
