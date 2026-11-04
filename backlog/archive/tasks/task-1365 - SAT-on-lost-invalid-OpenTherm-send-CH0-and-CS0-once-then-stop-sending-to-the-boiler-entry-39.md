---
id: TASK-1365
title: >-
  SAT: on lost OpenTherm hand the boiler back once (CS=0, CH=1, MM=T), then send
  nothing (entries 39, 47)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T21:41:00Z'
updated_date: '2026-10-29T05:09:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.3
references:
  - TASK-1412
priority: high
ordinal: 500000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 (entry 39): if the software has no valid OT, SAT stops: CH=0, CS=0 once, then no more messages to the boiler. Compare the firmware OT-offline handling (entry 39 cites it).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On OT loss SAT sends CS=0, CH=1, MM=T exactly once (entry 47), then nothing to the boiler while the loss lasts (host test red on old, green on fix)
- [x] #2 No false hand-back on PIC PS=1 enter/leave, a late first loop pass, or millis wrap (host tests)
- [x] #3 What counts as lost (Robert, Q33) and whether SAT resumes by itself (Sergeant D, Q34, entry 79) are recorded before commit
- [x] #4 An OT loss in the first minute after boot does not overwrite the saved PID state (host test red on old, green on fix)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
AC#4 (2026-10-29): satDisable(bool persistPid = true). The OT-loss path in satOtLinkStep() calls satDisable(satPidRoomSeen()), read before the reset: a loss before the first room reading since boot or restore (entry 115) resets the PID in memory but leaves the saved state on flash; a later loss after a room reading still persists zeros as before. A loss before the NTP-gated satLoadPidState() thus no longer overwrites the file, and the deferred restore later loads it. User switch-off, flush, safety trip keep persisting zeros. Also drop the PROVISIONAL wording (Q33 Robert A, Q34 entry 79 A) in SATcontrol.ino satOtLink block and the docs. Test: tests/test_sat_ot_loss.py new case, saved PID file + OT loss in the first 30 s after boot, file unchanged (red on HEAD 4a934f574, green on fix); plus a loss after a room reading still writes zeros. No control-behaviour change (persistence only; Robert's decision 2026-10-28 'follow advice').
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25: plan (second-read) stored; waiting for Sergeant D's OK on the open question (open-questions Q29/Q30/Q31) before implementation, per CLAUDE.md SAT rules.

2026-10-25 04:33 UTC Sergeant D answered Q30 (entry 47): option B, final message CS=0, CH=1, MM=T (same as satDisable), then nothing. Remaining plan points (no-valid-OT = no boiler reply for 30 s; automatic resume) are our design, stated in the plan; ready to implement.

2026-10-25: two workflows ran on this task (orphaned wf_73ecbf70 from before /clear wrote the code; wf_4c6959ba reviewed it). Review findings A (PS=1 exemption PIC-only, hold not resume), D (boot baseline seed), F (wrap-proof stamp resume) are in the tree; E documented; G (MsgID 1 ownership while lost) and H (satDisable wipes PID/HCR) go into Q34. Blocking: Q33/Q34 to Sergeant D; test_sat_ot_loss.py needs the psleave/pslost/otdps/late/wrap cases.

2026-10-25 Robert decided (interactive): commit provisionally now, mark the behaviour provisional in docs/task, keep the task In Progress until Q33/Q34 are answered; a different answer from Sergeant D becomes a follow-up change. Verification this session: test_sat_ot_loss 21/21 OK on fix, HEAD FAILED (failures=10, errors=2); all tests/test_sat_*.py + test_otd_ch_override OK; evaluate --quick exit 0, 0 failed.

2026-10-25 bench alpha.501+6948ad5 on OTGW32 (OT-Direct, no boiler, SAT on, simulation off), app then fs over OTA, 177 settings kept. (1) Boot without boiler: ot_link_lost true ~30 s after SAT's first pass (09:55:11), 'OpenTherm lost: boiler handed back', OT-Direct write overrides empty afterwards (CS and MM released), 0 OTD commands for the next 2 min. (2) OT replay start (POST /api/v2/simulate/start): 09:59:01 'OpenTherm back: SAT resumes', CS=23.0/MM=100/CH=1 re-sent. (3) Replay stop ~09:59:48: 10:00:18 (30 s later) CS=0, CH=1, MM=T once, then 0 OTD commands until capture end 10:01:48. Logs in the session scratchpad (telnet501.txt, telnet501b.txt). AC#2 open: Q33 asked, Q34 queued.

