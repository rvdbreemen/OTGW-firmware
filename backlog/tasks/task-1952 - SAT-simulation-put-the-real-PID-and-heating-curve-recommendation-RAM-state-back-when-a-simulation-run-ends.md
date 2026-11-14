---
id: TASK-1952
title: >-
  SAT simulation: put the real PID and heating-curve recommendation RAM state
  back when a simulation run ends
status: To Do
assignee: []
created_date: '2026-11-14T23:22:00Z'
labels:
  - sat
  - simulation
dependencies: []
priority: low
ordinal: 998000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1900 AC3 (d3, lane-1924). Since TASK-1900 satSavePidState() and satHCRSaveState() write nothing while settings.sat.bSimulation is on, so the files keep the real state and the next boot restores it. The RAM state after a run is still the simulated one: the PID integral/raw derivative/last error/last temperature (SATpid.ino satPidGetPersistState/satPidGetPersistLast) and the HCR ring (_hcr_dailyMedian, _hcr_head, _hcr_count, the 1440-sample intra-day ring, _hcr_lastDayNum, state.sat.sHeatCurveRec). Once simulation is off the 5-minute PID save and the next HCR day commit write that simulated state to flash. TASK-1869 solved the same for the thermal coefficient with a RAM stash on entry and a restore on exit in the loop. Options: stash/restore in RAM (PID: a few values, setters satPidRestoreState/satPidRestoreLast exist; HCR: 5.7 KB for the intra-day ring), or reload both files on exit (the PID boot restore runs through the restart state machine satRestartNotePidLoad/satRestartStep; satHCRLoadState() is boot-only and does not reset _hcr_lastDayNum or the recommendation text).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After a simulation run the real PID state and HCR data in RAM equal those before the run; host test on the real code, red before, green after
- [ ] #2 The chosen method (stash or reload) is named with its RAM cost
<!-- AC:END -->
