---
id: TASK-1299
title: >-
  capture-mqtt-debug: the telnet log stops at the device reboot and never
  reconnects (47 s of telnet in a 1 h 44 capture)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-22T12:00:00Z'
updated_date: '2026-10-23T07:49:00Z'
labels:
  - tooling
  - capture
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 424000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter's bundle 2026-10-22 (20261022-093809.zip): browser.log runs 09:38:09 to 11:22:33 and mqtt.log 89584 lines, but telnet.log has 460 lines from 09:38:11 to 09:38:58 and ends when the device rebooted for the firmware upgrade; summary.txt lists TelnetReconnectDelayMilliseconds 500 and an adaptive timeout, yet no reconnect happened. The SAT run therefore has no debug lines at all. Reproduce on the bench: start the capture, POST /safeboot + upload a firmware, check that telnet.log resumes after the two reboots; fix the reconnect loop (scripts/capture-mqtt-debug.* / capture-otgw.sh) and add a line to the transcript when a reconnect happens.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Bench: a capture spanning a safeboot firmware update resumes telnet after both reboots (transcript shows the reconnect lines)
- [x] #2 summary.txt reports telnet gaps (start/end of each disconnect) so a reporter sees when the log is incomplete
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Root cause (evidence): gruijter's bundle is from Linux (summary.txt: bash 5.3.9, /usr/bin/google-chrome) -> capture-mqtt-debug.sh. Telnet connected 09:38:11/09:38:17, last telnet line 09:38:58, no disconnect or reconnect line until Ctrl+C at 11:22 (worker exit 0). The Python worker (capture-mqtt-debug.sh ~918-955) only reconnects on the restart marker, a 0-byte recv (FIN) or an exception. A rebooting ESP (safeboot handover, crash, power loss) sends no FIN, so the socket stays half-open and select() reports nothing forever. The PowerShell worker in capture-mqtt-debug.bat (Test-TelnetDisconnected ~1084) has the same gap (Connected/poll stay true). macOS delegates to the .sh. Side bug: 'Telnet connected' is logged twice per connect (sh 874 + 904).

