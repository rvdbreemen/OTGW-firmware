---
id: TASK-2968
title: >-
  Flash tool: no factory wipe without confirmation, and the right setup-WiFi
  name after a reset (CLI review C1 C3)
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-20T15:35:00Z'
updated_date: '2026-11-25T05:15:00Z'
labels:
  - tools
  - ux
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1100000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2963 CLI review (docs/reviews/2026-11-20_ux-cli-tools/README.md, C1 and C3). Related: TASK-2962 (/flash skill, same flash_otgw scripts; one owner). C1: a plain run of flash_otgw.bat or flash_otgw.sh, which is what a double-click gives, runs FACTORY mode (write_flash -e: WiFi credentials and settings erased) right after the summary, with no confirmation (flash_otgw.bat:87, 202-237; flash_otgw.sh:564-580). C3: after a factory reset the gateway opens its setup portal as <hostname>-<MAC with colons> with default hostname OTGW2 (networkStuff.ino:321-332, OTGW-firmware.h:136), for example OTGW2-A4:CF:12:21:B4:F8; the .bat says OTGW-<MAC> (flash_otgw.bat:283-284), the .sh says OTGW-AP (flash_otgw.sh:585), and manual ch02:253 says OTGW-<MAC>.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A run of flash_otgw.bat or flash_otgw.sh without arguments shows a menu (1 update firmware keeping WiFi and settings, 2 factory reset erasing everything, Q quit) and never erases flash without a typed confirmation
- [ ] #2 Factory mode, from the menu or explicit arguments, asks the user to type ERASE before esptool runs; a new --yes flag skips the question and the bench automation (scripts/test_flash_automation.py or the bench recipe) uses it
- [ ] #3 Both tools name the setup network after a factory reset as OTGW2-<MAC> with the example OTGW2-A4:CF:12:21:B4:F8; no OTGW-AP or OTGW-<MAC> remains for the setup portal; manual ch02:253 fixed the same way (otgw-docs)
- [ ] #4 .bat and .sh behave the same; evidence: a transcript of each tool run without a board attached showing the menu and the ERASE prompt (no flash performed)
- [ ] #5 Interaction follows docs/reviews/2026-11-25_flash-tool-interaction-spec/README.md: a bare interactive run shows the 1/2/Q menu (Enter = update); a bare run without input exits 2 and flashes nothing
- [ ] #6 Factory mode asks for the case-sensitive word ERASE; anything else prints 'Cancelled: nothing was flashed.' and exits 1; --yes (-y) skips only that question; --update and --migrate ask nothing new
- [ ] #7 Existing tests (test_flash_otgw_sh_update, test_flash_usb_pid_filter, test_flash_migrate_settings_compare, test_bat_doubleclick_pause) stay green and new tests cover menu, ERASE (right/wrong word, closed stdin) and --yes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 otgw-ux-designer: interaction spec with exact texts, exit codes, edge cases and parity rules in docs/reviews/2026-11-25_flash-tool-interaction-spec/README.md. Scripts only, no firmware change. Note: --factory already exists in both tools but equals the default, so a bare run cannot be told from an explicit --factory today; the spec adds a 'mode chosen' notion (--update/--factory/--migrate/--bin/--app/--fs). C2 (pause on double-click) is already built; the pause must also run after a cancelled ERASE or quit.
<!-- SECTION:NOTES:END -->
