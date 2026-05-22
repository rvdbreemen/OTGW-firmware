---
id: TASK-662
title: >-
  Verify FSexplorer.html / index.html stream correctly from LittleFS (no
  f.readString)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-22 06:23'
updated_date: '2026-05-22 06:40'
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
- [x] #1 Identify the request-handler path that serves data/*.html in src/OTGW-firmware/FSexplorer.ino (or wherever httpServer route handlers live for those URLs)
- [x] #2 Confirm the handler uses httpServer.streamFile(f, mime) or chunked transfer (NOT f.readString())
- [ ] #3 If any handler loads into a String/char buffer, replace with streamFile and bump prerelease
- [x] #4 Document the result in this task's Final Summary; if no code change needed, AC #3 is N/A
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audited FSexplorer.ino streaming paths. Both >10KB HTML files are served via the correct streaming APIs — no code change required.

Findings:
- src/OTGW-firmware/FSexplorer.ino:113-114 — index.html served via httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN) + httpServer.sendContent() per line (chunked transfer with a 512-byte fixed buffer at line 117 to inject cache-bust ?v=<fsHash> into JS asset URLs). Buffer is static so no per-request heap traffic.
- src/OTGW-firmware/FSexplorer.ino:235 and :241 — FSexplorer.html served via httpServer.streamFile(f, F("text/html; charset=UTF-8")). Direct streamFile path.

No f.readString() or load-into-RAM patterns on either file. The CLAUDE.md ">10KB CRITICAL" rule is honoured.

AC #3 (fix + bump) is N/A because no code change was needed.

2.0.0-side audit covered separately via TASK-666 (port task in dev backlog) — index.html on 2.0.0 is 35 KB so the same streaming verification applies there.
<!-- SECTION:FINAL_SUMMARY:END -->
