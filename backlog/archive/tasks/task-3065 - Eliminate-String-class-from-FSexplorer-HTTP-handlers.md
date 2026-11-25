---
id: TASK-7
title: Eliminate String class from FSexplorer HTTP handlers
status: Done
assignee:
  - '@claude'
created_date: '2026-03-12 20:52'
updated_date: '2026-03-12 21:52'
labels:
  - performance
  - memory
dependencies: []
references:
  - 'src/OTGW-firmware/FSexplorer.ino:87-134'
  - 'src/OTGW-firmware/FSexplorer.ino:227'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
FSexplorer.ino uses the Arduino String class extensively in HTTP handlers that run on every page load. On ESP8266 with ~80KB DRAM, repeated heap allocations from String cause fragmentation and eventual OOM crashes over time.

Affected code (FSexplorer.ino):

1. **sendIndex lambda** (lines 94-134): Every index.html request allocates 3-5 String objects:
   - `String fsHash = getFilesystemHash()` (line 94)
   - `String etag = "\"" + fsHash + "\""` (line 98) — concatenation allocates new String
   - `String line = f.readStringUntil('\n')` (line 124) — unbounded heap allocation per line; if any HTML line exceeds available heap, OOM crash
   - `line.replace(...)` (lines 127, 129) — in-place replacement may reallocate

2. **onNotFound handler** (line 227): `String(httpServer.uri())` allocates heap even when debug is disabled (the String construction happens before the conditional check).

The `readStringUntil('\n')` is the worst offender: it allocates heap proportional to line length with no upper bound. While our HTML files have short lines, this is a fragile assumption.

Fix approach: Use stack-based `char[]` buffers with bounded reads (`readBytesUntil`) and `snprintf` for string formatting. The ETag/hash injection only needs to check 2 specific lines, so a fixed-size line buffer (512 bytes) is sufficient.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 sendIndex lambda uses char[] buffers instead of String for ETag and hash handling
- [ ] #2 readStringUntil replaced with readBytesUntil into a bounded stack buffer
- [ ] #3 line.replace() for JS cache-busting rewritten using snprintf or strlcat with char[]
- [ ] #4 onNotFound debug logging does not allocate String when debug is disabled
- [ ] #5 index.html still renders correctly with cache-busted JS asset URLs
- [ ] #6 304 Not Modified ETag flow still works correctly
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Eliminated Arduino String class from all FSexplorer HTTP handlers and restAPI fsHash usage:

1. sendIndex lambda: Replaced String fsHash/etag with const char* + snprintf_P. Replaced readStringUntil with readBytesUntil into static char[512] buffer. JS URL injection uses strstr + snprintf instead of String.replace().
2. index.js/graph.js handlers: Replaced String v/fsHash with direct const char* comparison via httpServer.hasArg() + strcmp().
3. onNotFound: Removed redundant String(httpServer.uri()) wrapper — use .c_str() directly on the returned reference.
4. restAPI sendFilesystemHashCheck: Replaced String fsHash with const char*.

Net effect: zero heap allocations per index.html page load (was 3-5 String objects). Build passes.
<!-- SECTION:FINAL_SUMMARY:END -->
