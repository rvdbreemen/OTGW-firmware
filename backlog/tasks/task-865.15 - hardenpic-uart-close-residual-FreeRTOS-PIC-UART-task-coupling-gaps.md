---
id: TASK-865.15
title: 'harden(pic-uart): close residual FreeRTOS PIC-UART task coupling gaps'
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-14 09:52'
updated_date: '2026-06-21 07:51'
labels:
  - async-esp32s3
dependencies:
  - TASK-865.6
  - TASK-865.9
  - TASK-865.10
parent_task_id: TASK-865
ordinal: 78000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (ADR-130 follow-up; recon 2026-06-14 found 4 residual gaps)
865.6/ADR-130 made the dedicated FreeRTOS PIC task the sole UART byte-I/O owner. The recon found gaps the sole-owner gate does not catch:
- P2 resetPic() race: resetPic() does a direct HardwareSerial::write("GW=R\r")+delay(100) (OTGWSerial.cpp:894-912), bypassing otTxQueue and the upgradeEvent guard. The ser2net GW=R path (OTGW-Core.ino:5026-5031) and the MQTT command path (MQTTstuff.ino:998) call resetOTGW()->resetPic() loop-side WHILE THE TASK IS UNPARKED (GW=R flips no park condition). A second concurrent UART owner. Low severity (tiny idempotent payload) but real.
- P3a TX byte reorder: platformQueueSend is xQueueSend send-to-BACK (platform_esp32.h:441-452); the short-write requeue (OTGW-Core.ino:662-674) puts the popped chunk behind chunks already queued, and the ser2net relay enqueues 1 byte per OTTxMsg (OTGW-Core.ino:4996), so a TX-buffer-full burst can interleave a command's bytes -> silent command corruption beyond the ADR-documented silent drop.
- P3b sim-mode entry: handlePICSerialSimulation() (OTGW-Core.ino:3661-3680) calls picSerialFlushRx() loop-side with NO waitForPICTaskParked() handshake -> one-tick double-read race with the task drain.
- P3c progress churn: fwupgradestep() (OTGW-Core.ino:5317-5345) throttles only the Debug log; sendWebSocketJSON()->otLogWs.textAll() fires on EVERY progress callback (many at the same pct), each a per-client heap alloc on AsyncTCP during the most heap-sensitive op, and it bypasses the ADR-121 heap gate that sendLogToWebSocket honors.

## What
- Route resetPic's GW=R through enqueuePICTx, OR wrap resetOTGW() in the park handshake (waitForPICTaskParked) so the task is parked during the direct write.
- Fix the TX requeue to preserve order (requeue-to-FRONT, or coalesce the ser2net relay into multi-byte OTTxMsg). Add a platformQueueSendToFront shim if needed.
- Add waitForPICTaskParked() to sim-mode entry before picSerialFlushRx().
- Dedupe fwupgradestep WS send on last-sent-integer-pct change; consciously decide the ADR-121 heap-gate trade-off for the progress path and record it.

## ADR
Amend ADR-130 (control-method UART access + TX ordering invariant). Add a note to ADR-133 on the progress-path heap-gate decision.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 build esp32 + esp32-classic exit 0; evaluate.py --quick no new failures
- [ ] #2 resetPic GW=R no longer races the PIC task: routed through enqueuePICTx or guarded by the park handshake; the ADR-130 sole-owner evaluator gate still passes
- [ ] #3 TX requeue preserves byte order under backpressure (requeue-to-front or coalesced relay); no command-byte interleave possible
- [ ] #4 sim-mode entry parks the PIC task before the loop-side UART flush
- [ ] #5 fwupgradestep WebSocket progress is deduped on integer-pct change; the progress-path heap-gate trade-off is recorded in ADR-133
- [ ] #6 ADR-130 amended (control-method UART access + TX ordering); ADR-133 progress-gate note added
- [ ] #7 FIELD (ESP32-S3, epic TASK-865): ser2net load soak shows no command corruption; PIC flash with live progress shows no heap-churn regression
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Landed (alpha bump pending): (A) control-method park handshake — g_picResetInProgress flag OR-ed into picSerialTaskShouldPark(); resetOTGW() and detectPIC() now raise it + waitForPICTaskParked() around the direct OTGWSerial.resetPic()/find(ETX) span so the loop is never a second concurrent UART owner. (B) TX short-write requeue moved from platformQueueSend (back) to new platformQueueSendToFront() shim (front) restoring strict FIFO under ser2net relay backpressure. (C) handlePICSerialSimulation() parks the PIC task before the loop-side picSerialFlushRx(). (D) fwupgradestep() dedupes WebSocket progress on integer-pct change (<=101 frames/flash), consciously left outside the ADR-121 heap gate. ADR-138 (Proposed) amends ADR-130 (park + TX ordering) and ADR-133 (progress-path heap-gate note); README.md index updated. evaluate.py --quick: 0 failures, sole-owner gate + ESP-abstraction boundary green. AC#1-#6 met; AC#7 remains: FIELD validation on ESP32-S3 (ser2net load soak shows no command corruption; PIC flash with live progress shows no heap-churn regression).

3-target build verified GREEN at HEAD (alpha.232): esp32, esp32-classic, esp32-combo all SUCCESS (fw+fs); esp32-combo bin now FITS (no overflow). evaluate.py --quick 0-fail. Code ACs were verified by the planning pass reading the committed source. Remaining = field/hardware AC(s) for Robert.
<!-- SECTION:NOTES:END -->
