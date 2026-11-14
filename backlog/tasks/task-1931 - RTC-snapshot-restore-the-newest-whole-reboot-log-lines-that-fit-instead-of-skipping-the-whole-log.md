---
id: TASK-1931
title: >-
  RTC snapshot: restore the newest whole reboot-log lines that fit instead of
  skipping the whole log
status: To Do
assignee: []
created_date: '2026-11-14T09:22:00Z'
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
<!-- AC:END -->