2026-10-25 correction: the bench unit at 192.168.88.61 (MAC 10:20:BA:21:B4:F8) used for this task's evidence was NOT the OTGW32 (Robert: OTGW32 not connected in the 48 h before 2026-10-25 evening). It was an ESP32-S3 without PIC and without boiler running the combo image in OT-Direct mode (ADR-160 bench default). The evidence stands as 'combo image, OT-Direct mode, no boiler'; any 'OTGW32' wording in earlier notes is wrong.

2026-10-25 correction 2: Robert confirmed the .88.61 unit (MAC 10:20:BA:21:B4:F8) is an S3 WITH a PIC; it ran in OT-Direct mode because the PIC was not detected at boot (TASK-1399). So the evidence is 'combo image in OT-Direct mode on a board whose PIC was not detected', not PIC-gateway behaviour.

2026-10-26 19:13 UTC Q33 answered by the maintainer (Robert, #alpha-testing msg 1564356412585287824): A = lost means no boiler reply for 30 s; a Data-Invalid reply still counts as a reply. This is the provisional design already in commit 31933adaf, so no code change. Q34 (resume by itself) asked of Sergeant D next.

2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-28: Sergeant D Q34 A (entry 79, msg 1564581168630861865): resume on the first boiler reply is confirmed; drop PROVISIONAL from SATcontrol.ino ~2544 comment and docs. Q33 was answered by Robert (msg 1564356412585287824).

2026-10-28 maintainer decision (Robert, in session): follow the advice: AC#2 reworded (Q33 was answered by Robert, Q34 by Sergeant D, entry 79); new AC: an OT loss shortly after boot must not overwrite the saved PID state (entry 79 Open: three flash writes per loss; a loss at boot overwrites the saved PID).

2026-10-29 AC#3 checked: Q33 answered by Robert (msg 1564356412585287824, A), Q34 by Sergeant D (entry 79, A); both recorded in open-questions.md and sergeant-d-corrections.md (entry 79).

2026-10-29 AC#4: satDisable(bool persistPid = true); the OT-loss hand-back passes satPidRoomSeen() && isNTPtimeSet() (read before the reset). Review (opus) finding MED held and fixed: a room reading before NTP plus a loss would write zeros with an unsynced stamp, so the NTP-gated restore discards the file; hence the NTP term. Other findings: RAM-vs-flash after a loss post-restore (persistence-only by decision; flash kept, RAM reset) and the satPersistAllToFs/FS-OTA case are LOW and pre-existing; doc wording fixed. Tests: tests/test_sat_ot_loss.py 24 OK on fix; on HEAD 421d17724 (OTGW_SAT_INO=git show HEAD:SATcontrol.ino) test_o and test_q FAIL 'PIDSAVES 1 file=zeros' != 'PIDSAVES 0 file=saved' (behavioural), plus 2 source guards. Other SAT suites green (cadence 26, room_loss 12, otd_ch_override 8, mqtt_state 26, overshoot_sustain 43, mqtt_fallback 8, pwm_override 73); evaluate --quick 0 FAIL; build.bat esp32-combo SUCCESS 06:07 (4 jobs). TASK-1412.3 dependency does not apply: persistence only, no control change.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT hands the boiler back on an OpenTherm loss (no boiler reply for 30 s: CS=0, CH=1, MM=T once, then silent) and resumes on the first boiler reply. Decisions: Q33 A (Robert 2026-10-26), Q34 A (Sergeant D entry 79), Q30 B (entry 47). AC#1/#2 evidence in earlier notes (host tests red/green, bench 2026-10-25 combo image in OT-Direct mode). AC#3: both answers recorded. AC#4: a loss before the first room reading or before NTP time no longer overwrites the saved PID file (satDisable(persistPid)); host test test_o/test_q red on HEAD, green on fix (24 OK); build SUCCESS, evaluate 0 FAIL. PROVISIONAL wording removed from code and docs. Not verified: no real boiler loss, and AC#4 not run on a bench (the harness stubs the PID file; the NTP-gated restore itself is not executed).
<!-- SECTION:FINAL_SUMMARY:END -->
