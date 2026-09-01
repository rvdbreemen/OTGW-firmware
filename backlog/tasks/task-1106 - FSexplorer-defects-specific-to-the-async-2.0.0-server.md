---
id: TASK-1106
title: FSexplorer defects specific to the async 2.0.0 server
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-01 18:39'
updated_date: '2026-09-01 19:24'
labels:
  - audit
  - fsexplorer
dependencies: []
ordinal: 268000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Three findings of the FSexplorer audit of 2026-09-01 that exist only on the 2.0.0 line. The async upload handler keeps its File handle and authorization flag in function-local statics, which was safe on the sequential ESP8266WebServer but not on ESPAsyncWebServer: two uploads in flight interleave at chunk level, so one file receives the other's bytes and both end truncated. The protectedFiles list in FSexplorer.html was never extended to the v2 and SAT assets this tree ships, so a Delete link appears next to v2.js and friends and removing one leaves the device serving a broken root page. The onNotFound handler decodes a URL that the async library has already decoded, so a stored name containing a percent or plus is looked up under a different name than it was written.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Two concurrent uploads each produce their own complete file, or the second is refused
- [x] #2 The v2 and SAT assets this tree serves are protected from deletion in the explorer
- [x] #3 A stored name containing a percent or plus is served under the name it was written
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed the three 2.0.0-only FSexplorer defects. Per-upload state moved off function-local statics onto request->_tempFile (a public member the request destructor closes), so two concurrent uploads no longer interleave into one File; the only residual is that the shared s_uploadOk status flag can attribute a status code to the wrong request, while the file contents are now correct per request. The protectedFiles whitelist gained the eight v2/SAT assets this tree ships so their Delete links no longer appear. onNotFound no longer double-decodes request->url() (the async library already decoded it), so a stored name containing a percent or plus is served under the name it was written. Build green on all three envs, evaluator clean. Noted for follow-up: v2.js FS_PROTECTED is a second, disagreeing list that still omits v2-bundle.css (out of scope here).
<!-- SECTION:FINAL_SUMMARY:END -->
