---
id: TASK-1720
title: 'OT-Direct: let the USB serial port speak the PIC line protocol for OTmonitor'
status: To Do
assignee: []
created_date: '2026-11-08T08:27:00Z'
updated_date: '2026-11-08T09:48:00Z'
labels:
  - otdirect
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
- [ ] #3 Bench gate: OTmonitor on Windows opens and closes the USB COM port 10x on a PIC-less unit with no reset (bootcount and uptime unchanged)
- [ ] #4 USB flash still works with the USB protocol setting ON and OFF: flash_otgw --update over USB (esptool auto-reset via USB-Serial-JTAG) succeeds on the PIC-less unit, plus the safeboot/recovery path; before/after evidence (Robert 2026-11-08)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert 2026-11-08 chose option B of the TASK-1721 research (docs/research/2026-11-08-usb-serial-otmonitor-outlet.md): opt-in USB line protocol on PIC-less hardware, gated on a bench test first. Step 1 (gate): on a PIC-less unit on USB, OTmonitor (Windows) connects/disconnects 10x over the USB COM port with no chip reset (uptime/bootcount unchanged); if it resets, B stops and we fall back to A. Step 2 only if clean: setting default off, OT-Direct only, writes through the low-level USB FIFO path in platform_esp32.h (not HWCDC::begin/end), USB console silent after board detection, its own line-buffer instance, no otgwStreamRxNote() from USB, non-blocking writes; new ADR amending ADR-168; reword the 'never write to Serial after init' rule to 'never write to the PIC UART'. Estimate ~80-150 lines, <2 KB flash (estimates).

Robert 2026-11-08: every test of this feature includes a flash over USB (setting on and off).

Robert 2026-11-08: even with USB in protocol mode (same output as port 25238), the regular USB flash must still work. Design consequence: stay on the hardware USB-Serial-JTAG peripheral, whose DTR/RTS reset-into-download is handled in silicon, independent of the firmware; never switch USB to TinyUSB/OTG, never reconfigure the USB-Serial-JTAG peripheral or its reset behaviour. Test: start flash_otgw --update over USB while the protocol stream is running (and once with OTmonitor just disconnected); esptool must enter download mode, flash, and the unit must boot.
<!-- SECTION:NOTES:END -->
