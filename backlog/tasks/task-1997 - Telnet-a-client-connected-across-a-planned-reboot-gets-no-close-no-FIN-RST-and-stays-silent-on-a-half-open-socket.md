---
id: TASK-1997
title: >-
  Telnet: a client connected across a planned reboot gets no close (no FIN/RST)
  and stays silent on a half-open socket
status: To Do
assignee: []
created_date: '2026-11-16T09:41:00Z'
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
- [ ] #1 Repro on the bench: a client connected across /ReBoot gets an EOF (FIN or RST) within a few seconds of the restart, or the capture tools are shown to reconnect on their own
- [ ] #2 Old-vs-fix evidence on the same reproduction (client silent on the old code, closed or reconnected on the fix)
<!-- AC:END -->
