---
id: TASK-2020
title: >-
  The 5-min LittleFS write probe still stalls the loop task ~200 ms when its
  write needs a flash erase
status: To Do
assignee: []
created_date: '2026-11-18T10:06:00Z'
updated_date: '2026-11-18T11:33:00Z'
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
- [ ] #1 The IDF behaviour is confirmed from the IDF documentation or source at our IDF version, quoted with a link
- [ ] #2 An option is chosen with the bench numbers and the reason recorded
- [ ] #3 The chosen option is implemented and the bench shows the write-probe stall gone or at the agreed rate
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Second topic (coordinator 2026-11-18): the loop task's own LittleFS steps (file flags every 10 s, Dallas labels, SAT markers) wait on the LittleFS lock while the writer runs a health check. Bench 370+41a9e44 (.61, 11:37:49): file-flags 124 ms two passes after the probe pass, during the writer's mount check (131 ms); 6144353 11:15:16 file-flags 136 ms; ad83aff (requests raised first) 191-394 ms with 2 stalls. Below the 200 ms stall threshold after respin 2, but a real lock wait. Candidate design (busy-skip, written and NOT built or tested on hardware): the writer sets a running flag before it takes a request; while a request is pending or running (at most 2 s after it was raised) the loop skips its own LittleFS steps without consuming their DUE() tick and does them in the next pass. Patch on 41a9e44 kept at RvdB/OTGW-worktree-leftovers/task-2020-busy-skip/busy-skip-on-41a9e44.patch. Concern (coordinator): it couples the loop's FS steps to writer state; decide together with the probe-cost options A-E, since both are about the writer and the loop sharing the LittleFS lock.

Data 41a9e44 (09, .61, 62 min): write-probe 138 -> 261 ms rising with uptime, one 613 ms at 12:17:49, then 138-139 ms; NO [loop-stall] at any probe tick, including the 613 ms one (ad83aff: write-probe 683 ms came with a 424 ms stall, but that one also had file-flags 394 ms). This WEAKENS the flash-erase hypothesis (a long erase would stall core 1 too): the earlier probe-tick stalls fit the lock wait of the loop's file-flags better. Check the IDF behaviour before acting on either.
<!-- SECTION:NOTES:END -->
