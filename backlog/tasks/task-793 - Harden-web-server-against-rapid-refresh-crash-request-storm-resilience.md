---
id: TASK-793
title: Harden web server against rapid-refresh crash (request-storm resilience)
status: To Do
assignee: []
created_date: '2026-05-31 21:20'
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
- [ ] #1 Crash signature captured under a scripted rapid-refresh storm (reset-reason + per-request heap trace) and the exhausted resource identified (heap / file-handle / TCP), recorded in the task notes
- [ ] #2 High-frequency streaming handlers (sendIndex, serveCssRevalidated, /index.js, /graph.js) guard on low largest-free-block and return 503 Retry-After instead of allocating under memory pressure
- [ ] #3 sendIndex chunked stream loop stops when the client has disconnected (no writes to a dead socket)
- [ ] #4 handleFileUpload closes the static fsUploadFile on UPLOAD_FILE_ABORTED and defensively on UPLOAD_FILE_START if already open (upload-abort handle leak removed)
- [ ] #5 Scripted rapid-refresh storm no longer crashes or reboots the device; it degrades to 503/slower under extreme load; python build.py exits 0 and python evaluate.py --quick shows no new failures
- [ ] #6 Eventual fix is ported to the 2.0.0 worktree as a sibling task
<!-- AC:END -->
