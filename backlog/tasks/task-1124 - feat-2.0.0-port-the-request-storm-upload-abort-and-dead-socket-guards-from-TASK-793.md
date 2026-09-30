---
id: TASK-1124
title: >-
  feat-2.0.0: port the request-storm upload-abort and dead-socket guards from
  TASK-793
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-04 06:57'
updated_date: '2026-09-30 10:28'
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
- [ ] #1 The async upload path closes its file handle when the client disconnects mid-body, verified by a scripted abort rather than by reading the code
- [ ] #2 A chunked response stops when its client is gone, verified under a storm
- [ ] #3 Heap-gated refusals carry Retry-After
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
<!-- SECTION:NOTES:END -->
