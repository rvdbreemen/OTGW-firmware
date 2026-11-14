---
id: TASK-1904
title: >-
  Log the raw hardware reset reason and the external 0x26 watchdog status in the
  reboot log
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T16:52:00Z'
updated_date: '2026-11-14T10:39:00Z'
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

Budget cross-check with ADR-233 (coordinator): LOG_LINE_LENGTH 160 adds at most 20 x 16 B = 320 B to reboot_log.txt; core-dump lines (REBOOT_LOG_LINE_MAX 240) already allowed a ~4.8 KB log before this task. The RTC snapshot (7424 B data) stores settings.ini, counter, log each whole or not at all, in that order: a log that does not fit is skipped (settings and counter stay), the snapshot does not fail. New host cases in test_fs_ota_rtc_snapshot.py: T11 worst case (settings.ini 5700 B, log 3200 B) -> settings and count restored, log not restored (skipped); T12 the .63 sizes (5659 B, 1178 B) plus 320 B -> all three restored. Harness FIX PASS, mutants ok (MB6 now also caught by T11/T12). On .63 the margin drops from ~580 B to ~260 B. ADR-233 Consequences got a line with these numbers (still Proposed, awaiting Robert's in-session yes).

Key X (stop the 0x26 feed) and the PIC, checked before the bench (coordinator): TinyI2CWatchdog.ino (rvdbreemen/ESPEasySlaves master, lines 405-413, 460) pulls Settings.ResetPin low, an EEPROM setting with default 1 (ATtiny pin number, not a PCB net). Which net that pin drives on the Classic PCB (ESP RST/EN only, or also the PIC MCLR) is not in the source, not in docs/hardware/PINOUT.md and not in memory reference_otgw_classic_0x26_watchdog_history; the schematic is not in the repo. Not verifiable here, so X stays out of the bench until Robert says what the 0x26 reset line drives. For T and W: an ESP reset is followed by detectPIC() in setup() (OTGW-firmware.ino:412/433), which resets the PIC as on every boot (ADR-201); that is the normal reboot path, no extra PIC action.

Hardware fact, source Robert (DM 1571095401417809941, relayed by the coordinator, 2026-11-14, answer A): on the Classic PCB the 0x26 watchdog's reset line goes only to the ESP's reset, not to the PIC. So probe key X (stop the 0x26 feed) is in 09's AC3 brief, on .63 only and only if its boot telnet shows 'INIT : external 0x26 watchdog present'. The armed 0x26 timeout is not verified from source; the brief says wait up to 3 min. Cross-check on that trial: initWatchDog()'s own existing read logs 'INIT : Reset by external WD!' (OTGW-Core.ino:1613).

Probe build 2.0.0-probe1904b FAILED to compile (2026-11-14 10:56): "'g_extWdResetAtBoot' was not declared in this scope" at OTGW-firmware.ino:546 (and the probe's g_probeExtWdReadUs at :310). Cause: the flag was defined in OTGW-Core.ino, which follows OTGW-firmware.ino in the single translation unit; the .ino prototype hoisting covers functions, not globals. The host harness compiled extracted functions with stubs and could not see it. Fix: the global moves to OTGW-firmware.h next to lastResetRaw (ADR-044, single point of instantiation); both definitions in OTGW-Core.ino removed. New static case S5 (flag defined once, in OTGW-firmware.h) and mutant MC7 (flag back in an .ino) in test_reboot_log_reset_raw.py: FIX PASS, OLD ff5eb599e fails S5 too, MC7 caught. tests/test_ino_prototype_hoisting.py OK.

AC3 bench by 09 (probe 2.0.0-probe1904c+72437d9, zip sha256 2417a8cc...217f0, OTA app+FS, githash checked) on .61 and .63, 2 trials per key per unit. T (task WDT): reboot after 39-46 s; device/info lastreset 'Task watchdog', resetraw 0x0c/0x0c, label 'software reset (CPU)', extwdreset false; reboot log 'Task watchdog (6) [rst 0x0c/0x0c]' + core-dump line. W (RTC WDT stage 0 RESET_SYSTEM): lastreset 'Other watchdog', resetraw 0x09/0x09, label 'RTC watchdog (digital core)', log 'Other watchdog (7) [rst 0x09/0x09]'. Correction of my brief: I expected 0x10; 09 checked ESP32-S3 rom/rtc.h:79 RTCWDT_SYS_RESET = 9 (digital core) vs :85 RTCWDT_RTC_RESET = 16 (digital core + RTC); a RESET_SYSTEM stage gives 0x09, so the firmware label is right and the brief was wrong. X (0x26) NOT testable: .63 boot telnet 'INIT : no external 0x26 watchdog responding' (twice); not pressed. The 0x26 hardware trigger stays untested on a unit; the code path is covered by host case E1 / mutant MC1. 0x26 read time at boot: .61 (no chip) 335-374 us, .63 287-296 us (one NACK; negligible boot cost). Every trial: bootcount +1, settings diff [] , reboot_log lines CRLF, no run-in (max line 92 B). PIC on .63 detected normally after every ESP reset (6.8), never flashed. Both units back on beta.240+a3dff10, settings diff []. Evidence: 09 scratchpad r1904/u61, r1904/u63.

Rebased onto origin/dev bebecded7 (2026-11-14): only the version-banner hunks conflicted (one per file, both sides version lines only; upstream kept by a resolver that stops on any other hunk). Net code diff excluding banners identical before and after the rebase (525 lines). Re-bump beta.240 -> beta.241 in its own commit. After the rebase: both harnesses PASS (reset-raw incl. --old-rev ff5eb599e, RTC snapshot), 13 related test files PASS, evaluate --quick 85/0.
<!-- SECTION:NOTES:END -->
