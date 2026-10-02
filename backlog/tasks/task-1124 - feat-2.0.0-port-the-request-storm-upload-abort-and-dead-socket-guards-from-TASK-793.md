---
id: TASK-1124
title: >-
  feat-2.0.0: port the request-storm upload-abort and dead-socket guards from
  TASK-793
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-04 06:57'
updated_date: '2026-10-02 19:53'
labels:
  - 2.0.0
  - port
dependencies: []
priority: medium
ordinal: 275000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sibling of TASK-793 on the 1.x line, which is AC #6 of that task. Three changes landed there; each needs checking against this line rather than copying, because the 2.0.0 web stack is ESPAsyncWebServer, not ESP8266WebServer, so the failure modes differ.\n\n1. Upload abort leaked a file handle. On 1.x, handleFileUpload() had no UPLOAD_FILE_ABORTED branch, so a client disconnecting mid-body left the static File open for the lifetime of the sketch, one per abort, until LittleFS ran out of open handles. Check whether the async upload handler on this line has an equivalent abort path and whether it closes.\n\n2. The index stream did not check for a disconnected client. On 1.x, sendIndex() streamed about 11 KB of index.html without ever testing the socket. The async server uses a chunked response callback instead, which may already stop on disconnect; verify rather than assume.\n\n3. The heap-gate 503 refusals now carry Retry-After, so a refused client backs off rather than re-requesting immediately and holding the heap in the state that caused the refusal. This one is likely to apply unchanged wherever this line refuses on heap.\n\nContext worth carrying over: on 1.x the storm no longer produces a crash. The original StoreProhibited came from an unchecked ~1460-byte allocation in BufferedStreamDataSource::get_buffer() and was fixed under TASK-843. What exhausts now is serving concurrency. This line has its own and different limit, the LWIP pcb pool, which is a known crash source here, so the equivalent measurement is worth taking independently.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The async upload path closes its file handle when the client disconnects mid-body, verified by a scripted abort rather than by reading the code
- [ ] #2 A chunked response stops when its client is gone, verified under a storm
- [x] #3 Heap-gated refusals carry Retry-After
- [ ] #4 A scripted rapid-refresh storm is run against a real device and its outcome recorded: request outcomes, reboot count either side, and heap or pcb headroom
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-30 verification tooling ready (scripts only, no firmware, no bump).
scripts/tests/refresh_storm.py: --upload-abort N with fin, rst, stall and mixed modes against the real upload endpoint; read-back with a Content-Length check; cleanup through /api/listfiles?delete= (GET /?delete deletes nothing) and a 404 check; storm arms including a /ws subscriber leg; device/info snapshots with bootcount, lastreset, uptime and hd_tcp_active_pcbs; a final gate-balance check. Readback rule: a stale_previous after a fin or stall abort is a FAIL; after an rst abort it is only a FAIL when a fin/stall iteration in the same run is also stale, otherwise INCONCLUSIVE, because an RST can drop the queued data before the upload handler opens the file (AsyncTCP.cpp:494-500). A failed control upload or a 401 stops the run as INCONCLUSIVE.
Evidence: py_compile OK; tests/test_refresh_storm.py 37 tests, OK (2 /ws tests skipped on Python 3.12 without websocket-client; all 37 OK on 3.14 in the workflow run). The workflow ran 11 targeted tests against the pre-fixup tool: 6 failures + 2 errors, including the misdiagnosis 'stale_previous' on rst-only runs that the rule above now avoids. A stub green run proves the tool's mechanics, not the firmware.
Code finding to confirm on the bench: the multipart parser hands data to the upload handler at the end of every receive callback (ESPAsyncWebServer WebRequest.cpp:195, :583), not in 1460-byte pieces, so a fin/stall abort of N bytes should store all bytes received.
Scope per the triage (planner decision, for the maintainer): option B, Retry-After only on the heap-gated 503 refusals; the three <4 KB 500s stay as they are (1.x TASK-793 precedent, no API contract change). Firmware part not implemented yet.
OPEN: AC#1/#2/#4 run on the OTGW32 bench once it is back on the network.

2026-09-30 AC#3 implemented (alpha.385), bench confirmation pending.
- restAPI.ino sendApiBusy(): webPushHeader(Retry-After, 1) then sendApiError(503, ...), the same staging pattern sendApiMethodNotAllowed() uses for Allow. Used for every heap- or concurrency-gated REST refusal: the REST backpressure gate ('Server busy: too many concurrent requests, please retry'), 'Heap too low for verify', and the three 'low heap' refusals (debug dump, device/info, SATcontrol sat status). Semantic 503s (no PIC / no OT-direct hardware / MQTT not connected / scan during PIC flash / verification refused / PIC transmit queue full) keep sendApiError() and carry no Retry-After. Value 1 matches the static-file gate (webServerCompat.h, TASK-960); 1.x uses 2 for its file refusals.
- docs/api/openapi.yaml states the rule in the error conventions and in the 429-vs-503 text. Additive header, no ADR (bug-fix level within ADR-035/172 conventions).
- Build: build.bat --target esp32-combo SUCCESS (fw + fs, fresh 12:27-12:28, alpha.385+05e8c6c, images under %LOCALAPPDATA%/OTGW-capture/img-alpha385); evaluate.py --quick 70 passed / 0 / 0.
AC#3 stays unchecked until a bench curl -i of a gated 503 (e.g. during a refresh_storm.py storm) shows 'Retry-After: 1'.

