---
id: TASK-1253
title: >-
  Fix reboot_log.txt: older lines lose their line feed, and boot-time entries
  show 2106-02-07
status: Done
assignee:
  - '@claude'
created_date: '2026-10-18 23:16'
updated_date: '2026-10-20 02:19'
labels:
  - bug
  - diagnostics
dependencies: []
priority: medium
ordinal: 247000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the 1.x bench unit .88.68 on 2026-10-18 while verifying TASK-1247/1248, present on 1.7.7-beta.1 too (old capture transcript), so not new in 1.7.8.
1. updateRebootLog() (helperStuff.ino ~485-493) copies the previous lines with readBytesUntil('\n', ...), which drops the '\n', and writes them back with print() without it. Every line except the newest ends with a bare CR, so most viewers show the history as one long line ('...no traffic2106-02-07 ...'). Field logs on GH #682 are harder to read because of it.
2. The timestamp is written before NTP has synced: time(nullptr) is not set yet and the entry reads '2106-02-07 07:28:2x' (uint32 max in CET). It should say the time is unknown (or log uptime) instead of a date in 2106.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every line of reboot_log.txt ends with CRLF after several reboots (od/cat -A on the downloaded file)
- [x] #2 An entry written before NTP sync does not show a 2106 date; it states that the time is not set
- [x] #3 Old-vs-fix shown on the 1.x bench unit; esp8266 build and evaluate.py --quick green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Line endings (helperStuff.ino updateRebootLog ~485-493): read each old line up to '' instead of '
', swallow a following '
', and write it back with an explicit "
". Splitting on CR also repairs existing files where earlier builds left CR-only records, on the first boot of the fixed build.
2. Time not set (~464-466): take time(nullptr) once; if it is outside 2020..2100 (NTP not synced), write "time not set" instead of the date. Line keeps " - reboot cause: " so readLatestCrashLog() still finds it.
3. Bench old-vs-fix on .88.68: before = current build (downloaded 2026-10-20: 256 bytes, 3 CR / 1 LF, all dates 2106-02-07). Fix: OTA, then 3 reboots (telnet operator reboot + REST), download /reboot_log.txt, od -c: every line CRLF, no 2106. Same reboots double as TASK-1247 AC #1/#2 evidence (reason suffix present after doRestart, absent after a power cycle if I can trigger one; otherwise stated as not verified).
4. build.bat full + evaluate.py --quick; CHANGELOG [Unreleased] Fixed bullet.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-20: fixed in e800d0c4c (local). Bench .88.68 old build: reboot_log.txt 256 bytes, 3 CR / 1 LF, all dates 2106-02-07. Fix 1.7.8-beta.5+84be30d via OTA + 2 telnet R reboots: 6 lines, 6 CR, 6 LF, every line CRLF (old CR-only lines repaired), the 3 new entries read "time not set"; the 3 inherited lines keep their 2106 date (history from older builds, not rewritten). Telnet clock right after boot read 07:28:29 (unset), matching the 2106-02-07 07:28 dates. build.bat fresh bins + "Build completed successfully!"; evaluate.py --quick 37/37.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
reboot_log.txt is readable again and no longer dates entries in 2106.

Changes (helperStuff.ino updateRebootLog):
- Old lines are read up to CR, a following LF is swallowed, and each line is written back with CRLF. Before, readBytesUntil('
') dropped the LF and print() never added one, so every older record ended in a bare CR. A CR-only log from an earlier build is repaired on the first boot.
- An entry whose time(nullptr) is outside 2020..2100 (NTP not synced yet) says "time not set" instead of 2106-02-07. " - reboot cause: " stays, so readLatestCrashLog() still finds it.

Evidence: bench .88.68 old vs fix (counts in notes). build.bat + evaluate.py --quick 37/37. Follow-up not done: boot entries are written before NTP sync, so they will usually say "time not set"; logging after sync would need a deferred write.
<!-- SECTION:FINAL_SUMMARY:END -->
