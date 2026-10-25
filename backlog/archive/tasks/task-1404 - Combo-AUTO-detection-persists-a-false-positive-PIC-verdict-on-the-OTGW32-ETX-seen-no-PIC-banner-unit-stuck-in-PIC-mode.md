---
id: TASK-1404
title: >-
  Combo AUTO detection persists a false-positive PIC verdict on the OTGW32 (ETX
  seen, no PIC banner): unit stuck in PIC mode
status: Done
assignee: []
created_date: '2026-10-25T21:21:00Z'
updated_date: '2026-10-25T21:22:00Z'
labels:
  - combo
  - pic
  - bug
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 543000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench 2026-10-25, OTGW32 (USB 303A:1001 MAC AC:27:6E:CE:45:D8, COM8, .63), confirmed OTGW32 by Robert. Before: alpha.435, settings boardmode=1, hardwaremode PIC, picfwversion 6.8 / pic16f1847 (!), otgwconnected false, otdirectavailable false. OTA to alpha.508+c7ee107, set boardmode=0, /ReBoot: boardmode back to 1, hardwaremode PIC, picfwversion 'no pic found', otdirectavailable false. /bootdetect.log boot#1..#3: eMode=1 pic=1 mode=1 pro=0 RST=12 RX=44 TX=43 I2C=35/36. detectPIC() sets bAvailable = OTGWSerial.find(ETX) (OTGW-Core.ino:1049): any 0x03 byte within the Stream timeout counts, and AUTO persists the positive verdict (OTGW-firmware.ino:431, ADR-201). This is the false-positive risk named in ADR-201's Consequences, now observed. Open: what drives 0x03 onto GPIO44 on the OTGW32 (floating RX, a peripheral, the PIC reset pin GPIO12 being I2C SDA on other maps?), and why alpha.435 reported a real 6.8/pic16f1847 banner. Forced mode 2 does not help either: its recovery probe (TASK-1031) would flip it back to Classic on the same false ETX.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root cause of the false ETX on the OTGW32 found with evidence (USB-JTAG console capture of the boot on COM8, or a logic/serial capture)
- [ ] #2 Detection requires more than a single 0x03 byte (for example ETX followed by a parsed banner or a PR=A reply) before it persists Classic; host test red on HEAD, green on fix
- [ ] #3 OTGW32 in auto boots OT-Direct (otdirectavailable true) and boardmode stays 0 across 3 boots; a Classic with a live PIC still persists 1/3
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25 RETRACTED: Robert corrected that the unit is a Classic with an S3 (live PIC), not the OTGW32. Detection was right: /bootdetect.log pic=1 mode=1, and 1 min after boot device/info reads picfwversion 6.8, pic16f1847, gateway. The 'no pic found' right after boot was the banner not yet received. Not a bug.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Not a bug: retracted. The unit on the bench was a Classic with an S3 and a live PIC; combo detection correctly persisted iBoardMode=1 (ADR-201).
<!-- SECTION:FINAL_SUMMARY:END -->
