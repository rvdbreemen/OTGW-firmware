---
id: TASK-1593
title: >-
  OT-Direct: a refused SAT write is reported as delivered (addCommandToQueue
  returns Sent while the OT-Direct command ring refuses it)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-03T18:33:00Z'
updated_date: '2026-11-03T18:57:00Z'
labels:
  - sat
  - otdirect
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 738000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From TASK-1412.11 (Q-1412.11a, Robert 2026-11-03: yes). On OT-Direct addCommandToQueue() returns CmdQueueResult Sent unconditionally (OTGW-Core.ino ~3856-3860) and handleOTDirectCommand() is void (OTDirect.ino ~2749). When the otCmdEnqueue ring (12 slots, coalesced per MsgID) is full the write is refused but SAT's commit-on-success (TASK-1574) caches it as delivered; recovery only via the 5 s CS resend / 60 s MM repair.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 handleOTDirectCommand reports whether the command was accepted; addCommandToQueue maps a refusal to Dropped on OT-Direct (PIC path unchanged)
- [x] #2 A host test drives the real OT-Direct handler with a full ring: old code reports Sent (red), fix reports Dropped and SAT does not commit its cache (green); the LL-013 open_decision test_ll013b becomes a passing contract test
- [x] #3 Every other caller of handleOTDirectCommand still compiles and behaves the same (listed with file:line)
- [ ] #4 esp32-combo build, OTA flash and bench check on marvin-otgw32 (OT-Direct): SAT CS/MM still delivered, no new Dropped in normal traffic
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 (sonnet agent; lead reran in the integration tree): handleOTDirectCommand returns bool (false on every former early return / error reply and when enqueueWriteCommand finds the ring full); addCommandToQueue OT-Direct branch maps false to Dropped (OTGW-Core.ino:3860), PIC path unchanged. Callers: OTGW-Core.ino:3858 (uses it), :4375 sendPICSerial (ignores, identical). tests/test_sat_command_delivery.py OtDirectRefusal on the real ring/enqueue/CS=/MM= branch: OLD (git show HEAD copies) CS=40.0 Sent MM=80 Sent with the cache committed while ring depth stayed 11 (2 failures), FIX CS/MM Dropped, cache not committed (16 OK incl. not-full Sent, coalesced CS Sent). LL-013b now a passing contract (lowload contract 32 OK, 401 s). pm_chain 32 OK, no_return_startup 19 OK, cmd_cache 27, otd_ch_override 8, ot_loss 24, evaluate 0 failed. Lead checked: satCmdResend only resends when bLastSentValid, so a refused first CS does not resend 0.0. Not covered: ring-full on raw otCmdEnqueue paths (MH/MW/M2, GW, TP). Behaviour change: an invalid user command on OT-Direct now returns Dropped from addCommandToQueue (reply unchanged). Tag beta.59.
<!-- SECTION:NOTES:END -->
