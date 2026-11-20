---
id: TASK-1997
title: >-
  Telnet: a client connected across a planned reboot gets no close (no FIN/RST)
  and stays silent on a half-open socket
status: Done
assignee:
  - '@claude'
created_date: '2026-11-16T09:41:00Z'
updated_date: '2026-11-17T04:56:00Z'
labels:
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1021000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by 09 on the bench (2026-11-16 10:25, .61 OTGW32, 2.0.0-beta.341+3cb72c3). A telnet client (port 23) connected before POST /ReBoot receives the shutdown lines up to '[reboot] prepareForReboot ... stopping telnet+otgwstream, total=25ms' and then nothing: recv() never returns EOF, so a client that waits for data (with a read timeout, no keepalive, never sending) stays on a half-open socket and logs nothing after the reboot. In the bench run the capture held 42 lines / 2706 bytes over 10 min while the unit was back online after 18.5 s and a fresh connection showed the normal stream (13 OT frames in 15 s). Impact: a field capture tool or a tester's telnet session that spans a reboot can silently stop capturing (the reboot and everything after it is lost) unless the client reconnects on its own. Repro: 1) connect a raw TCP client to <ip>:23 and read with a 0.5 s timeout, never send; 2) POST /ReBoot; 3) the client sees the [reboot] lines up to 'stopping telnet+otgwstream', then no EOF and no data, indefinitely (10 min observed). Evidence: 09 scratchpad closeout/mq/telnet.raw, mq1778.py. Open question for the fix: whether prepareForReboot's telnet stop closes the client sockets so a FIN goes out before esp_restart(), or the FIN is queued but not sent in the ~25 ms before restart; and whether our capture tools (scripts/otgw_capture.py, capture-otgw.sh) reconnect on silence. Not a firmware crash; low priority (coordinator 2026-11-16).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Repro on the bench: a client connected across /ReBoot gets an EOF (FIN or RST) within a few seconds of the restart, or the capture tools are shown to reconnect on their own
- [x] #2 Old-vs-fix evidence on the same reproduction (client silent on the old code, closed or reconnected on the fix)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-17 d3 analysis (read only, no code). Cause: prepareForReboot() (helperStuff.ino:776) has '// debugTelnet.stop();' commented out on purpose, so the port 23 client sockets are never closed before the restart; OTGWstream.stop() (25238) does run. That line is a deliberate maintainer decision (memory feedback_prepareforreboot_telnet_stop: 'do not re-enable', 'check with the user before adding any .stop() calls to prepareForReboot()'; history 201921f26, 5cf906fb4; the reason given was the ESP8266 shutdown path, and dev is ESP32-S3 only). So a firmware fix (stop the server, or close only the clients) needs Robert's OK first. The second route in AC1 already exists in our tool: scripts/otgw_capture.py (TASK-1299) sets SO_KEEPALIVE (idle 10 s, interval 5 s, count 3; apply_keepalive :819) and runs a silence watchdog (:1256-1276) that health-probes a quiet link and reconnects when the probe fails or the uptime went down. 09's repro used a raw client without keepalive (mq1778.py). Not checked: capture-otgw.sh (shell path). Proposal: AC1 evidence = 09 runs otgw_capture.py across POST /ReBoot on .61 and shows the reconnect line; firmware change only if Robert wants it (one question via the coordinator).

2026-11-17 Decision (Robert, option A, DM 1572091964726190203, relayed by the coordinator): prepareForReboot() stays as it is (no telnet stop, the deliberate decision holds); the capture tool's keepalive + silence watchdog (scripts/otgw_capture.py :819, :1256-1276) is the answer. Remaining: 09's bench run (otgw_capture.py across POST /ReBoot on .61) for AC1/AC2 evidence; then close with that evidence.

2026-11-17 bench 09 on .61 (OTGW32, 2.0.0-beta.356+4601510, SAT off): scripts/otgw_capture.py --device .61 --duration 150 --no-prompt. 05:52:58 'Telnet connected', keepalive applied (SO_KEEPALIVE, SIO_KEEPALIVE_VALS, TCP_KEEPIDLE/INTVL/CNT; idle 10 s, interval 5 s, count 10 fixed by Windows). 05:53:39 POST /ReBoot (HTTP 200), bootcount 1012 -> 1013. Tool lines: 'Telnet gap: 05:53:40 -> 05:53:56 (15 s, 1 failed connect attempts, read failed)' and 'Telnet reconnected after 2 attempt(s): 05:53:56', data flows again after it. Evidence: 09 scratchpad t1997\20261117-055258\ (summary.txt, transcript, cap.out).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Decision (Robert, option A, DM 1572091964726190203): no firmware change; prepareForReboot() keeps its deliberately disabled debugTelnet.stop() (helperStuff.ino:776), and the capture tool's TCP keepalive + silence watchdog (scripts/otgw_capture.py apply_keepalive :819, watchdog :1256-1276, TASK-1299) is the answer. AC1 (via the capture-tool route): 09 on .61 beta.356, otgw_capture.py across POST /ReBoot: the tool saw the dead link 1 s after the restart ('read failed') and logged 'Telnet reconnected after 2 attempt(s)' 16 s after the reboot request, with data flowing again, no user action. AC2: the same reproduction (connect, POST /ReBoot) with a plain client without keepalive stayed silent on a half-open socket for 10 min (09, 2026-11-16, mq1778.py, the filing evidence), while the capture tool reconnected; there is no old-vs-fix firmware pair because the firmware stays as decided. Not covered: capture-otgw.sh (shell path) was not checked or benched.
<!-- SECTION:FINAL_SUMMARY:END -->
