---
id: TASK-1442
title: >-
  PIC error replies (NG/BV/SE/OR) count as a confirmed command: rejected
  commands are not reported
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T05:14:00Z'
updated_date: '2026-10-29T07:00:00Z'
labels:
  - bug
  - mqtt
  - pic
dependencies: []
priority: low
ordinal: 260000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the bench 2026-10-29 while verifying TASK-1431 (.68, beta.8): checkOTGWcmdqueue() matches a reply on the two command letters only, so 'ZZ: NG' (unknown command) removes ZZ=1 from the queue as confirmed and clears command_retry_exceeded. A command the PIC rejects (NG, BV, SE, OR, NS) is therefore neither retried nor reported; the user only sees it in the OT log. PR=x is the exception (register-letter match), which is why PR=Z with 'PR: BV' is retried 5 times and then reported as dropped.

Decision needed from the maintainer: should a PIC error reply be reported (for example on command_dropped with the error code, or a separate command_rejected topic), and should it still clear the retry-exceeded flag? Retrying an error reply makes no sense, so the open question is only reporting.
Evidence: C:\Users\rvdbr\AppData\Local\OTGW-capture\bench-68-beta8-20261029-0559\telnet_zz.log, telnet_drop_tp.log
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Maintainer decided whether and how a PIC error reply to a queued command is reported
- [x] #2 A queued command answered with a PIC error code (NG, BV, SE, OR, NS, NF, OE) is published once on otgw-firmware/command_dropped as '<command> (<code>)' (not retained), counts in cmd_dropped, latches command_retry_exceeded ON and is removed without retry; host harness on the real queue code is red on HEAD and green with the fix
- [x] #3 An error reply never sets command_retry_exceeded OFF; a normal confirmation still does (same harness)
- [x] #4 Bench .68: ZZ=1 via MQTT gives command_dropped 'ZZ=1 (NG)', flag ON, cmd_dropped +1; a following confirmed TT= sets the flag OFF
- [x] #5 build.bat and evaluate.py --quick green; docs/api/MQTT.md and CHANGELOG describe the rejected-command report
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. isPicErrorReply(): code after 'XX: ' is one of NG SE BV OR NS NF OE (PROGMEM list).
2. checkOTGWcmdqueue(): an error reply goes to the first queued command with the same two letters (no PR register match: an error reply has no letter), reported via reportCommandRejected() -> reportCommandDropped('<cmd> (<code>)'), removed without retry, flag not cleared.
3. Host harness test/host/test_cmdQueueReject.py slicing the real queue old vs fix.
4. Docs: MQTT.md rows + queue paragraph, CHANGELOG Fixed.
5. Build (after the 2.0.0 session's build slot), bench .68: ZZ=1 via MQTT.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29 maintainer decision: report a PIC error reply on the existing topic. A queued command answered with an error code (NG, BV, SE, OR, NS, NF, OE) is published on otgw-firmware/command_dropped as '<cmd> (<code>)', e.g. 'ZZ=1 (NG)', counts in cmd_dropped, latches command_retry_exceeded ON and is removed without retry; it does not clear the flag. Port to 2.0.0 afterwards. Beta.9: after this fix plus field feedback on beta.8.

2026-10-29: implemented (OTGW-Core.ino isPicErrorReply, reportCommandRejected, checkOTGWcmdqueue).
- python test/host/test_cmdQueueReject.py --old-rev HEAD (76a89bc3d+task commits) -> RESULT: PASS. FIX passes R1 (ZZ: NG -> 'ZZ=1 (NG)', flag ON retained, count 1, one send), R2 (SB: OR reported, flag stays ON), R3 (PR: BV -> 'PR=Q (BV)' after one send), C1 (after a drop a confirmed TT= sets OFF), C2 (PR: S=16.00 matches PR=S), C3 (unanswered PR=Z dropped after 5 tries). OLD fails R1-R3, passes C1-C3.
- Behaviour change to note: an older PIC that answers BV for a PR= register it lacks (e.g. PR=Q on PIC 4.x/5.x) is now reported at once instead of after 5 tries; beta.8 already reported it after 5 tries, so the report itself is not new. Check on .16 (PIC 5.8) whether the settings cycle hits such a register every cycle (would repeat the report).
- Build waits for the 2.0.0 head session's TASK-1408 build (one build at a time).

2026-10-29 bench .68 (PIC gateway 6.8), firmware 1.7.8-beta.9+e67cc43 (TASK-1442 working tree) via web OTA, temp broker 192.168.88.32: ZZ=1 via MQTT command topic -> sent once 07:58:51, PIC 'ZZ: NG', telnet 'CmdQueue: PIC refused [0]:[ZZ=1] with [NG]', MQTT command_dropped 'ZZ=1 (NG)' and command_retry_exceeded ON, device/info cmd_dropped 0 -> 1; TT=21.5 07:59:01 -> 'TT: 21.50', command_retry_exceeded OFF. Settings restored, identical to the pre-test copy. Evidence: C:\Users\rvdbr\AppData\Local\OTGW-capture\bench-68-beta8-20261029-0559\telnet_1442.log, mqtt_1442.log, settings_pre1442.ini/settings_post1442.ini.
- build.bat green 07:55 (beta.9+e67cc43 .ino.bin/.littlefs.bin, 'Build completed successfully'); evaluate.py --quick 38 passed 0 failed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A command the PIC refuses with an error code (NG, SE, BV, OR, NS, NF, OE) is now reported instead of being taken as confirmed: it is published once on otgw-firmware/command_dropped with the code appended (e.g. 'ZZ=1 (NG)'), counts in cmd_dropped, latches command_retry_exceeded ON, and is removed without retry. A refusal never sets the flag OFF; a real confirmation still does. A refused PR= readout ('PR: BV' from an older PIC) is now reported after one send instead of after 5 tries.

Changes: OTGW-Core.ino isPicErrorReply(), reportCommandRejected(), checkOTGWcmdqueue(); docs/api/MQTT.md; CHANGELOG Fixed. New host harness test/host/test_cmdQueueReject.py (python, slices the real queue per git revision).

Verification: harness --old-rev HEAD: old fails R1-R3, passes C1-C3; fix passes all 6. Bench .68: ZZ=1 -> 'ZZ=1 (NG)', flag ON, cmd_dropped +1, TT= confirmation -> OFF. build.bat green, evaluate.py 38/0.

Not verified: an older PIC (5.8 on .16) answering BV for a PR= register; a 90 s passive telnet capture there showed one PR= reply and no error, so how often such a report would repeat is unmeasured. 2.0.0 port follows as a separate dev task.
<!-- SECTION:FINAL_SUMMARY:END -->
