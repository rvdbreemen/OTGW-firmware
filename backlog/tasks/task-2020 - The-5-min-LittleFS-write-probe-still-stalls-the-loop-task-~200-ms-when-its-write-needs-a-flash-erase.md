---
id: TASK-2020
title: >-
  The 5-min LittleFS write probe still stalls the loop task ~200 ms when its
  write needs a flash erase
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T10:06:00Z'
updated_date: '2026-11-18T12:07:00Z'
labels:
  - littlefs
  - performance
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1037000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1995 Step B (beta.370). With the probe on the fsWriter task, 09 still saw '[fs-slow] write-probe 228 ms' and '[loop-stall] 217 ms gap, slowest step 5->20 202 ms' (370+ad83aff, .61, 10:53:05), while the mount-check stalls were gone. Hypothesis (unverified, cite the ESP-IDF doc 'Concurrency Constraints for Flash on SPI1' and CONFIG_SPI_FLASH_AUTO_SUSPEND of our S3 libs before acting): a flash erase/write via SPI1 disables the cache on both cores, so core 1 stalls whichever task writes. Options: (A) accept, one ~200 ms stall per 5 min; (B) probe only after a failed mount check (loses the reads-OK-writes-fail case the probe exists for, TASK-1205); (C) keep and document; (D, coordinator) stretch the interval 5 -> 30 min; (E, coordinator) rewrite the same bytes in place without an erase, if LittleFS allows that (check: LittleFS is copy-on-write, so likely not). Decide with the numbers of the 370 respin bench; Robert only if TASK-1205's ACs change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The IDF behaviour is confirmed from the IDF documentation or source at our IDF version, quoted with a link
- [ ] #2 An option is chosen with the bench numbers and the reason recorded
- [ ] #3 The chosen option is implemented and the bench shows the write-probe stall gone or at the agreed rate
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Second topic (coordinator 2026-11-18): the loop task's own LittleFS steps (file flags every 10 s, Dallas labels, SAT markers) wait on the LittleFS lock while the writer runs a health check. Bench 370+41a9e44 (.61, 11:37:49): file-flags 124 ms two passes after the probe pass, during the writer's mount check (131 ms); 6144353 11:15:16 file-flags 136 ms; ad83aff (requests raised first) 191-394 ms with 2 stalls. Below the 200 ms stall threshold after respin 2, but a real lock wait. Candidate design (busy-skip, written and NOT built or tested on hardware): the writer sets a running flag before it takes a request; while a request is pending or running (at most 2 s after it was raised) the loop skips its own LittleFS steps without consuming their DUE() tick and does them in the next pass. Patch on 41a9e44 kept at RvdB/OTGW-worktree-leftovers/task-2020-busy-skip/busy-skip-on-41a9e44.patch. Concern (coordinator): it couples the loop's FS steps to writer state; decide together with the probe-cost options A-E, since both are about the writer and the loop sharing the LittleFS lock.

Data 41a9e44 (09, .61, 62 min): write-probe 138 -> 261 ms rising with uptime, one 613 ms at 12:17:49, then 138-139 ms; NO [loop-stall] at any probe tick, including the 613 ms one (ad83aff: write-probe 683 ms came with a 424 ms stall, but that one also had file-flags 394 ms). This WEAKENS the flash-erase hypothesis (a long erase would stall core 1 too): the earlier probe-tick stalls fit the lock wait of the loop's file-flags better. Check the IDF behaviour before acting on either.

2026-11-18 AC1 (otgw-docs, sources only, no build). Our IDF: CONFIG_IDF_INIT_VERSION "5.5.1" and '# CONFIG_SPI_FLASH_AUTO_SUSPEND is not set' in D:/DevData/platformio/packages/framework-arduinoespressif32-libs/esp32s3 sdkconfig. IDF v5.5.1 ESP32-S3 'Concurrency Constraints for Flash on SPI1' (https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/spi_flash/spi_flash_concurrency.html), quoted: 'If this option [CONFIG_SPI_FLASH_AUTO_SUSPEND] is disabled, the caches must be disabled while reading/writing/erasing operations.' and 'The way that these APIs disable the caches suspends all the other tasks. Besides, all non-IRAM-safe interrupts will be disabled. The other core will be polling in a busy loop. These will be restored until the Flash operation completes.' So a flash write or erase started on the fsWriter task (core 0) also stops the loop task on core 1 for its whole duration: Step B moved the probe off the loop task but cannot move the erase stall off it. That matches 09's 370+ad83aff '[fs-slow] write-probe 228 ms' with '[loop-stall] 217 ms ... 5->20 202 ms' at one tick, and a 613 ms probe without a stall on 41a9e44 (the probe time includes lfs_fs_size() reads, which disable the cache only per short read; only a write that needs an erase stops the other core for long). Auto suspend, 'Optional Features for Flash' v5.5.1 S3 (https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/spi_flash/spi_flash_optional_feature.html): 'This feature is only supported on ESP32-S3, ESP32-C2, ESP32-C3, ESP32-C6, and ESP32-H2 for now', chips 'XM25QxxC series, GD25QxxE series, FM25Q32', and 'There are multiple limitations about the auto-suspend feature'. Not verified: which flash chip our S3 Mini / OTGW32 boards carry (esptool flash_id would tell). A second, separate mechanism: the LittleFS lock wait (file-flags 107/124 ms on the loop task while the writer runs a check, 370 bench); the parked busy-skip patch (RvdB/OTGW-worktree-leftovers/task-2020-busy-skip/busy-skip-on-41a9e44.patch) addresses that one only, not the erase stall. The probe itself: updateLittleFSStatus('/.health') opens, writes 'ok' and closes every 5 min (helperStuff.ino, fsWriteProbe DECLARE_TIMER_MIN 5 :317), only to set LittleFSmounted false when a write fails; it dates from the 1.x line (9e5b13cd5).

2026-11-18 RECOMMENDATION for AC2 (otgw-docs, for otgw-dev who implements AC2/AC3): (A) drop the write from the 5-min probe and keep it read-only (mount state plus platformFSInfo); report a filesystem that cannot be written from the real writes (fsWriter service result, fsCommitTempFile failure) instead. That removes the periodic erase and its whole-chip stall at the source, and saves about 288 small writes a day of flash wear. Risk: a mounted but unwritable FS is noticed at the next real write instead of within 5 min; with SAT on the PID file is written every 5 min anyway. (D) add the parked busy-skip patch for the separate lock-wait stalls. Not recommended: (B) a rarer probe only lowers the rate, the size stays; (C) CONFIG_SPI_FLASH_AUTO_SUSPEND needs a supported flash chip on every board (unverified), a custom sdkconfig build of the IDF libs, and the IDF names multiple limitations, too much for 2.0.0. Bench for AC3 (09): [loop-stall] with step 5->20 at probe ticks over 60 min, old 370 vs fix, same FS state.
<!-- SECTION:NOTES:END -->
