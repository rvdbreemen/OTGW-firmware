---
id: TASK-1918
title: >-
  The safeboot updater overwrites the RTC reboot reason at 0x50000200, so a
  restart through safeboot logs no planned reason
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T02:34:00Z'
updated_date: '2026-11-14T14:32:00Z'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Mechanism: a 32 B RTC_NOINIT guard array defined in OTGW-firmware.h (top of the single translation unit, ADR-044), so the linker places it first at 0x50000200, the range safeboot's bootloader load overwrites. The other records shift up by 32 B; RTC slow memory left after the main .rtc.force_slow: 0x2000-0x1f94 = 108 B (tight, noted for TASK-1931 and later RTC users). 2. Gate (AC2): scripts/check_rtc_layout.py reads the ELF with the toolchain nm and fails when any main-firmware symbol in .rtc_noinit, other than the guard, starts below 0x50000220; build.py runs it after the esp32 ELF copy and fails the build. Red on the beta.243 ELF, green on the fix build. Host test tests/test_rtc_layout_check.py on synthetic nm output (old layout red, guard layout green, guard missing red, a record moved below 0x220 red). 3. No safeboot change and no safeboot build: safeboot's 0x20 B stay as they are. 4. Bench (09, WiFi only, no USB): app OTA through safeboot on .63; the boot after safeboot shows '- [OTA] handover to safeboot' (AC3). 5. Open: a Proposed ADR for the cross-image RTC layout rule (first 32 B of RTC slow memory belong to safeboot), with the gate per ADR-080.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Verified (2026-11-14, primary sources): safeboot ELF (wt-1924 build 12:28) LOAD segment VirtAddr 0x50000200 FileSiz 0x20 (s_sleep_sub_mode_ref_cnt, .rtc.force_slow; readelf -l); main ELF beta.243+c6dc184: g_rtcRebootReason 0x50000200 size 0x34, g_rtcWifiDeadRestarts 0x50000234, g_rtcWifiSkipPortal 0x5000023c, g_fsOtaRtc 0x50000244 size 0x1d10, _rtc_noinit_end 0x50001f54, main .rtc.force_slow 0x50001f54-0x50001f74 (nm -n -S). Device evidence on the old code (09's .63 reboot_log from the beta.241 bench, scratchpad b241/rlog.bin): each app OTA shows the boot after safeboot without a reason ('11:59:31 Software reset (3) [rst 0x0c/0x0c]', also 11:26:31 and 11:20:04) where 'POST /safeboot' stored '[OTA] handover to safeboot' (OTGW-ModUpdateServer-esp32.h:440), while the filesystem OTA right after it, which does not pass safeboot, keeps its reason ('12:00:22 ... - [OTA] Rebooting...', :420). That is the AC1 symptom; 09 judges and ticks AC1.

Implementation (coordinator GO 2026-11-14): 32 B RTC_NOINIT guard g_rtcSafebootGuard in OTGW-firmware.h (used attribute + one volatile read in setup() after the capture*() calls, which the wiring tests require first, so --gc-sections keeps it); scripts/check_rtc_layout.py (toolchain nm -n -S on the ELF: fails when a .rtc_noinit symbol other than the guard starts below 0x50000220, or the guard is missing or not at 0x50000200) called from build.py collect_pio_artifacts() right after the ELF copy for the ESP32 family, failing the build; ADR-239 Proposed (binding, gate per ADR-080). Gate red on the real beta.243 ELF (g_rtcRebootReason at 0x50000200). Upgrade transition (coordinator point 2), real code: test_reboot_reason case 8 (old reason, WiFi records and snapshot header read 0x20 B in: no reason, 3 reasons incl. the longest), test_wifi_skip_portal and test_wifi_deadlink subtests (old snapshot data[] bytes, zeros and the real settings.ini head: portal opens, count restarts from 0), test_fs_ota_rtc_snapshot T13 (stale armed, not consistent snapshot read 0x20 B higher: 0 formats, user's settings kept; unshifted control: 1 format) with MB6 now also failing T13. Host: 13 related test files PASS (rtc_layout_check 7, reboot_reason 10, wifi_skip_portal 38, wifi_deadlink 29, fs_ota_rtc_snapshot, reboot_log_reset_raw, reboot_log_time_not_set, crashlog_coredump, static_integrity, ino_prototype_hoisting, build 16, build_safeboot 11, build_staging 18, build_lock, build_jobs, build_config_path, build_bat_call_probes, firmware_ota_path); test_adr_governance 17/17; evaluate --quick 85/0. RTC slow left after the fix: 108 B. Still to prove on the real fix ELF: the guard lands at 0x50000200 (gate result in the build).

Rebased onto origin/dev fbcf1e6db (09's beta.246, TASK-1936): code identical (271 lines excl. banners); re-bump beta.246 -> beta.247.
<!-- SECTION:NOTES:END -->
