---
id: TASK-1900
title: >-
  SAT simulation must not save the PID state and the heating-curve
  recommendation files
status: To Do
assignee: []
created_date: '2026-11-13T16:21:00Z'
labels:
  - sat
  - simulation
dependencies: []
priority: medium
ordinal: 968000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in d3's read-only analysis for TASK-1895 (workflow wf_a2aa95ff-0f6, trace + independent verifier); line numbers at origin/dev 1c7757419. While settings.sat.bSimulation is on, the 5-minute PID save (SATcontrol.ino:7829-7831, satSavePidState at :3660, /sat_pid_state.json) and the HCR day-commit (SATcycles.ino:1680, satHCRSaveState at :1536, /sat/sat_hcr.json) still write to LittleFS, with no simulation guard. After a sim run the next boot restores a PID integral and a curve recommendation learned from simulated rooms. Same class as TASK-1869 (thermal coefficient: no save during sim, stash and restore the real RAM state); TASK-1891 (sim cool rate) is a different issue. The curve self-tune file is already guarded (SATcontrol.ino:6345-6353, 6481-6484). Fix in the TASK-1869 pattern: no save while simulating; decide per learner whether the real RAM state is stashed on entering simulation and restored on leaving it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: in SAT simulation neither satSavePidState() nor satHCRSaveState() writes its file; red on the current code, green on the fix
- [ ] #2 Outside simulation both still save as before (regression check in the same test)
- [ ] #3 After a simulation run the real PID integral and the HCR day data are what they were before the run (stash/restore), or the task notes say why a learner does not need it
<!-- AC:END -->
