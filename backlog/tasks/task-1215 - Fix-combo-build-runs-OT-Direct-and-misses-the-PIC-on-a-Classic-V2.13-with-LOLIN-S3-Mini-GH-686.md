---
id: TASK-1215
title: >-
  Fix: combo build runs OT-Direct and misses the PIC on a Classic V2.13 with
  LOLIN S3 Mini (GH #686)
status: To Do
assignee: []
created_date: '2026-10-06 15:42'
updated_date: '2026-10-06 19:42'
labels:
  - bug
  - needs-info
  - esp32
  - pic
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/686'
priority: high
ordinal: 335000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
GitHub #686 (temnyvlad, 2026-10-06). Hardware: Nodo Shop OTGW V2.13 (2025), PIC16F1847 with gateway 6.8 (answers PR=A/PR=M over USB with the S3 removed), LOLIN S3 Mini pre-flashed by Nodo Shop, USB-C power, THERM/BOILER not connected. Firmware v2.0.0-alpha.376. Symptom: UI says 'No PIC detected ... (OT-Direct / OTGW32)', Monitor shows OT-Direct active, PR=A from the UI answers 'OpenTherm Gateway OTGW32' (OTDirect.ino:3188, the ESP answering itself), OT-Direct mode was Master out of the box and logs its own TSet 45.00. Reporter holds off wiring to a boiler.
Open questions: (1) which image Nodo Shop flashed: esp32 (OTGW32, HAS_PIC=0, never probes a PIC) or esp32-combo (ADR-127 runtime detection) - the asset token answers it; (2) if combo, why detectPIC() missed a live PIC (the reporter is on alpha.376; only TASK-1179, banner re-enable, landed after it); (3) whether OT-Direct Master on a Classic carrier drives pins that are wired to the PIC.
Workaround and discriminator: settings boardmode=1 (Classic S3 Mini) forces the PIC path (OTGW-firmware.ino setup, iBoardMode 0=auto 1=Classic 2=OT-Direct 3=Classic Pro).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reporter data collected: firmware asset name/target, /api/v2/device/info (board, hardware_type, hardwaremode, picfwversion, otdirectavailable) and the telnet boot log with the 'Board mode:' line, before and after setting boardmode=1
- [x] #2 Root cause identified from that data (wrong image, detection miss, or hardware), with evidence
- [ ] #3 Fixed or answered: a combo build on a Classic V2.13 with a live PIC selects the PIC path in auto mode, or the image/flash guidance is corrected; reporter confirms
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-06: replied on GH #686 (approved by the maintainer): keep the boiler disconnected; asked for the exact firmware file/version, and for /api/v2/device/info plus the telnet boot log ('Board mode:' line) after forcing boardmode=1. Waiting for the reporter.

2026-10-06 18:15 reporter data (GH #686): fwversion 2.0.0-alpha.376+local, compiled 2026-09-25 20:28, build #6933 (a local build by Nodo Shop: 'the latest version from GitHub'). device/info: board 'Nodoshop OTGW32 (ESP32-S3)', hardware_type otgw32, hardwaremode OT-Direct, otdirectavailable true. Telnet banner 'PIC : no pic found', no 'Board mode:' line; settings have no boardmode key (181 keys). With the S3 removed the PIC answers PR=A 'OpenTherm Gateway 6.8'.
ROOT CAUSE (AC#2): wrong image. Nodo Shop flashed the PIC-less esp32 (OTGW32, OT-Direct) target, which never probes a PIC and has no boardmode setting (that exists only with HAS_RUNTIME_HW_DETECT, the combo build). Not a detection bug in the combo path. Own mistake in the first reply: the settings POST format is {"name":"boardmode","value":1}, not {"boardmode":1}; on this build it would not have mattered.
Open: there is no public 2.0.0 release, so pointing the reporter at a combo/classic image needs a maintainer decision. Also worth telling Nodo Shop which target to pre-flash on Classic boards.

2026-10-06: replied on GH #686 (maintainer chose 'let the reporter build'): wrong image explained without naming the board; corrected my POST-format mistake; build instructions for the esp32-combo target from dev (submodules, build.sh/build.bat) and a full flash of the merged-full image with flash_otgw.sh/.bat (wipes WiFi and settings). Partition tables of the esp32 and combo targets are identical. AC#3 waits for the reporter's confirmation that the PIC is found.
<!-- SECTION:NOTES:END -->
