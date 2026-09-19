---
id: TASK-1140
title: >-
  Fix: capture-usb-serial.bat defaults to 115200 but the 1.x firmware UART runs
  at 9600
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-19 14:28'
updated_date: '2026-09-19 14:48'
labels:
  - bug
dependencies: []
priority: high
ordinal: 221000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while analysing the capture Appiejs attached to GH #684 on 2026-09-19. His usb-serial.log is 541 bytes of framing garbage, and the cause is our tooling rather than his device.

The 1.x firmware never sets 115200. The only baud in the tree is HardwareSerial::begin(9600, SERIAL_8N1) at src/libraries/OTGWSerial/OTGWSerial.cpp:836, because that UART is the PIC link and the PIC runs at 9600. A grep for 115200 across src/OTGW-firmware/ and src/libraries/OTGWSerial/ returns nothing.

scripts/capture-usb-serial.bat declares [int]$Baud = 115200 (line 68) and its help text states that 115200 is 'the OTGW application baud, which is what the exception dump + stack trace print at' (lines 96-97). For this firmware both halves of that sentence look wrong: the application baud is 9600, and the ESP8266 panic handler prints over the same UART at whatever divisor is currently configured, so a crash dump on this firmware should also come out at 9600. That second point is reasoned from how the core works and is NOT verified on hardware; verify before relying on it.

Consequence: every reporter who runs the script as documented captures unreadable bytes, and neither they nor we can tell from the file that the rate was the problem rather than the device. It cost one full round trip on #684 already.

The -Baud 74880 advice for the boot-ROM reset-cause banner is unrelated and stays correct.

Check the 2.0.0 copy separately before changing anything there: that line is ESP32 and its console baud may genuinely differ.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The default baud matches what the 1.x firmware actually configures, with the source line cited in the script
- [x] #2 The help text no longer claims 115200 is the application baud, and says which rate to use for what
- [x] #3 A capture taken with the new default against a real 1.x gateway yields readable OTGW PIC output rather than framing garbage
- [x] #4 The 2.0.0 copy is checked and either changed with its own justification or explicitly left alone
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fixed in 79cb3a04.

Evidence, real hardware (CH340 OTGW on COM3):
- Reading the port directly: 9600 gives readable GW=R (the firmware polling the PIC for gateway mode), 115200 gives 0 bytes in the same window.
- capture-usb-serial.bat with the new 9600 default wrote '16:40:59.229  GW=R', 20 bytes, fully printable.
- The bytes immediately after a reset are unreadable at 9600 as expected: that is the ESP8266 boot ROM banner at 74880, which is why the -Baud 74880 advice stays in the help.

Evidence, capture-otgw.sh serial mode against a pseudo-terminal (WSL cannot reach a COM port without usbipd, which is not installed here): realistic PIC output gives 80 clean lines with no warning, the wrong-baud byte pattern gets 20 of 20 lines flagged, and a --serial path that does not exist exits 2.

Bug caught by that test before shipping: OTGW lines are CRLF terminated, read strips the LF and leaves the CR, and 0x0D is outside printable ASCII, so the garbage detector counted 77 of 81 healthy lines as a baud mismatch. Stripping the CR first fixed it. Without the pseudo-terminal case this would have shipped as a false alarm telling reporters their working capture was broken.

Still open: AC about the 2.0.0 copy. Not checked, deliberately: that tree is ESP32 and its console baud may genuinely be 115200.

AC4 resolved: there is no capture-usb-serial.bat in the 2.0.0 tree, so there was no copy to change. Nothing was touched there.

It would have been wrong to change anyway. On 2.0.0 the PIC link runs on dedicated pins (OTGWSerial(PICRST, LED2, PIN_PIC_RX, PIN_PIC_TX) in OTGW-firmware.h:78-80) at 9600, but that is a different interface from the USB console, so 9600 is not automatically right for a console capture there.

Two things found in that tree worth recording, neither acted on:

1. scripts/capture-serial.py defaults to --baud 115200. Whether that is right depends on which interface it is pointed at, and on ESP32-S3 those are genuinely separate. Not verified, not changed, flagged only.

2. The 2.0.0 tree already ships capture-mqtt-debug.sh (1584 lines) and capture-mqtt-debug-macos.sh for Linux and macOS, using bash plus embedded Python workers. I built the 1.x capture-otgw.sh without first checking whether a portable capture already existed in the sibling tree. Mine is not redundant (450 lines, bash and curl only, no python3 dependency, and the 1.x REST surface differs) but the overlap is real and the check should have come first.
<!-- SECTION:NOTES:END -->
