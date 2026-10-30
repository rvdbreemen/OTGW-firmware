---
id: TASK-1418
title: >-
  SAT PID derivative: no-change test == 0 and last temperature updated every
  update, as pid.py (entry 77, Q27)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-28T06:46:00Z'
updated_date: '2026-10-30T05:50:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1368
  - TASK-1417
references:
  - docs/sat/sergeant-d-corrections.md
documentation:
  - docs/sat/sat-low-load-execution-backlog-2026-10-28.md
priority: medium
ordinal: 582000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-27 (entry 77, Q27: match thermo-nova). Replace the derivative reference-temperature logic (SATpid.ino ~49-56, ~226-270) with a last temperature overwritten on every PID update (pid.py:171, :211) and an exact zero no-change test. After TASK-1368. Bench old vs new with the room EMA; report to Sergeant D if D becomes near-dead.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test reproduces pid.py derivative on a recorded room series (old differs, new matches)
- [ ] #2 Bench old-vs-new D magnitude reported
- [ ] #3 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 Robert requested this execution-graph update. Preserve the earlier reconciliation. Pending PID writers run 1368 -> 1270 -> 1416 -> 1417 -> 1418 -> 1414 -> 1423 -> 1415; 1381 remains a separate branch after 1270 and Q53. Revalidate task premises against preceding commits. Integration also requires the DHW and auto-tune outcomes through their linked verification gates. Approval and bench gates remain in force.

2026-10-30 (otgw-firmware-84, wt-1416; after the rebase onto origin/dev c00c5f025 the commit is 46b83541f on top of TASK-1417 af90462b0; no bump). _pid_derivRefTemp removed: the delta is taken against _pid_lastRoomTemp (overwritten at the end of every satPidStep, as pid.py:171); no-change test tempDelta == 0.0f (pid.py:211). pid.py:199-232 opened at the pin. AC#1 evidence: tests/test_sat_pid_derivative.py runs the real satPidStep/satPidNoteRoom + real satRoomFiltered EMA against a line-by-line transcription of pid.py _update_derivative (oracle) on synthetic series (not a recording): fix 4/4 OK, all 181 steps match; origin/dev FAILED (failures=3), first difference t=720 s 3.10e-4 vs 9.70e-5; step-wise sensor without EMA agrees on both. AC#2 (bench) NOT done, host measurement only: mean |raw derivative| through the EMA 6.97e-5 C/s new vs 1.87e-4 old (factor 2.68; true slope 1.67e-4); at curve 40 (kd ~12544) D drops from ~2.3 to ~0.9 C. Not near-dead, but clearly weaker: per entry 77 to report to Sergeant D with the data (coordinator queues). Best-control note: with a filtered (EMA) input pid.py's rule under-reads the true slope (~42%), because at a 60 s PID step every other derivative step is skipped while the previous temperature moves on; the former separate reference tracked the true slope. Behaviour follows entry 77 as decided.

Bench 2026-10-30 (coordinator otgw-firmware-b8, Classic-S3 .63, PIC 6.8): batch rebuilt after the 05:50 PC restart; esp32-combo alpha.547+6240b05 and alpha.548+8ded885 built (artifacts 06:12 / 06:42, githash = HEAD), evaluate --quick 79/0/0 on both; OTA app via safeboot + FS via /update?cmd=100, 174 settings keys kept. TASK-1418 (alpha.547): SAT simulation sample 06:27-06:32: rd 0 inside the 0.1 deadband, rd 0.000238 C/s and D 2.33 at error 0.128. AC#2 (old vs new D on the bench) NOT done: the bench unit was already on the new app; only the host measurement exists (D ~2.3 -> ~0.9 at curve 40). Still to report to Sergeant D per entry 77.
<!-- SECTION:NOTES:END -->
