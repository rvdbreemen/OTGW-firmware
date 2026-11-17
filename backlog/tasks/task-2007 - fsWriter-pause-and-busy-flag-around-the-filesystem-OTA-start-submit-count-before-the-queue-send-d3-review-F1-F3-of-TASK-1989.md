---
id: TASK-2007
title: >-
  fsWriter: pause and busy flag around the filesystem-OTA start; submit count
  before the queue send (d3 review F1-F3 of TASK-1989)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-16T19:46:00Z'
updated_date: '2026-11-17T09:28:00Z'
labels:
  - fs
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1028000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Second-reader review by d3 of TASK-1989 (7e1ae2649, 2026-11-16, code reading). F1 (medium, pre-existing race, not a regression): the fs-OTA start in OTGW-ModUpdateServer-esp32.h runs fsWriterDrain(2000) -> _keepFiles() -> fsOtaRtcFill() -> LittleFS.end() -> state.flash.bFsImageWriting = true on the async task while the loop keeps submitting saves; a job submitted after the drain passes the writer guard (fsWriter.ino:59 waits only on bFsImageWriting || !LittleFSmounted, and LittleFSmounted stays true across end()). Effects: (a) a write newer than the _keepFiles heap copy is lost at the restore, (b) the writer can be inside open/write while end() unmounts. Before TASK-1989 the loop wrote without any fs guard, so the same race existed unguarded. Fix: a pause flag set before the drain (fsWriterSubmit then waits or refuses) and a busy flag around fsWriterService(); at the OTA start: pause, wait for !busy, run the queued jobs on the caller, then _keepFiles/end. F2 (low, new in 1989): s_fswSubmitted is incremented after platformQueueSend, so fsWriterPending() can read 0 between the send and the increment and a drain from another task returns early: count before the send, undo on failure. F3 (nit): a job over 256 B is cut silently: refuse and log.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real fsWriter: a submit during an OTA-start drain is not lost and never runs after LittleFS.end(); red on the current code, green on the fix
- [x] #2 Host test: fsWriterPending() never reads 0 while a submitted job is not yet written (count before the send)
- [x] #3 A job over 256 B is refused with a telnet line, not cut
- [x] #4 Bench by the bench-tester: fs OTA while SAT saves run (where a SAT tick is available), sat_*.json intact
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-16 (95, taken over with ba's OK; ba: nothing local). Fix in lane-95-2007 (worktree wt-95-2007, not pushed; build + bench tomorrow after d3). fsWriter.ino: F1 the writer task runs fsWriterStep() in a loop (queue read without waiting, 20 ms delay when idle or held); fsWriterStep() sets s_fswBusy, then checks s_fswPaused / bFsImageWriting / !LittleFSmounted, takes one job, writes it, clears busy; fsWriterPauseAndFlush(timeout) sets paused, waits until !busy (false on timeout, queue left as it is), then writes every queued job on the caller; fsWriterResume() clears paused. The flags and both counters are std::atomic (seq_cst): the writer stores busy then loads paused, the pauser stores paused then loads busy, so one always sees the other. OTGW-ModUpdateServer-esp32.h FS-OTA start: fsWriterPauseAndFlush(2000) (logs '[OTA] fswriter: still busy after 2 s' on timeout) -> _keepFiles() -> fsOtaRtcFill() -> LittleFS.end() -> bFsImageWriting = true -> fsWriterResume(); a save queued after the flush waits for the remount and lands after the restored copies. F2 s_fswSubmitted++ before platformQueueSend, -- when it fails (also around the queue-full drain, so that drain does not wait for this job). F3 a job over FSW_DATA_LEN (256 B) or a path of FSW_PATH_LEN (32) chars or more is refused with '[fswriter] <path> refused: <n> B data, ...', never cut (the path was also cut silently by strlcpy before). Heap: FSW_QUEUE_DEPTH 6 -> 3 (one slot per state file, ~870 B back); the heap telnet line (logHeapStats) gets fsw_stack_free=<bytes> via platformIpcStackHighWater("fswriter") (works for any task name); the 4096 B stack stays until field values exist.

Tests (tests/test_fs_writer.py, the real fsWriter.ino; OTGW_FSW_INO/OTGW_FSW_H point at another copy; the writer pass is now the real fsWriterStep() instead of a re-implemented task body): FIX 16/16 OK. OLD (origin fsWriter.ino/.h) 4 FAIL: F1(a) 'final {old}' (the save after the copy is lost at the restore), F1(b) 'after_end_writes 1' (the writer wrote in the gap between LittleFS.end() and bFsImageWriting = true), F2 'pending_during_send 0', F3 'submit 1' with the 300 B cut to 256 B; 2 skipped on old (the flush wait and timeout cases need the new API). New cases also cover the flush waiting for a busy writer and the 2 s timeout (returns false, queue kept). Known limit, kept on purpose: after that timeout the OTA logs and goes on, so a writer stuck over 2 s inside one job could still be in open/write at LittleFS.end(). Related tests via run_tests.py (15 files that read helperStuff.ino / the OTA header / fsWriter) all PASS; test_sat_pid_persist, test_sat_pid_cadence, test_sat_restart_quiet, test_sat_sim_no_learner_save, test_fs_update_keep OK; evaluate.py --quick 89/0. Not verified yet: the seq_cst handshake and <atomic> on the target (build tomorrow), AC4 bench.

2026-11-17 AC4 bench FAILED on beta.364+c8bc478 (09, .63, Classic + PIC, SAT enabled inactive, sim off; 09 scratchpad b364-63\): sat_pid_state.json ts 1794733218 before; fs-only OTA from 09:50:32, POST /api/v2/sat/flush at 09:50:37 (HTTP 200, during the upload); remount 09:50:38.338, '[OTA] Restoring /sat_pid_state.json (90 bytes): OK' at .623, deferred reboot .354 after; AFTER: ts still 1794733218, the flush's write lost. Files intact and parseable, no 'still busy' line, fsw_stack_free = 1352 B (heap line after the reboot). Cause (95, code): OTGW-ModUpdateServer-esp32.h _remountAfterFsWrite() clears bFsImageWriting at the remount, which releases the writer ~300 ms BEFORE _restoreRuntimeFiles() writes the heap copies back; the held save was written first and then overwritten by the restore (satPersistAllToFs() rewrites the PID file only when a room reading was seen, not on this bench). This ordering predates TASK-2007 (TASK-1989 design); the earlier 2007 note and comment 'a job queued after that lands after the restored copies' was wrong, and the first host test modelled the restore BEFORE the flag drop, so it could not see it. Fix: fsWriterPause() (pause without flush) in _remountAfterFsWrite() before the flag drops; fsWriterResume() after the restore in both callers (the end handler, after the whole mounted / no-version.hash / mount-failed branch, and the abort path). Host test: otaFinish() now runs in the real order (remount clears the flag, one writer pass, restore, resume): beta.364 writer logic 2 FAIL ('final {old}' in both OTA cases), fix 17/17; a wiring test checks pause before the flag drop and a resume after the restore for each of the two remount calls. Not changed: the error path at ~:503 (Update error, no remount, no restore) clears the flag without a pause; it has no restore to be overwritten by. Next: rebase on d3's 362, rebuild, rebench the same sat/flush case on .63.

AC4 bench PASS (09, .63, Classic + PIC, SAT enabled inactive, sim off), 2.0.0-beta.365+cc60bfd, 2026-11-17 10:18-10:22 (09 scratchpad b365\): smoke sha256 match, githash cc60bfd, 'changed: none', bootcount 194 -> 196 (OTA) -> 197 (fs OTA). Same reproduction as the failed 364 run: fs-only OTA, POST /api/v2/sat/flush during the upload (10:21:06.055, epoch 1794907266; End: success .367; post-remount .764; restore of sat_pid_state.json 10:21:07.037). AFTER the reboot sat_pid_state.json ts = 1794907266 = the flush epoch: the held save landed after the restored copy. OLD 364+c8bc478 (09:50): ts stayed 1794733218, the save lost. sat_energy.json / sat_energy_estimate.json intact; 'still busy' 0; fsw_stack_free 2948 B (1352 on 364). No ot/ots keys in the new file: expected, satPidStateAppendOutside() (SATcontrol.ino:2888-2895) appends them only with a current outside value. Pushed ad8dc9cd6..cc60bfd0e (beta.363-365), zip posted #alpha-testing 1572180595709055009. AC4 for 09 to tick.

AC4 ticked by 09 (2026-11-17), old vs fix on the SAME reproduction on .63 (Classic + ESP32-S3, PIC 6.8, SAT enabled but inactive, satsimulation false): fs-only OTA, POST /api/v2/sat/flush fired during the upload. OLD 2.0.0-beta.364+c8bc478 (09:50): flush at 09:50:37.542 (upload end 37.943), restore of /sat_pid_state.json at 38.623, reboot; after it sat_pid_state.json ts = 1794733218 = the pre-OTA value, so the flushed save was LOST. FIX 2.0.0-beta.365+cc60bfd (10:21): flush at 10:21:06.055 (epoch 1794907266, upload end 06.367), post-remount 06.764, restore 07.037, reboot; after it ts = 1794907266 = exactly the flush epoch, so the held save landed after the restored copy. Both runs: sat_energy.json / sat_energy_estimate.json intact, '[OTA] fswriter: still busy after 2 s' 0 times; fsw_stack_free 1352 (364) and 2948 (365) in the heap line. Evidence: 09 scratchpad b364-63/ and b365/t2007/ (t2007.log, telnet.raw, sat_*-before/after.json).
<!-- SECTION:NOTES:END -->
