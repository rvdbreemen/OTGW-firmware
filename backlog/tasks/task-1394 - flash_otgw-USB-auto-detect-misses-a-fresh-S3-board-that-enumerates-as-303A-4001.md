---
id: TASK-1394
title: >-
  flash_otgw: USB auto-detect misses a fresh S3 board that enumerates as
  303A:4001
status: To Do
assignee: []
created_date: '2026-10-25T17:21:00Z'
updated_date: '2026-10-30T21:56:00Z'
labels:
  - tooling
  - flash
milestone: 2.0.0
dependencies: []
ordinal: 533000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
martreides (Marthreides, #alpha-testing 1563963515872608460, 2026-10-25) and Schelte Bron (otgw, support channel) hit the same: the flash scripts auto-detect only USB VID/PID 303A:1001 (flash_otgw.bat:183-188, flash_otgw.sh:266/:421-429), but the board came up as ID_MODEL_ID=4001 before the first flash (factory firmware with its own USB stack); --port works; after our first flash it identifies as 1001 (built-in USB-Serial-JTAG). Fix idea: also accept 303A:4001 (check esptool --port-filter semantics for several PIDs), or fall back to listing candidate ports; update README text.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A board enumerating as 303A:4001 is found without --port (repro with the PID filter, red on HEAD), both .bat and .sh
- [ ] #2 README/usage text names both IDs
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 triage: Schelte (.otgw, #nederlandse-ondersteuning 1563931061094981744, 2026-10-25) also reports: after WiFi provisioning the browser could not connect although the board answered ping; a power cycle fixed it. Possibly separate from the VID/PID detection; needs its own repro if it recurs.

2026-10-30 maintainer decision (Robert, in session otgw-firmware-ea): released for pickup ('vrijgeven'); label needs-maintainer-ok removed. Priority after beta-1 blockers unless it is one.
<!-- SECTION:NOTES:END -->
