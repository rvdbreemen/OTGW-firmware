---
id: TASK-1162
title: >-
  Under request overload, large static files stall mid-body and the client is
  left with a short 200
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-23 21:39'
updated_date: '2026-10-02 20:06'
labels:
  - web
  - bug
dependencies: []
priority: medium
ordinal: 293000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen 2026-09-23 on the OTGW32 bench (alpha.376): three back-to-back GETs of /settings.ini (5752 B on LittleFS):
- the first returned 4172 B with HTTP 200;
- the second got no response (curl 000);
- the third returned the full 5752 B.
A truncated 200 is worse than an error, because it looks like a valid file.

Re-scoped 2026-10-02 (maintainer's choice):
- Sequentially the symptom does not reproduce on alpha.401. Four bench cells gave 840 requests with no short 200: steady state, right after a LittleFS write, app-only reboot, and settings flush.
- It does reproduce under request overload: the refresh_storm.py arms with 6 and 8 workers, 3-4x the ADR-165 cap of 2.
- There, large static files (45-393 KB) stop mid-body with the connection still open. The client waits out its 10 s read timeout and is left with a 200 and part of the body.
- In the same 8-worker arm the device also stopped accepting new connections for about 20 s, then recovered without a reboot.
This task covers the overload behaviour.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduce: repeated GETs of a multi-KB static file on the bench, record size and status per request
- [ ] #2 Root cause identified
- [ ] #3 Under the 8-worker storm (refresh_storm.py --workers 8, seed 1124) no static response stalls mid-body: each is served complete, refused with a 503 before the body, or ends in a connection abort the client sees at once
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-30 reproduction tooling ready (scripts only, no firmware, no bump).
Premise shift from the triage: in both failure windows REST timed out too, and on this bench BLE is dormant (no PSRAM, risk-ack false), so the symptom looks like device-wide HTTP unresponsiveness after a LittleFS image write, not a static-file bug. Reproduce first; fix only after the cause is known.
scripts/tests/static_integrity.py: strictly sequential raw-socket GETs with Connection: close (never more than one request in flight, --gap default 0.2 s). Per request it records status, Content-Length, bytes received, close type (complete, FIN-short, RST, timeout, connect-fail) and elapsed ms. Targets: /settings.ini, /sat-slider.js (control), /index.html, /v2-bundle.css, /index.js, plus REST controls /api/v2/device/info and /api/v2/settings; expected sizes come from /api/listfiles. Each batch records heap telemetry from device/info; 503s count as gate responses, not failures. --batches / --minutes (a timed run needs --batches 0), --csv output.
Evidence: py_compile OK; tests/test_static_integrity.py 33 tests OK (re-run in the main tree today) against a local stub server that serves a correct file, a truncated body under a longer Content-Length, an RST and a stall. Workflow review: 28 mutants of the classifier all killed (the original version let 6 survive; the fixup closed them).
Bench plan (cells, per the triage): after a LittleFS image write, an app-only-reboot control, steady state, and an alternating-value settings flush, each with REST and heap telemetry. Not run yet: the OTGW32 is off the network until it is provisioned. AC#3 rewording (option b) and the title re-scope wait for the cell data.

2026-10-02 bench cells on the OTGW32, alpha.401+0d35143, scripts/tests/static_integrity.py: strictly sequential, one request in flight, 30 batches x 7 targets = 210 requests per cell. CSVs in %LOCALAPPDATA%/OTGW-capture/task1162-bench-20261002/.

Cells:
- steady state: CLEAN. 210 of 210 complete, no 503, pcbs max 1, no reboot.
- right after a LittleFS image write (app + fs over USB, measured from the first health 200): CLEAN 210/210.
- app-only reboot control: CLEAN 210/210.
- settings flush: 80 alternating POSTs of ui_graphtimewindow during the run, each rewriting settings.ini, all answered 200.
  - One GET /settings.ini answered HTTP 500 with a complete 23 B error body, during a rewrite. That is an honest error status, not a short 200.
  - 5 responses matched a listing re-read: the file really changed between the reads.
  - No reboot.

The 2026-09-23 symptom does not reproduce on alpha.401 under sequential load, after an FS write included: no truncated 200, no curl 000.

Short 200s do reproduce under overload. The TASK-1124 request storm the same evening (refresh_storm.py, 15 paths, 33% client aborts) returned 2 short 200s (body shorter than its Content-Length) in each of the 6-worker and 8-worker arms. Those arms also had about 20 timeouts and the pcb pool at 11-15 of 16. Arms with 2 and 4 workers had none.

AC#1 done: the defect is reproduced (under overload) and every request is recorded with size and status.

AC#2 and AC#3 stay open. The root cause of a mid-body close under overload is not identified. The task's scope (sequential symptom vs overload-only) needs the maintainer's call before a fix is designed.

2026-10-02 correction, re-scope and hypotheses.

Correction to my earlier note. It called the storm's short 200s "body shorter than its Content-Length". That is true but incomplete:
- All six were stalls, not closes: four in the first storm, two in the re-run.
- In each, the connection stayed open (eof false) and no byte arrived within the 10 s read timeout.
- So the "RST instead of FIN" option I put to the maintainer does not apply.
- Stalled paths: /v2.js twice (292022 B), / (45055 B), /index.js (392995 B), /v2.html (50340 B), /graph.js (46699 B).
- Received before the stall: 11283 to 275513 B. Every stalled file is at least 45 KB; no smaller file stalled.

Serve path: webSendFile() (webServerCompat.h) takes the file gate, then sends beginResponse(LittleFS, path), an AsyncFileResponse. The gate slot comes back on disconnect, so a stalled response holds its slot until the client leaves.

Hypotheses, none confirmed:

H1, ESPAsyncWebServer 3.11.0 in-flight credit (as resolved in .pio/libdeps; ASYNCWEBSERVER_USE_CHUNK_INFLIGHT defaults to 1 in ESPAsyncWebServer.h:72, _in_flight_credit starts at 2 in WebResponseImpl.h:70). In AsyncAbstractResponse::write_send_buffs() (WebResponses.cpp:346-500):
- a credit comes back only with an ACK, never with a poll;
- once _sentLength > CONFIG_LWIP_TCP_WND_DEFAULT, a call made with no credit returns at once;
- every send pass takes a credit, even one that sent nothing (OOM on the 2xMSS buffer, or add() returning 0 under ERR_MEM).
- So one empty pass, just as the last in-flight data is ACKed, leaves credit 0 and nothing in flight. No ACK can come, every poll is ignored, and the response sits until the client leaves.
- Supporting: AsyncTCP's 5 s ack timeout only runs while data is unacked, and no server-side close came within 10 s.
- No upstream release from 3.11.1 to 3.12.1 mentions this.

H2, TCP retransmission backoff on a congested WiFi link. Weaker: the 5 s ack timeout would then normally close the connection within the 10 s window.

H3, async_tcp starvation. The 8-worker arm also stopped accepting connections for about 20 s.

Evidence that separates them: a client-side capture of a stalled connection. The capture needs admin rights for pktmon; Wireshark/npcap is not installed.
- H1: last server segment ACKed, window open, then silence.
- H2: retransmissions with growing gaps.
- H3: the device stops ACKing the client at all.
Then old-vs-fix with the storm. For H1 that means a build with -D ASYNCWEBSERVER_USE_CHUNK_INFLIGHT=0, which is a build flag, not a library edit.
<!-- SECTION:NOTES:END -->
