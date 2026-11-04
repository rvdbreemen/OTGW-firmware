---
id: TASK-1394
title: >-
  flash_otgw: USB auto-detect misses a fresh S3 board that enumerates as
  303A:4001
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T17:21:00Z'
updated_date: '2026-10-31T22:46:00Z'
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
- [x] #1 A board enumerating as 303A:4001 is found without --port (repro with the PID filter, red on HEAD), both .bat and .sh
- [x] #2 README/usage text names both IDs
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 triage: Schelte (.otgw, #nederlandse-ondersteuning 1563931061094981744, 2026-10-25) also reports: after WiFi provisioning the browser could not connect although the board answered ping; a power cycle fixed it. Possibly separate from the VID/PID detection; needs its own repro if it recurs.

2026-10-30 maintainer decision (Robert, in session otgw-firmware-ea): released for pickup ('vrijgeven'); label needs-maintainer-ok removed. Priority after beta-1 blockers unless it is one.

2026-10-31 worktree cleanup: uncommitted work for this task found in lane wt-1475 (flash_otgw.bat/.sh, flash_esp.py, manuals ch02/h02); archived as D:\Users\Robert\Documents\GitHub\RvdB\lane-archive-20261031\wt-1475.uncommitted.patch, lane kept for the merge.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
flash_otgw.bat/.sh auto-detect pass --port-filter pid=0x1001 AND pid=0x4001 (fresh S3 with factory firmware enumerates as USB CDC 303A:4001); flash_esp.py lists (0x303A, 0x4001); usage text and manuals ch02/h02 name both IDs. Merged from lane wt-1475 (uncommitted work found during the 2026-10-31 worktree cleanup).
Evidence: esptool keeps a port when its pid is in the pid-filter list (esptool/cli_util.py:553 'port.pid not in pids', checked in the installed C:\Python312 esptool), so repeated filters are ORed. tests/test_flash_usb_pid_filter.py: new 4/4 OK, old files 5 FAIL (bat, sh, flash_esp, usage x2). test_flash_zip_modes and test_build_safeboot still pass.
Not verified: a physical fresh board enumerating as 303A:4001 (none available); the esptool version inside the flash tool venv was not checked separately.
<!-- SECTION:FINAL_SUMMARY:END -->
