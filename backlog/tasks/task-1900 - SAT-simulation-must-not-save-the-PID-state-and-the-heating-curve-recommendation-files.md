---
id: TASK-1900
title: >-
  SAT simulation must not save the PID state and the heating-curve
  recommendation files
status: Done
assignee:
  - '@claude'
created_date: '2026-11-13T16:21:00Z'
updated_date: '2026-11-15T05:33:00Z'
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
- [x] #1 Host test: in SAT simulation neither satSavePidState() nor satHCRSaveState() writes its file; red on the current code, green on the fix
- [x] #2 Outside simulation both still save as before (regression check in the same test)
- [x] #3 After a simulation run the real PID integral and the HCR day data are what they were before the run (stash/restore), or the task notes say why a learner does not need it
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix (d3, lane-1924): satSavePidState() (SATcontrol.ino) and satHCRSaveState() (SATcycles.ino) return at once while settings.sat.bSimulation is on, the TASK-1869 rule. The guard sits in the savers, not only at the two call sites named in the description, so it covers every caller: the 5-minute PID save, the HCR day commit, satPersistAllToFs() (forced write before a filesystem update), satDisable(persistPid) and the sat/flush command. During a filesystem update in simulation the update server's KEEP_LIST copy carries the real files over, which is the wanted result. No control change: only file writes are skipped.

AC3 NOT done, reason: the real PID integral and HCR day data stay correct ON FLASH (the files are not written during the run), so the next boot restores the real state. The RAM state after a run is still the simulated one until a reboot, and once simulation is off the 5-minute save would write it. Restoring RAM at the end of a run is more than the TASK-1869 stash: the PID restore at boot runs through the restart state machine (satRestartNotePidLoad / satRestartStep), and satHCRLoadState() is boot-only (it does not reset _hcr_lastDayNum or the recommendation text), and a RAM stash of the HCR intra-day ring is 1440 floats (5.7 KB). The open part goes to a follow-up task, see below.

AC1/AC2 evidence: new tests/test_sat_sim_no_learner_save.py compiles the real satSavePidState() and satHCRSaveState(): fix OK, 'sim=1 pid=0 hcr=0', 'sim=0 pid=1 hcr=1'; old (HEAD 690b47ec8 copies) FAILED, 'sim=1 pid=1 hcr=1'. Updated on purpose: tests/test_fs_update_keep.py (test_savers_write_unconditionally now allows exactly the simulation guard, with the reason) and tests/test_sat_pid_cadence.py (bSimulation in the settings stub). Full SAT suite: 93 files OK in foreground chunks; test_sat_lowload_contract.py separately: 'Ran 35 tests in 809.970s, OK (skipped=1, expected failures=2)', missing LL ids none. evaluate.py --quick 0 failed. AC3 open: follow-up TASK-1952.

Lane-1924 pushed 2026-11-15: origin/dev 5da0c91df..2f4818aa1. Build esp32-combo 2.0.0-beta.281+fe0e61a OK (build/logs/build-20261115-020302.log, 'Build completed successfully'); 'git diff fe0e61a48 2f4818aa1 -- src/' empty, so the pushed code is the benched code. evaluate.py --quick 0 failed; OTD tests 42 files, host suites, SAT suite per file green on the rebased tree. Bench 09 on .61 with beta.281: SAT sim on 7 min: /sat_pid_state.json and /sat/sat_hcr.json hash unchanged; after sim off the PID file rewritten within 6.7 min. PASS for AC1/AC2. AC3 -> TASK-1952. Version beta.281.

AC3 delivered by TASK-1952 (beta.295, bench 09 PASS): at the end of a run the real PID state and HCR data are reloaded from the untouched files.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT simulation no longer writes /sat_pid_state.json or /sat/sat_hcr.json (guards in the savers, every caller covered); TASK-1952 puts the real RAM state back when a run ends. AC1/AC2: tests/test_sat_sim_no_learner_save.py red/green, bench 09 beta.281 PASS (hashes unchanged in 7 min of simulation, PID file rewritten after). AC3: TASK-1952.
<!-- SECTION:FINAL_SUMMARY:END -->
