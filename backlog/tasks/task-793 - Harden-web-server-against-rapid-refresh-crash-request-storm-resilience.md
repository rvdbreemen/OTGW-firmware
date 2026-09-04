---
id: TASK-793
title: Harden web server against rapid-refresh crash (request-storm resilience)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-31 21:20'
updated_date: '2026-09-04 06:58'
labels:
  - webui
  - stability
  - esp8266
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
A user reports that rapidly clicking browser refresh can crash/reboot the ESP8266. Framing note: this is NOT a debounce problem. A browser refresh is a burst of legitimate HTTP requests (plus aborted in-flight ones); the client cannot be debounced. The fix is to make the web server resilient to request bursts and aborted connections: bound and guard its scarce resources so a storm degrades gracefully (503 / slower) instead of crashing.

Key reasoning (corrects an initial mis-diagnosis): ESP8266WebServer is single-threaded (httpServer.handleClient() in loop(), one request per loop). Streaming handlers open AND close their File within one synchronous call, so file handles do NOT accumulate on the refresh path. Therefore a LittleFS file-handle leak is UNLIKELY the refresh trigger (the real handle leak is in handleFileUpload via the static fsUploadFile on upload-abort, but uploads do not happen on F5). The most likely refresh-storm cause is heap fragmentation / OOM in the high-frequency streaming handlers that have NO heap guard: sendIndex (index.html), serveCssRevalidated (4x CSS), /index.js, /graph.js. The REST path gates on free heap (restAPI.ino:834, <4096 -> 500) but these asset handlers do not. TCP PCB / socket churn (lingering TIME_WAIT, ~5 LWIP PCBs) may accelerate it; secondary.

MEASURE FIRST: do not fix blind. Capture the crash signature (ESP.getResetReason / boot log / MQTT lastreset; stack dump via exception decoder if any) and a per-request heap trace (getFreeHeap + getMaxFreeBlockSize) under a scripted refresh storm. The reset reason arbitrates: OOM -> heap; Exception+stack -> null-deref/leak; wdt reset -> loop hanging writing to a dead socket. Existing telemetry to reuse: getHeapHealth/logHeapStats/emergencyHeapRecovery (helperStuff.ino ~940/1097/1126), reset-reason (OTGW-firmware.ino ~72/173).

Candidate hardening (confirm scope after measuring): (1) heap-guard the streaming handlers (sendIndex, serveCssRevalidated, /index.js, /graph.js in FSexplorer.ino) - before streaming check getMaxFreeBlockSize() >= threshold, else send 503 + Retry-After and return, never allocate into OOM; reuse HEAP_* thresholds. (2) client-connected check in the sendIndex chunked stream loop (FSexplorer.ino ~143) - bail when httpServer.client().connected() is false. (3) fix the upload-abort leak - close fsUploadFile on UPLOAD_FILE_ABORTED and defensively on UPLOAD_FILE_START if already open (FSexplorer.ino ~506-540). (4) only if measurement implicates PCBs: keep-alive/socket tuning.

Cross-worktree: same web server exists on 2.0.0; port the eventual fix there as a sibling task.

Source: webserver-resilience audit (session 2026-05-31). build.py + evaluate.py are the compile/lint gates; the authoritative test is the on-device refresh storm.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Crash signature captured under a scripted rapid-refresh storm (reset-reason + per-request heap trace) and the exhausted resource identified (heap / file-handle / TCP), recorded in the task notes
- [x] #2 High-frequency streaming handlers (sendIndex, serveCssRevalidated, /index.js, /graph.js) guard on low largest-free-block and return 503 Retry-After instead of allocating under memory pressure
- [x] #3 sendIndex chunked stream loop stops when the client has disconnected (no writes to a dead socket)
- [x] #4 handleFileUpload closes the static fsUploadFile on UPLOAD_FILE_ABORTED and defensively on UPLOAD_FILE_START if already open (upload-abort handle leak removed)
- [x] #5 Scripted rapid-refresh storm no longer crashes or reboots the device; it degrades to 503/slower under extreme load; python build.py exits 0 and python evaluate.py --quick shows no new failures
- [x] #6 Eventual fix is ported to the 2.0.0 worktree as a sibling task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-08 backlog audit: verdict PARTIAL. Some hardening landed but named acceptance criteria remain unmet. Left in To Do rather than In Progress because nobody is actively working it; In Progress on this board means someone is.

