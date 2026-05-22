---
id: TASK-662
title: >-
  Verify FSexplorer.html / index.html stream correctly from LittleFS (no
  f.readString)
status: To Do
assignee: []
created_date: '2026-05-22 06:23'
labels:
  - verification
dependencies: []
references:
  - src/OTGW-firmware/data/index.html
  - src/OTGW-firmware/data/FSexplorer.html
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Frontend agent flagged that FSexplorer.html (20 KB) and index.html (15 KB) on dev are past the CLAUDE.md '>10KB = CRITICAL, must stream' threshold. Delta in main..dev is zero, but the streaming-vs-load contract is silently auditable. On 2.0.0 the index.html is even bigger (35 KB).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Identify the request-handler path that serves data/*.html in src/OTGW-firmware/FSexplorer.ino (or wherever httpServer route handlers live for those URLs)
- [ ] #2 Confirm the handler uses httpServer.streamFile(f, mime) or chunked transfer (NOT f.readString())
- [ ] #3 If any handler loads into a String/char buffer, replace with streamFile and bump prerelease
- [ ] #4 Document the result in this task's Final Summary; if no code change needed, AC #3 is N/A
<!-- AC:END -->
