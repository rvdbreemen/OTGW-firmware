---
id: TASK-1478
title: >-
  Weather fetch blocks the main loop ~5 s on every fetch: the stream parser
  waits for the 5 s byte timeout on a keep-alive connection
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T14:36:00Z'
updated_date: '2026-10-29T15:02:00Z'
labels:
  - sat
  - weather
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 638000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in gruijter's field capture (TASK-1474, alpha.527, 2026-10-29). All 18-20 Open-Meteo fetches take 5.15-5.43 s from 'Weather: fetching' to the result line, and all 8 loop stalls >= 1 s in the capture (5186-5281 ms) sit inside a fetch. Cause (second-reader checked): weatherParseStream() (SATweather.ino, wstreamGet ~L90-99) keeps reading until wstreamGet() returns -1, which only happens after 5000 ms without a byte or when the connection closes; arduino-esp32 HTTPClient defaults to reuse/keep-alive (HTTPClient.h:294, Connection: keep-alive), so the socket stays open and every fetch ends with a 5 s idle wait on the loop. getStreamPtr() is the raw socket, so chunked-encoding size lines also reach the parser. Unchanged on origin/dev. Same class as TASK-1471 (outbound connections must fail or finish quietly); this one hits a working internet connection.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A host or bench reproduction shows the ~5 s tail on the current code (old) and a fetch that ends within 1 s of the last JSON byte on the fix (new)
- [ ] #2 The parser stops at the end of the top-level JSON object or the response is read with HTTP/1.0 / Connection: close, and chunk-size lines can no longer reach the number parser
- [ ] #3 On the bench no [loop-stall] >= 1000 ms line appears around a weather fetch over at least 4 fetches
<!-- AC:END -->
