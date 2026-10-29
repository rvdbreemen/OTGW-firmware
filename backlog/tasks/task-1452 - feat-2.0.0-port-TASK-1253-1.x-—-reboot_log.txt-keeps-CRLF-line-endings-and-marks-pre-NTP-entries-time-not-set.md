---
id: TASK-1452
title: >-
  feat-2.0.0: port TASK-1253 (1.x) — reboot_log.txt keeps CRLF line endings and
  marks pre-NTP entries 'time not set'
status: To Do
assignee: []
created_date: '2026-10-29T06:23:00Z'
labels:
  - port-from-1.x
  - diagnostics
  - littlefs
  - ntp
  - low-priority
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 612000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of 1.x TASK-1253, commit e800d0c4c (1.x src/OTGW-firmware/helperStuff.ino:464-475 and :496-508).

Problem on 2.0.0 (origin/dev, same code as 1.x before the fix):
1. origin/dev:src/OTGW-firmware/helperStuff.ino:352-357: updateRebootLog() copies the previous lines with infh.readBytesUntil('\n', ...) and writes them back with outfh.print(line) and no terminator. readBytesUntil drops the LF but keeps the CR, so after one reboot every older record ends in a bare CR; viewers (FSexplorer, browser, capture scripts) show the whole history as one line, and readLatestCrashLog() (helperStuff.ino:263-285, splits on '\n') reads the old records as one merged chunk truncated at 159 chars.
2. origin/dev:src/OTGW-firmware/helperStuff.ino:328-330 formats time(nullptr) as a date unconditionally. When NTP has not synced by the time setup() calls updateRebootLog (OTGW-firmware.ino:526; NTP started at :490), the entry gets a fake date. On ESP8266 that was 2106-02-07; on ESP32-S3 it is expected to be ~1970-01-01 (not verified on hardware; record the real value in the RED run).

Mechanism to port (copy the 1.x shape):
- Before formatting: const int64_t now = (int64_t)time(nullptr); if (now >= 1577836800LL && now < 4102444800LL) format the date as today; else DebugTf a 'reboot log: clock not set' line and write "time not set - reboot cause: %s (%x) %s\r\n". Keep the literal ' - reboot cause: ' so isRebootLogSummaryLine() (helperStuff.ino:222-225) and readLatestCrashLog() still find the entry.
- In the copy loop: readBytesUntil('\r', line, sizeof(line)-1); if (infh.peek() == '\n') infh.read(); NUL-terminate; skip leading '\n' chars; keep the existing 'longer than 3 chars' filter on strlen(start); print start followed by F("\r\n"). This also repairs a CR-only file written by older 2.0.0 builds on the first boot after the update.

Do NOT port: version.h / version.hash / CHANGELOG lines of e800d0c4c. Do not change appendBootDetectLog() (OTGW-firmware.ino:150-209): it already writes '\n' per line. Do not touch the NTP state machine guards (networkStuff.ino:724-760). Do not change readLatestCrashLog() unless a test shows it needs it (it already trims CR/LF).

Risks: an existing CRLF-correct entry whose payload contains a stray '\r' would be split; the register-dump line (platformResetRegisterDump) must keep ending in CRLF or be repaired by the rewrite; check it on the bench. Entries already written with a fake 1970/2106 date stay as they are (they age out after 20 entries).

## Evidence that dev lacks it

- origin/dev:src/OTGW-firmware/helperStuff.ino:296 updateRebootLog() is the pre-fix code
- origin/dev:src/OTGW-firmware/helperStuff.ino:328-330 unconditional ZonedDateTime::forUnixSeconds64(time(nullptr)) date formatting, no validity guard
- origin/dev:src/OTGW-firmware/helperStuff.ino:352-357 readBytesUntil('\n') then outfh.print(line) with no terminator (same line-ending bug)
- origin/dev:src/OTGW-firmware/helperStuff.ino:222-225 isRebootLogSummaryLine matches ' - reboot cause: ' (same contract as 1.x)
- origin/dev:src/OTGW-firmware/helperStuff.ino:269-271 readLatestCrashLog splits on '\n' and trims CR/LF, so CR-only records merge into one read
- origin/dev:src/OTGW-firmware/OTGW-firmware.ino:490,526 startNTP() before updateRebootLog(lastReset), same order as 1.x
- origin/dev:src/OTGW-firmware/networkStuff.ino:724,745 NTP guard against bogus pre-sync time exists only in the NTP state machine, not in the reboot log
- git log origin/dev --grep 'reboot_log|reboot log|2106': no commit touching updateRebootLog line endings or pre-NTP date
- git grep 'reboot_log|reboot log' origin/dev -- backlog/tasks backlog/archive: no sibling task for this fix (hits are TASK-1208/1222/1286/1328/1339/1438/927, unrelated)