Fix (both workers, same behaviour):
1. TCP keepalive on the telnet socket (idle 10 s, interval 5 s, 3 probes): the rebooted device answers a probe with RST, so the OS reports the dead connection within ~25 s. Python: SO_KEEPALIVE + TCP_KEEPIDLE/TCP_KEEPINTVL/TCP_KEEPCNT where available (Linux), TCP_KEEPALIVE on macOS; PowerShell: Socket.IOControl(KeepAliveValues) (works on Windows PowerShell 5.1 and 7).
2. Silence watchdog as fallback: no telnet byte for --telnet-idle-reconnect-seconds (default 60; all debug toggles are on, the device logs every few seconds) -> close and reconnect with reason 'no data for N s'. Value and strategy written to summary.txt.
3. Gap reporting: on every disconnect remember the time of the last received byte; on reconnect write 'Telnet gap: <start> -> <end> (<n> s, <reason>)' to summary.txt and a marker line into telnet.log; at stop write 'Telnet gaps: <count>, total <n> s' (and a line for a gap still open at stop).
4. Remove the duplicate 'Telnet connected' summary line.
No firmware change (scripts/** needs no prerelease bump). Not on otgw-1.x.x unless the 1.x capture scripts share the code (check and note).

Verification:
- Reproduce half-open on the bench without waiting for a safeboot: start the capture (old scripts), hard-reset the OTGW32 over USB (esptool run, no FIN reaches the PC), confirm telnet.log stops and no reconnect line appears for >2 min (old .sh under Git Bash/python and old .bat); then the same with the fix: reconnect within ~30 s, gap line in summary.txt.
- AC1: capture (fixed .bat, the Windows path; plus fixed .sh) across a real WiFi safeboot update (POST /safeboot, POST /update on safeboot with the same firmware): telnet.log resumes after both reboots, summary shows the reconnects and gaps.
- AC2: summary.txt gap lines from the runs above.
- bash -n / python compile of the embedded worker; evaluate --quick.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 plan revision after advisor review: (1) TCP keepalive is the primary detector; the silence watchdog no longer reconnects on silence alone: after --telnet-idle-check-seconds (default 60) without telnet bytes it probes GET /api/v2/health and reconnects only when the probe fails or the reported uptime went down (device rebooted); a quiet healthy device keeps its connection (verify: ~5 min quiet capture, zero reconnects). (2) Failed connect attempts while the device is down (safeboot serves no port 23) are not logged one per attempt: the first failure is logged, the rest are counted into the gap line 'Telnet gap: start -> end (n s, k failed attempts, reason)'. (3) Linux path verified for real under WSL Ubuntu (same OS family as the reporter); macOS TCP_KEEPALIVE path only guarded with hasattr, not run. Windows keepalive probe count is fixed by the OS (detection up to ~60 s while the device stays down, immediate RST once it is back). (4) otgw-1.x.x carries its own different capture-mqtt-debug.bat and capture-otgw.sh: sibling task, not folded in here. (5) AC1 uploads the HEAD alpha.462 .ino.bin through safeboot (Origin header for its same-origin check). Gap marker lines in telnet.log carry a '### capture:' prefix.

2026-10-23 bench evidence (OTGW32 .88.61; transcripts in %TEMP%/rep-1299/<label>/; runners in the session scratchpad):
- USB hard reset (esptool read-mac, hard-reset after; no FIN reaches the PC):
  - old .sh (HEAD, WSL Ubuntu, python 3.12): last device line 08:37:08 = reset time, nothing until capture end 08:41:55, no reconnect line, 'Telnet connected' twice.
  - new .sh: 'Telnet read failed: [Errno 104] Connection reset by peer' -> 1 failed attempt logged, reconnected 08:43:34, lines to the end 08:48:06; 'Telnet gap: 08:43:22 -> 08:43:41 (20 s, 2 failed connect attempts, read failed)' (before the gap-end fix), '### capture:' marker.
  - old .bat (HEAD, CRLF-normalised copy): last device line 08:51:04 = reset time, nothing until 08:55:54.
  - new .bat: 'Telnet disconnected' -> reconnected 08:58:12, lines to 09:02:49; 'Telnet gap: 08:57:56 -> 08:58:12 (16 s, 1 failed connect attempts, connection closed)', marker 16 s (gap-end fix verified).
- WiFi safeboot update (POST /safeboot, safeboot POST /update with alpha.462+2a0c4a0.ino.bin):
  - new .bat: one gap over both reboots 'Telnet gap: 09:03:59 -> 09:04:33 (34 s, 6 failed connect attempts, connection closed)', reconnected into fwversion 2.0.0-alpha.462+2a0c4a0, lines to 09:09:14.
  - new .sh: 'Connection reset by peer', 15 failed attempts counted, 'Telnet gap: 09:10:54 -> 09:11:18 (25 s, ...)', lines to 09:15:32.
  - old .sh: last device line 09:16:26 (the /safeboot moment), nothing until 09:21:13: the reporter's exact signature; safeboot sends no FIN on Linux.
- Loopback fake device (agent): watchdog phase A silent+healthy -> 0 reconnects (one probe per idle period), B uptime 6->1 -> 'device rebooted (uptime reset)', C health 503/down -> 'health probe failed' (503 to be changed to 'alive' after review), both workers.
- Settings: 178 keys identical before/after the safeboot updates. Bench now on alpha.462+2a0c4a0.
- Static: bash -n OK, evaluate --quick 0 fail, .bat CRLF 2608/2608.
Review round 1 (3 lenses): fixing (1) .sh totals lost when stop_child kills the worker (SIGTERM handler), (2) watchdog gap start = max(last byte, last good probe), (3) HTTP 503/429 from health = alive, (4) .bat keepalive line 'count fixed by Windows (10)', (5) misleading SelectError comment, (6) new tests/test_capture_telnet_worker.py (extracted worker vs fake device). Sibling for otgw-1.x.x: TASK-1308.

2026-10-23 09:50 final bench check with the review-fixed scripts (USB hard reset): .sh gap 16 s (8 failed attempts counted) then a device-side FIN at 09:42:12 handled with a 1 s gap ('remote closed connection'; cause on the device not investigated), lines to the end 09:43:09; .bat gap 14 s, keepalive line 'count fixed by Windows (10)', lines to the end 09:48:45. tests/test_capture_telnet_worker.py: Windows 'Ran 8 tests in 60.193s OK (skipped=1)' (SIGTERM case POSIX-only), WSL 8/8 OK (agent), HEAD script 7 failures.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Root cause: a rebooting ESP (WiFi safeboot update, crash, power loss) sends no FIN, so the telnet capture socket stayed half-open and both capture workers (Python worker in capture-mqtt-debug.sh, used on Linux/macOS; PowerShell worker in capture-mqtt-debug.bat) only reconnected on FIN, an exception or the restart marker: telnet logging stopped for good (gruijter's Linux bundle: 47 s of telnet in a 1 h 44 capture). Fix in both workers: TCP keepalive (idle 10 s, interval 5 s; 3 probes on Linux, count fixed by Windows), a silence watchdog gated by GET /api/v2/health (reconnect only when the probe gets no answer or the uptime went down; a 503/429 answer counts as alive), and gap reporting: 'Telnet gap: start -> end (n s, k failed connect attempts, reason)' in summary.txt (failed attempts counted, not listed), '### capture:' marker in telnet.log, 'Telnet gaps: N, total S s' and an open gap at stop (also on SIGTERM). Duplicate 'Telnet connected' lines removed. New tests/test_capture_telnet_worker.py runs the real extracted Python worker against an in-test fake device.

Evidence: AC1 bench (OTGW32): WiFi safeboot update (POST /safeboot + safeboot /update): old .sh on Linux (WSL Ubuntu) silent from the /safeboot moment to the end (4 min 47 s, the reporter's signature); new .sh reconnected after 25 s, new .bat after 34 s into the new firmware alpha.462+2a0c4a0; USB hard reset: old .sh/.bat silent ~5 min, new .sh 12-16 s, new .bat 14-16 s; final review-fixed scripts re-checked. AC2: gap lines as above in every run; open-gap-at-stop and SIGTERM totals in the unittest. unittest: fixed 8 OK (Windows: 1 POSIX-only skip; WSL 8/8), HEAD 7 failures. Loopback fake device: quiet healthy device 0 reconnects, uptime drop and unreachable health reconnect, 503 keeps the session (both workers). Settings unchanged over the safeboot updates (178 keys). evaluate --quick 0 fail, bash -n OK, .bat CRLF intact.

Not verified: the macOS TCP_KEEPALIVE path (guarded, not run); a device that stays down long enough for keepalive alone to fire on Windows (up to ~60 s by the OS probe count); the cause of the device-side telnet close at 09:42:12 in the final .sh run (handled by the script). otgw-1.x.x has its own capture scripts: sibling TASK-1308.
<!-- SECTION:FINAL_SUMMARY:END -->
