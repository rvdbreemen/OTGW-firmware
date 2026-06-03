---
id: TASK-820
title: >-
  perf(rest): restFlushTxBuf allocates a 4KB heap String per flush on fragmented
  ESP32 heap
status: To Do
assignee: []
created_date: '2026-06-03 16:09'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
restFlushTxBuf (jsonStuff.ino:207) calls httpServer.sendContent(sTxBuf.data). There is no single-arg const char* sendContent overload, so this binds to sendContent(const String&), constructing a temporary ~4KB String (heap alloc) on every flush. On a 42-53% fragmented ESP32 heap (field: @sergeantd OTGW32) this is wasteful and a latent failure mode: if the alloc fails, content.length()==0 -> sendContent("",0) sets _chunked=false and silently terminates the response. Fix: call the two-arg overload httpServer.sendContent(sTxBuf.data, sTxBuf.len) -- no temporary, no alloc, no failure mode. Found while root-causing TASK-819 (NOT its cause; that was out-of-order raw sendContent). 2.0.0-only (coalescing is ESP32). Low risk, clear win.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 restFlushTxBuf uses the two-arg sendContent(const char*, size_t) overload; no String temporary allocated per flush
- [ ] #2 Build green ESP32 + ESP8266; evaluate --quick no new failures
<!-- AC:END -->
