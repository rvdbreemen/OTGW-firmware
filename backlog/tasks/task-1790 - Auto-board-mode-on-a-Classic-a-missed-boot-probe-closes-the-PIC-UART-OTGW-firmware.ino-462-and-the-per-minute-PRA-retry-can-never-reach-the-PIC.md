---
id: TASK-1790
title: >-
  Auto board mode on a Classic: a missed boot probe closes the PIC UART
  (OTGW-firmware.ino:462) and the per-minute PR=A retry can never reach the PIC
status: To Do
assignee: []
created_date: '2026-11-10T22:36:00Z'
updated_date: '2026-11-10T22:48:00Z'
labels:
  - bug
  - combo
  - pic
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 883000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while writing TASK-1788 docs (dev c21f07f03). In Auto (iBoardMode 0) a missed boot detectPIC() leads to OTGWSerial.end() at OTGW-firmware.ino:462, before the isOtgw32HardwarePossible() branch. On a LOLIN module (PSRAM or Pro IMU) OT-Direct is not started (:466-471), eMode stays HW_MODE_DEGRADED, and nothing reopens the UART: OTGWSerial.begin only at :405 and :421. The per-minute retry in doTaskEvery60s (:853, :890-899) still logs 'PIC is unknown, probe pic using PR=A' and enqueues PR=A, but the UART is closed, so a PIC that comes up late (bad seating, slow power) is never found until the next reboot. In forced PIC mode (1/3) the UART stays open and a banner reply recovers to HW_MODE_PIC via applyPICBannerInfo (OTGW-Core.ino:841-843). thaghostnl's second capture (TASK-1786) was in mode 1 and still got no PIC reply, so this gap does not explain his case. Not verified on hardware yet; read from code only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduced on the Classic-S3 bench (.63) on the current code: PIC held in reset (or not answering) during boot in Auto, then released; PIC never detected without a reboot (red)
- [ ] #2 On the fix the same reproduction finds the PIC without a reboot (green), and an OTGW32 in Auto still keeps its UART closed (ADR-125 floating RX)
- [ ] #3 Host test or gate red on the old code, green on the fix
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Code check by otgw-firmware-a4 (2026-11-10, origin/dev 4595fa65a, read only, not on hardware): (1) CONFIRMED Auto path, no PIC after 3 tries -> OTGWSerial.end() at OTGW-firmware.ino:462, BEFORE the isOtgw32HardwarePossible() branch (:463); the LOLIN branch (:466-475, TASK-1730) leaves HW_MODE_DEGRADED and never calls begin() again (begin only at :405 and :421). (2) CONFIRMED the doTaskEvery60s retry (:885-899) gates only on !isOTDirectEnabled() and an unknown deviceid, so it enqueues PR=A every minute. (3) EXTRA: after end() the core sets _uart = NULL (HardwareSerial.cpp:512-514), availableForWrite() returns 0 (esp32-hal-uart.c:1131-1133), and the PIC TX drain (OTGW-Core.ino:807-821) re-queues the first chunk to the front on every tick: otTxQueue never drains, later PIC TX piles up until the queue is full and sends are dropped; no RX either, so applyPICBannerInfo (OTGW-Core.ino:841-843) can never run. (4) Fix direction (opinion): keep the UART open in the LOLIN branch (the ADR-125 floating-RX concern is the OTGW32 branch), or re-begin() before the retry when !isOtgw32HardwarePossible(). AC1 red on .63 first.
<!-- SECTION:NOTES:END -->
