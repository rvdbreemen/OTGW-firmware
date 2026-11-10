---
id: TASK-1790
title: >-
  Auto board mode on a Classic: a missed boot probe closes the PIC UART
  (OTGW-firmware.ino:462) and the per-minute PR=A retry can never reach the PIC
status: To Do
assignee: []
created_date: '2026-11-10T22:36:00Z'
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