## Platform notes

Line-ending bug is identical on ESP32 (same code). Pre-NTP time on ESP32-S3 is not 2106 but most likely 1970-01-01 (newlib time since boot) or an RTC-retained value after a soft reset; I did not verify which on hardware, so the bench check must record what time(nullptr) actually returns before sync. The 2020..2100 window rejects 1970 and 2106 alike. Use int64_t for the comparison (time_t is 64-bit on arduino-esp32 3.x). PSTR/snprintf_P are no-ops/aliases on ESP32 but keep them for code parity. Dev's TASK-1286 heap copy of reboot_log.txt across a filesystem update copies bytes verbatim and is unaffected.

## Reviewer corrections (applied; second-reader findings from the port evaluation)

Verdict holds: every cited origin/dev line checks out, and dev has the exact pre-fix copy loop and the unguarded date format. I found no equivalent fix on dev under any other name, in the code or in the dev backlog. The analyst says the ESP32 pre-sync time value is not verified, and that is correct. Two side observations that are not part of this port: (1) origin/dev:helperStuff.ino:320 writes snprintf_P(log_line_regs, LOG_LINE_LENGTH, ...) where log_line_regs is 120 bytes (:305) and LOG_LINE_LENGTH is 140. That size mismatch is a latent overflow on dev. Check whether 1.x has the same line before porting next to it. (2) On 1.x the fix sits next to the g_bootRestartReason handling (helperStuff.ino:~460). That belongs to a different 1.x change and is not part of e800d0c4c's port scope.

Line refs in the draft are correct. The task should state the 1.x fix range precisely: e800d0c4c covers helperStuff.ino:464-475 and :496-508, and it must not pull in the adjacent g_bootRestartReason block (~:455-462), which comes from a different 1.x change. Optionally add a note: while editing updateRebootLog on dev, log_line_regs[120] is written with size LOG_LINE_LENGTH (140) at origin/dev:helperStuff.ino:320. Fix that with sizeof(log_line_regs), or file it as a separate task. The ACs are already red/green and verifiable.

Source: port evaluation 2026-10-29 (1.x v1.7.7..otgw-1.x.x 76a89bc3d against origin/dev b07e3194a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 RED on current origin/dev build (bench .61 OTGW32 or .63 Classic+S3): after two software reboots, /reboot_log.txt byte counts show fewer LF than CR (CR-only older records), recorded with the exact command and counts; time(nullptr) value at updateRebootLog time recorded from telnet when NTP is not yet synced (e.g. WiFi unreachable or NTP server blocked via the MikroTik)
- [ ] #2 GREEN on the fix build: after OTA plus two reboots every line in /reboot_log.txt ends in CRLF (CR count == LF count == line count), and a CR-only file inherited from the old build is repaired on the first boot
- [ ] #3 With NTP unavailable at boot, the new entry reads 'time not set - reboot cause: ...' instead of a 1970 or 2106 date; with NTP synced, the entry still carries the real local date and time
- [ ] #4 /api/v2/device/crashlog still returns the latest summary plus register/External Reason detail line after a crash-type reset (or a host harness that feeds a sample CRLF and 'time not set' file through the real readLatestCrashLog/isRebootLogSummaryLine shows the summary is found)
- [ ] #5 Optional host check: a harness that #includes the real updateRebootLog copy loop against a fake File shows old code producing CR-only output and the fix producing CRLF from both CR-only and CRLF input
- [ ] #6 Build for esp32, esp32-classic and esp32-combo succeeds and python evaluate.py --quick shows no new failures
<!-- AC:END -->
