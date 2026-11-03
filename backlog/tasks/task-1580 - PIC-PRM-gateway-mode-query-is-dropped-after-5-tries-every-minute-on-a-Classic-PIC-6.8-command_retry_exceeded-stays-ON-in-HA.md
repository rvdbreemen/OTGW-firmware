---
id: TASK-1580
title: >-
  PIC: PR=M (gateway-mode query) is dropped after 5 tries every minute on a
  Classic PIC 6.8; command_retry_exceeded stays ON in HA
status: To Do
assignee: []
created_date: '2026-11-03T05:02:00Z'
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
- [ ] #1 Reproduced: cmd_dropped rate and a raw PIC serial/telnet capture show whether the PIC answers PR=M and why the queue does not match it
- [ ] #2 Fix: a host test on the real checkCommandResponse/queue code red on the old code, green on the fix
- [ ] #3 Bench: a Classic PIC unit runs 1 h with cmd_dropped not increasing and command_retry_exceeded OFF
<!-- AC:END -->
