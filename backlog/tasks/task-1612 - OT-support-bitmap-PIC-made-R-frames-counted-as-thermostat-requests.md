---
id: TASK-1612
title: 'OT support bitmap: PIC-made R frames counted as thermostat requests'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-04T21:28:00Z'
updated_date: '2026-11-04T23:53:00Z'
labels:
  - opentherm
  - pic
dependencies: []
priority: low
ordinal: 752000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while fixing TASK-1391. The master branch of the bitmap block (OTGW-Core.ino, masterslave == 0) counts every master frame into thermostatSentRead/Write, including R frames the PIC built itself (the R26 in T57 R26 B26 A57, or PIC poll requests). Only T frames are what the thermostat sends. Needs the same evidence path as TASK-1391 (capture + host test on the real block).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Capture shows (or refutes) PIC R frames setting thermostatSentRead/Write for ids the thermostat never sent
- [x] #2 If confirmed: only T frames set the thermostat bitmaps (host test red on HEAD, green on fix)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 (otgw-firmware-6c). AC1 confirmed in gruijter capture 20261030-092718 (alpha.549): the only R requests for an id/direction the thermostat never sent are 2 x R56 Write-Data (PIC DHW setpoint); every other R id was also sent as T by this thermostat. With the old code those set thermostatSentWrite[56]; in the T57->R26 replacement pattern the R26 also set thermostatSentRead[26]. OTDirect bridges its own requests as 'R' and the thermostat's as 'T' (OTDirect.ino:1380), so the rule holds on both paths. AC2: OTGW-Core.ino master branch: boilerLastMasterWasWrite still follows every request (T or R, the boiler answers the R); thermostatSentRead/Write only for rsptype OTGW_THERMOSTAT. /ot-thermo.json -> format 2 (format-1 file not loaded, rewritten), because thermostat bits are never retracted. Only consumer: REST /api/v2/otgw/ot-support tsR/tsW. Tests: tests/test_boiler_support_bitmap_b_only.py +3 thermostat tests; on HEAD 40effe0ea 2 FAIL ('a PIC R56 write marked thermostatSentWrite[56]', 'the PIC's R26 marked thermostatSentRead[26]'), on the fix 8/8 OK; test_fs_update_keep OK; evaluate --quick 0 failed 0 warnings. NOT host-tested: the format-2 load/save (LittleFS); bench: telnet shows 'ot-thermo.json is format 1 ... starting fresh' once. C4 doc updated.
<!-- SECTION:NOTES:END -->
