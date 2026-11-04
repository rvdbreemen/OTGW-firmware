---
id: TASK-820
title: >-
  perf(rest): restFlushTxBuf allocates a 4KB heap String per flush on fragmented
  ESP32 heap
status: Done
assignee:
  - '@claude'
created_date: '2026-06-03 16:09'
updated_date: '2026-06-03 20:23'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
restFlushTxBuf (jsonStuff.ino:207) calls httpServer.sendContent(sTxBuf.data). There is no single-arg const char* sendContent overload, so this binds to sendContent(const String&), constructing a temporary ~4KB String (heap alloc) on every flush. On a 42-53% fragmented ESP32 heap (field: @sergeantd OTGW32) this is wasteful and a latent failure mode: if the alloc fails, content.length()==0 -> sendContent("",0) sets _chunked=false and silently terminates the response. Fix: call the two-arg overload httpServer.sendContent(sTxBuf.data, sTxBuf.len) -- no temporary, no alloc, no failure mode. Found while root-causing TASK-819 (NOT its cause; that was out-of-order raw sendContent). 2.0.0-only (coalescing is ESP32). Low risk, clear win.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 restFlushTxBuf uses the two-arg sendContent(const char*, size_t) overload; no String temporary allocated per flush
- [x] #2 Build green ESP32 + ESP8266; evaluate --quick no new failures
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped in 2.0.0-alpha.153 (commit e10d7e69, pushed origin/feature-dev-2.0.0). restFlushTxBuf() sent the coalescing buffer via single-arg httpServer.sendContent(sTxBuf.data), binding to sendContent(const String&) -> a ~4KB heap String temporary per flush on the no-PSRAM ESP32-S3 (~53KB free heap, 40-50% frag). A failed alloc would give length()==0 -> 0-length chunk -> early-terminated chunked response (silent truncation). Fixed by the explicit-length overload sendContent(sTxBuf.data, sTxBuf.len): no alloc, no String, no truncation failure mode; dropped the now-redundant NUL write. ESP32-only (HAS_REST_TX_COALESCING); ESP8266 untouched. Build green esp8266 fw+fs + esp32 fw+fs; evaluate --quick 0 fail. Decided AGAINST growing the buffer / whole-response Content-Length approach: RAM is the scarce resource on this board (no PSRAM, ~53KB free), and the existing 4KB coalescing already solved the perf problem (send times tens of ms).
<!-- SECTION:FINAL_SUMMARY:END -->
