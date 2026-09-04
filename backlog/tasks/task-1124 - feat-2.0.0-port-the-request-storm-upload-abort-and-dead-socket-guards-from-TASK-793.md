---
id: TASK-1124
title: >-
  feat-2.0.0: port the request-storm upload-abort and dead-socket guards from
  TASK-793
status: To Do
assignee: []
created_date: '2026-09-04 06:57'
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
