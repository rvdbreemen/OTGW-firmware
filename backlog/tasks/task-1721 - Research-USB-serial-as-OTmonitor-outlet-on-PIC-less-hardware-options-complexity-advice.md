---
id: TASK-1721
title: >-
  Research: USB serial as OTmonitor outlet on PIC-less hardware - options,
  complexity, advice
status: Done
assignee: []
created_date: '2026-11-08T08:55:00Z'
updated_date: '2026-11-08T09:47:00Z'
labels:
  - research
  - otdirect
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 830000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-08: research tjfs's feature request TASK-1720 (Discord msg 1568902343880216738): use the native USB port as an outlet for the PIC line protocol traffic so OTmonitor can connect over USB as on a classic OTGW. Explain the options, evaluate the added complexity, advise. Research only, no firmware change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Report in docs/research/ lists the options: TCP 25238 only; USB CDC carries the PIC line protocol instead of the debug log; mode switch on one CDC; second CDC interface. Per option: ESP32-S3 USB-Serial-JTAG vs TinyUSB OTG, effect on USB flashing, safeboot, recovery and boot log, code and flash and RAM cost, risks
- [ ] #2 Claims cite primary sources: our code file:line, ESP-IDF and arduino-esp32 docs, otmonitor source in other-projects; and say what was not verified
- [ ] #3 Clear recommendation with consequences, put to Robert as one question
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08 research done (report docs/research/2026-11-08-usb-serial-otmonitor-outlet.md, uncommitted): bridge output already funnels through otDirectBridgeWriteLine and input through handleOTDirectBridgeStream, so USB reuse is cheap (est. 80-150 lines); real costs are losing the USB diagnostic console (ADR-168, core Serial rule wording), OTmonitor treating stray log lines as command answers (otmonitor.tcl:580-583/448-452), and an unverified USB-Serial-JTAG reset on port open/close (esptool reset.py:125-143). Recommendation: A (TCP 25238, documented) now; B (opt-in USB protocol in OT-Direct mode) only after a Windows OTmonitor connect/disconnect bench test shows no reset; reject C and D (TinyUSB). Open question for Robert: A, B-gated or D.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Report docs/research/2026-11-08-usb-serial-otmonitor-outlet.md (options A-D, costs, risks, cited sources, not-verified list). Spot-checked: platformio.ini:169-170 USB-Serial-JTAG flags; otmonitor.tcl:448-452 default pattern * matches the first pending command. Robert chose B (opt-in, bench-gated), recorded on TASK-1720.
<!-- SECTION:FINAL_SUMMARY:END -->
