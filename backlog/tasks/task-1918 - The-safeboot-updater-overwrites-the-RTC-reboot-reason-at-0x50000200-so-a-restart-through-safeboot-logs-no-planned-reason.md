---
id: TASK-1918
title: >-
  The safeboot updater overwrites the RTC reboot reason at 0x50000200, so a
  restart through safeboot logs no planned reason
status: To Do
assignee: []
created_date: '2026-11-14T02:34:00Z'
labels:
  - bug
  - ota
  - safeboot
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 983000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while planning TASK-1774 (ELF evidence, not seen on a device). The main firmware keeps the doRestart() reason in RTC_NOINIT g_rtcRebootReason (helperStuff.ino:287-292, TASK-1447): 0x34 B at 0x50000200 in the beta.233+6afbade ELF (nm). The safeboot ELF (safeboot/.pio/build/safeboot/firmware.elf, wt-1834 build 2026-11-14) has a LOAD segment of 0x20 B at 0x50000200 (readelf -l), the IDF variable s_sleep_sub_mode_ref_cnt in .rtc.force_slow (nm). The bootloader loads it at every safeboot start, so the magic and the first 28 reason bytes are overwritten. Consequence (inference): after an app OTA through safeboot, or any restart that passes through safeboot, the next main boot finds no valid magic and reboot_log.txt shows the plain reset cause instead of the planned reason. g_rtcWifiDeadRestarts (0x50000234) and g_rtcWifiSkipPortal (0x5000023c) are outside the overlap. Fix direction: move the record above 0x50000220 (pin or reorder), or reserve the first 32 B in the main ELF, plus a host or link check that fails when an RTC_NOINIT record of the main firmware starts below 0x50000220. Related: TASK-1774 option A needs the same rule for its snapshot.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Bench: the reason of a planned restart through safeboot is shown on device as lost on the old build (reboot_log line after an app OTA)
- [ ] #2 Fix keeps every main-firmware RTC_NOINIT record out of 0x50000200-0x5000021F, with a check that goes red when one lands there
- [ ] #3 Bench: after the fix the planned reason survives an app OTA through safeboot
<!-- AC:END -->
