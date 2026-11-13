---
id: TASK-1869
title: >-
  SAT thermal model: a SAT simulation run writes the learned thermal coefficient
  to settings (satthermalcoeff)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-12T14:38:00Z'
updated_date: '2026-11-13T08:27:00Z'
labels:
  - sat
  - bug
dependencies: []
priority: medium
ordinal: 945000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-firmware-09 during the TASK-1817 bench on .61 (beta.216+0a10492, 2026-11-12): with satsimulation on, the persisted satthermalcoeff went from 0.0966 to 0.0998; 09 restored it. Code: the thermal learning block saves the EMA every hour through updateSetting("SATthermalcoeff") (SATcontrol.ino:6678-6683) with no satsimulation guard in that block (read 6625-6690). The self-tune already stops writing in simulation (window file not written, tuner restarts on the sim switch). A sim run should not change the real house parameters. Not decided whether thermal learning should pause in sim or learn in RAM only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: in SAT simulation the thermal learning does not call updateSetting(SATthermalcoeff); red on the current code, green on the fix
- [x] #2 Real (non-sim) learning still saves every hour, unchanged
- [x] #3 Bench .61: after a SAT sim run satthermalcoeff in /api/v2/settings is unchanged
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Skip the hourly SATthermalcoeff save while satsimulation is on. Also stash the real model (EMA, learning time, valid flag) when the simulation starts and restore it when it ends: without that, the EMA learned in simulation would be saved by the next real save. Tuning logic (sample, EMA, sanity range) unchanged. Built ourselves: a few lines, project-specific.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented on lane-1869 (uncommitted, build number 220 after 95's 217-219 and ba's 218): satUpdateThermalLearning() (SATcontrol.ino) stashes _thermal_coeffEma, _thermal_totalLearnMs and bThermalModelValid on the switch into simulation and restores them on the switch out; a decay period never spans the switch; the hourly save is skipped while satsimulation is on. New tests/test_sat_thermal_sim_persist.py compiles the real constants, statics and function. Fix: 2/2 OK. Old code (HEAD 94ddd22c8): 2 FAIL, sim run writes 3x (stored 0.0966 -> 0.1586), and the real run after it stores 0.1600 instead of 0.1554. Mutant with only the save guard (no restore): test 2 FAIL (0.1599 vs 0.1554), so the restore is needed. Two test-scenario artifacts fixed on the way (a room jump inside a decay period; the hourly save counted from millis()). evaluate --quick 0 failed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A SAT simulation run no longer writes the learned thermal coefficient. satUpdateThermalLearning() skips the hourly save while satsimulation is on, and the real EMA, learn time and model-valid flag are stashed on entering simulation and restored on leaving it. Shipped in 2.0.0-beta.220 (db2a92118, pushed with dcfad3fe1).
AC1/AC2: tests/test_sat_thermal_sim_persist.py compiles the real function: old code fails 2/2 (3 sim writes 0.0966->0.1586; the real run afterwards stores 0.1600 vs 0.1554), the fix passes 2/2; a mutant without the restore fails test 2. Rerun green after each rebase. evaluate --quick 86 passed, 0 failed.
AC3: bench by otgw-bench-tester (09) on .61, fw 2.0.0-beta.220+dcfad3f: bootcount +2, crashlog empty, settings diff none; satthermalcoeff 0.0966 unchanged through the sim phase with 3 accepted samples (uptime 1:17, 1:23, 1:28) that each reached the hourly save branch, no 'coeff updated' line on telnet. Evidence in 09's scratchpad b220. Honest note: the old-code bench reference is beta.216 (0.0966->0.0998, TASK-1817), not re-run on 219 with this recipe; the host test is the red/green proof.
Build: esp32-combo, 2x 'Successfully created ESP32S3 image', 'Build completed successfully!'. Posted to #alpha-testing (message 1570716381908041769). Follow-up from 09's bench: the sim's default cool rate cannot exercise thermal learning (TASK filed).
<!-- SECTION:FINAL_SUMMARY:END -->
