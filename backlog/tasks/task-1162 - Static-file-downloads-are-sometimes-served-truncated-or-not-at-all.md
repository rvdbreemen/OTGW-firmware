---
id: TASK-1162
title: Static file downloads are sometimes served truncated or not at all
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-23 21:39'
updated_date: '2026-10-02 19:21'
labels:
  - web
  - bug
dependencies: []
priority: medium
ordinal: 293000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen 2026-09-23 on the OTGW32 bench (alpha.376): three back-to-back GETs of /settings.ini (5752 B on LittleFS) returned 4172 B with HTTP 200, then no response (curl 000), then the full 5752 B. A truncated 200 is worse than an error: it looks like a valid file. Relevant to anyone saving settings.ini before a flash, and to web UI assets. Possibly the ADR-147 file-serve gate or chunked streaming under load; not investigated yet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduce: repeated GETs of a multi-KB static file on the bench, record size and status per request
- [ ] #2 Root cause identified
- [ ] #3 A static file is either served complete or fails with an error status; never a short 200
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
<!-- SECTION:NOTES:END -->