Regression found 2026-09-30 by a full offline host-suite run: scripts/tests/test_heap_soak_driver.py (TASK-1036 WP8) reads sendDeviceInfoV2() for sendApiError(503, F(...)), and this task's sendApiBusy() replaced that call, so FirmwareContract failed in setUpClass and 0 tests ran (exit 5). Fixed in the test: api_error_message() also accepts sendApiBusy(F(...)) as a 503 (sendApiError(503) plus Retry-After). After the fix: 19 tests OK. The test is absent on origin/dev, so this never shipped.

2026-10-01 AC#3 verified with a host harness on the real code (t1124.py/.cpp, session scratchpad).

This replaces the earlier note's bench-curl criterion. That criterion was this agent's own choice, and the AC does not require the bench.

Setup:
- Sliced from the sources by anchor, each slice audited verbatim and brace-balanced, compiled with MSVC:
  - webServerCompat.h: the WEB_MAX_PENDING_HEADERS staging struct, webPushHeader(), webApplyHeaders(), webSend();
  - restAPI.ino: sendCorsOriginHeader(), sendApiError(), and sendApiBusy() where it exists.
- Only the AsyncWebServer boundary is stubbed (beginResponse, addHeader, send).
- The gated call (B1) is read from each revision's own restAPI.ino.

Results:
- FIX passes every case:
  - B1: a heap-gated refusal answers 503 with Retry-After: 1 and body {"error":{"status":503,"message":"low heap"}};
  - B2: a capability 503 (MQTT not connected) has no Retry-After;
  - B3: Access-Control-Allow-Origin is kept next to Retry-After;
  - B4: no Retry-After leaks into the next response;
  - S1: all five heap/concurrency-gated sites (low heap x3 in restAPI.ino/SATcontrol.ino, 'Heap too low for verify', the REST backpressure 'Server busy') call sendApiBusy(), none sendApiError(503).
- OLD (63f22a5e1^, the commit before sendApiBusy) fails exactly B1 (no Retry-After) and S1 (0 via sendApiBusy, 5 via sendApiError(503)); B2-B4 pass.
- Mutants fail exactly their case:
  - push removed: B1;
  - one site reverted: S1;
  - WEB_MAX_PENDING_HEADERS 1: B3, which shows the header cap matters.

Boundary of this evidence: it reaches the AsyncWebServer response object, not the wire. The bench storm of AC#4 (scripts/tests/refresh_storm.py) records the 503s and their headers, and shows the wire too.

Still open: AC#1, #2 and #4, which explicitly need a scripted abort or storm against a real device.

2026-10-02 bench runs on the OTGW32, alpha.401+0d35143, scripts/tests/refresh_storm.py (unchanged), Python 3.14 venv with websocket-client. Logs in %LOCALAPPDATA%/OTGW-capture/refresh-storm/.

AC#1 PASS (--upload-abort 30 --abort-mode mixed):
- 30 aborted uploads, 10 each of fin, rst and stall, each sending 4096 B. Every readback was prefix_current 4096 B, so the file holds exactly the bytes sent: the handle is closed and flushed when the client goes.
- On a stall the device closed by itself after 3.1-3.8 s.
- After the 30 aborts a full upload returned 303 with a complete 16384 B readback, so no file handles were exhausted.
- Cleanup delete 200, then GET 404.
- bootcount 2 -> 2. Heap delta -131 B per abort, below the 512 B LittleFS cache a leaked handle holds.

Storm (--workers 2,4,6,8 --duration 45 --ws-subs 2), recorded for AC#2/AC#4; tool verdict FAIL:
- arm 2 workers, PASS:
  - 252 requests, 77 aborted (fin/rst after 1024 body bytes), no 503;
  - gate balance 10/10, first 200 after stop 47 ms, heap 77256 -> 77248 (min 37884), probe max 5 of 16 pcbs.
- arm 4 workers, PASS:
  - 493 requests, 256 x 503 with Retry-After (248 file gate, 7 rest_busy, 1 low_heap), 73 aborted;
  - gate balance 10/10, first 200 after stop 60 ms, heap back to baseline, maxblk min 9716.
- arm 6 workers, FAIL:
  - 20 timeouts and 2 short 200s (body shorter than declared, the TASK-1162 shape);
  - gate balance 5/10 sequential;
  - probe max 11 of 16 pcbs.
- arm 8 workers, FAIL:
  - 21 timeouts and 2 short 200s;
  - device/info timed out after the arm and its quiet period;
  - gate balance 0/10;
  - probe max 15 of 16 pcbs, heap min 31496.
- After the run the device recovered by itself (gate balance 10/10, pair pass) without a reboot: bootcount 2 -> 2.
- Tool defect: the probe thread ended during the 8-worker arm with KeyError 'total_ms' (refresh_storm.py probe_loop reads r['total_ms'], and a timed-out exchange has none), so that arm has no probe samples after the crash.
- The maintainer rejected my fix of that tool line on 2026-10-02 and asked me to wait. AC#2 and AC#4 stay open until the maintainer decides between a fixed tool plus a re-run, or accepting this run as recorded.

2026-10-02 tool fix, after the maintainer chose 'fix the tool and re-run the storm':
- refresh_storm.py probe_loop now builds its line through probe_record(), which reads status and total_ms with .get().
- Evidence: tests/test_refresh_storm.py TestProbeRecord.test_probe_loop_keeps_probing_after_a_failed_connect drives the real probe_loop against a closed port.
  - Old tool (HEAD): fails with [KeyError('total_ms')], the bench crash.
  - Fix: passes, with one refused line per probe and the loop still running.
- Full suite: 39 tests OK. py_compile OK. evaluate.py --quick: 71 passed, 0 failed.
<!-- SECTION:NOTES:END -->
