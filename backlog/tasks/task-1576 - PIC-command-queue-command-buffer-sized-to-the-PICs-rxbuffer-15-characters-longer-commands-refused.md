---
id: TASK-1576
title: >-
  PIC command queue: command buffer sized to the PIC's rxbuffer (15 characters),
  longer commands refused
status: Done
assignee:
  - '@claude'
created_date: '2026-11-02T22:01:00Z'
updated_date: '2026-11-04T05:46:00Z'
labels:
  - pic
  - command-queue
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 722000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-02: check the PIC's maximum buffer and match the firmware buffers. otgw-6.6 gateway.asm: rxbuffer res 16 (line 574); SerialReceive advances rxpointer only while no digit carry (addlw 1 / skpdc, ~1473-1476), so it runs 0..15 and sticks at 15; SerialCmdEnd writes 0 over the CR (~1481). A command of up to 15 characters arrives whole; a 16th character overwrites position 15 and the CR then clears it, so a longer command is silently cut to 15 characters without an error code. The ESP queue entry was char cmd[15] (14 characters), refusing 15-character commands the PIC handles. Change: OT_cmd_t.cmd[16] (15 + NUL), kMaxCmdLen 15, commands longer than 15 refused (Dropped), never truncated; snapshot fields sized alike.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OT_cmd_t.cmd holds 15 characters + NUL; a 15-character command is queued and sent whole; a 16-character command is refused (CmdQueueResult::Dropped); host test on the real queue code (old: 15 characters refused)
- [x] #2 No other buffer cuts a queued command below 15 characters (inflight copy, snapshot oldest/inflight, rejected-report message); evaluate --quick 0 failed
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-02: OT_cmd_t.cmd = PIC_CMD_MAX_LEN + 1 (15 + NUL, OTGW-Core.h); snapshot oldest/inflight 16; the inflight copy, the rejected message and restAPI's kMaxCmdLen use sizeof(cmdqueue[0].cmd) and follow. tests/test_pic_pm_chain.py test_command_length_matches_the_pic_buffer (harness reads PIC_CMD_MAX_LEN from OTGW-Core.h): with the old 14-character entry FAILED (15-character command refused), with 15 OK (15 sent whole, 16 refused as Dropped); 28/28.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audit of every buffer that receives a queued PIC/OT command (queue entry, in-flight copy, snapshot, reject message, REST, MQTT, NTP, SAT, PM chain): none is smaller than the 15-character command limit; the queue refuses a longer command as Dropped instead of truncating (OTGW-Core.ino ~3867/3882). evaluate 0 failed. Side finding recorded: restAPI.ino ~626 msgidBuf[4] cuts a zero-padded '0127' to '012'.
<!-- SECTION:FINAL_SUMMARY:END -->
