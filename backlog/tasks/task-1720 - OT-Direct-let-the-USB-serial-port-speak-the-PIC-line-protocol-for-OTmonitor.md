---
id: TASK-1720
title: 'OT-Direct: let the USB serial port speak the PIC line protocol for OTmonitor'
status: To Do
assignee: []
created_date: '2026-11-08T08:27:00Z'
labels:
  - otdirect
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 829000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Proposal by tjfs 2026-11-08 08:17 #alpha-testing (msg 1568902343880216738): make the USB serial port on PIC-less hardware work like the classic OTGW (PIC line protocol), so OTmonitor users can connect over USB. Today OTmonitor works over TCP port 25238 (OTDirect.ino handleOTDirectBridgeStream/otDirectBridgeWriteLine). Serial is native USB CDC (ARDUINO_USB_CDC_ON_BOOT=1). Check what uses USB at runtime and the effect on USB flashing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert decides whether USB carries the PIC line protocol on PIC-less hardware
- [ ] #2 If yes: OTmonitor over USB shows frames and accepts commands as over TCP 25238 (bench), USB flashing unaffected
<!-- AC:END -->
