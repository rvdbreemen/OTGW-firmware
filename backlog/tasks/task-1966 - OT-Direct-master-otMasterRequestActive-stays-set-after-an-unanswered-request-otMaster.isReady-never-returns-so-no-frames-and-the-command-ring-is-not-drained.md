---
id: TASK-1966
title: >-
  OT-Direct master: otMasterRequestActive stays set after an unanswered request;
  otMaster.isReady() never returns, so no frames and the command ring is not
  drained
status: To Do
assignee: []
created_date: '2026-11-15T10:14:00Z'
labels:
  - otdirect
  - bench-finding
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1004000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Description: Bench 09, 2026-11-15 ~11:00, .61 (OT-Direct, no boiler, no thermostat), beta.298+bb38dc5, uptime 3:32 h, bootcount 717, no crashlog, otdmode master. Facts: (1) REST POST /api/v2/otgw/commands answers 202 queued, but nothing runs. OT=7.6 gives no override in /api/v2/otdirect/overrides (it did right after the boot of the 298 bench at ~07:30). PR=T/PR=A give no 'PR:' reply. No 'OTD: cmd' line on telnet, although debug OTDirect and OTmsg are on. (2) Telnet 30 s: no single R/B frame; only 'loopOTDirect(2758): OTD: state online=0 mode=master' and flameRatio lines, so the loop task runs. (3) At 11:01, 9 posts gave 202 then 8x 503 'Command not queued (queue full)': the TASK-1761 text ring (7 places) was full. At 11:03 the ring accepted 9 posts again (202), but OT=7.6 still gave no override: the ring is emptied without the commands running. Not explained; maybe otCmdApplyPendingFlush (top of loopOTDirect). Code reading (origin/dev): otdCmdTextDrain() runs only if !otMasterRequestActive (OTDirect.ino:2530). otMasterRequestActive is cleared only in handleMasterResponse() (:1997), which runs only if otMaster.isReady() (:2538). otMaster.isReady() (libraries/OpenTherm/src/OpenTherm.cpp:134) needs rxStatus IDLE, txStatus IDLE and micros()-delayTimestamp < 0x80000000. 95's library reading: txStatus TX_DATA has no timeout (only WAIT_RESPONSE, 1 s), so a TX timer that does not reach its last bit keeps isReady() false forever. Second, unverified candidate: setDelay() (OpenTherm.cpp:389-395) compares 'newDelay > delayTimestamp' without wrap safety; micros() wraps every ~71.6 min. Likely trigger (unconfirmed): the restore after the 298 bench, ~08:10: mode switch to master, then SW=0/SH=0/OT=-/MM=- right after each other. The 30 s telnet check after it already showed no MsgID 99. A reboot (the beta.301 flash) clears the state. 95 files the watchdog/recovery fix separately. Evidence: 09 scratchpad b301/stall/ (debug.txt with otbus/otd state, device_info.json, health.json, otdirect_*.json, tel30.raw local, ring_probe_after.txt, overrides_after_OT76.json). Present in public beta.294 too. Fix direction (95, no library change): watchdog in loopOTDirect, flag set > ~1.5 s without isReady -> log line, otMaster.end()/begin(), flag false. 95 checked: the mode switch only touches otSlave; otMaster.end/begin only in GW=R and at boot, so the trigger is open.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce on .61: mode switch gateway->master with commands right after (and/or past a micros() wrap at ~71.6 min uptime); record the time, the trigger, the telnet lines before the stall, and whether device/info uptime/heap still move
- [ ] #2 Cause with evidence: which of rxStatus/txStatus/delayTimestamp keeps isReady() false, and why the text ring empties without running the commands
- [ ] #3 Host test on the real loopOTDirect code with a fake library stuck in TX_DATA: red on the current code (request stays active, ring not drained / 503), green on the fix (recovery within ~1.5 s, log line, commands run again)
- [ ] #4 Bench on .61: frames and command handling keep running >= 2 h (past at least one micros() wrap) and after the repro sequence
<!-- AC:END -->
