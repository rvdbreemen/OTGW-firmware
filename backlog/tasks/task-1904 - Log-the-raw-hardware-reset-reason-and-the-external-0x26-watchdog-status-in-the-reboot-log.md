---
id: TASK-1904
title: >-
  Log the raw hardware reset reason and the external 0x26 watchdog status in the
  reboot log
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T16:52:00Z'
updated_date: '2026-11-14T09:12:00Z'
labels:
  - bug
  - stability
  - diagnostics
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 969000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-1865/1899 trace (gruijter beta.218, 13 Nov): two resets were reported as 'Other watchdog' (ESP_RST_WDT, platform_esp32.h:216), which covers RTC, super and CPU-level watchdogs, and left no core dump, so the actual watchdog cannot be told apart. On the Classic the 'Reset by External WD' text from the 0x26 probe goes into a local buffer at OTGW-firmware.ino:545 that is discarded, and the boot telnet line comes before a capture reconnects. Persist in reboot_log.txt / device info: the raw hardware reset cause (rtc_get_reset_reason / esp_rom reset reason per core, the 'rst:0x..' code) and whether the 0x26 watchdog reported an external reset, so field resets can be classified without a USB console.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 reboot_log.txt and /api/v2/device/info carry the raw per-core hardware reset reason code next to the mapped text
- [ ] #2 On the Classic the 0x26 external-reset indication is persisted to the reboot log
- [ ] #3 Bench: a forced task-watchdog reset and a forced RTC-watchdog reset produce different, correct entries
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 analysis (ba lane, before code): (1) the 0x26 status in the reboot log is dead code on the S3: updateRebootLog() (helperStuff.ino:470) reads it only when platformIsExternalReset() (esp_reset_reason()==ESP_RST_EXT, platform_esp32.h:791) is true, and the S3 mapping get_reset_reason() (local framework-espidf esp_system/port/soc/esp32s3/reset_reason.c) never returns ESP_RST_EXT (caveat: pinned libs precompiled, not proven identical). Also initWatchDog() (OTGW-Core.ino:1564, presence probe + status read) runs at OTGW-firmware.ino:564, after updateRebootLog() at :541. (2) 'Other watchdog' (ESP_RST_WDT) covers six raw S3 codes (soc/esp32s3/include/soc/reset_reasons.h): 0x09 CORE_RTC_WDT, 0x10 SYS_RTC_WDT, 0x12 SYS_SUPER_WDT, 0x0D CPU_RTC_WDT, 0x0B CPU_MWDT0, 0x11 CPU_MWDT1; esp_rom_get_reset_reason(cpu) per core separates them. 0x01 is shared by power-on, chip brown-out and the chip-level super WDT. (3) Not verified: how the 0x26 chip resets the ESP physically (probably EN/CHIP_PU -> 0x01); its status byte bit 0 is the authoritative signal. Plan sent to the coordinator (platform shim for the raw code; read the 0x26 status before the reboot log is written; raw code + 0x26 line in reboot_log.txt; resetraw/extwdreset in device/info; host harness old/fix; throwaway probe build for the AC3 bench).

Implementation (coordinator GO): platform shim platformResetRawCode(core)/platformResetRawCodes()/platformResetRawLabel() (platform_esp32.h, esp_rom_get_reset_reason; labels for every S3 soc_reset_reason_t value, 0x01 = 'power-on, brown-out or super watchdog (not distinguishable)'). readExtWatchdogResetFlag() (OTGW-Core.ino, HAS_PIC_WATCHDOG; stub on OTGW32) reads the 0x26 status byte once, right after the boot-time disarm in setup() and before updateRebootLog() and initWatchDog(). TinyI2CWatchdog source (rvdbreemen/ESPEasySlaves TinyI2CWatchdog.ino, single source): Status bit 0 is set in resetTarget() (a 500 ms low pulse on the ESP reset line) and cleared only by a feed (0xA5) or the chip's own power-up; a read and a register-7 disarm leave it, so the read before the first feed is valid. updateRebootLog(): the dead platformIsExternalReset() gate replaced by the cached flag, own line 'External watchdog 0x26 reset the ESP (status bit 0)'; boot line gets '[rst 0xNN/0xNN]' (core 0/core 1). LOG_LINE_LENGTH 140 -> 160 (worst case 157 incl. CRLF; the readers use REBOOT_LOG_LINE_MAX 240) plus a guard that ends a cut-off line with CRLF (before, a cut line ran into the next line of the file). device/info: resetraw, resetrawlabel, extwdreset (sendDeviceInfoV2, not sendHealth). Not changed: the 0x26 presence probe and feed logic in initWatchDog().

Host evidence: new test/host/test_reboot_log_reset_raw.py (+ .cpp; the real updateRebootLog() and copyOlderRebootLogLines(), the real label table checked against the pinned libs' reset_reasons.h, 18 codes). FIX PASS (E1-E4, S1-S4). OLD ff5eb599e fails E1 (0x26 reset not in the log), E2 (no raw code), E4, S1-S4. Mutants MC1 (gate back to platformIsExternalReset) E1, MC2 (no raw code) E2/E4, MC3 (flag read after the log) S1, MC4 (0x01 label guesses) S4, MC6 (140 without the CRLF guard: the boot line runs into the next) E4; MC5 (140 with the guard) passes, which shows the guard keeps a cut line whole. tests/test_reboot_log_time_not_set.py: the pinned format string updated to the new one (the only line parser, rebootLogLineHasCause(), uses strstr ' - reboot cause: ' and is unaffected). 12 related test files: 12 PASS after that edit; evaluate --quick 85/0. Not yet: the I2C timing of the early read on a board without the chip (coordinator point 2), and AC3 (bench, throwaway probe build).
<!-- SECTION:NOTES:END -->