2026-09-04: worked and measured. The task premise had aged, so it was re-checked before anything was written.

AC #1, crash signature: THERE IS NO CRASH ANY MORE, and that is the finding. A scripted storm (6 and 8 workers against index.html, index.css, index.js, graph.js and index_dark.css, one request in three abandoned mid-body) did not crash or reboot the device on either the pre-fix or the post-fix build. runtime.reboots held at 4 across the 8-worker run while runtime.uptime_sec advanced 83 to 116. The exhausted resource is serving concurrency, not memory: heap stayed near 17 KB free with a 14 KB largest block throughout, and the ESP8266 serves one HTTP client at a time, so past a few concurrent clients it degrades to timeouts by design. The crash this task was written against was the StoreProhibited from the unchecked ~1460-byte allocation in BufferedStreamDataSource::get_buffer(), identified and fixed under TASK-843.

AC #2, 503 guard on streaming handlers: already in place before this task was picked up. streamFileGuarded() (FSexplorer.ino) refuses with 503 below HTTP_SERVE_MIN_MAXBLOCK (TASK-843), and canServeHttp() gates handleClient() entirely (TASK-841, ADR-147). What was missing was Retry-After, which both refusals now carry, so a refused client backs off instead of re-requesting immediately and holding the heap in the state that caused the refusal.

AC #3, sendIndex stops on a dead client: implemented. It streamed about 11 KB without ever testing the socket; each sendContent() to a gone client still walks the write path before failing.

AC #4, upload-abort handle close: implemented, and it was a real leak the code already knew about. The comment at the write branch said outright that an aborted upload leaves fsUploadFile open because there is no UPLOAD_FILE_ABORTED branch. There is one now, plus a defensive close at UPLOAD_FILE_START, since fsUploadFile is a static and a leaked handle outlives the request that opened it. LittleFS allows a bounded number of open handles, so this was the storm-plus-upload path to filesystem exhaustion.

AC #5, storm outcome: 6 workers over 45 s went from 14 of 47 requests completed (33 timeouts) before to 18 of 47 (29 timeouts) after. At 8 workers the device is saturated: 3 of 55. No crash, no reboot, in any run. Build completed successfully (1.7.6-beta.1+aee0f3c, firmware and filesystem); evaluate.py --quick 35/37 pass, 0 failures.

AC #6, 2.0.0 port: TASK-1124 on the dev worktree (commit ec28d2e1). Framed as verify-then-port rather than copy, because that line runs ESPAsyncWebServer, so the abort and disconnect paths differ and its saturation limit is the LWIP pcb pool rather than single-client serving.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Three request-storm defects closed on the paths a rapid refresh actually stresses, and the crash the task was opened for is confirmed already fixed.

Aborted uploads leaked a file handle. handleFileUpload() had no UPLOAD_FILE_ABORTED branch, which the code acknowledged in a comment, so a client disconnecting mid-body left the static File open for the lifetime of the sketch, one per abort, until LittleFS ran out of open handles. There is now an abort branch and a defensive close at upload start.

sendIndex() streamed about 11 KB of index.html without checking whether the client was still connected. Every write to a gone socket still walks the write path before failing, which is wasted loop time exactly when the device is under pressure. It now stops.

The two heap-gated 503 refusals carry Retry-After, making the refusal actionable rather than merely visible.

The original crash does not reproduce: it was the unchecked ~1460-byte allocation in BufferedStreamDataSource::get_buffer(), fixed under TASK-843, with the TASK-841 gate and the TASK-1039 reaper on top. Measured under a scripted storm, the device now degrades to timeouts rather than crashing, because it serves one HTTP client at a time. Completed requests rose from 14 of 47 to 18 of 47 at six workers; reboot count and heap headroom were unchanged throughout.

Ported to 2.0.0 as TASK-1124.
<!-- SECTION:FINAL_SUMMARY:END -->
