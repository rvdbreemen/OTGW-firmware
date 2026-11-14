---
id: TASK-1931
title: >-
  RTC snapshot: restore the newest whole reboot-log lines that fit instead of
  skipping the whole log
status: To Do
assignee: []
created_date: '2026-11-14T09:22:00Z'
updated_date: '2026-11-14T23:21:00Z'
labels:
  - reliability
  - ota
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 989000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up from TASK-1904 / ADR-233 (coordinator, 2026-11-14). fsOtaRtcFill() (OTGW-ModUpdateServer-esp32.h, fsOtaRtcSnapshot block) stores settings.ini, reboot_count.txt and reboot_log.txt each whole or not at all, in that order, in 7424 B of RTC data. A reboot log that does not fit after settings.ini is skipped whole: host case T11 (test/host/test_fs_ota_rtc_snapshot.py; settings.ini 5700 B, log 20 x 160 B = 3200 B) restores settings and counter but no log, and the boot starts a new one. The log can reach about 4.8 KB (core-dump lines up to REBOOT_LOG_LINE_MAX 240 B), while about 1.7 KB is left next to a 5.7 KB settings.ini; on .63 the margin is about 260 B after TASK-1904. The reboot log after a crash during a filesystem OTA is exactly the diagnostic we want then, so losing it whole is the worst part of the budget limit. Direction: when the whole log does not fit, keep the newest whole lines that do (the log is newest-first, so a prefix cut at a CRLF), never a partial line. This changes ADR-233's rule 'each file whole or not at all' for the log, so it needs a superseding or amending ADR after ADR-233 is accepted (adr-kit, Robert's acceptance). Not to be built before that.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 An amending/superseding ADR for ADR-233 (log: newest whole lines that fit) is accepted
- [ ] #2 Host case: a log larger than the free budget restores its newest whole lines (cut at a CRLF, never a partial line) together with settings.ini and the counter; old (skip whole) red, fix green
- [ ] #3 Bench on a unit with a near-full budget: after a reset during a filesystem OTA the newest reboot-log lines are back
- [ ] #4 The filesystem-OTA keep list keeps a full reboot log: the reboot_log.txt cap in KEEP_LIST (OTGW-ModUpdateServer-esp32.h) and KEEP_TOTAL_MAX follow the real worst case (LOG_LINES x line length), with a check that fails when they drift apart; heap trade-off recorded
- [ ] #5 A full 20-line reboot log is protected by the RTC snapshot during a filesystem write: 09 on beta.264 (.61, 2026-11-15): '[OTA] /reboot_log.txt (1920 B) does not fit the RTC snapshot (1755 B free): not protected during this filesystem write' (ADR-233 skip path); either the budget fits the real worst case or the newest whole lines are kept
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Second budget for the reboot log (found 2026-11-14 while documenting TASK-1524, coordinator: add here, not a new task). The filesystem-OTA keep path (heap copies, src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h KEEP_LIST, about line 824; firmware code, not a library under src/libraries) caps reboot_log.txt at 3072 B, commented '20 lines of at most 140 B'. Since TASK-1904 a boot line is up to 160 B (LOG_LINE_LENGTH), so 20 full boot lines are 3200 B and the log is skipped on a filesystem update (logged as over its cap); core-dump lines of up to 240 B (REBOOT_LOG_LINE_MAX) could already exceed 3072 before TASK-1904. Typical logs stay under (09 measured at most about 100 B per line, about 2 KB). KEEP_TOTAL_MAX is 10240 and the sum of the per-file maxima is already 9824, so raising the reboot-log cap by 1024 B (to 4096) needs KEEP_TOTAL_MAX raised too: a heap trade-off (KEEP_HEAP_RESERVE 8192 above each copy still applies). The RTC path (ADR-233) covers a reset during the write separately. Root issue for both: '20 x 140' is baked into constants; a single source (LOG_LINES x REBOOT_LOG_LINE_MAX) would prevent the drift.

Third budget finding (09, beta.264 bench item 4, 2026-11-15, capture s1780/i4/cap/): with 20 lines the reboot log is 1920 B and the RTC snapshot has 1755 B free after settings.ini and the counter, so ADR-233 skips it and the log is unprotected during the write (here it survived through the heap keep/restore; a failed write would lose it). Same theme as this task's first AC (keep the newest whole lines that fit) and the keep-list cap AC.
Bench observation by 09 (2026-11-15 00:12, .61 beta.264+a695744, FS OTA via ota_flash.py --no-app --fs during a capture, TASK-1524 AC2): telnet '[OTA] /reboot_log.txt (1920 B) does not fit the RTC snapshot (1755 B free): not protected during this filesystem write' followed by '[OTA] RTC snapshot: 5669 B of settings, reboot counter and reboot log'. With 20 reboot-log lines (1920 B) the ADR-233 RTC snapshot skips the reboot log, so it is protected only by the heap keep/restore path. In this run it survived (20 lines after the reboot, top line '[OTA] Rebooting...'), settings API + settings.ini diff [] . Evidence: 09 scratchpad s1780/i4/cap/ (merged transcript, telnet section).
<!-- SECTION:NOTES:END -->
