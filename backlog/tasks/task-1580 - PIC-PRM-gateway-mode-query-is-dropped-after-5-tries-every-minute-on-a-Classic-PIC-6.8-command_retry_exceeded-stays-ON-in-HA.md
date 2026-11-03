---
id: TASK-1580
title: >-
  PIC: PR=M (gateway-mode query) is dropped after 5 tries every minute on a
  Classic PIC 6.8; command_retry_exceeded stays ON in HA
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-03T05:02:00Z'
updated_date: '2026-11-03T05:46:00Z'
labels:
  - pic
  - bug
dependencies: []
priority: high
ordinal: 726000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-11-03 06:00 on the beta.17 soak unit .63 (Classic + S3 Mini, PIC 6.8, SAT enabled): device/info cmd_dropped = 620 after 8 h 23 min, +1 per minute. MQTT command_dropped payload = 'PR=M'. Read-only telnet: 06:01:09 'CmdQueue: Insert queue in slot[3]:cmd[PR=M]', 06:01:30 command_dropped published; no 'PR: M=' reply line in the 75 s trace (the telnet log is lossy, TASK-1314, so the missing line alone is no proof; the firmware's own drop is). queryOTGWgatewaymode() queues PR=M once per minute (OTGW-Core.ino ~1103); checkCommandResponse matches PR replies on the register letter. TASK-1314 showed PIC 6.8 answers PR=M on a 1.x unit. Effect: a dropped command is published every minute and command_retry_exceeded (HA problem entity) stays latched ON on a healthy unit. Unknown: whether beta.18+ (queue redesign TASK-1567..1576, s_inflight echo pacing) still shows it, and since which build (TASK-1439 added the drop report at alpha.524).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduced: cmd_dropped rate and a raw PIC serial/telnet capture show whether the PIC answers PR=M and why the queue does not match it
- [x] #2 Fix: a host test on the real checkCommandResponse/queue code red on the old code, green on the fix
- [ ] #3 Bench: a Classic PIC unit runs 1 h with cmd_dropped not increasing and command_retry_exceeded OFF
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 06:15 root cause (lead, read-only telnet + code): .63 runs with satsimulation=true. sendPICSerial() returns early when satSimulationBlocksBusTx() refuses the command (OTGW-Core.ino beta.17 sendPICSerial, ~line 28 of the function), but handleCommandQueue() still does retrycnt++ and after OTGW_CMD_RETRY=5 calls reportCommandDropped(). Telnet: PR=M inserted slot[3] 06:08:09, 'Queue slot [3] due' at 10.8/15.8/20.8/25.8/30.8 s, sat/sim/last_cmd = PR=M (blocked), command_dropped at 06:08:30. SAT's CS/MM/CH are also blocked but SAT replaces them every 5 s, resetting retrycnt, so only PR=M drops. Normal operation (simulation off) is not affected. Release notes known issue corrected accordingly. Fix direction: a command refused by the simulation is removed from the queue without a drop report (sendPICSerial reports blocked).

2026-11-03 (agent, verified by lead): sendPICSerial() returns bool (false only where satSimulationBlocksBusTx refuses); handleCommandQueue() treats a held-back command as consumed: clears s_inflight, releases the prio slot, removes it, no retry/drop/flag. Also found: a held-back command occupied the in-flight slot, delaying every other command by the 1000 ms echo timeout per try. tests/test_pic_pm_chain.py: 3 new tests; old code 2 FAIL (dropped 1 instead of 0; next CS at 1000 ms), fix 32 OK; simulation-off silent PIC still drops after 5 tries (passes old and new). Full tests 104/104, evaluate 0 failed. AC3 bench: 1 h on .63 with satsimulation on, cmd_dropped flat (beta.38 soak).
<!-- SECTION:NOTES:END -->
