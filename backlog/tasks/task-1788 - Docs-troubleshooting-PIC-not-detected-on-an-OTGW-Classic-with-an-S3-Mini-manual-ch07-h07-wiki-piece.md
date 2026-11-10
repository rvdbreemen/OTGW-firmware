---
id: TASK-1788
title: >-
  Docs: troubleshooting 'PIC not detected' on an OTGW Classic with an S3 Mini
  (manual ch07/h07 + wiki piece)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-10T22:23:00Z'
updated_date: '2026-11-10T22:35:00Z'
labels:
  - docs
  - support
milestone: 2.0.0
dependencies: []
ordinal: 882000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Users get stuck when the combo firmware on a Classic with a LOLIN S3 Mini does not find the PIC (TASK-1786, thaghostnl). Add a troubleshooting section to docs/manuals/en/ch07-troubleshooting.md and docs/manuals/nl/h07-probleemoplossing.md, beginner first then deeper, plus a short wiki-ready piece. Every claim checked against src/OTGW-firmware (file:line in the notes) and ADR-201. Docs-only; coordinator gates the push.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 EN section in ch07 covers symptom (device/info hardwaremode Degraded, otcommandinterface None), seating check, Board Mode auto/pic/otdirect, /bootdetect.log via FSexplorer, capture across a restart, what to send
- [x] #2 NL section in h07 with the same content
- [x] #3 Short wiki-ready piece in docs/ (EN)
- [x] #4 Every behaviour claim has a file:line citation in the task notes; no OTGW32 naming in the user-facing text
- [ ] #5 Committed locally on dev; pushed only after the coordinator's go
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fact sources (dev c21f07f03): detectPIC resets the PIC and waits for ETX; ETX -> HW_MODE_PIC, miss -> HW_MODE_DEGRADED (OTGW-Core.ino:1053-1078). Boot resolution: iBoardMode 2 forced OT-Direct with recovery probe (OTGW-firmware.ino:394-418); 1/3 forced PIC, detectPIC once (:427-429); 0 auto, 3 tries 60 ms apart, persists only a positive verdict (:441-475, ADR-201). Auto + no PIC on a LOLIN module (PSRAM or Pro IMU) does not start OT-Direct, stays Degraded (:466-471, OTGW-firmware.h:854-861 isOtgw32HardwarePossible). Detection runs before startTelnet (:383-491 vs :516); SetupDebug goes to debugTelnet (OTGW-firmware.ino:40-44, debugStuff.h:71-72), so a telnet capture never shows the boot detection lines. /bootdetect.log: newest first, 30 lines, format boot#N t= eMode pic mode pro RST RX TX I2C (OTGW-firmware.ino:149-213). eMode 0 unknown,1 PIC,2 OT-Direct,3 Degraded (Hardwaretypes.h:25-30). hardwaremode names PIC/OT-Direct/Degraded/Detecting (OTGW-firmware.h:668-675), device/info hardwaremode + otcommandinterface PIC/OT-Direct/None (restAPI.ino:3320, 3402-3404). Runtime retry 'PIC is unknown, probe pic using PR=A' each minute while no device id (OTGW-firmware.ino:890-899). Classic S3 Mini pins RST=12 RX=44 TX=43 I2C=35/36 (boards.h:264-268). Web UI Board mode values 0 Auto-detect, 1 PIC (Classic), 2 OT-Direct (no PIC) (v2.js:5130, 1636-1638), change confirm text v2.js:991. S3 Mini outer row = D1 mini footprint, inner row = extra GPIOs (docs/hardware/PINOUT.md:116-118).

Written: docs/manuals/en/ch07-troubleshooting.md 7.1a, docs/manuals/nl/h07-probleemoplossing.md 7.1a, docs/wiki-drafts/PIC-not-detected-on-Classic-with-S3-Mini.md. Correction to the brief: the boot probe is reset + wait for ETX (OTGW-Core.ino:1053-1078), not PR=A; PR=A is the once-a-minute runtime retry in doTaskEvery60s (OTGW-firmware.ino:853, 890-899). The capture tool does not fetch /bootdetect.log (scripts/otgw_capture.py has no reference), so the docs ask for it separately. Board mode labels are English only in v2.js (1636-1638), so the NL text uses the English UI labels. Example t= values in the bootdetect samples are marked as examples. tests/test_manual_sat_commands.py: 6 passed. grep for OTGW32 and em dashes in the added lines: 0.

Review fixes: (1) Auto mode closes the PIC UART after a missed boot probe (OTGWSerial.end() OTGW-firmware.ino:462; begin only at :405/:421), so the per-minute PR=A cannot reach the PIC in Auto; in forced PIC (1) the UART stays open and a banner reply sets bAvailable + HW_MODE_PIC (OTGW-Core.ino:841-843). Docs now scope the no-reboot recovery to PIC (Classic) and recommend that mode while troubleshooting. (2) Telnet 'never' verified: AsyncSimpleTelnet::write returns 0 with no connected client (src/libraries/SimpleTelnet/src/AsyncSimpleTelnet.h:481), no pre-connect buffer; detection runs before startTelnet (:516). (3) Inner-row wording made a check instead of a claim about the PCB.
<!-- SECTION:NOTES:END -->
