---
id: TASK-1395
title: >-
  Telnet goes silent during setup() when a client sends q/D while the unit boots
  (readSettings on async_tcp races setup)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T18:39:00Z'
updated_date: '2026-11-05T16:34:00Z'
labels:
  - telnet
  - debug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 534000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From Sergeant D's alpha.505 capture 2026-10-25 19:28 (TASK-1339 notes; analysis wf_a5432f10, verified). The capture script reconnects telnet ~9 s after a reboot and sends 'q' (readSettings(true) + 54 KB settings dump, run on async_tcp holding the telnet mutex, AsyncSimpleTelnet.h:417-431 -> networkStuff.ino:615-617 -> handleDebug.ino:368-370) while setup() still runs; setup() blocks on the telnet mutex (AsyncSimpleTelnet.h:175) and the static _bol prefix is overwritten (spliced line at u.txt:2831). Telnet output then stopped for the rest of the run while the unit kept working. Also a W5500 was found on his unit (not on the bench). Two candidate causes, neither proven. Separate race: 'q' runs readSettings(true) on async_tcp concurrently with setup() (settings struct + _bol).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Bench repro: reboot with the capture script attached so q/D arrive during setup(); telnet silent or not (without a W5500)
- [ ] #2 If reproduced: telnet keeps streaming after a q during setup (red/green), and q does not run readSettings concurrently with setup()
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: 'q' runs readSettings(true) from async_tcp (handleDebug.ino:364); no setup gate. Repro first.

2026-11-04 (otgw-firmware-6c) AC1 bench repro on .63 (Classic+S3, beta.79+a9dd048, no W5500), script tn_repro.py: GET /ReBoot, wait until :23 refuses, reconnect telnet as soon as possible (+8.2..9.2 s), send q then D, read 150 s. 3 of 3 runs: telnet did NOT go silent (bytes per 10 s bin stayed 4.3-33 KB up to +150 s, last byte at +150 s). The race itself is real: readSettings output starts at byte ~1600 while 'Setup finished!' comes at byte 36911-59295, and 2-6 spliced lines per run (setup() and the q dump interleaved, e.g. 'updateSettin22:03:42.966469 ...'). So: silence not reproduced without a W5500 (that part stays a W5500/field question), the concurrent readSettings during setup() is reproduced. (A first run connected at +2 s to the old session before the reboot; discarded.) Note: scripts/otgw_capture.py already waits for 'Setup finished!' / uptime > 0 before q/D, so the field trigger is gone client-side; the firmware gate covers any client. AC2 fix: handleDebugChar() (handleDebug.ino) refuses every action key until state.bSetupComplete (set as the last line of setup(), OTGW-firmware.ino:579) with a notice; toggles and 'h' still work. Host test tests/test_telnet_setup_gate.py compiles the real handleDebugChar with recording stubs: on HEAD b1fe73d2e 2 of 4 FAIL ('q' ran 'q' while setup() still runs); on the fix 4 of 4 OK. evaluate.py --quick: 82 passed, 0 failed, 0 warnings. Bench check after build (lead): reboot + q at +8 s prints 'Key q ignored: setup still running' and no readSettings before 'Setup finished!'.

2026-11-04 Robert (interactive, this session): waive the 'telnet keeps streaming' clause of AC2: not reproducible without a W5500 (bench 3/3 kept streaming); no follow-up task, a new report from a W5500 user becomes a new bug. Close after the lead's bench check of the gate.

2026-11-05: AC2 is conditional ('If reproduced'): the q-during-setup streaming problem was not reproduced in the bench runs (beta.87 .61: action keys refused until 'Setup finished!', toggles handled before); nothing to fix.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Telnet action keys are refused until setup() is done (bSetupComplete), toggles still work. Evidence: tests/test_telnet_setup_gate.py 2 of 4 FAIL on old, 4/4 on the fix; bench .61 beta.87: 'r' during setup -> 'ignored: setup still running', after setup accepted. AC2 conditional, not reproduced.
<!-- SECTION:FINAL_SUMMARY:END -->
