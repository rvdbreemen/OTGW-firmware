---
id: TASK-1972
title: Capture fetched an old core dump while device/crashlog says none is available
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T11:57:00Z'
updated_date: '2026-11-15T21:29:00Z'
labels:
  - capture
  - coredump
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1009000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by 09 on .63 (2026-11-15, TASK-1964 bench): the capture tool fetched a 31268-byte coredump-*.bin (ESP core dump header + ELF) from GET /api/v2/device/coredump while /api/v2/device/crashlog reported available:false. Probably an old dump that stays in the coredump partition after the crash it belongs to. Risk: someone analyses an old dump as a new crash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Find out from the code how device/crashlog decides available and how device/coredump decides to serve the partition; say which is right for an old dump (with file:line)
- [x] #2 At minimum the capture labels a core dump with what tells its age (boot count / reset reason / app sha vs the running firmware) in the summary and the file name, so an old dump is not taken for a new crash; host test on the fetch
- [ ] #3 If the firmware should not serve or should flag a dump that belongs to an earlier crash: fix with a host test red/green, openapi updated
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC1 (code, origin/dev 35ebdc225): the two endpoints read different stores. GET /api/v2/device/crashlog (restAPI.ino:3607 sendDeviceCrashLog) returns readLatestCrashLog() (helperStuff.ino:371): the newest reboot-cause line in /reboot_log.txt followed by a detail line ('ESP register contents:', 'External Reason:' or 'Core dump:', helperStuff.ino:353). GET /api/v2/device/coredump (restAPI.ino:3637 sendDeviceCoreDump) serves the core-dump partition whenever esp_core_dump_image_check() passes (platform_esp32.h:985 platformCoreDumpSize()). Nothing in src/ ever erases that partition (no esp_core_dump_image_erase call), so a dump stays until the next crash overwrites it; the boot after the crash writes it into the reboot log once ('Core dump: <task> pc=... elf=<8 hex> bt=...', platform_esp32.h:954, NVS fingerprint otgw_cd/fp so only once), and that log keeps REBOOT_LOG_LINES 20 lines (OTGW-firmware.h:248). After enough reboots, or a filesystem reset, the crash pair is gone while the dump remains: crashlog available:false next to a served dump is that state. Both endpoints are right for their own store; neither says the dump's age.

AC2: scripts/otgw_capture.py coredump_age() + fetch_coredump(): after a 200 the tool reads device/crashlog and device/info (no credentials) and names the file coredump-<run>-app-<first 9 hex of the dump's ELF sha256>-<latest|old|unknown>.bin; the summary says latest when the newest logged crash's 'elf=' matches the dump, OLD when the log holds no crash or a crash of another ELF, with the running fwversion and the last reset. tests/test_capture_tool.py: 4 new cases (latest, old without a logged crash = 09's .63 case, old with another ELF, unknown), red on the old tool (4 failed), green on the fix; CoreDumpFetchCase 11/11, test_capture_tool 57 (2 skipped), test_capture_device_data 18/18. Side fix in the same lane: test_capture_device_data was red on origin since 3a156a401 (TASK-1963: mergeGraphHistory() calls graphTrim()) and the web UI CSV gained a live-only dhw column (8cdb13ae4, TASK-1776); the node harness now cuts graphTrim() too (constants read from v2.js) and the capture CSV carries the empty dhw column, byte-identical to the web UI export again. Not used: tonight's real probe dump from .61 as a fixture (in 09's scratchpad, not checked in); the age logic only reads the sha header and the crashlog JSON.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: the two endpoints read different stores. device/crashlog is the newest reboot-cause + detail pair in /reboot_log.txt (helperStuff.ino:371, 20 lines, OTGW-firmware.h:248); device/coredump serves the core-dump partition whenever its checksum holds (platform_esp32.h:985); nothing in src/ erases that partition, so an earlier crash's dump stays downloadable after its log lines rolled out: 09's .63 state (crashlog available:false next to a 31 KB dump). Both are right for their own store. AC2: otgw_capture.py coredump_age(): the dump is named coredump-<run>-app-<9 hex ELF sha>-<latest|old|unknown>.bin, latest when the newest logged 'Core dump: ... elf=<8 hex>' matches the dump's sha, and the summary gives the reason with the running fwversion and the last reset; 4 new cases in tests/test_capture_tool.py red on the old tool (4 failed), green on the fix; test_capture_tool 57 (2 skipped), test_capture_device_data 18, the 6 other files that import the tool 14/14, all -j 1. Same commit fixes test_capture_device_data, red on origin since 3a156a401 (graphTrim) and 8cdb13ae4 (live-only dhw CSV column). AC3: split into TASK-1983 (coordinator decision 2026-11-15), not needed for 09's case: the firmware does not publish the running image's ELF sha256, which an exact same-build test would need. Pushed in e22c59ed1.
<!-- SECTION:FINAL_SUMMARY:END -->
