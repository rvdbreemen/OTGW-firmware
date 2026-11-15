---
id: TASK-1952
title: >-
  SAT simulation: put the real PID and heating-curve recommendation RAM state
  back when a simulation run ends
status: Done
assignee:
  - '@claude'
created_date: '2026-11-14T23:22:00Z'
updated_date: '2026-11-15T05:32:00Z'
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
- [x] #1 After a simulation run the real PID state and HCR data in RAM equal those before the run; host test on the real code, red before, green after
- [x] #2 The chosen method (stash or reload) is named with its RAM cost
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
DESIGN (d3, 2026-11-15). Chosen: mirror the boot restore. PID: on entering simulation stash the held integral and raw derivative (satPidGetPersistState); on leaving it call satPidReset() and then satPidRestoreState(integral, rawDerivative), the boot path (SATpid.ino:94): the next step initialises timestamps and the last room temperature from live data. Not chosen: restoring the last error, last room temperature and derivative time too (satPidRestoreLast is boot-only by design, '_pid_roomSeen' gate SATpid.ino:137, and a room temperature from before the run against the live one after it would give a derivative step). HCR: the file is not written during the run (TASK-1900), so on leaving simulation reload it (satHCRLoadState) and recompute the recommendation text (satHeatingCurveRecommendation); RAM cost 0 (no 5.7 KB stash of the 1440-sample ring). Transition detection on the loop task like TASK-1869 (satUpdateThermalLearning, SATcontrol.ino ~7059), placed where it runs every pass also while SAT is off. Firmware-only feature (no thermo-nova counterpart), so no Sergeant D question (ADR-196 exception); state handling only, no control rule changes.

Implementation changed from the stash in the design note to a reload from the files, for one reason: a unit that boots with simulation on takes the stash before the deferred PID file restore (after NTP), so a stash would hold zeros. The files are untouched during a run (TASK-1900), so they ARE the pre-run real state. satSimEndRestore() (SATcontrol.ino, markers), called every satControlLoopRun() pass right after satCurveTuneService(), before every early return (satControlLoop runs also while SAT is off): on the flag going on->off it calls satPidReset(), satLoadPidState() (the boot path: integral + raw derivative warm start; satPidRestoreLast stays boot-gated and so the derivative timestamps re-initialise from live data), satHCRLoadState() and satHeatingCurveRecommendation(). RAM cost 0 (AC2). Limits: the PID file is the last save before the run, up to 5 min older than the RAM state at the start (same as a reboot); without a set clock satLoadPidState() returns WAIT_NTP and the PID cold-starts.

Evidence AC1: new tests/test_sat_sim_end_restore.py on the real satSimEndRestore(): on->off reloads once in boot order (reset, loadpid, loadhcr, rec), entering a run or never simulating reloads nothing, a unit booted in simulation reloads when it ends, two runs reload twice, and it sits before the first early return of satControlLoopRun (5/5 OK). On the code before (HEAD 147324f96) the function does not exist: red by construction. That the files hold the pre-run state is TASK-1900's tests/test_sat_sim_no_learner_save.py; that satLoadPidState() restores them is tests/test_sat_pid_persist.py (both OK). Harnesses cutting satControlLoopRun: 27 files OK; test_sat_curve_selftune needed the call after satCurveTuneService() (its source check), done. evaluate.py --quick 0 failed. Not benched (needs a sim run + file comparison on .61: suggested for the next bench).

Behaviour proof added (coordinator review): class SimEndRestoreBehaviour in tests/test_sat_sim_end_restore.py compiles the real PID persistence and PID state code (pieces of test_sat_pid_persist, real files) and the real head of satControlLoopRun() up to satRetryRelease() (present in old and new code): save I=1.25 RD=0.02, simulation on, the run moves them to 7.77/0.5, simulation off. Fix: 'I=1.2500 RD=0.0200 HCR=1 REC=1' OK. Old (SATcontrol.ino at 147324f96): FAILED, 'I=7.7700 RD=0.5000 HCR=0 REC=0' (fails on the values). Whole file 6/6 OK; test_sat_pid_persist OK.

Build esp32-combo 2.0.0-beta.295+58332e4 OK (wrapper exit 0, 05:42; ino sha256 6b11f926ffd7..., littlefs 4480311997ab...). Bench 09 on .61 (via coordinator): PASS incl. step 5 (boot with simulation on): reload line seen, file i=1.2345 -> status PID I 1.235 after the run, files untouched during the simulation, 4x simulation on/off without a crash. Field only: 'the next 5-min save keeps the real value' (on the bench SAT goes inactive after the no-boiler hand-back, so no periodic save runs).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
When a SAT simulation run ends, satSimEndRestore() reloads the real PID state (satPidReset + satLoadPidState, the boot path) and the heating-curve data (satHCRLoadState + recommendation) from the files, which TASK-1900 keeps untouched during the run; RAM cost 0; also right for a unit that boots with simulation on. AC1: tests/test_sat_sim_end_restore.py, behaviour class red on old (I=7.7700) and green on the fix (I=1.2500); bench 09 beta.295 PASS. AC2: method and RAM cost named. Field: the next periodic save after a run.
<!-- SECTION:FINAL_SUMMARY:END -->
